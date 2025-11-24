import * as net from 'net';
import { EventEmitter } from 'events';
import type { IPCMessage, IPCResponse } from '../types';

interface PendingRequest {
  resolve: (value: any) => void;
  reject: (error: Error) => void;
  timeout?: NodeJS.Timeout;
}

interface IPCClientOptions {
  reconnectDelay?: number;
  maxReconnectAttempts?: number;
  timeout?: number;
}

/**
 * Unix Domain Socket IPC 클라이언트
 */
export class IPCClient extends EventEmitter {
  private socketPath: string;
  private options: Required<IPCClientOptions>;
  private socket: net.Socket | null = null;
  private isConnected = false;
  private reconnectAttempts = 0;
  private pendingRequests = new Map<number, PendingRequest>();
  private requestIdCounter = 0;

  constructor(socketPath: string, options: IPCClientOptions = {}) {
    super();
    this.socketPath = socketPath;
    this.options = {
      reconnectDelay: options.reconnectDelay || 5000,
      maxReconnectAttempts: options.maxReconnectAttempts || 10,
      timeout: options.timeout || 5000,
    };
  }

  /**
   * IPC 서버에 연결
   */
  connect(): Promise<void> {
    return new Promise((resolve, reject) => {
      this.socket = net.createConnection(this.socketPath);

      this.socket.on('connect', () => {
        this.isConnected = true;
        this.reconnectAttempts = 0;
        this.emit('connected');
        resolve();
      });

      this.socket.on('data', (data: Buffer) => {
        this.handleData(data);
      });

      this.socket.on('error', (error: Error) => {
        this.emit('error', error);
        if (!this.isConnected) {
          reject(error);
        }
      });

      this.socket.on('close', () => {
        this.isConnected = false;
        this.emit('disconnected');
        this.handleReconnect();
      });

      this.socket.setTimeout(this.options.timeout, () => {
        this.emit('timeout');
        this.socket?.destroy();
      });
    });
  }

  /**
   * 재연결 처리
   */
  private handleReconnect(): void {
    if (this.reconnectAttempts < this.options.maxReconnectAttempts) {
      this.reconnectAttempts++;
      this.emit('reconnecting', this.reconnectAttempts);

      setTimeout(() => {
        this.connect().catch((error) => {
          this.emit('reconnect-failed', error);
        });
      }, this.options.reconnectDelay);
    } else {
      this.emit('max-reconnect-attempts-reached');
    }
  }

  /**
   * 수신 데이터 처리
   */
  private handleData(data: Buffer): void {
    try {
      const message: IPCResponse | { type: string; data: any } = JSON.parse(data.toString());

      // 응답 메시지 처리
      if ('request_id' in message && this.pendingRequests.has(message.request_id)) {
        const pending = this.pendingRequests.get(message.request_id)!;
        this.pendingRequests.delete(message.request_id);

        if (pending.timeout) {
          clearTimeout(pending.timeout);
        }

        if (message.error) {
          pending.reject(new Error(message.error));
        } else {
          pending.resolve(message.data);
        }
      }

      // 알림 메시지 처리 (WebSocket용)
      if ('type' in message && message.type === 'notification') {
        this.emit('notification', message.data);
      }
    } catch (error) {
      this.emit('parse-error', error);
    }
  }

  /**
   * 요청 전송
   */
  send<T = any>(request: Omit<IPCMessage, 'request_id'>): Promise<T> {
    return new Promise((resolve, reject) => {
      if (!this.isConnected) {
        return reject(new Error('Not connected to IPC server'));
      }

      const requestId = ++this.requestIdCounter;
      const message: IPCMessage = {
        request_id: requestId,
        ...request,
      };

      // Timeout 설정
      const timeout = setTimeout(() => {
        if (this.pendingRequests.has(requestId)) {
          this.pendingRequests.delete(requestId);
          reject(new Error('Request timeout'));
        }
      }, this.options.timeout);

      // Pending request 저장
      this.pendingRequests.set(requestId, { resolve, reject, timeout });

      // 메시지 전송
      this.socket!.write(JSON.stringify(message) + '\n', (error) => {
        if (error) {
          clearTimeout(timeout);
          this.pendingRequests.delete(requestId);
          reject(error);
        }
      });
    });
  }

  /**
   * 연결 종료
   */
  disconnect(): void {
    if (this.socket) {
      this.socket.destroy();
      this.socket = null;
      this.isConnected = false;
    }

    // 모든 pending requests 거부
    this.pendingRequests.forEach(({ reject, timeout }) => {
      if (timeout) clearTimeout(timeout);
      reject(new Error('Connection closed'));
    });
    this.pendingRequests.clear();
  }

  /**
   * 연결 상태 확인
   */
  connected(): boolean {
    return this.isConnected;
  }
}
