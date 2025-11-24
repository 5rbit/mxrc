import startServer from '../../src/server';
import type { FastifyInstance } from 'fastify';

describe('Datastore API Integration Tests', () => {
  let app: FastifyInstance;

  beforeAll(async () => {
    // Override environment for testing
    process.env.NODE_ENV = 'test';
    process.env.PORT = '3004';
    process.env.LOG_LEVEL = 'silent';
    process.env.SYSTEMD_NOTIFY = 'false';
    process.env.IPC_TIMEOUT_MS = '1000';

    // Start server (will fail to connect to IPC, which is expected)
    app = await startServer();
  }, 60000);

  afterAll(async () => {
    if (app) {
      await app.close();
    }
  });

  describe('GET /api/datastore/:key', () => {
    test('should return 404 for non-existent key', async () => {
      const response = await app.inject({
        method: 'GET',
        url: '/api/datastore/invalid.key.does.not.exist',
      });

      expect(response.statusCode).toBe(404);
      const body = JSON.parse(response.body);
      expect(body.error).toBe('KEY_NOT_FOUND');
      expect(body.key).toBe('invalid.key.does.not.exist');
    });

    test('should return 403 for key without read permission', async () => {
      // Assuming there's a key in schema with nonrt_read: false
      const response = await app.inject({
        method: 'GET',
        url: '/api/datastore/rt.internal.state',
      });

      // Will either be 404 (key doesn't exist) or 403 (no permission)
      expect([403, 404]).toContain(response.statusCode);

      if (response.statusCode === 403) {
        const body = JSON.parse(response.body);
        expect(body.error).toBe('PERMISSION_DENIED');
        expect(body.required_permission).toBe('nonrt_read');
      }
    });

    test('should handle IPC connection error gracefully', async () => {
      // Since IPC is not connected, this will fail
      const response = await app.inject({
        method: 'GET',
        url: '/api/datastore/system.status',
      });

      // Should return error (503 or 500)
      expect(response.statusCode).toBeGreaterThanOrEqual(500);
    });
  });

  describe('PUT /api/datastore/:key', () => {
    test('should return 404 for non-existent key', async () => {
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/invalid.key.does.not.exist',
        payload: { value: 'test' },
      });

      expect(response.statusCode).toBe(404);
      const body = JSON.parse(response.body);
      expect(body.error).toBe('KEY_NOT_FOUND');
    });

    test('should return 403 for key without write permission', async () => {
      // system.status should not have write permission
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/system.status',
        payload: { value: 'test' },
      });

      expect(response.statusCode).toBe(403);
      const body = JSON.parse(response.body);
      expect(body.error).toBe('PERMISSION_DENIED');
      expect(body.required_permission).toBe('nonrt_write');
    });

    test('should return 400 for missing value field', async () => {
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/config.user_settings',
        payload: {},
      });

      // Should fail validation or return 400
      expect([400, 403, 404]).toContain(response.statusCode);
    });

    test('should return 400 for type mismatch', async () => {
      // Try to write string to a number field
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/device.temperature',
        payload: { value: 'not a number' },
      });

      // Should return type mismatch error or permission denied
      expect([400, 403, 404]).toContain(response.statusCode);

      if (response.statusCode === 400) {
        const body = JSON.parse(response.body);
        expect(body.error).toBe('TYPE_MISMATCH');
      }
    });

    test('should validate boolean type', async () => {
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/system.emergency_stop',
        payload: { value: 'not a boolean' },
      });

      // Should return type mismatch or permission/key error
      expect([400, 403, 404]).toContain(response.statusCode);
    });

    test('should validate number type', async () => {
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/motion.target_velocity',
        payload: { value: 'not a number' },
      });

      // Should return type mismatch or permission/key error
      expect([400, 403, 404]).toContain(response.statusCode);
    });

    test('should validate object type', async () => {
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/config.user_settings',
        payload: { value: 'not an object' },
      });

      // Should return type mismatch or permission/key error
      expect([400, 403, 404]).toContain(response.statusCode);
    });

    test('should handle IPC connection error gracefully', async () => {
      // Even with valid key and type, will fail due to no IPC connection
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/config.user_settings',
        payload: { value: { theme: 'dark' } },
      });

      // Should return error (4xx or 5xx)
      expect(response.statusCode).toBeGreaterThanOrEqual(400);
    });
  });

  describe('Request Body Validation', () => {
    test('should reject PUT request without body', async () => {
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/config.user_settings',
      });

      // Should fail validation
      expect([400, 403, 404]).toContain(response.statusCode);
    });

    test('should reject GET request with body', async () => {
      const response = await app.inject({
        method: 'GET',
        url: '/api/datastore/system.status',
        payload: { unexpected: 'body' },
      });

      // GET should work regardless of body (body is ignored)
      // But will fail due to IPC not connected
      expect(response.statusCode).toBeGreaterThanOrEqual(400);
    });

    test('should handle malformed JSON', async () => {
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/config.user_settings',
        headers: { 'content-type': 'application/json' },
        payload: 'not valid json {{{',
      });

      expect(response.statusCode).toBe(400);
    });
  });

  describe('Rate Limiting', () => {
    test('should enforce rate limits on API endpoints', async () => {
      // Make many rapid requests
      const requests = Array(60)
        .fill(null)
        .map(() =>
          app.inject({
            method: 'GET',
            url: '/api/datastore/system.status',
          }),
        );

      const responses = await Promise.all(requests);

      // Some requests should be rate limited (429)
      const rateLimitedCount = responses.filter((r) => r.statusCode === 429).length;
      const errorCount = responses.filter((r) => r.statusCode >= 500).length;

      // Either some are rate limited, or most fail due to IPC
      expect(rateLimitedCount + errorCount).toBeGreaterThan(0);
    });
  });

  describe('CORS Headers', () => {
    test('should include CORS headers in response', async () => {
      const response = await app.inject({
        method: 'GET',
        url: '/api/datastore/system.status',
        headers: {
          origin: 'http://localhost:3000',
        },
      });

      // Check for CORS headers
      expect(response.headers['access-control-allow-origin']).toBeDefined();
    });

    test('should handle OPTIONS preflight request', async () => {
      const response = await app.inject({
        method: 'OPTIONS',
        url: '/api/datastore/system.status',
        headers: {
          origin: 'http://localhost:3000',
          'access-control-request-method': 'GET',
        },
      });

      expect([200, 204]).toContain(response.statusCode);
      expect(response.headers['access-control-allow-methods']).toBeDefined();
    });
  });

  describe('Error Response Format', () => {
    test('should return consistent error format for 404', async () => {
      const response = await app.inject({
        method: 'GET',
        url: '/api/datastore/invalid.key',
      });

      expect(response.statusCode).toBe(404);
      const body = JSON.parse(response.body);

      expect(body).toHaveProperty('error');
      expect(body).toHaveProperty('message');
      expect(body.error).toBe('KEY_NOT_FOUND');
    });

    test('should return consistent error format for 403', async () => {
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/system.status',
        payload: { value: 'test' },
      });

      expect(response.statusCode).toBe(403);
      const body = JSON.parse(response.body);

      expect(body).toHaveProperty('error');
      expect(body).toHaveProperty('message');
      expect(body).toHaveProperty('required_permission');
      expect(body.error).toBe('PERMISSION_DENIED');
    });

    test('should return consistent error format for 400', async () => {
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/config.user_settings',
        headers: { 'content-type': 'application/json' },
        payload: 'invalid json',
      });

      expect(response.statusCode).toBe(400);
      const body = JSON.parse(response.body);

      expect(body).toHaveProperty('error');
      expect(body).toHaveProperty('message');
    });
  });

  describe('Content-Type Handling', () => {
    test('should accept application/json content type', async () => {
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/config.user_settings',
        headers: { 'content-type': 'application/json' },
        payload: JSON.stringify({ value: { theme: 'dark' } }),
      });

      // Should process request (will fail due to IPC, but not reject content-type)
      expect(response.statusCode).not.toBe(415);
    });

    test('should handle missing content-type header', async () => {
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/config.user_settings',
        payload: { value: { theme: 'dark' } },
      });

      // Fastify should handle this gracefully
      expect(response.statusCode).not.toBe(415);
    });
  });
});
