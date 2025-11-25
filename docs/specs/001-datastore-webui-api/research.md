# Research: Datastore WebUI API 기술 조사

**Feature**: 001-datastore-webui-api
**Date**: 2025-01-24
**Purpose**: Technical Context의 NEEDS CLARIFICATION 항목 해결 및 구현 기술 선정

---

## 1. Node.js에서 C++ 공유 메모리 접근 방법

### Decision
**Unix Domain Socket 기반 IPC 프로토콜 사용**

### Rationale
`tbb::concurrent_hash_map`은 내부적으로 커스텀 allocator를 완전히 지원하지 않아 shared memory에서 직접 사용이 어렵습니다. Node.js native addon으로 shared memory를 직접 접근하는 것은 기술적으로 가능하지만, pointer serialization과 memory alignment 문제로 복잡도가 매우 높습니다. Unix Domain Socket은 TCP 대비 20-40% 빠른 성능을 제공하며, native pipe와 거의 동일한 성능을 보입니다. 구조화된 데이터 전송에 적합하고, `VersionedData<T>` 구조체를 JSON 또는 binary protocol로 직렬화하여 전송할 수 있어 구현 복잡도와 성능의 균형이 가장 우수합니다.

### Alternatives Considered
- **Native Addon + Shared Memory 직접 접근**: 최고 성능 (latency < 1μs) | `tbb::concurrent_hash_map` allocator 문제, pointer serialization 복잡, 타입 안전성 보장 어려움 | 구현 복잡도가 프로젝트 일정 대비 과도함
- **Named Pipe**: Unix Domain Socket과 유사한 성능 | Windows 호환성 고려 시 추가 구현 필요 | 크로스 플랫폼 지원에서 Unix Domain Socket이 더 표준적
- **ZeroMQ**: 풍부한 messaging pattern 지원 | Unix Domain Socket과 성능 차이 거의 없음, 추가 dependency | 단순 req/res pattern에는 오버스펙

### Implementation Notes
```javascript
// C++ 측: Unix socket server
const char* socket_path = "/tmp/mxrc_ipc.sock";
int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
// ... bind, listen

// Node.js 측: net module 사용
const net = require('net');
const client = net.createConnection('/tmp/mxrc_ipc.sock');
client.write(JSON.stringify({cmd: 'read', key: 'device_0'}));
```

**주의사항**: Socket buffer 크기 조정 필요 (`SO_SNDBUF`, `SO_RCVBUF`), Binary protocol (MessagePack, Protocol Buffers) 사용 시 성능 20-30% 추가 개선 가능

---

## 2. HTTP 프레임워크 선택: Express vs Fastify

### Decision
**Fastify + @fastify/websocket**

### Rationale
Fastify는 hello-world 시나리오에서 45,140 req/s를 처리하며 Express의 10,702 req/s 대비 4.5배 빠릅니다. MongoDB 연동 시에도 8,800 req/s (Express 6,500 req/s 대비 40% 향상)로 초당 1,000건 목표를 충분히 초과 달성합니다. 평균 latency는 21.65ms로 Express의 92.82ms 대비 76% 낮습니다. Route 기반 WebSocket 통합이 용이하며 (`@fastify/websocket`), schema-based validation이 내장되어 있어 타입 안전성도 우수합니다. 메모리 사용량도 Express 대비 적습니다.

### Alternatives Considered
- **Express.js**: 최대 생태계 지원, 레거시 코드 풍부 | 성능이 4.5배 낮음, latency 높음 | 1,000 req/s는 달성 가능하지만 여유가 적음
- **Koa.js**: Express 대비 개선된 async/await 지원 | Fastify 대비 성능 낮음, 플러그인 생태계 작음 | 성능과 생태계 모두에서 중간 수준

### Implementation Notes
```javascript
const fastify = require('fastify')({ logger: true });
await fastify.register(require('@fastify/websocket'));

fastify.get('/api/data', {
  schema: {
    response: { 200: { type: 'object', properties: { value: { type: 'number' } } } }
  }
}, async (request, reply) => {
  return { value: 42 };
});

fastify.get('/ws', { websocket: true }, (connection, req) => {
  connection.socket.on('message', msg => { /* ... */ });
});
```

