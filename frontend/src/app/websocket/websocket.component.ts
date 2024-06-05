import { Component, ChangeDetectorRef, OnDestroy, ViewChild, ElementRef, AfterViewInit, Renderer2 } from '@angular/core';
import { Subscription } from 'rxjs';
import { WebsocketService } from '../websocket-service/websocket.service';
import { FormsModule } from '@angular/forms';
import { CommonModule } from '@angular/common';
import { CdkVirtualScrollViewport } from '@angular/cdk/scrolling';
import cytoscape from 'cytoscape';
import fcose from 'cytoscape-fcose';
import { debounce } from 'lodash';

cytoscape.use(fcose);

@Component({
  selector: 'app-websocket',
  standalone: true,
  imports: [FormsModule, CommonModule],
  templateUrl: './websocket.component.html',
  styleUrls: ['./websocket.component.scss']
})
export class WebsocketComponent implements OnDestroy, AfterViewInit {
  message: string = '';
  receivedMessages: string[] = [];
  branches: { [key: string]: { messages: string[], expanded: boolean } } = {};
  private socketSubscription: Subscription | undefined;
  searchQuery: string = '';
  filteredBranches: { key: string, value: { messages: string[], expanded: boolean } }[] = [];
  stopButtonDisabled: boolean = false;
  stopInProgress: boolean = false;
  msgJson: any;
  domainsonly: boolean = false;
  ignorerobots: boolean = false;
  notifications: { id: number, message: string }[] = [];
  notificationCounter: number = 0;

  @ViewChild(CdkVirtualScrollViewport) viewport: CdkVirtualScrollViewport | undefined;
  @ViewChild('cy') cyRef: ElementRef | undefined;
  cy: any;
  private elementsToAdd: any[] = [];
  private addElementsDebounced = debounce(this.addElementsToCytoscape, 300);

  constructor(private websocketService: WebsocketService, private cdr: ChangeDetectorRef, private renderer: Renderer2) {
    this.connectWebSocket();
  }

  ngOnDestroy(): void {
    this.disconnectWebSocket();
  }

  ngAfterViewInit() {
    this.cy = cytoscape({
      container: this.cyRef?.nativeElement,
      elements: [],
      style: [
        {
          selector: 'node',
          style: {
            'label': 'data(id)',
            'text-valign': 'center',
            'color': '#FFBF00',
            'background-color': '#0074D9',
            'background-opacity': 0.8,
            'border-width': 2,
            'border-color': '#001f3f',
            'shape': 'round-rectangle',
            'width': 'mapData(weight, 10, 100, 20, 60)', 
            'height': 'mapData(weight, 10, 100, 20, 60)'
          }
        },
        {
          selector: 'edge',
          style: {
            'width': 2,
            'line-color': '#7FDBFF',
            'target-arrow-color': '#7FDBFF',
            'target-arrow-shape': 'triangle',
            'curve-style': 'bezier',
            'arrow-scale': 1.5
          }
        },
        {
          selector: 'edge[bidirectional]',
          style: {
            'width': 3,
            'line-color': '#FF4136',
            'source-arrow-color': '#FF4136',
            'source-arrow-shape': 'triangle',
            'target-arrow-color': '#FF4136',
            'target-arrow-shape': 'triangle',
            'curve-style': 'bezier',
            'arrow-scale': 1.8
          }
        }
      ],
      layout: {
        name: 'fcose'
      }
    });
  }

  connectWebSocket(): void {
    this.socketSubscription = this.websocketService.connect().subscribe(
      (message: any) => {
        console.log(message);
        if (message.message_type === 'new_domain') {
          this.queueDomainRelationship(message.parent_domain, message.child_domain);
        }
        if (message.message_type === 'stop_start') {
          this.stopButtonDisabled = true;
          this.addNotification('Stop process started');
        }
        if (message.message_type === 'stop_end') {
          this.stopButtonDisabled = false;
          this.stopInProgress = false;
          this.addNotification('Stop process ended');
        }
        this.receivedMessages.push(message.childLink);
        this.createBranches(this.receivedMessages);
        this.filterBranches();
        this.cdr.detectChanges();
      },
      (error: any) => {
        console.error('WebSocket error:', error);
        this.disconnectWebSocket();
        this.connectWebSocket();
      }
    );
  }
  

