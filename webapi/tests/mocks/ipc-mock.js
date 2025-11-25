const net = require('net');
const fs = require('fs');

/**
 * Mock IPC Server
 * MXRC Core를 시뮬레이션하는 Unix Domain Socket 서버
 * 테스트 및 개발 환경에서 사용
 */

class MockIPCServer {
  constructor(socketPath = '/tmp/mxrc_ipc_mock.sock') {
    this.socketPath = socketPath;
    this.server = null;
    this.clients = new Set();

    // Mock Datastore
    this.datastore = new Map([
      ['robot_position', { value: [0.0, 0.0, 0.0], version: 1, timestamp: Date.now() }],
      ['robot_velocity', { value: [0.0, 0.0, 0.0], version: 1, timestamp: Date.now() }],
      ['robot_acceleration', { value: [0.0, 0.0, 0.0], version: 1, timestamp: Date.now() }],
      ['ethercat_sensor_position', {
        value: Array(64).fill(0).map((_, i) => i * 0.1),
        version: 1,
        timestamp: Date.now(),
      }],
      ['ethercat_target_position', {
        value: Array(64).fill(0),
        version: 1,
        timestamp: Date.now(),
      }],
    ]);

    // Subscriptions: key -> Set<socket>
    this.subscriptions = new Map();
  }

  /**
   * 서버 시작
   */
  start() {
    return new Promise((resolve, reject) => {
      // 기존 소켓 파일 삭제
      if (fs.existsSync(this.socketPath)) {
        fs.unlinkSync(this.socketPath);
      }

      this.server = net.createServer((socket) => {
        this.handleConnection(socket);
      });

      this.server.listen(this.socketPath, () => {
        console.log(`Mock IPC server listening on ${this.socketPath}`);
        resolve();
      });

      this.server.on('error', (error) => {
        console.error('Mock IPC server error:', error);
        reject(error);
      });

      // 데이터 업데이트 시뮬레이션 (10Hz)
      this.startDataSimulation();
    });
  }

  /**
   * 클라이언트 연결 처리
   * @param {net.Socket} socket - 클라이언트 소켓
   */
  handleConnection(socket) {
    this.clients.add(socket);
    console.log('Mock IPC client connected');

    let buffer = '';

    socket.on('data', (data) => {
      buffer += data.toString();

      // 메시지 단위 파싱 (newline으로 구분)
      const messages = buffer.split('\n');
      buffer = messages.pop(); // 마지막 불완전한 메시지는 버퍼에 유지

      messages.forEach((messageStr) => {
        if (messageStr.trim()) {
          try {
            const message = JSON.parse(messageStr);
            this.handleMessage(socket, message);
          } catch (error) {
            console.error('Mock IPC parse error:', error);
          }
        }
      });
    });

    socket.on('close', () => {
      this.clients.delete(socket);

      // 구독 정리
      this.subscriptions.forEach((subscribers) => {
        subscribers.delete(socket);
      });

      console.log('Mock IPC client disconnected');
    });

    socket.on('error', (error) => {
      console.error('Mock IPC socket error:', error);
    });
  }

  /**
   * 메시지 처리
   * @param {net.Socket} socket - 클라이언트 소켓
   * @param {Object} message - 요청 메시지
   */
  handleMessage(socket, message) {
    const { request_id, command, key, value, keys } = message;

    switch (command) {
      case 'read':
        this.handleRead(socket, request_id, key);
        break;
      case 'write':
        this.handleWrite(socket, request_id, key, value);
        break;
      case 'subscribe':
        this.handleSubscribe(socket, request_id, keys);
        break;
      case 'unsubscribe':
        this.handleUnsubscribe(socket, request_id, keys);
        break;
      default:
        this.sendResponse(socket, request_id, null, 'Unknown command');
    }
  }

  /**
   * Read 명령 처리
   */
  handleRead(socket, requestId, key) {
    const data = this.datastore.get(key);

    if (data) {
      this.sendResponse(socket, requestId, {
        value: data.value,
        version: data.version,
        timestamp: data.timestamp,
      });
    } else {
      this.sendResponse(socket, requestId, null, `Key '${key}' not found`);
    }
  }