**주의사항**: Schema validation 비용 고려 (10-15% overhead), production에서 logger level 조정 (`logger: { level: 'warn' }`)

---

## 3. systemd 통합 방법

### Decision
**sd-notify npm 패키지 사용** (공식 systemd 제공)

### Rationale
`sd-notify`는 systemd에서 공식 제공하는 Node.js wrapper로 libsystemd와 직접 바인딩되어 신뢰성이 가장 높습니다. Watchdog 지원이 내장되어 있으며 `startWatchdogMode(ms)` 헬퍼 함수로 간단히 구현 가능합니다. journald 로깅은 stdout/stderr를 systemd가 자동으로 캡처하므로 별도 구현 불필요합니다. native binding을 직접 구현할 필요가 없어 유지보수 부담이 적고, systemd 업데이트에도 안정적으로 대응할 수 있습니다.

### Alternatives Considered
- **systemd-daemon (native binding)**: 최고 성능, 모든 sd-daemon API 접근 | native 컴파일 필요, 의존성 관리 복잡 | 프로젝트에서 사용할 기능(notify, watchdog)만으로는 성능 차이 미미
- **직접 구현 (node-addon-api)**: 완전한 제어 가능 | 개발/테스트/유지보수 비용 높음, systemd API 변경 시 대응 필요 | ROI가 낮음

### Implementation Notes
```javascript
const notify = require('sd-notify');

// 서비스 시작 완료 알림
notify.ready();

// Watchdog 설정 (WatchdogSec의 절반 주기로 ping)
const watchdogInterval = notify.watchdogInterval();
if (watchdogInterval > 0) {
  notify.startWatchdogMode(watchdogInterval / 2);
}

// 수동 watchdog ping (health check 실패 시 중단)
const healthOk = await checkSystemHealth();
if (healthOk) notify.watchdog();
```

**systemd service 파일**:
```ini
[Service]
Type=notify
WatchdogSec=30
StandardOutput=journal
StandardError=journal
```

---

## 4. WebSocket 라이브러리 선택

### Decision
**ws (표준 WebSocket 라이브러리)**

### Rationale
`ws`는 50,000개 이상의 동시 연결을 단일 서버에서 처리할 수 있으며, 낮은 latency와 최소 overhead를 제공합니다. Binary 데이터 전송을 네이티브 지원하고, Fastify와 공식 플러그인(`@fastify/websocket`)으로 완벽히 통합됩니다. Socket.IO 대비 메시지 크기가 작고 serialization/deserialization overhead가 없어 RT 시스템 모니터링에 적합합니다. 표준 WebSocket protocol을 준수하여 다양한 클라이언트와 호환됩니다.

### Alternatives Considered
- **Socket.IO**: 자동 재연결, room/namespace 기능, fallback 지원 | 커스텀 protocol로 overhead 증가, 메시지 크기 20-30% 증가 | 본 프로젝트에서 불필요한 기능들이 많음
- **uWebSockets.js**: 최고 성능 (ws 대비 2-3배) | C++ 의존성, 생태계 작음, Fastify 통합 어려움 | 현재 성능 목표로는 ws로 충분

### Implementation Notes
```javascript
// Fastify + ws
fastify.register(require('@fastify/websocket'));

fastify.get('/monitor', { websocket: true }, (connection, req) => {
  const { socket } = connection;

  const interval = setInterval(() => {
    if (socket.readyState === socket.OPEN) {
      socket.send(JSON.stringify({ timestamp: Date.now(), data: getRealtimeData() }));
    }
  }, 100); // 10Hz update

  socket.on('close', () => clearInterval(interval));
});
```

**주의사항**: Binary frame 사용 시 성능 향상 (`socket.send(Buffer.from(...))` ), backpressure 처리 필요 (bufferedAmount 확인)

---

## 5. IPC 스키마 타입 매핑

### Decision
**Zod (TypeScript-first validation)**