  createBranches(receivedMessages: string[]): void {
    for (const message of receivedMessages) {
      let domain = '';
      try {
        const url = new URL(message);
        domain = url.hostname;
      } catch (error) {
        console.error('Invalid URL:', error);
      }

      if (!this.branches[domain]) {
        this.branches[domain] = { messages: [message], expanded: false };
      } else {
        if (!this.branches[domain].messages.includes(message)) {
          this.branches[domain].messages.push(message);
        }
      }
    }
  }

  filterBranches() {
    this.filteredBranches = Object.keys(this.branches)
      .filter(key => key.toLowerCase().includes(this.searchQuery.toLowerCase()) && this.branches[key].messages.length > 0)
      .map(key => ({ key: key, value: this.branches[key] }));
  }
  
  disconnectWebSocket(): void {
    if (this.socketSubscription) {
      this.socketSubscription.unsubscribe();
    }
    this.websocketService.close();
  }

  sendMessage(): void {
    this.websocketService.sendMessage(this.message);
  }

  toggleBranch(branch: { key: string, value: { messages: string[], expanded: boolean } }): void {
    branch.value.expanded = !branch.value.expanded;
  }

  startCrawl(): void {
    const startMessage = {
      action: 'start',
      domain: this.message,
      params: {
        domainsOnly: false,
        ignoreRobots: false
      }
    };
    this.websocketService.sendMessage(startMessage);
    this.stopButtonDisabled = false;
    this.stopInProgress = false;
    this.addNotification('Crawl started');
  }

  stopCrawl(): void {
    if (this.stopInProgress) return;
    this.stopInProgress = true;

    const stopMessage = {
      action: 'stop'
    };
    this.websocketService.sendMessage(stopMessage);
  }

  clear(): void {
    this.stopCrawl();
    this.cy.elements().remove();
    this.clearBranches();
  }

  clearBranches(): void {
    this.receivedMessages = [];
    this.branches = {};
    this.filteredBranches = [];
    this.searchQuery = '';
    this.cdr.detectChanges();
    this.filterBranches();
  }

  queueDomainRelationship(parentDomain: string, childDomain: string): void {
    this.elementsToAdd.push({ parentDomain, childDomain });
    this.addElementsDebounced();
  }

  addElementsToCytoscape(): void {
    if (!this.cy) return;

    this.cy.batch(() => {
      for (const { parentDomain, childDomain } of this.elementsToAdd) {
        if (!this.cy.getElementById(parentDomain).length) {
          this.cy.add({
            group: 'nodes',
            data: { id: parentDomain }
          });
        }

        if (!this.cy.getElementById(childDomain).length) {
          this.cy.add({
            group: 'nodes',
            data: { id: childDomain }
          });
        }

        const edgeId = `${parentDomain}-${childDomain}`;
        const reverseEdgeId = `${childDomain}-${parentDomain}`;

        const existingEdge = this.cy.getElementById(edgeId);
        const reverseEdge = this.cy.getElementById(reverseEdgeId);

        if (existingEdge.length && reverseEdge.length) {
          existingEdge.data('bidirectional', true);
          reverseEdge.remove();
        } else if (existingEdge.length) {
          existingEdge.data('bidirectional', true);
        } else if (reverseEdge.length) {
          reverseEdge.data('bidirectional', true);
        } else {
          this.cy.add({
            group: 'edges',
            data: { id: edgeId, source: parentDomain, target: childDomain }
          });
        }
      }

      this.cy.layout({
        name: 'fcose',
        animate: true,
        animationDuration: 1000,
        randomize: false,
        fit: true,
        padding: 30,
        nodeDimensionsIncludeLabels: true,
        uniformNodeDimensions: true,
        nodeRepulsion: 4500,
        idealEdgeLength: 300,
        edgeElasticity: 0.45,
        gravity: 0.25
      }).run();
    });

    this.elementsToAdd = [];
  }

  addNotification(message: string): void {
    const id = this.notificationCounter++;
    this.notifications.push({ id, message });

    setTimeout(() => {
      this.notifications = this.notifications.filter(notification => notification.id !== id);
      this.cdr.detectChanges();
    }, 3000);
  }
}
