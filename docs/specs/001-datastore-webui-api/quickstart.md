# Quickstart Guide: Datastore WebUI API

**Feature**: 001-datastore-webui-api
**Date**: 2025-01-24
**Purpose**: 개발자가 빠르게 API 서버를 설치, 실행, 테스트할 수 있도록 안내

---

## Prerequisites

### System Requirements

- **OS**: Ubuntu 24.04 LTS (PREEMPT_RT)
- **Node.js**: 20 LTS 이상
- **MXRC Core**: RT/Non-RT 프로세스가 설치되어 있어야 함
- **systemd**: 서비스 관리를 위해 필요

### Check Prerequisites

```bash
# Node.js 버전 확인
node --version  # v20.x.x 이상이어야 함

# MXRC Core 상태 확인
systemctl status mxrc-rt
systemctl status mxrc-nonrt

# IPC 스키마 파일 존재 확인
ls -l /path/to/mxrc/config/ipc/ipc-schema.yaml
```

---

## Installation

### 1. Clone Repository (이미 있다면 skip)

```bash
git clone https://github.com/your-org/mxrc.git
cd mxrc
```

### 2. Install Dependencies

```bash
cd webapi
npm install
```

### 3. Build Native Addon (IPC Bridge)

Native addon은 C++ 공유 메모리 Datastore와 통신하기 위한 브리지입니다.

```bash
npm run build:native
```

**Note**: 빌드 에러 발생 시:
- `node-gyp` 의존성 확인: `npm install -g node-gyp`
- 컴파일러 확인: `g++ --version` (GCC 11+ 필요)

---

## Configuration

### Environment Variables

`.env` 파일 생성:

```bash
cd webapi
cp .env.example .env
```

`.env` 파일 내용:

```bash
# API Server
PORT=3000
NODE_ENV=development

# IPC Configuration
IPC_SOCKET_PATH=/tmp/mxrc_ipc.sock
IPC_SCHEMA_PATH=/path/to/mxrc/config/ipc/ipc-schema.yaml
IPC_TIMEOUT_MS=5000

# Rate Limiting
RATE_LIMIT_POINTS=10
RATE_LIMIT_DURATION=1

# Logging
LOG_LEVEL=info  # trace, debug, info, warn, error

# systemd (production only)
SYSTEMD_NOTIFY=false
WATCHDOG_ENABLED=false
```

**Important**: 개발 환경에서는 `IPC_SOCKET_PATH`를 MXRC Core가 생성한 socket 경로와 일치시켜야 합니다.

---

## Development

### 1. Start Mock MXRC Core (Optional)

MXRC Core 없이 API 서버를 테스트하려면 mock IPC 서버를 사용합니다:

```bash
npm run dev:mock
```

이 명령은 다음을 수행합니다:
- Mock IPC 서버 시작 (`/tmp/mxrc_ipc_mock.sock`)
- 가짜 Datastore 데이터 제공
- API 서버 자동 재시작 (nodemon)

### 2. Start API Server (with Real MXRC Core)

실제 MXRC Core와 연동하려면:

```bash
# MXRC Core가 실행 중인지 확인
systemctl status mxrc-rt

# API 서버 시작
npm run dev
```

### 3. Verify Server is Running

```bash
# Health check
curl http://localhost:3000/api/health

# Expected output:
# {
#   "status": "healthy",
#   "services": {
#     "mxrc_rt": "running",
#     "mxrc_nonrt": "running",
#     "api_server": "running"
#   },
#   "timestamp": "2025-01-24T10:00:00.000Z"
# }
```

---

## Testing

### Unit Tests

```bash
# 전체 단위 테스트 실행
npm run test:unit

# 특정 테스트 파일 실행
npm run test:unit -- --testPathPattern=schema-validator

# Watch mode (파일 변경 시 자동 실행)
npm run test:unit -- --watch
```

### Integration Tests

```bash
# 통합 테스트 실행 (Mock IPC 서버 사용)
npm run test:integration

# 실제 MXRC Core와 통합 테스트 (주의: RT 프로세스 영향)
MXRC_CORE_RUNNING=true npm run test:integration
```

### Coverage

```bash
# 테스트 커버리지 리포트 생성
npm run test:coverage

# 리포트 확인
open coverage/lcov-report/index.html  # macOS
xdg-open coverage/lcov-report/index.html  # Linux
```

**Target Coverage**:
- Lines: 80% 이상
- Functions: 80% 이상
- Branches: 70% 이상

---