### Rationale
Zod는 TypeScript 타입을 자동으로 추론하여 런타임 검증과 컴파일 타임 타입 안전성을 동시에 제공합니다. Bundle size가 45kb로 가장 작고, 성능도 Yup과 유사하거나 더 빠릅니다. `Vector3d` (double[3]), `array<double, 64>` 같은 고정 크기 배열을 `z.tuple()` 또는 `z.array().length()`로 정확히 검증할 수 있습니다. IPC YAML schema를 Zod schema로 변환하면 JavaScript 타입이 자동 생성되어 개발 생산성이 높습니다.

### Alternatives Considered
- **Yup**: 폼 validation에 최적화, async validation 지원 | TypeScript 타입 추론 약함, bundle size 60kb | 본 프로젝트는 서버 측 IPC 검증이 주 목적
- **Joi**: 복잡한 서버 측 validation, 성숙한 API | TypeScript 지원 약함, bundle size 큼 | 클라이언트 측 번들링 시 부담

### Implementation Notes
```typescript
import { z } from 'zod';

// Vector3d: [x, y, z]
const Vector3dSchema = z.tuple([z.number(), z.number(), z.number()]);
type Vector3d = z.infer<typeof Vector3dSchema>; // [number, number, number]

// array<double, 64>
const DoubleArray64Schema = z.array(z.number()).length(64);

// VersionedData<T>
const VersionedDataSchema = <T extends z.ZodTypeAny>(dataSchema: T) =>
  z.object({
    version: z.number(),
    timestamp: z.number(),
    data: dataSchema,
  });

// 사용 예
const DeviceStateSchema = VersionedDataSchema(
  z.object({
    position: Vector3dSchema,
    torque: DoubleArray64Schema,
  })
);

// IPC 메시지 검증
const result = DeviceStateSchema.safeParse(ipcMessage);
if (!result.success) {
  console.error(result.error.format());
}
```

**주의사항**: 큰 배열(64개 double) 검증 시 성능 영향 있을 수 있음, production에서 선택적 검증 고려

---

## 6. Rate Limiting 전략

### Decision
**Token Bucket 알고리즘 + rate-limiter-flexible 라이브러리**

### Rationale
Token Bucket은 순간적인 burst traffic을 허용하면서도 평균 요청률을 제한하여 RT 프로세스를 보호합니다. `rate-limiter-flexible`은 DoS 공격(100k req/s 수준)에도 대응 가능하며, 평균 처리 시간이 0.7ms로 매우 빠릅니다. Redis 등 distributed storage를 지원하여 multi-instance 환경에서도 일관된 제한을 적용할 수 있습니다. RT 시스템 보호를 위해 **초당 10-20 요청** 제한을 권장하며, burst capacity는 50-100으로 설정하여 정상적인 사용 패턴은 허용하되 과도한 요청은 차단합니다.

### Alternatives Considered
- **Fixed Window**: 구현 단순 | Window 경계에서 burst 취약점 (double rate 가능) | RT 시스템 보호에 부적합
- **Sliding Window**: 정확한 rate 제한, burst 방지 | 메모리 사용량 높음(모든 요청 timestamp 저장), 복잡도 높음 | Token Bucket이 더 효율적
- **express-rate-limit**: 설정 간단, Express 전용 | 기능 제한적, 분산 환경 지원 약함 | Fastify 사용 시 rate-limiter-flexible이 더 적합

### Implementation Notes
```javascript
const { RateLimiterMemory } = require('rate-limiter-flexible');

// RT 프로세스 보호: 초당 10 요청, burst 50
const rateLimiter = new RateLimiterMemory({
  points: 10,          // 초당 10 토큰 생성
  duration: 1,         // 1초 window
  blockDuration: 60,   // 제한 초과 시 60초 차단
  execEvenly: false,   // burst 허용
});

// Fastify middleware
fastify.addHook('preHandler', async (request, reply) => {
  const key = request.ip; // 또는 API key
  try {
    await rateLimiter.consume(key, 1); // 1 토큰 소비
  } catch (rateLimiterRes) {
    reply.code(429).send({ error: 'Too Many Requests', retryAfter: rateLimiterRes.msBeforeNext });
  }
});
```

