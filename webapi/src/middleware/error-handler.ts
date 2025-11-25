import type { FastifyError, FastifyRequest, FastifyReply } from 'fastify';
import type { Logger } from 'pino';
import type { ErrorResponse } from '../types';

const ERROR_CODES: Record<string, { status: number; message: string }> = {
  KEY_NOT_FOUND: { status: 404, message: 'Key not found in schema' },
  PERMISSION_DENIED: { status: 403, message: 'Permission denied' },
  TYPE_MISMATCH: { status: 400, message: 'Type mismatch' },
  INVALID_REQUEST: { status: 400, message: 'Invalid request' },
  RATE_LIMIT_EXCEEDED: { status: 429, message: 'Too many requests' },
  INVALID_MESSAGE: { status: 400, message: 'Invalid message format' },
  TOO_MANY_KEYS: { status: 400, message: 'Too many keys' },
  SERVICE_UNAVAILABLE: { status: 503, message: 'Service unavailable' },
  IPC_CONNECTION_FAILED: { status: 503, message: 'IPC connection failed' },
  INTERNAL_ERROR: { status: 500, message: 'Internal server error' },
};

export function createErrorResponse(
  code: string,
  message?: string,
  details: Record<string, unknown> = {},
): ErrorResponse {
  const errorDef = ERROR_CODES[code] || ERROR_CODES.INTERNAL_ERROR;
  return {
    error: code,
    message: message || errorDef.message,
    ...details,
  };
}

export function errorHandler(logger: Logger) {
  return (error: FastifyError, request: FastifyRequest, reply: FastifyReply) => {
    const logLevel = reply.statusCode >= 500 ? 'error' : 'warn';
    logger[logLevel]({ err: error, req: request }, 'Request error');

    if (reply.sent) return;

    if ((error as any).validation) {
      return reply.code(400).send(createErrorResponse('INVALID_REQUEST', 'Request validation failed', {
        validation_errors: (error as any).validation,
      }));
    }

    if (error.message?.includes('IPC')) {
      return reply.code(503).send(createErrorResponse('IPC_CONNECTION_FAILED', error.message));
    }

    const statusCode = reply.statusCode >= 400 ? reply.statusCode : 500;
    reply.code(statusCode).send(createErrorResponse('INTERNAL_ERROR', error.message));
  };
}

export function notFoundHandler() {
  return (request: FastifyRequest, reply: FastifyReply) => {
    reply.code(404).send({
      error: 'NOT_FOUND',
      message: `Route ${request.method} ${request.url} not found`,
    });
  };
}

export function handleIPCError(error: Error, reply: FastifyReply) {
  const errorMessage = error.message || 'IPC operation failed';

  if (errorMessage.includes('Not connected') || errorMessage.includes('Connection')) {
    return reply.code(503).send(createErrorResponse('IPC_CONNECTION_FAILED', errorMessage));
  }

  if (errorMessage.includes('timeout')) {
    return reply.code(503).send(createErrorResponse('SERVICE_UNAVAILABLE', 'IPC request timeout'));
  }

  return reply.code(500).send(createErrorResponse('INTERNAL_ERROR', errorMessage));
}