## API Usage Examples

### HTTP API

#### 1. Read Datastore Key

```bash
# 로봇 위치 읽기
curl http://localhost:3000/api/datastore/robot_position

# Response:
# {
#   "key": "robot_position",
#   "value": [1.5, 2.3, 0.8],
#   "version": 12345,
#   "timestamp": "2025-01-24T10:00:00.123Z"
# }
```

#### 2. Write Datastore Key (Non-RT Only)

```bash
# 목표 위치 설정
curl -X PUT http://localhost:3000/api/datastore/ethercat_target_position \
  -H "Content-Type: application/json" \
  -d '{"value": [1.57, 0.0, 3.14]}'

# Response:
# {
#   "key": "ethercat_target_position",
#   "success": true,
#   "version": 12346
# }
```

#### 3. Health Check

```bash
curl http://localhost:3000/api/health
```

### WebSocket API

#### Using wscat

```bash
# Install wscat globally
npm install -g wscat

# Connect to WebSocket
wscat -c ws://localhost:3000/ws -s datastore-v1

# Subscribe to keys
> {"type":"subscribe","keys":["robot_position","robot_velocity"]}

# Receive updates
< {"type":"subscribed","keys":["robot_position","robot_velocity"],"timestamp":"2025-01-24T10:00:00Z"}
< {"type":"update","key":"robot_position","value":[1.5,2.3,0.8],"version":12345,"timestamp":"2025-01-24T10:00:01Z"}

# Unsubscribe
> {"type":"unsubscribe","keys":["robot_velocity"]}

# Close
Ctrl+C
```

#### Using JavaScript Client

```javascript
// client.js
const WebSocket = require('ws');

const ws = new WebSocket('ws://localhost:3000/ws', 'datastore-v1');

ws.on('open', () => {
  console.log('Connected');
  ws.send(JSON.stringify({
    type: 'subscribe',
    keys: ['robot_position', 'robot_velocity']
  }));
});

ws.on('message', (data) => {
  const msg = JSON.parse(data);
  if (msg.type === 'update') {
    console.log(`${msg.key}: ${JSON.stringify(msg.value)} (v${msg.version})`);
  }
});

ws.on('close', () => console.log('Disconnected'));
ws.on('error', (err) => console.error('Error:', err));
```

Run:
```bash
node client.js
```

---

## Production Deployment

### 1. Build for Production

```bash
# Install production dependencies only
NODE_ENV=production npm ci

# Build native addon
npm run build:native
```

### 2. Configure systemd Service

systemd service 파일 생성: `/etc/systemd/system/mxrc-webapi.service`

```ini
[Unit]
Description=MXRC Web API Server for Datastore
After=mxrc-rt.service
BindsTo=mxrc-rt.service
PartOf=mxrc.target

[Service]
Type=notify
User=mxrc
WorkingDirectory=/opt/mxrc/webapi
ExecStart=/usr/bin/node /opt/mxrc/webapi/src/server.js
Restart=on-failure
RestartSec=5s

# Environment
Environment=NODE_ENV=production
Environment=PORT=3000
Environment=IPC_SOCKET_PATH=/tmp/mxrc_ipc.sock
Environment=IPC_SCHEMA_PATH=/opt/mxrc/config/ipc/ipc-schema.yaml

# systemd Watchdog
WatchdogSec=30

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=mxrc-webapi

# Security (optional)
NoNewPrivileges=true
PrivateTmp=true

# Resource Limits
MemoryMax=512M
CPUQuota=50%

[Install]
WantedBy=mxrc.target
```

### 3. Deploy Service

```bash
# Copy service file
sudo cp systemd/mxrc-webapi.service /etc/systemd/system/

# Reload systemd
sudo systemctl daemon-reload

# Enable service (auto-start on boot)
sudo systemctl enable mxrc-webapi

# Start service
sudo systemctl start mxrc-webapi

# Check status
sudo systemctl status mxrc-webapi

# View logs
sudo journalctl -u mxrc-webapi -f
```

### 4. Verify Deployment

```bash
# Health check
curl http://localhost:3000/api/health

# Check systemd watchdog is working
sudo systemctl show mxrc-webapi -p WatchdogTimestamp

# Monitor logs
sudo journalctl -u mxrc-webapi --since "5 minutes ago"
```

---

## Troubleshooting

### Issue 1: IPC Connection Failed

**Symptoms**:
```
Error: connect ENOENT /tmp/mxrc_ipc.sock
```