**권장 설정**:
- **일반 API**: 초당 10-20 요청, burst 50-100
- **Critical path** (RT 영향 큰 작업): 초당 5 요청, burst 10
- **WebSocket**: Connection당 초당 50 메시지 (더 높은 throughput 필요)

**주의사항**: 분산 환경(여러 Node.js 인스턴스)에서는 `RateLimiterRedis` 사용, health check endpoint는 rate limit 제외

---

## Summary of Technical Decisions

| 항목 | 선택 기술 | 주요 이유 |
|------|----------|----------|
| IPC 메커니즘 | Unix Domain Socket | 성능/복잡도 균형, 구조화 데이터 전송 적합 |
| HTTP 프레임워크 | Fastify | 4.5배 빠른 성능, schema validation 내장 |
| systemd 통합 | sd-notify (npm) | 공식 wrapper, 신뢰성, 간편한 Watchdog 지원 |
| WebSocket | ws | 표준 프로토콜, 낮은 overhead, Fastify 통합 |
| 타입 검증 | Zod | TypeScript 우선, 작은 번들 크기, 타입 추론 |
| Rate Limiting | rate-limiter-flexible | Token Bucket, 고성능, 분산 환경 지원 |

---

## Next Steps

1. ✅ **Phase 0 완료**: 모든 NEEDS CLARIFICATION 해결됨
2. ⬜ **Phase 1 시작**: data-model.md, contracts/, quickstart.md 생성
3. ⬜ **기술 스택 업데이트**: Technical Context에 확정된 기술 반영

---

## Sources

- [TBB concurrent_hash_map Documentation](https://www.intel.com/content/www/us/en/docs/onetbb/developer-guide-api-reference/2021-9/concurrent-hash-map.html)
- [Stack Overflow: TBB concurrent_hash_map in shared memory](https://stackoverflow.com/questions/22506155/tbb-concurent-hash-map-in-shared-memory)
- [Express vs Fastify Performance Comparison - Michael Guay](https://michaelguay.dev/express-vs-fastify-a-performance-benchmark-comparison/)
- [Better Stack: Express.js vs Fastify Comparison](https://betterstack.com/community/guides/scaling-nodejs/fastify-express/)
- [Fastify Official Benchmarks](https://fastify.dev/benchmarks/)
- [GitHub: systemd/node-sd-notify](https://github.com/systemd/node-sd-notify)
- [npm: sd-notify](https://www.npmjs.com/package/sd-notify)
- [Stack Overflow: Differences between socket.io and websockets](https://stackoverflow.com/questions/10112178/differences-between-socket-io-and-websockets)
- [DEV: Node.js WebSockets - ws vs socket.io](https://dev.to/alex_aslam/nodejs-websockets-when-to-use-ws-vs-socketio-and-why-we-switched-di9)
- [Better Stack: Joi vs Zod Comparison](https://betterstack.com/community/guides/scaling-nodejs/joi-vs-zod/)
- [DEV: Yup vs Zod vs Joi Comparison](https://dev.to/gimnathperera/yup-vs-zod-vs-joi-a-comprehensive-comparison-of-javascript-validation-libraries-4mhi)
- [LogRocket: Zod vs Yup Schema Validation](https://blog.logrocket.com/comparing-schema-validation-libraries-zod-vs-yup/)
- [60devs: Performance of IPC in Node.js](https://60devs.com/performance-of-inter-process-communications-in-nodejs.html)
- [GitHub: fastify/fastify-websocket](https://github.com/fastify/fastify-websocket)
- [LogRocket: Using WebSockets with Fastify](https://blog.logrocket.com/using-websockets-with-fastify/)
- [npm: rate-limiter-flexible](https://www.npmjs.com/package/rate-limiter-flexible)
- [LogRocket: Rate Limiting in Node.js](https://blog.logrocket.com/rate-limiting-node-js/)
- [DEV: Protect Your Node.js API - Rate Limiting](https://dev.to/odunayo_dada/protect-your-nodejs-api-rate-limiting-with-fixed-window-sliding-window-and-token-bucket-4278)

---

**Last Updated**: 2025-01-24
**Status**: Phase 0 완료
