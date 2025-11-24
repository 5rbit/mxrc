import 'dotenv/config';
import fastify, { type FastifyInstance } from 'fastify';
import cors from '@fastify/cors';
import websocket from '@fastify/websocket';
import { createRequestLoggerOptions, logger, setupIPCLogging } from './utils/logger';
import { errorHandler, notFoundHandler } from './middleware/error-handler';
import { systemdNotify } from './utils/systemd-notify';
import { IPCBridge } from './ipc/bridge';
import { SchemaLoader } from './config/schema-loader';
import { SchemaGenerator } from './config/schema-generator';
import { getCorsOptions } from './middleware/cors';

// Fastify 인스턴스 타입 확장
declare module 'fastify' {
  interface FastifyInstance {
    ipcBridge: IPCBridge;
    schemaLoader: SchemaLoader;
    schemaGenerator: SchemaGenerator;
  }
}

/**
 * Fastify 서버 초기화 및 시작
 */
async function startServer(): Promise<FastifyInstance> {
  // Fastify 인스턴스 생성
  const app = fastify({
    logger: true,
  });

  try {
    // IPC 스키마 로더 초기화
    const schemaPath = process.env.IPC_SCHEMA_PATH || '../config/ipc/ipc-schema.yaml';
    app.schemaLoader = new SchemaLoader(schemaPath);
    app.schemaLoader.load();
    logger.info(`IPC schema loaded: ${app.schemaLoader.getAllKeys().length} keys`);

    // Zod 스키마 생성기 초기화
    app.schemaGenerator = new SchemaGenerator();

    // IPC 브리지 초기화 및 연결
    const socketPath = process.env.IPC_SOCKET_PATH || '/tmp/mxrc_ipc.sock';
    app.ipcBridge = new IPCBridge(socketPath, {
      timeout: parseInt(process.env.IPC_TIMEOUT_MS || '5000', 10),
    });

    // IPC 이벤트 로깅
    setupIPCLogging(app.ipcBridge, logger);

    // IPC 연결 (실패 시 경고만 출력, 서버는 계속 시작)
    try {
      await app.ipcBridge.connect();
      logger.info('IPC bridge connected');
    } catch (error) {
      logger.warn({ err: error }, 'IPC bridge connection failed, starting server anyway');
    }

    // CORS 플러그인 등록
    await app.register(cors, getCorsOptions());

    // WebSocket 플러그인 등록
    await app.register(websocket);

    // Routes 등록
    const { datastoreRoutes } = await import('./routes/datastore');
    const { healthRoutes } = await import('./routes/health');
    const { websocketRoutes } = await import('./routes/websocket');
    await app.register(datastoreRoutes, { prefix: '/api' });
    await app.register(healthRoutes, { prefix: '/api' });
    await app.register(websocketRoutes, { prefix: '/api' });

    // Error handlers
    app.setErrorHandler((error, request, reply) => {
      errorHandler(logger)(error, request, reply);
    });
    app.setNotFoundHandler((request, reply) => {
      notFoundHandler()(request, reply);
    });

    // 서버 시작
    const port = parseInt(process.env.PORT || '3000', 10);
    const host = process.env.HOST || '0.0.0.0';

    await app.listen({ port, host });
    logger.info(`Server listening on ${host}:${port}`);

    // systemd ready notification
    systemdNotify.ready();
    systemdNotify.status('Server running');

    // systemd Watchdog 시작 (health check 포함)
    systemdNotify.startWatchdog(async () => {
      // Health check: IPC 연결 상태 확인
      return app.ipcBridge?.isConnected() || false;
    });

    // Graceful shutdown
    const shutdown = async (signal: string) => {
      logger.info(`Received ${signal}, shutting down gracefully...`);
      systemdNotify.stopping();
      systemdNotify.stopWatchdog();

      // IPC 연결 종료
      if (app.ipcBridge) {
        app.ipcBridge.disconnect();
      }

      // 서버 종료
      await app.close();
      logger.info('Server closed');
      process.exit(0);
    };

    process.on('SIGTERM', () => shutdown('SIGTERM'));
    process.on('SIGINT', () => shutdown('SIGINT'));
  } catch (error) {
    logger.error({ err: error }, 'Failed to start server');
    process.exit(1);
  }

  return app;
}

// 서버 시작
if (require.main === module) {
  startServer().catch((error) => {
    console.error('Fatal error:', error);
    process.exit(1);
  });
}

export default startServer;
