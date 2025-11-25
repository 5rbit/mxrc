import * as fs from 'fs';
import * as yaml from 'js-yaml';
import * as path from 'path';
import type { DatastoreKey } from '../types';

/**
 * IPC 스키마 로더
 * config/ipc/ipc-schema.yaml 파일을 로드하고 파싱합니다.
 */
export class SchemaLoader {
  private schemaPath: string;
  private schema: any = null;
  private keystoreMap: Map<string, DatastoreKey> = new Map();

  constructor(schemaPath: string) {
    this.schemaPath = schemaPath;
  }

  /**
   * 스키마 파일 로드 및 파싱
   * @returns 파싱된 스키마 객체
   * @throws 파일 읽기 또는 파싱 실패 시
   */
  load(): any {
    try {
      // 절대 경로로 변환
      const absolutePath = path.resolve(this.schemaPath);

      if (!fs.existsSync(absolutePath)) {
        throw new Error(`Schema file not found: ${absolutePath}`);
      }

      // YAML 파일 읽기
      const fileContents = fs.readFileSync(absolutePath, 'utf8');

      // YAML 파싱
      this.schema = yaml.load(fileContents);

      // Datastore keys를 Map으로 변환
      if (this.schema && this.schema.datastore_keys) {
        Object.entries(this.schema.datastore_keys).forEach(([keyName, keyDef]: [string, any]) => {
          this.keystoreMap.set(keyName, {
            name: keyName,
            type: keyDef.type,
            access: keyDef.access || {
              rt_read: false,
              rt_write: false,
              nonrt_read: false,
              nonrt_write: false,
            },
            description: keyDef.description || '',
            hot_key: keyDef.hot_key || false,
            default_value: keyDef.default_value,
          });
        });
      }

      return this.schema;
    } catch (error) {
      throw new Error(`Failed to load IPC schema: ${(error as Error).message}`);
    }
  }

  /**
   * 키가 스키마에 존재하는지 확인
   */
  hasKey(keyName: string): boolean {
    return this.keystoreMap.has(keyName);
  }

  /**
   * 키 메타데이터 조회
   */
  getKey(keyName: string): DatastoreKey | null {
    return this.keystoreMap.get(keyName) || null;
  }

  /**
   * 모든 키 목록 조회
   */
  getAllKeys(): string[] {
    return Array.from(this.keystoreMap.keys());
  }

  /**
   * 읽기 권한 확인 (Non-RT)
   */
  canRead(keyName: string): boolean {
    const key = this.getKey(keyName);
    return key ? key.access.nonrt_read : false;
  }

  /**
   * 쓰기 권한 확인 (Non-RT)
   */
  canWrite(keyName: string): boolean {
    const key = this.getKey(keyName);
    return key ? key.access.nonrt_write : false;
  }

  /**
   * Hot key 여부 확인
   */
  isHotKey(keyName: string): boolean {
    const key = this.getKey(keyName);
    return key ? key.hot_key || false : false;
  }

  /**
   * 스키마 버전 조회
   */
  getSchemaVersion(): string {
    return this.schema?.schema_version || 'unknown';
  }

  /**
   * Namespace 조회
   */
  getNamespace(): string {
    return this.schema?.namespace || 'default';
  }
}
