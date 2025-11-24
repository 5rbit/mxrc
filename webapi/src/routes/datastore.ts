import type { FastifyInstance, FastifyRequest, FastifyReply } from 'fastify';
import {
  validateKeyExists,
  validateReadPermission,
  validateWritePermission,
  validateValueType,
} from '../middleware/schema-validator';
import { apiRateLimiterMiddleware } from '../middleware/rate-limiter';
import { handleIPCError } from '../middleware/error-handler';

/**
 * Datastore API Routes
 */
export async function datastoreRoutes(fastify: FastifyInstance): Promise<void> {
  const { schemaLoader, schemaGenerator, ipcBridge } = fastify as any;

  /**
   * GET /api/datastore/:key
   * Datastore 키 읽기
   */
  fastify.get<{
    Params: { key: string };
  }>(
    '/datastore/:key',
    {
      preHandler: [
        apiRateLimiterMiddleware,
        validateKeyExists(schemaLoader),
        validateReadPermission(schemaLoader),
      ],
    },
    async (request: FastifyRequest<{ Params: { key: string } }>, reply: FastifyReply) => {
      const { key } = request.params;

      try {
        // IPC 브리지를 통해 Datastore 읽기
        const data = await ipcBridge.read(key);

        // 응답 생성
        return reply.code(200).send({
          key: data.key,
          value: data.value,
          version: data.version,
          timestamp: data.timestamp.toISOString(),
        });
      } catch (error) {
        // IPC 에러 처리
        return handleIPCError(error as Error, reply);
      }
    },
  );

  /**
   * PUT /api/datastore/:key
   * Datastore 키 쓰기 (Non-RT 허용 키만)
   */
  fastify.put<{
    Params: { key: string };
    Body: { value: unknown };
  }>(
    '/datastore/:key',
    {
      preHandler: [
        apiRateLimiterMiddleware,
        validateKeyExists(schemaLoader),
        validateWritePermission(schemaLoader),
        validateValueType(schemaLoader, schemaGenerator),
      ],
    },
    async (
      request: FastifyRequest<{
        Params: { key: string };
        Body: { value: unknown };
      }>,
      reply: FastifyReply,
    ) => {
      const { key } = request.params;
      const { value } = request.body;

      try {
        // IPC 브리지를 통해 Datastore 쓰기
        const result = await ipcBridge.write(key, value);

        // 응답 생성
        return reply.code(200).send({
          key: result.key,
          success: result.success,
          version: result.version,
        });
      } catch (error) {
        // IPC 에러 처리
        return handleIPCError(error as Error, reply);
      }
    },
  );
}
