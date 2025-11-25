import { z, type ZodTypeAny } from 'zod';
import type { DatastoreKey, ValidationResult } from '../types';

/**
 * IPC 타입 → Zod 스키마 변환기
 */
export class SchemaGenerator {
  private typeMap: Record<string, ZodTypeAny>;

  constructor() {
    this.typeMap = {
      double: z.number(),
      float: z.number(),
      int: z.number().int(),
      uint: z.number().int().nonnegative(),
      int32: z.number().int(),
      uint32: z.number().int().nonnegative(),
      int64: z.number().int(),
      uint64: z.number().int().nonnegative(),
      string: z.string(),
      bool: z.boolean(),
      boolean: z.boolean(),
    };
  }

  /**
   * IPC 타입 문자열을 Zod 스키마로 변환
   */
  generateSchema(typeStr: string): ZodTypeAny {
    // 기본 타입 확인
    if (this.typeMap[typeStr]) {
      return this.typeMap[typeStr];
    }

    // Vector3d: [x, y, z]
    if (typeStr === 'Vector3d') {
      return z.tuple([z.number(), z.number(), z.number()]);
    }

    // array<T, N> 패턴 매칭
    const arrayMatch = typeStr.match(/^array<(\w+),\s*(\d+)>$/);
    if (arrayMatch) {
      const [, elementType, size] = arrayMatch;
      const elementSchema = this.typeMap[elementType] || z.any();
      return z.array(elementSchema).length(parseInt(size, 10));
    }

    // std::array<T, N> 패턴 매칭
    const stdArrayMatch = typeStr.match(/^std::array<(\w+),\s*(\d+)>$/);
    if (stdArrayMatch) {
      const [, elementType, size] = stdArrayMatch;
      const elementSchema = this.typeMap[elementType] || z.any();
      return z.array(elementSchema).length(parseInt(size, 10));
    }

    // vector<T> 패턴 매칭 (가변 길이)
    const vectorMatch = typeStr.match(/^(?:std::)?vector<(\w+)>$/);
    if (vectorMatch) {
      const [, elementType] = vectorMatch;
      const elementSchema = this.typeMap[elementType] || z.any();
      return z.array(elementSchema);
    }

    // 알 수 없는 타입은 any로 처리
    return z.any();
  }

  /**
   * VersionedData<T> 스키마 생성
   */
  generateVersionedDataSchema(dataSchema: ZodTypeAny): z.ZodObject<any> {
    return z.object({
      value: dataSchema,
      version: z.number().int().nonnegative(),
      timestamp: z.date(),
    });
  }

  /**
   * Datastore value response 스키마 생성
   */
  generateDatastoreValueSchema(keyName: string, typeStr: string): z.ZodObject<any> {
    const valueSchema = this.generateSchema(typeStr);

    return z.object({
      key: z.literal(keyName),
      value: valueSchema,
      version: z.number().int().nonnegative(),
      timestamp: z.string().datetime(),
    });
  }

  /**
   * Datastore write request 스키마 생성
   */
  generateWriteRequestSchema(typeStr: string): z.ZodObject<any> {
    const valueSchema = this.generateSchema(typeStr);

    return z.object({
      value: valueSchema,
    });
  }

  /**
   * 스키마 로더의 키 정의에서 Zod 스키마 생성
   */
  generateSchemasForKey(keyDef: DatastoreKey): {
    valueSchema: ZodTypeAny;
    requestSchema: z.ZodObject<any>;
    responseSchema: z.ZodObject<any>;
  } {
    const valueSchema = this.generateSchema(keyDef.type);
    const requestSchema = this.generateWriteRequestSchema(keyDef.type);
    const responseSchema = this.generateDatastoreValueSchema(keyDef.name, keyDef.type);

    return {
      valueSchema,
      requestSchema,
      responseSchema,
    };
  }

  /**
   * 타입 검증 (Zod safeParse 래퍼)
   */
  validate<T = unknown>(schema: ZodTypeAny, data: unknown): ValidationResult<T> {
    const result = schema.safeParse(data);

    if (result.success) {
      return {
        success: true,
        data: result.data as T,
        error: null,
      };
    }

    return {
      success: false,
      data: null,
      error: result.error.format(),
    };
  }
}
