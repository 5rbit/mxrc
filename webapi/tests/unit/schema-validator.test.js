const { validateSubscriptionKeys } = require('../../src/middleware/schema-validator');
const SchemaLoader = require('../../src/config/schema-loader');

describe('Schema Validator Middleware', () => {
  let schemaLoader;

  beforeAll(() => {
    // Mock schema loader
    schemaLoader = {
      hasKey: jest.fn(),
      canRead: jest.fn(),
      getKey: jest.fn(),
    };
  });

  afterEach(() => {
    jest.clearAllMocks();
  });

  describe('validateSubscriptionKeys', () => {
    test('should validate all keys successfully', () => {
      const keys = ['robot_position', 'robot_velocity'];

      schemaLoader.hasKey.mockReturnValue(true);
      schemaLoader.canRead.mockReturnValue(true);

      const result = validateSubscriptionKeys(schemaLoader, keys);

      expect(result.valid).toBe(true);
      expect(result.invalidKeys).toEqual([]);
      expect(result.forbiddenKeys).toEqual([]);
    });

    test('should detect invalid keys', () => {
      const keys = ['robot_position', 'unknown_key'];

      schemaLoader.hasKey.mockImplementation((key) => key === 'robot_position');
      schemaLoader.canRead.mockReturnValue(true);

      const result = validateSubscriptionKeys(schemaLoader, keys);

      expect(result.valid).toBe(false);
      expect(result.invalidKeys).toEqual(['unknown_key']);
      expect(result.forbiddenKeys).toEqual([]);
    });

    test('should detect forbidden keys (no read permission)', () => {
      const keys = ['robot_position', 'internal_state'];

      schemaLoader.hasKey.mockReturnValue(true);
      schemaLoader.canRead.mockImplementation((key) => key === 'robot_position');

      const result = validateSubscriptionKeys(schemaLoader, keys);

      expect(result.valid).toBe(false);
      expect(result.invalidKeys).toEqual([]);
      expect(result.forbiddenKeys).toEqual(['internal_state']);
    });

    test('should handle empty keys array', () => {
      const keys = [];

      const result = validateSubscriptionKeys(schemaLoader, keys);

      expect(result.valid).toBe(true);
      expect(result.invalidKeys).toEqual([]);
      expect(result.forbiddenKeys).toEqual([]);
    });

    test('should detect both invalid and forbidden keys', () => {
      const keys = ['robot_position', 'unknown_key', 'internal_state'];

      schemaLoader.hasKey.mockImplementation((key) => key !== 'unknown_key');
      schemaLoader.canRead.mockImplementation((key) => key === 'robot_position');

      const result = validateSubscriptionKeys(schemaLoader, keys);

      expect(result.valid).toBe(false);
      expect(result.invalidKeys).toEqual(['unknown_key']);
      expect(result.forbiddenKeys).toEqual(['internal_state']);
    });
  });
});
