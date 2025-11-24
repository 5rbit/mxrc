const startServer = require('../../src/server');
const MockIPCBridge = require('../mocks/ipc-bridge-mock');

describe('Datastore API Integration Tests', () => {
  let app;

  beforeAll(async () => {
    // Override environment for testing
    process.env.NODE_ENV = 'test';
    process.env.PORT = '3001';
    process.env.LOG_LEVEL = 'silent';
    process.env.SYSTEMD_NOTIFY = 'false';

    // Start server with mock IPC bridge
    app = await startServer();

    // Replace real IPC bridge with mock
    app.ipcBridge = new MockIPCBridge();
    await app.ipcBridge.connect();
  });

  afterAll(async () => {
    if (app) {
      app.ipcBridge.disconnect();
      await app.close();
    }
  });

  describe('GET /api/datastore/:key', () => {
    test('should return 200 for existing key', async () => {
      const response = await app.inject({
        method: 'GET',
        url: '/api/datastore/robot_position',
      });

      expect(response.statusCode).toBe(200);
      const body = JSON.parse(response.body);
      expect(body).toMatchObject({
        key: 'robot_position',
        value: expect.any(Array),
        version: expect.any(Number),
        timestamp: expect.any(String),
      });
      expect(body.value).toHaveLength(3);
    });

    test('should return 404 for non-existent key', async () => {
      const response = await app.inject({
        method: 'GET',
        url: '/api/datastore/unknown_key',
      });

      expect(response.statusCode).toBe(404);
      const body = JSON.parse(response.body);
      expect(body.error).toBe('KEY_NOT_FOUND');
    });

    test('should return 403 for key without read permission', async () => {
      // Mock schema loader to deny read permission
      const originalCanRead = app.schemaLoader.canRead;
      app.schemaLoader.canRead = jest.fn().mockReturnValue(false);

      const response = await app.inject({
        method: 'GET',
        url: '/api/datastore/robot_position',
      });

      expect(response.statusCode).toBe(403);
      const body = JSON.parse(response.body);
      expect(body.error).toBe('PERMISSION_DENIED');

      // Restore
      app.schemaLoader.canRead = originalCanRead;
    });

    test('should return 503 when IPC is disconnected', async () => {
      // Disconnect IPC
      app.ipcBridge.disconnect();

      const response = await app.inject({
        method: 'GET',
        url: '/api/datastore/robot_position',
      });

      expect(response.statusCode).toBe(503);
      const body = JSON.parse(response.body);
      expect(body.error).toBe('IPC_CONNECTION_FAILED');

      // Reconnect for other tests
      await app.ipcBridge.connect();
    });
  });

  describe('PUT /api/datastore/:key', () => {
    test('should return 200 for valid write', async () => {
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/robot_position',
        payload: {
          value: [5.0, 10.0, 15.0],
        },
      });

      expect(response.statusCode).toBe(200);
      const body = JSON.parse(response.body);
      expect(body).toMatchObject({
        key: 'robot_position',
        success: true,
        version: expect.any(Number),
      });
    });

    test('should return 404 for non-existent key', async () => {
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/unknown_key',
        payload: {
          value: [1, 2, 3],
        },
      });

      expect(response.statusCode).toBe(404);
      const body = JSON.parse(response.body);
      expect(body.error).toBe('KEY_NOT_FOUND');
    });

    test('should return 403 for key without write permission', async () => {
      // Mock schema loader to deny write permission
      const originalCanWrite = app.schemaLoader.canWrite;
      app.schemaLoader.canWrite = jest.fn().mockReturnValue(false);

      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/robot_position',
        payload: {
          value: [1, 2, 3],
        },
      });

      expect(response.statusCode).toBe(403);
      const body = JSON.parse(response.body);
      expect(body.error).toBe('PERMISSION_DENIED');

      // Restore
      app.schemaLoader.canWrite = originalCanWrite;
    });

    test('should return 400 for invalid value type', async () => {
      const response = await app.inject({
        method: 'PUT',
        url: '/api/datastore/robot_position',
        payload: {
          value: 'invalid', // Should be array
        },
      });

      expect(response.statusCode).toBe(400);
      const body = JSON.parse(response.body);
      expect(body.error).toBe('TYPE_MISMATCH');
    });
  });
});
