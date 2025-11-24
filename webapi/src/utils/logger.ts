import pino, { type Logger } from 'pino';
import type { IPCBridge } from '../ipc/bridge';

export function createLogger(options: Record<string, any> = {}): Logger {
  const logLevel = process.env.LOG_LEVEL || 'info';
  const isProduction = process.env.NODE_ENV === 'production';
  const isTest = process.env.NODE_ENV === 'test';

  const defaults: any = {
    level: logLevel,
    timestamp: () => `,"time":"${new Date().toISOString()}"`,
    base: {
      service: 'mxrc-webapi',
      env: process.env.NODE_ENV || 'development',
    },
  };

  // Only use pino-pretty in development (not in test or production)
  if (!isProduction && !isTest) {
    defaults.transport = {
      target: 'pino-pretty',
      options: {
        colorize: true,
        translateTime: 'yyyy-mm-dd HH:MM:ss',
        ignore: 'pid,hostname',
      },
    };
  }

  return pino({ ...defaults, ...options });
}

export const logger = createLogger();

export function createChildLogger(bindings: Record<string, any>): Logger {
  return logger.child(bindings);
}

export function createRequestLoggerOptions() {
  return {
    logger: createLogger(),
    disableRequestLogging: false,
  };
}

export function setupIPCLogging(ipcBridge: IPCBridge, logger: Logger): void {
  ipcBridge.on('connected', () => logger.info('IPC connected'));
  ipcBridge.on('disconnected', () => logger.warn('IPC disconnected'));
  ipcBridge.on('reconnecting', (attempt: number) => logger.info({ attempt }, 'IPC reconnecting'));
  ipcBridge.on('error', (error: Error) => logger.error({ err: error }, 'IPC error'));
  ipcBridge.on('timeout', () => logger.warn('IPC timeout'));
  ipcBridge.on('max-reconnect-attempts-reached', () => logger.error('IPC max reconnect attempts reached'));
}
