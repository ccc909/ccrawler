// Import the necessary modules and dependencies
import { Injectable } from '@angular/core';
import { webSocket, WebSocketSubject } from 'rxjs/webSocket';
import { first } from 'rxjs/operators';

@Injectable({
  providedIn: 'root'
})
export class WebsocketService {
  private socket$: WebSocketSubject<any>;

  constructor() {
    this.socket$ = webSocket('ws://127.0.0.1:9001');
  }

  connect(): WebSocketSubject<any> {
    return this.socket$;
  }

  sendMessage(message: any): void {
    this.socket$.next(message);
  }

  receiveOneMessage(): Promise<any> {
    return new Promise<any>((resolve, reject) => {
      this.socket$.pipe(first()).subscribe({
        next: (message) => {
          resolve(message);
        },
        error: (err) => {
          reject(err);
        },
        complete: () => {
          // WebSocket completed
        }
      });
    });
  }

  close(): void {
    this.socket$.complete();
  }
}
