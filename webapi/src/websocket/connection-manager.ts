import type { FastifyBaseLogger } from 'fastify';
import type { WebSocket } from 'ws';

/**
 * WebSocket 연결 정보
 */
export interface ConnectionInfo {
  socket: WebSocket;
  subscriptions: Set<string>;
  connectedAt: Date;
  lastActivity: Date;
}

/**
 * WebSocket 메시지 타입
 */
export interface WSMessage {
  type: 'subscribe' | 'unsubscribe' | 'notification' | 'error';
  key?: string;
  keys?: string[];
  data?: unknown;
  error?: string;
  timestamp?: string;
}

/**
 * WebSocket 연결 관리자
 */
export class ConnectionManager {
  private connections: Map<string, ConnectionInfo> = new Map();
  private keySubscriptions: Map<string, Set<string>> = new Map();
  private logger: FastifyBaseLogger;
  private connectionIdCounter = 0;

  constructor(logger: FastifyBaseLogger) {
    this.logger = logger;
  }

  /**
   * 새로운 연결 추가
   */
  addConnection(socket: WebSocket): string {
    const connectionId = `conn_${++this.connectionIdCounter}_${Date.now()}`;

    this.connections.set(connectionId, {
      socket,
      subscriptions: new Set(),
      connectedAt: new Date(),
      lastActivity: new Date(),
    });

    this.logger.info({ connectionId }, 'WebSocket connection established');
    return connectionId;
  }

  /**
   * 연결 제거
   */
  removeConnection(connectionId: string): void {
    const conn = this.connections.get(connectionId);
    if (!conn) return;

    // 모든 구독 해제
    for (const key of conn.subscriptions) {
      this.unsubscribe(connectionId, key);
    }

    this.connections.delete(connectionId);
    this.logger.info({ connectionId }, 'WebSocket connection closed');
  }

  /**
   * 키 구독
   */
  subscribe(connectionId: string, key: string): boolean {
    const conn = this.connections.get(connectionId);
    if (!conn) {
      this.logger.warn({ connectionId, key }, 'Connection not found for subscription');
      return false;
    }

    // 연결의 구독 목록에 추가
    conn.subscriptions.add(key);
    conn.lastActivity = new Date();

    // 키의 구독자 목록에 추가
    if (!this.keySubscriptions.has(key)) {
      this.keySubscriptions.set(key, new Set());
    }
    this.keySubscriptions.get(key)!.add(connectionId);

    this.logger.debug({ connectionId, key }, 'Subscribed to key');
    return true;
  }

  /**
   * 키 구독 해제
   */
  unsubscribe(connectionId: string, key: string): boolean {
    const conn = this.connections.get(connectionId);
    if (!conn) {
      this.logger.warn({ connectionId, key }, 'Connection not found for unsubscription');
      return false;
    }

    // 연결의 구독 목록에서 제거
    conn.subscriptions.delete(key);
    conn.lastActivity = new Date();

    // 키의 구독자 목록에서 제거
    const subscribers = this.keySubscriptions.get(key);
    if (subscribers) {
      subscribers.delete(connectionId);
      if (subscribers.size === 0) {
        this.keySubscriptions.delete(key);
      }
    }

    this.logger.debug({ connectionId, key }, 'Unsubscribed from key');
    return true;
  }

  /**
   * 특정 키의 모든 구독자에게 알림 전송
   */
  notifySubscribers(key: string, data: unknown): number {
    const subscribers = this.keySubscriptions.get(key);
    if (!subscribers || subscribers.size === 0) {
      return 0;
    }

    let sentCount = 0;
    const message: WSMessage = {
      type: 'notification',
      key,
      data,
      timestamp: new Date().toISOString(),
    };

    for (const connectionId of subscribers) {
      const conn = this.connections.get(connectionId);
      if (!conn) continue;

      try {
        if (conn.socket.readyState === 1) { // OPEN state
          conn.socket.send(JSON.stringify(message));
          conn.lastActivity = new Date();
          sentCount++;
        } else {
          // 연결이 끊어진 경우 정리
          this.removeConnection(connectionId);
        }
      } catch (error) {
        this.logger.error({ connectionId, key, error }, 'Failed to send notification');
        this.removeConnection(connectionId);
      }
    }

    this.logger.debug({ key, sentCount }, 'Notifications sent');
    return sentCount;
  }

  /**
   * 특정 연결에 메시지 전송
   */
  sendToConnection(connectionId: string, message: WSMessage): boolean {
    const conn = this.connections.get(connectionId);
    if (!conn) return false;

    try {
      if (conn.socket.readyState === 1) {
        conn.socket.send(JSON.stringify(message));
        conn.lastActivity = new Date();
        return true;
      }
    } catch (error) {
      this.logger.error({ connectionId, error }, 'Failed to send message');
      this.removeConnection(connectionId);
    }

    return false;
  }

  /**
   * 연결 통계 조회
   */
  getStats() {
    return {
      totalConnections: this.connections.size,
      totalSubscriptions: Array.from(this.connections.values())
        .reduce((sum, conn) => sum + conn.subscriptions.size, 0),
      subscribedKeys: this.keySubscriptions.size,
    };
  }

  /**
   * 특정 키의 구독자 수 조회
   */
  getSubscriberCount(key: string): number {
    return this.keySubscriptions.get(key)?.size || 0;
  }

  /**
   * 비활성 연결 정리 (heartbeat용)
   */
  cleanupInactiveConnections(timeoutMs: number): number {
    const now = Date.now();
    let cleanedCount = 0;

    for (const [connectionId, conn] of this.connections) {
      const inactiveMs = now - conn.lastActivity.getTime();
      if (inactiveMs > timeoutMs) {
        this.removeConnection(connectionId);
        cleanedCount++;
      }
    }

    if (cleanedCount > 0) {
      this.logger.info({ cleanedCount }, 'Cleaned up inactive connections');
    }

    return cleanedCount;
  }
}
