/**
 * Core Types for MXRC WebAPI
 */

// Datastore Key 메타데이터
export interface DatastoreKey {
  name: string;
  type: string;
  access: {
    rt_read: boolean;
    rt_write: boolean;
    nonrt_read: boolean;
    nonrt_write: boolean;
  };
  description: string;
  hot_key?: boolean;
  default_value?: unknown;
}

// VersionedData<T> - Datastore 값
export interface VersionedData<T = unknown> {
  value: T;
  version: number;
  timestamp: Date;
}

// Datastore Value (API 응답용)
export interface DatastoreValue<T = unknown> {
  key: string;
  value: T;
  version: number;
  timestamp: Date | string;
}

// Datastore Write 응답
export interface DatastoreWriteResponse {
  key: string;
  success: boolean;
  version: number;
}

// IPC 메시지
export interface IPCMessage {
  request_id: number;
  command: 'read' | 'write' | 'subscribe' | 'unsubscribe';
  key?: string;
  keys?: string[];
  value?: unknown;
}

// IPC 응답
export interface IPCResponse<T = unknown> {
  request_id: number;
  data?: T;
  error?: string;
}

// IPC Notification
export interface IPCNotification<T = unknown> {
  type: 'notification';
  data: {
    key: string;
    value: T;
    version: number;
    timestamp: number | string;
  };
}

// WebSocket 메시지
export interface WebSocketMessage {
  type: 'subscribe' | 'unsubscribe' | 'ping';
  keys?: string[];
}

// WebSocket 응답
export interface WebSocketResponse {
  type: 'subscribed' | 'unsubscribed' | 'update' | 'error' | 'pong';
  keys?: string[];
  key?: string;
  value?: unknown;
  version?: number;
  timestamp?: string;
  code?: string;
  message?: string;
  details?: Record<string, unknown>;
}

// Health Status
export interface HealthStatus {
  status: 'healthy' | 'degraded' | 'unhealthy';
  services: {
    mxrc_rt: 'running' | 'stopped' | 'failed';
    mxrc_nonrt: 'running' | 'stopped' | 'failed';
    api_server: 'running' | 'stopped' | 'failed';
  };
  timestamp: Date | string;
  details?: {
    ipc_latency_ms?: number;
    memory_usage_mb?: number;
    uptime_seconds?: number;
  };
}

// Error Response
export interface ErrorResponse {
  error: string;
  message: string;
  [key: string]: unknown;
}

// Rate Limiter Result
export interface RateLimiterResult {
  allowed: boolean;
  retryAfter?: number;
}

// Validation Result
export interface ValidationResult<T = unknown> {
  success: boolean;
  data: T | null;
  error: unknown | null;
}

// Subscription Validation
export interface SubscriptionValidation {
  valid: boolean;
  invalidKeys: string[];
  forbiddenKeys: string[];
}
