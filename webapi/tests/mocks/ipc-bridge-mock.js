/**
 * Mock IPC Bridge
 * 단위 테스트용 IPC Bridge mock
 * 실제 IPC 서버 없이 테스트 가능
 */

class MockIPCBridge {
  constructor() {
    this.connected = false;
    this.mockData = new Map([
      ['robot_position', { value: [1.0, 2.0, 3.0], version: 100, timestamp: new Date() }],
      ['robot_velocity', { value: [0.1, 0.2, 0.3], version: 101, timestamp: new Date() }],
      ['ethercat_sensor_position', {
        value: Array(64).fill(0).map((_, i) => i * 0.1),
        version: 200,
        timestamp: new Date(),
      }],
    ]);

    this.subscriptions = new Map();
    this.eventListeners = new Map();
  }

  /**
   * 연결 (mock)
   */
  async connect() {
    this.connected = true;
    this.emit('connected');
    return Promise.resolve();
  }

  /**
   * 읽기 (mock)
   */
  async read(key) {
    if (!this.connected) {
      throw new Error('Not connected to IPC server');
    }

    const data = this.mockData.get(key);
    if (!data) {
      throw new Error(`Key '${key}' not found`);
    }

    return {
      key,
      value: data.value,
      version: data.version,
      timestamp: data.timestamp,
    };
  }

  /**
   * 쓰기 (mock)
   */
  async write(key, value) {
    if (!this.connected) {
      throw new Error('Not connected to IPC server');
    }

    const data = this.mockData.get(key);
    if (!data) {
      throw new Error(`Key '${key}' not found`);
    }

    // 데이터 업데이트
    data.value = value;
    data.version += 1;
    data.timestamp = new Date();

    // 구독자에게 알림
    this.notifySubscribers(key);

    return {
      key,
      success: true,
      version: data.version,
    };
  }

  /**
   * 구독 (mock)
   */
  async subscribe(keys, callback) {
    if (!this.connected) {
      throw new Error('Not connected to IPC server');
    }

    const keyArray = Array.isArray(keys) ? keys : [keys];

    keyArray.forEach((key) => {
      if (!this.subscriptions.has(key)) {
        this.subscriptions.set(key, new Set());
      }
      this.subscriptions.get(key).add(callback);
    });

    return Promise.resolve();
  }

  /**
   * 구독 해제 (mock)
   */
  async unsubscribe(keys, callback = null) {
    const keyArray = Array.isArray(keys) ? keys : [keys];

    keyArray.forEach((key) => {
      if (this.subscriptions.has(key)) {
        if (callback) {
          this.subscriptions.get(key).delete(callback);
        } else {
          this.subscriptions.get(key).clear();
        }
      }
    });

    return Promise.resolve();
  }

  /**
   * 구독자에게 알림 전송 (mock)
   */
  notifySubscribers(key) {
    if (!this.subscriptions.has(key)) {
      return;
    }

    const data = this.mockData.get(key);
    if (!data) {
      return;
    }

    this.subscriptions.get(key).forEach((callback) => {
      callback({
        key,
        value: data.value,
        version: data.version,
        timestamp: data.timestamp,
      });
    });
  }

  /**
   * 연결 해제 (mock)
   */
  disconnect() {
    this.connected = false;
    this.subscriptions.clear();
    this.emit('disconnected');
  }

  /**
   * 연결 상태 확인 (mock)
   */
  isConnected() {
    return this.connected;
  }

  /**
   * 이벤트 리스너 등록 (mock)
   */
  on(event, listener) {
    if (!this.eventListeners.has(event)) {
      this.eventListeners.set(event, new Set());
    }
    this.eventListeners.get(event).add(listener);
  }

  /**
   * 이벤트 리스너 제거 (mock)
   */
  off(event, listener) {
    if (this.eventListeners.has(event)) {
      this.eventListeners.get(event).delete(listener);
    }
  }

  /**
   * 이벤트 발생 (mock)
   */
  emit(event, ...args) {
    if (this.eventListeners.has(event)) {
      this.eventListeners.get(event).forEach((listener) => {
        listener(...args);
      });
    }
  }

  /**
   * Mock 데이터 설정 (테스트용)
   */
  setMockData(key, value, version = 1) {
    this.mockData.set(key, {
      value,
      version,
      timestamp: new Date(),
    });
  }

  /**
   * Mock 데이터 조회 (테스트용)
   */
  getMockData(key) {
    return this.mockData.get(key);
  }

  /**
   * 연결 에러 시뮬레이션 (테스트용)
   */
  simulateConnectionError() {
    this.connected = false;
    this.emit('error', new Error('Connection failed'));
  }

  /**
   * Timeout 시뮬레이션 (테스트용)
   */
  simulateTimeout() {
    this.emit('timeout');
  }

  /**
   * Mock 초기화 (테스트 간 상태 리셋)
   */
  reset() {
    this.connected = false;
    this.mockData.clear();
    this.subscriptions.clear();
    this.eventListeners.clear();

    // 기본 mock 데이터 재설정
    this.mockData.set('robot_position', { value: [1.0, 2.0, 3.0], version: 100, timestamp: new Date() });
    this.mockData.set('robot_velocity', { value: [0.1, 0.2, 0.3], version: 101, timestamp: new Date() });
    this.mockData.set('ethercat_sensor_position', {
      value: Array(64).fill(0).map((_, i) => i * 0.1),
      version: 200,
      timestamp: new Date(),
    });
  }
}

module.exports = MockIPCBridge;