  /**
   * Write 명령 처리
   */
  handleWrite(socket, requestId, key, value) {
    const data = this.datastore.get(key);

    if (data) {
      // 데이터 업데이트
      data.value = value;
      data.version += 1;
      data.timestamp = Date.now();

      this.sendResponse(socket, requestId, {
        success: true,
        version: data.version,
      });

      // 구독자에게 알림
      this.notifySubscribers(key);
    } else {
      this.sendResponse(socket, requestId, null, `Key '${key}' not found`);
    }
  }

  /**
   * Subscribe 명령 처리
   */
  handleSubscribe(socket, requestId, keys) {
    keys.forEach((key) => {
      if (!this.subscriptions.has(key)) {
        this.subscriptions.set(key, new Set());
      }
      this.subscriptions.get(key).add(socket);
    });

    this.sendResponse(socket, requestId, { subscribed: keys });
  }

  /**
   * Unsubscribe 명령 처리
   */
  handleUnsubscribe(socket, requestId, keys) {
    keys.forEach((key) => {
      if (this.subscriptions.has(key)) {
        this.subscriptions.get(key).delete(socket);
      }
    });

    this.sendResponse(socket, requestId, { unsubscribed: keys });
  }

  /**
   * 응답 전송
   */
  sendResponse(socket, requestId, data, error = null) {
    const response = {
      request_id: requestId,
      data,
      error,
    };

    socket.write(JSON.stringify(response) + '\n');
  }

  /**
   * 구독자에게 알림 전송
   */
  notifySubscribers(key) {
    if (!this.subscriptions.has(key)) {
      return;
    }

    const data = this.datastore.get(key);
    const notification = {
      type: 'notification',
      data: {
        key,
        value: data.value,
        version: data.version,
        timestamp: data.timestamp,
      },
    };

    this.subscriptions.get(key).forEach((socket) => {
      socket.write(JSON.stringify(notification) + '\n');
    });
  }

  /**
   * 데이터 업데이트 시뮬레이션 (10Hz)
   */
  startDataSimulation() {
    this.simulationInterval = setInterval(() => {
      // robot_position 업데이트 (sin wave)
      const time = Date.now() / 1000;
      const posData = this.datastore.get('robot_position');
      if (posData) {
        posData.value = [
          Math.sin(time) * 0.5,
          Math.cos(time) * 0.5,
          Math.sin(time * 2) * 0.2,
        ];
        posData.version += 1;
        posData.timestamp = Date.now();
        this.notifySubscribers('robot_position');
      }

      // robot_velocity 업데이트
      const velData = this.datastore.get('robot_velocity');
      if (velData) {
        velData.value = [
          Math.cos(time) * 0.5,
          -Math.sin(time) * 0.5,
          Math.cos(time * 2) * 0.4,
        ];
        velData.version += 1;
        velData.timestamp = Date.now();
        this.notifySubscribers('robot_velocity');
      }
    }, 100); // 10Hz
  }

  /**
   * 서버 종료
   */
  stop() {
    if (this.simulationInterval) {
      clearInterval(this.simulationInterval);
    }

    if (this.server) {
      this.server.close();
      this.clients.forEach((socket) => socket.destroy());
      this.clients.clear();
    }

    if (fs.existsSync(this.socketPath)) {
      fs.unlinkSync(this.socketPath);
    }

    console.log('Mock IPC server stopped');
  }
}

// CLI로 실행 가능하도록
if (require.main === module) {
  const server = new MockIPCServer();
  server.start().catch((error) => {
    console.error('Failed to start mock IPC server:', error);
    process.exit(1);
  });

  // Graceful shutdown
  process.on('SIGINT', () => {
    console.log('Shutting down mock IPC server...');
    server.stop();
    process.exit(0);
  });
}

module.exports = MockIPCServer;