**Solutions**:
1. MXRC Core가 실행 중인지 확인:
   ```bash
   systemctl status mxrc-rt
   ```
2. IPC socket 파일 경로 확인:
   ```bash
   ls -l /tmp/mxrc_ipc.sock
   ```
3. 권한 확인:
   ```bash
   # API 서버 사용자가 socket 파일에 접근 가능한지 확인
   sudo -u mxrc stat /tmp/mxrc_ipc.sock
   ```

### Issue 2: Native Addon Build Failed

**Symptoms**:
```
gyp ERR! build error
```

**Solutions**:
1. node-gyp 의존성 설치:
   ```bash
   sudo apt-get install -y build-essential python3
   npm install -g node-gyp
   ```
2. Node.js 헤더 재설치:
   ```bash
   node-gyp install
   ```
3. 캐시 정리 후 재빌드:
   ```bash
   rm -rf build node_modules
   npm install
   npm run build:native
   ```

### Issue 3: Health Check Returns "degraded"

**Symptoms**:
```json
{
  "status": "degraded",
  "services": {
    "mxrc_rt": "running",
    "mxrc_nonrt": "stopped"
  }
}
```

**Solutions**:
1. Non-RT 프로세스 재시작:
   ```bash
   sudo systemctl restart mxrc-nonrt
   ```
2. 전체 MXRC 시스템 재시작:
   ```bash
   sudo systemctl restart mxrc.target
   ```

### Issue 4: Rate Limit Exceeded

**Symptoms**:
```json
{
  "error": "Too Many Requests",
  "retryAfter": 60000
}
```

**Solutions**:
1. Rate limit 설정 완화 (`.env`):
   ```bash
   RATE_LIMIT_POINTS=20  # 기본값 10에서 증가
   ```
2. 클라이언트에서 요청 속도 제어
3. Health check endpoint는 rate limit에서 제외되므로 사용

### Issue 5: Memory Usage Too High

**Symptoms**:
API 서버 메모리 사용량 > 512MB

**Solutions**:
1. Node.js heap 크기 제한:
   ```bash
   node --max-old-space-size=512 src/server.js
   ```
2. WebSocket 연결 수 제한 (코드 수정 필요)
3. 로그 레벨 조정:
   ```bash
   LOG_LEVEL=warn  # debug 대신 warn 사용
   ```

---

## Performance Tuning

### 1. IPC Optimization

**Socket Buffer Size** (MXRC Core 측 조정):
```cpp
// C++ 코드
int sndbuf = 256 * 1024;  // 256KB
int rcvbuf = 256 * 1024;  // 256KB
setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
```

### 2. WebSocket Throttling

고빈도 키 throttling 설정 (`src/routes/websocket.js`):
```javascript
const throttleConfig = {
  hot_keys: {
    max_updates_per_sec: 100,
    keys: ['robot_position', 'ethercat_sensor_position']
  },
  normal_keys: {
    max_updates_per_sec: 10
  }
};
```

### 3. Logging Performance

프로덕션 환경에서는 로그 레벨을 `warn` 이상으로 설정:
```bash
LOG_LEVEL=warn
```

JSON 로깅 사용 (journald 파싱 효율):
```javascript
const logger = require('pino')({
  level: process.env.LOG_LEVEL || 'info',
  timestamp: () => `,"time":"${new Date().toISOString()}"`
});
```

---

## Next Steps

1. ✅ **Development 환경 구축 완료**: API 서버 로컬 실행
2. ⬜ **Web UI (React) 구현**: 별도 feature로 진행
3. ⬜ **보안 강화**: API Key 또는 OAuth2 인증 추가
4. ⬜ **Monitoring**: Prometheus 메트릭 export
5. ⬜ **Load Testing**: Apache Bench, k6를 사용한 성능 테스트

---

## Resources

- **API Documentation**: [contracts/openapi.yaml](./contracts/openapi.yaml)
- **WebSocket Protocol**: [contracts/websocket.md](./contracts/websocket.md)
- **Data Model**: [data-model.md](./data-model.md)
- **Research**: [research.md](./research.md)
- **Spec**: [spec.md](./spec.md)
- **Plan**: [plan.md](./plan.md)

---

## Support

문제가 발생하면:
1. 로그 확인: `journalctl -u mxrc-webapi -f`
2. Health check: `curl http://localhost:3000/api/health`
3. GitHub Issues에 보고 (로그 첨부)

---

**Last Updated**: 2025-01-24
**Status**: Phase 1 - Quickstart Guide 완료
