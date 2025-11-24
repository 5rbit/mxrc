const MockIPCBridge = require('../mocks/ipc-bridge-mock');

describe('IPC Bridge', () => {
  let ipcBridge;

  beforeEach(() => {
    ipcBridge = new MockIPCBridge();
    ipcBridge.reset();
  });

  afterEach(() => {
    if (ipcBridge.isConnected()) {
      ipcBridge.disconnect();
    }
  });

  describe('Connection', () => {
    test('should connect successfully', async () => {
      await expect(ipcBridge.connect()).resolves.toBeUndefined();
      expect(ipcBridge.isConnected()).toBe(true);
    });

    test('should disconnect successfully', async () => {
      await ipcBridge.connect();
      ipcBridge.disconnect();
      expect(ipcBridge.isConnected()).toBe(false);
    });

    test('should emit connected event', async () => {
      const connectedHandler = jest.fn();
      ipcBridge.on('connected', connectedHandler);

      await ipcBridge.connect();

      expect(connectedHandler).toHaveBeenCalled();
    });

    test('should emit disconnected event', async () => {
      const disconnectedHandler = jest.fn();
      ipcBridge.on('disconnected', disconnectedHandler);

      await ipcBridge.connect();
      ipcBridge.disconnect();

      expect(disconnectedHandler).toHaveBeenCalled();
    });
  });

  describe('Read Operations', () => {
    beforeEach(async () => {
      await ipcBridge.connect();
    });

    test('should read existing key', async () => {
      const result = await ipcBridge.read('robot_position');

      expect(result).toMatchObject({
        key: 'robot_position',
        value: expect.any(Array),
        version: expect.any(Number),
        timestamp: expect.any(Date),
      });
      expect(result.value).toHaveLength(3);
    });

    test('should throw error for non-existent key', async () => {
      await expect(ipcBridge.read('unknown_key')).rejects.toThrow("Key 'unknown_key' not found");
    });

    test('should throw error when not connected', async () => {
      ipcBridge.disconnect();
      await expect(ipcBridge.read('robot_position')).rejects.toThrow('Not connected to IPC server');
    });
  });

  describe('Write Operations', () => {
    beforeEach(async () => {
      await ipcBridge.connect();
    });

    test('should write to existing key', async () => {
      const newValue = [10.0, 20.0, 30.0];
      const result = await ipcBridge.write('robot_position', newValue);

      expect(result).toMatchObject({
        key: 'robot_position',
        success: true,
        version: expect.any(Number),
      });

      // Verify value was updated
      const readResult = await ipcBridge.read('robot_position');
      expect(readResult.value).toEqual(newValue);
    });

    test('should increment version on write', async () => {
      const initialResult = await ipcBridge.read('robot_position');
      const initialVersion = initialResult.version;

      await ipcBridge.write('robot_position', [5.0, 5.0, 5.0]);

      const updatedResult = await ipcBridge.read('robot_position');
      expect(updatedResult.version).toBe(initialVersion + 1);
    });

    test('should throw error for non-existent key', async () => {
      await expect(ipcBridge.write('unknown_key', [1, 2, 3])).rejects.toThrow("Key 'unknown_key' not found");
    });
  });

  describe('Subscribe/Unsubscribe Operations', () => {
    beforeEach(async () => {
      await ipcBridge.connect();
    });

    test('should subscribe to keys', async () => {
      const callback = jest.fn();
      await expect(ipcBridge.subscribe(['robot_position'], callback)).resolves.toBeUndefined();
    });

    test('should receive notifications on data change', async () => {
      const callback = jest.fn();
      await ipcBridge.subscribe(['robot_position'], callback);

      // Trigger data change
      const newValue = [100.0, 200.0, 300.0];
      await ipcBridge.write('robot_position', newValue);

      expect(callback).toHaveBeenCalledWith({
        key: 'robot_position',
        value: newValue,
        version: expect.any(Number),
        timestamp: expect.any(Date),
      });
    });

    test('should unsubscribe from keys', async () => {
      const callback = jest.fn();
      await ipcBridge.subscribe(['robot_position'], callback);
      await ipcBridge.unsubscribe(['robot_position'], callback);

      // Trigger data change
      await ipcBridge.write('robot_position', [1, 2, 3]);

      // Callback should not be called after unsubscribe
      expect(callback).not.toHaveBeenCalled();
    });

    test('should support multiple subscribers', async () => {
      const callback1 = jest.fn();
      const callback2 = jest.fn();

      await ipcBridge.subscribe(['robot_position'], callback1);
      await ipcBridge.subscribe(['robot_position'], callback2);

      await ipcBridge.write('robot_position', [1, 2, 3]);

      expect(callback1).toHaveBeenCalled();
      expect(callback2).toHaveBeenCalled();
    });
  });
});
