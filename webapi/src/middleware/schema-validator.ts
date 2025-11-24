import type { FastifyRequest, FastifyReply } from 'fastify';
import type { SchemaLoader } from '../config/schema-loader';
import type { SchemaGenerator } from '../config/schema-generator';
import type { SubscriptionValidation } from '../types';

/**
 * 키 존재 확인 middleware
 */
export function validateKeyExists(schemaLoader: SchemaLoader) {
  return async (request: FastifyRequest<{ Params: { key: string } }>, reply: FastifyReply) => {
    const { key } = request.params;

    if (!schemaLoader.hasKey(key)) {
      return reply.code(404).send({
        error: 'KEY_NOT_FOUND',
        message: `Key '${key}' not found in schema`,
        key,
      });
    }
  };
}

/**
 * 읽기 권한 확인 middleware
 */
export function validateReadPermission(schemaLoader: SchemaLoader) {
  return async (request: FastifyRequest<{ Params: { key: string } }>, reply: FastifyReply) => {
    const { key } = request.params;

    if (!schemaLoader.canRead(key)) {
      return reply.code(403).send({
        error: 'PERMISSION_DENIED',
        message: `Read permission denied for key '${key}'`,
        key,
        required_permission: 'nonrt_read',
      });
    }
  };
}

/**
 * 쓰기 권한 확인 middleware
 */
export function validateWritePermission(schemaLoader: SchemaLoader) {
  return async (request: FastifyRequest<{ Params: { key: string } }>, reply: FastifyReply) => {
    const { key } = request.params;

    if (!schemaLoader.canWrite(key)) {
      return reply.code(403).send({
        error: 'PERMISSION_DENIED',
        message: `Write permission denied for key '${key}'`,
        key,
        required_permission: 'nonrt_write',
      });
    }
  };
}

/**
 * 타입 검증 middleware (PUT 요청용)
 */
export function validateValueType(
  schemaLoader: SchemaLoader,
  schemaGenerator: SchemaGenerator,
) {
  return async (
    request: FastifyRequest<{
      Params: { key: string };
      Body: { value: unknown };
    }>,
    reply: FastifyReply,
  ) => {
    const { key } = request.params;
    const { value } = request.body;

    // 키 메타데이터 조회
    const keyDef = schemaLoader.getKey(key);
    if (!keyDef) {
      return reply.code(404).send({
        error: 'KEY_NOT_FOUND',
        message: `Key '${key}' not found in schema`,
      });
    }

    // Zod 스키마 생성 및 검증
    const { valueSchema } = schemaGenerator.generateSchemasForKey(keyDef);
    const result = schemaGenerator.validate(valueSchema, value);

    if (!result.success) {
      return reply.code(400).send({
        error: 'TYPE_MISMATCH',
        message: `Type mismatch for key '${key}'`,
        key,
        expected_type: keyDef.type,
        validation_errors: result.error,
      });
    }

    // 검증된 값을 request에 저장
    (request as any).validatedValue = result.data;
  };
}

/**
 * WebSocket 구독 키 검증
 */
export function validateSubscriptionKeys(
  schemaLoader: SchemaLoader,
  keys: string[],
): SubscriptionValidation {
  const invalidKeys: string[] = [];
  const forbiddenKeys: string[] = [];

  keys.forEach((key) => {
    if (!schemaLoader.hasKey(key)) {
      invalidKeys.push(key);
    } else if (!schemaLoader.canRead(key)) {
      forbiddenKeys.push(key);
    }
  });

  return {
    valid: invalidKeys.length === 0 && forbiddenKeys.length === 0,
    invalidKeys,
    forbiddenKeys,
  };
}
