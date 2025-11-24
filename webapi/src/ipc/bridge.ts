import { IPCClient } from './client';
import type { DatastoreValue, DatastoreWriteResponse } from '../types';
import { EventEmitter } from 'events';

type SubscriptionCallback = (data: DatastoreValue) => void;

interface IPCBridgeOptions {
  timeout?: number;
  reconnectDelay?: number;
  maxReconnectAttempts?: number;
}

/**
 * IPC 브리지
 * IPC 클라이언트를 래핑하여 Datastore 작업을 위한 고수준 API 제공
 */
export class IPCBridge extends EventEmitter {
  private client: IPCClient;
  private subscriptions = new Map<string, Set<SubscriptionCallback>>();

  constructor(socketPath: string, options: IPCBridgeOptions = {}) {
    super();
    this.client = new IPCClient(socketPath, options);
  }

  /**
   * IPC 서버에 연결
   */
  async connect(): Promise<void> {
    await this.client.connect();

    // Notification 리스너 등록
    this.client.on('notification', (data: any) => {
      this.handleNotification(data);
    });
  }

  /**
   * Datastore 키 읽기
   */
  async read<T = unknown>(key: string): Promise<DatastoreValue<T>> {
    const response = await this.client.send<{
      value: T;
      version: number;
      timestamp: number | string;
    }>({
      command: 'read',
      key,
    });

    return {
      key,
      value: response.value,
      version: response.version,
      timestamp: new Date(response.timestamp),
    };
  }

  /**
   * Datastore 키 쓰기
   */
  async write(key: string, value: unknown): Promise<DatastoreWriteResponse> {
    const response = await this.client.send<{
      success: boolean;
      version: number;
    }>({
      command: 'write',
      key,
      value,
    });

    return {
      key,
      success: response.success,
      version: response.version,
    };
  }

  /**
   * Datastore 키 구독 (변경 알림)
   */
  async subscribe(
    keys: string | string[],
    callback: SubscriptionCallback,
  ): Promise<void> {
    const keyArray = Array.isArray(keys) ? keys : [keys];

    // IPC 서버에 구독 요청
    await this.client.send({
      command: 'subscribe',
      keys: keyArray,
    });

    // 로컬 구독 맵에 저장
    keyArray.forEach((key) => {
      if (!this.subscriptions.has(key)) {
        this.subscriptions.set(key, new Set());
      }
      this.subscriptions.get(key)!.add(callback);
    });
  }

  /**
   * Datastore 키 구독 해제
   */
  async unsubscribe(
    keys: string | string[],
    callback?: SubscriptionCallback,
  ): Promise<void> {
    const keyArray = Array.isArray(keys) ? keys : [keys];

    // 로컬 구독 맵에서 제거
    keyArray.forEach((key) => {
      if (this.subscriptions.has(key)) {
        if (callback) {
          this.subscriptions.get(key)!.delete(callback);
        } else {
          this.subscriptions.get(key)!.clear();
        }

        // 구독자가 없으면 맵에서 제거
        if (this.subscriptions.get(key)!.size === 0) {
          this.subscriptions.delete(key);
        }
      }
    });

    // 구독자가 없는 키만 IPC 서버에 구독 해제 요청
    const keysToUnsubscribe = keyArray.filter((key) => !this.subscriptions.has(key));

    if (keysToUnsubscribe.length > 0) {
      await this.client.send({
        command: 'unsubscribe',
        keys: keysToUnsubscribe,
      });
    }
  }

  /**
   * Notification 처리
   */
  private handleNotification(data: {
    key: string;
    value: unknown;
    version: number;
    timestamp: number | string;
  }): void {
    const { key, value, version, timestamp } = data;

    // WebSocket 알림용 이벤트 발생
    this.emit('data_changed', key, {
      key,
      value,
      version,
      timestamp: new Date(timestamp),
    });

    if (this.subscriptions.has(key)) {
      const callbacks = this.subscriptions.get(key)!;
      callbacks.forEach((callback) => {
        try {
          callback({
            key,
            value,
            version,
            timestamp: new Date(timestamp),
          });
        } catch (error) {
          console.error(`Subscription callback error for key ${key}:`, error);
        }
      });
    }
  }

  /**
   * 연결 종료
   */
  disconnect(): void {
    this.subscriptions.clear();
    this.client.disconnect();
  }

  /**
   * 연결 상태 확인
   */
  isConnected(): boolean {
    return this.client.connected();
  }
}
