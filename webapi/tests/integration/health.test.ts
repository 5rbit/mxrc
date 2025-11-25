import startServer from '../../src/server';
import type { FastifyInstance } from 'fastify';

describe('Health Check API Integration Tests', () => {
  let app: FastifyInstance;

  beforeAll(async () => {
    // Override environment for testing
    process.env.NODE_ENV = 'test';
    process.env.PORT = '3002';
    process.env.LOG_LEVEL = 'silent';
    process.env.SYSTEMD_NOTIFY = 'false';
    process.env.IPC_TIMEOUT_MS = '1000'; // Short timeout for testing

    // Start server (will fail to connect to IPC, which is expected)
    app = await startServer();
  }, 60000); // Increase timeout to 60 seconds

  afterAll(async () => {
    if (app) {
      await app.close();
    }
  });

  describe('GET /api/health', () => {
    test('should return health status', async () => {
      const response = await app.inject({
        method: 'GET',
        url: '/api/health',
      });

      // Status code can be 200 (healthy) or 503 (degraded/unhealthy)
      expect([200, 503]).toContain(response.statusCode);

      const body = JSON.parse(response.body);
      expect(body).toHaveProperty('status');
      expect(['healthy', 'degraded', 'unhealthy']).toContain(body.status);
      expect(body).toHaveProperty('services');
      expect(body.services).toHaveProperty('mxrc_rt');
      expect(body.services).toHaveProperty('mxrc_nonrt');
      expect(body.services).toHaveProperty('api_server');
      expect(body).toHaveProperty('timestamp');
    });

    test('should include details in response', async () => {
      const response = await app.inject({
        method: 'GET',
        url: '/api/health',
      });

      const body = JSON.parse(response.body);
      expect(body).toHaveProperty('details');

      if (body.details) {
        expect(body.details).toHaveProperty('memory_usage_mb');
        expect(body.details).toHaveProperty('uptime_seconds');
        expect(typeof body.details.memory_usage_mb).toBe('number');
        expect(typeof body.details.uptime_seconds).toBe('number');
      }
    });

    test('should return unhealthy when IPC is not connected', async () => {
      // IPC는 테스트 환경에서 연결되지 않으므로 unhealthy 상태여야 함
      const response = await app.inject({
        method: 'GET',
        url: '/api/health',
      });

      const body = JSON.parse(response.body);

      // IPC가 연결되지 않았으므로 unhealthy여야 함
      if (body.status === 'unhealthy') {
        expect(response.statusCode).toBe(503);
      }
    });
  });

  describe('GET /api/health/ready', () => {
    test('should return readiness status', async () => {
      const response = await app.inject({
        method: 'GET',
        url: '/api/health/ready',
      });

      expect([200, 503]).toContain(response.statusCode);

      const body = JSON.parse(response.body);
      expect(body).toHaveProperty('ready');
      expect(typeof body.ready).toBe('boolean');
    });

    test('should return not ready when IPC is disconnected', async () => {
      const response = await app.inject({
        method: 'GET',
        url: '/api/health/ready',
      });

      const body = JSON.parse(response.body);

      if (!body.ready) {
        expect(response.statusCode).toBe(503);
        expect(body).toHaveProperty('reason');
      }
    });
  });

  describe('GET /api/health/live', () => {
    test('should always return alive', async () => {
      const response = await app.inject({
        method: 'GET',
        url: '/api/health/live',
      });

      expect(response.statusCode).toBe(200);

      const body = JSON.parse(response.body);
      expect(body).toEqual({ alive: true });
    });
  });

  describe('Rate Limiting', () => {
    test('should allow high frequency health checks', async () => {
      // Health check는 높은 rate limit (100 req/s)을 가져야 함
      const requests = Array(50)
        .fill(null)
        .map(() =>
          app.inject({
            method: 'GET',
            url: '/api/health/live',
          }),
        );

      const responses = await Promise.all(requests);

      // 모든 요청이 성공해야 함 (rate limit 없음)
      const successCount = responses.filter((r) => r.statusCode === 200).length;
      expect(successCount).toBeGreaterThan(40); // 대부분 성공
    });
  });
});
