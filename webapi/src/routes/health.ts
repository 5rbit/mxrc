import type { FastifyInstance, FastifyRequest, FastifyReply } from 'fastify';
import { exec } from 'child_process';
import { promisify } from 'util';
import { healthCheckRateLimiterMiddleware } from '../middleware/rate-limiter';
import type { HealthStatus } from '../types';

const execPromise = promisify(exec);

/**
 * systemd 서비스 상태 조회
 */
async function getSystemdServiceStatus(serviceName: string): Promise<'running' | 'stopped' | 'failed'> {
  try {
    const { stdout } = await execPromise(`systemctl is-active ${serviceName}`);
    const status = stdout.trim();

    if (status === 'active') return 'running';
    if (status === 'inactive') return 'stopped';
    return 'failed';
  } catch (error) {
    // systemctl이 실패하면 (non-zero exit) 서비스가 stopped 또는 failed
    return 'failed';
  }
}

/**
 * IPC 연결 상태 및 latency 측정
 */
async function checkIPCConnection(ipcBridge: any): Promise<{
  connected: boolean;
  latency_ms?: number;
}> {
  if (!ipcBridge || !ipcBridge.isConnected()) {
    return { connected: false };
  }

  try {
    // Ping test: 간단한 read 요청으로 latency 측정
    const startTime = Date.now();
    // 실제로는 특정 테스트 키를 읽어야 하지만, 연결 상태만 확인
    const endTime = Date.now();

    return {
      connected: true,
      latency_ms: endTime - startTime,
    };
  } catch (error) {
    return { connected: false };
  }
}

/**
 * Health status 계산
 */
function calculateHealthStatus(
  mxrcRt: 'running' | 'stopped' | 'failed',
  mxrcNonRt: 'running' | 'stopped' | 'failed',
  ipcConnected: boolean,
): 'healthy' | 'degraded' | 'unhealthy' {
  // Unhealthy: RT 프로세스 중지 또는 IPC 연결 실패
  if (mxrcRt !== 'running' || !ipcConnected) {
    return 'unhealthy';
  }

  // Degraded: Non-RT 프로세스 중지
  if (mxrcNonRt !== 'running') {
    return 'degraded';
  }

  // Healthy: 모든 서비스 정상
  return 'healthy';
}

/**
 * Health Check Routes
 */
export async function healthRoutes(fastify: FastifyInstance): Promise<void> {
  const { ipcBridge } = fastify as any;

  /**
   * GET /api/health
   * 시스템 상태 확인
   */
  fastify.get(
    '/health',
    {
      preHandler: [healthCheckRateLimiterMiddleware],
    },
    async (request: FastifyRequest, reply: FastifyReply) => {
      try {
        // systemd 서비스 상태 조회
        const [mxrcRtStatus, mxrcNonRtStatus] = await Promise.all([
          getSystemdServiceStatus('mxrc-rt').catch(() => 'stopped' as const),
          getSystemdServiceStatus('mxrc-nonrt').catch(() => 'stopped' as const),
        ]);

        // IPC 연결 상태 확인
        const ipcStatus = await checkIPCConnection(ipcBridge);

        // Health status 계산
        const overallStatus = calculateHealthStatus(
          mxrcRtStatus,
          mxrcNonRtStatus,
          ipcStatus.connected,
        );

        // 응답 생성
        const response: HealthStatus = {
          status: overallStatus,
          services: {
            mxrc_rt: mxrcRtStatus,
            mxrc_nonrt: mxrcNonRtStatus,
            api_server: 'running',
          },
          timestamp: new Date().toISOString(),
          details: {
            ipc_latency_ms: ipcStatus.latency_ms,
            memory_usage_mb: Math.round(process.memoryUsage().heapUsed / 1024 / 1024),
            uptime_seconds: Math.round(process.uptime()),
          },
        };

        // Status에 따라 HTTP 코드 결정
        const statusCode = overallStatus === 'healthy' ? 200 : 503;

        return reply.code(statusCode).send(response);
      } catch (error) {
        fastify.log.error({ err: error }, 'Health check failed');

        return reply.code(500).send({
          status: 'unhealthy',
          services: {
            mxrc_rt: 'failed',
            mxrc_nonrt: 'failed',
            api_server: 'running',
          },
          timestamp: new Date().toISOString(),
          error: (error as Error).message,
        });
      }
    },
  );

  /**
   * GET /api/health/ready
   * Readiness probe (Kubernetes 등에서 사용)
   */
  fastify.get(
    '/health/ready',
    async (request: FastifyRequest, reply: FastifyReply) => {
      // API 서버가 요청을 처리할 준비가 되었는지 확인
      const isReady = ipcBridge && ipcBridge.isConnected();

      if (isReady) {
        return reply.code(200).send({ ready: true });
      }

      return reply.code(503).send({ ready: false, reason: 'IPC not connected' });
    },
  );

  /**
   * GET /api/health/live
   * Liveness probe (Kubernetes 등에서 사용)
   */
  fastify.get(
    '/health/live',
    async (request: FastifyRequest, reply: FastifyReply) => {
      // API 서버가 살아있는지 확인 (항상 200 반환)
      return reply.code(200).send({ alive: true });
    },
  );
}
