import { Component } from '@angular/core';
import { RouterOutlet } from '@angular/router';
import { WebsocketComponent } from './websocket/websocket.component';

@Component({
  selector: 'app-root',
  standalone: true,
  imports: [RouterOutlet, WebsocketComponent],
  templateUrl: './app.component.html',
  styleUrl: './app.component.scss'
})
export class AppComponent {
  title = 'project';
}
