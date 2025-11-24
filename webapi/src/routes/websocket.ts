import type { FastifyInstance, FastifyRequest } from 'fastify';
import type { SocketStream } from '@fastify/websocket';
import { ConnectionManager, type WSMessage } from '../websocket/connection-manager';

/**
 * WebSocket Routes
 */
export async function websocketRoutes(fastify: FastifyInstance): Promise<void> {
  const { ipcBridge, schemaLoader } = fastify as any;
  const connectionManager = new ConnectionManager(fastify.log);

  // IPC 브리지에서 데이터 변경 이벤트 수신
  if (ipcBridge) {
    ipcBridge.on('data_changed', (key: string, data: unknown) => {
      connectionManager.notifySubscribers(key, data);
    });
  }

  /**
   * WebSocket /api/ws
   */
  fastify.get(
    '/ws',
    { websocket: true },
    (connection: SocketStream, request: FastifyRequest) => {
      const { socket } = connection;
      const connectionId = connectionManager.addConnection(socket);

      fastify.log.info(
        { connectionId, remoteAddress: request.socket.remoteAddress },
        'WebSocket client connected',
      );

      // 메시지 수신 처리
      socket.on('message', (rawMessage: Buffer) => {
        try {
          const message = JSON.parse(rawMessage.toString()) as WSMessage;

          switch (message.type) {
            case 'subscribe':
              handleSubscribe(connectionId, message);
              break;

            case 'unsubscribe':
              handleUnsubscribe(connectionId, message);
              break;

            default:
              sendError(connectionId, `Unknown message type: ${message.type}`);
          }
        } catch (error) {
          fastify.log.error({ connectionId, error }, 'Failed to process message');
          sendError(connectionId, 'Invalid message format');
        }
      });

      // 연결 종료 처리
      socket.on('close', () => {
        fastify.log.info({ connectionId }, 'WebSocket client disconnected');
        connectionManager.removeConnection(connectionId);
      });

      // 에러 처리
      socket.on('error', (error: Error) => {
        fastify.log.error({ connectionId, error }, 'WebSocket error');
        connectionManager.removeConnection(connectionId);
      });

      // Ping/Pong for keepalive
      const pingInterval = setInterval(() => {
        if (socket.readyState === 1) {
          socket.ping();
        } else {
          clearInterval(pingInterval);
        }
      }, 30000); // 30초마다 ping

      socket.on('close', () => {
        clearInterval(pingInterval);
      });
    },
  );

  /**
   * Subscribe 메시지 처리
   */
  function handleSubscribe(connectionId: string, message: WSMessage) {
    const keys = message.keys || (message.key ? [message.key] : []);

    if (keys.length === 0) {
      sendError(connectionId, 'No keys specified for subscription');
      return;
    }

    const results: { key: string; success: boolean; error?: string }[] = [];

    for (const key of keys) {
      // 스키마 검증: 키 존재 여부
      if (!schemaLoader.hasKey(key)) {
        results.push({ key, success: false, error: 'Key not found in schema' });
        continue;
      }

      // 스키마 검증: 읽기 권한
      if (!schemaLoader.canRead(key)) {
        results.push({ key, success: false, error: 'Read permission denied' });
        continue;
      }

      // 구독 처리
      const success = connectionManager.subscribe(connectionId, key);
      results.push({ key, success });
    }

    // 응답 전송
    connectionManager.sendToConnection(connectionId, {
      type: 'subscribe',
      data: results,
      timestamp: new Date().toISOString(),
    });

    fastify.log.debug({ connectionId, keys: results }, 'Subscription processed');
  }

  /**
   * Unsubscribe 메시지 처리
   */
  function handleUnsubscribe(connectionId: string, message: WSMessage) {
    const keys = message.keys || (message.key ? [message.key] : []);

    if (keys.length === 0) {
      sendError(connectionId, 'No keys specified for unsubscription');
      return;
    }

    const results: { key: string; success: boolean }[] = [];

    for (const key of keys) {
      const success = connectionManager.unsubscribe(connectionId, key);
      results.push({ key, success });
    }

    // 응답 전송
    connectionManager.sendToConnection(connectionId, {
      type: 'unsubscribe',
      data: results,
      timestamp: new Date().toISOString(),
    });

    fastify.log.debug({ connectionId, keys: results }, 'Unsubscription processed');
  }

  /**
   * 에러 메시지 전송
   */
  function sendError(connectionId: string, error: string) {
    connectionManager.sendToConnection(connectionId, {
      type: 'error',
      error,
      timestamp: new Date().toISOString(),
    });
  }

  /**
   * WebSocket 통계 조회 (디버깅용)
   */
  fastify.get('/ws/stats', async (request, reply) => {
    const stats = connectionManager.getStats();
    return reply.code(200).send(stats);
  });

  // 주기적으로 비활성 연결 정리 (5분 이상 활동 없으면 제거)
  const cleanupInterval = setInterval(() => {
    connectionManager.cleanupInactiveConnections(5 * 60 * 1000);
  }, 60 * 1000); // 1분마다 실행

  // Fastify 종료 시 cleanup interval 정리
  fastify.addHook('onClose', async () => {
    clearInterval(cleanupInterval);
  });
}
