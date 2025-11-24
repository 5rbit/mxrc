# Tasks: Datastore WebUI를 위한 Decoupled API 서버

**Input**: Design documents from `/home/tory/workspace/mxrc/mxrc/docs/specs/001-datastore-webui-api/`
**Status**: In Progress
**Progress**: 27/57 tasks completed (47%)
**Last Updated**: 2025-01-24
**Prerequisites**: plan.md ✅, spec.md ✅, research.md ✅, data-model.md ✅, contracts/ ✅

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 작업 설명은 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Task, Action, test, model, service 등)
- 파일 경로와 코드는 원래대로 표기합니다

**예시**:
- ✅ 좋은 예: "IPC 브리지 구현 in webapi/src/ipc/bridge.js"
- ❌ 나쁜 예: "Implement IPC bridge in webapi/src/ipc/bridge.js"

---

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 병렬 실행 가능 (다른 파일, 의존성 없음)
- **[Story]**: User Story 번호 (US1, US2, US3, US4)
- 정확한 파일 경로 포함

---

## Implementation Strategy

### MVP Approach
**MVP = User Story 1만 구현**: Datastore 데이터 실시간 모니터링 (HTTP GET endpoint)
- 이것만으로도 시스템 상태 파악 가능
- 독립적으로 테스트 및 배포 가능

### Incremental Delivery
1. **US1** (P1): HTTP GET API → 즉시 가치 제공
2. **US4** (P4): Health check → US1과 병렬 개발 가능
3. **US2** (P2): WebSocket → US1 완료 후 실시간 업데이트 추가
4. **US3** (P3): HTTP PUT API → 마지막으로 제어 기능 추가

---

## Dependency Graph

```
Setup (Phase 1)
    ↓
Foundational (Phase 2) ← 모든 User Story의 전제조건
    ↓
    ├→ US1 (P1) 🎯 MVP ← 최우선 완료
    ├→ US4 (P4) ← US1과 병렬 가능 (독립적)
    ↓
US2 (P2) ← US1 완료 후 (IPC + routes 재사용)
    ↓
US3 (P3) ← US1, US2 완료 후 (쓰기 권한 검증 필요)
```

---

## Phase 1: Setup (프로젝트 초기화)

**Purpose**: Node.js 프로젝트 구조 생성 및 기본 설정

- [x] T001 프로젝트 디렉토리 생성 in webapi/
- [x] T002 package.json 초기화 및 기본 의존성 설치 (Node.js 20, Fastify, ws, zod, js-yaml, sd-notify, rate-limiter-flexible)
- [x] T003 [P] .env.example 파일 생성 (PORT, IPC_SOCKET_PATH, IPC_SCHEMA_PATH, LOG_LEVEL)
- [x] T004 [P] .gitignore 파일 생성 (node_modules/, .env, build/, coverage/)
- [x] T005 [P] README.md 작성 (quickstart.md 기반)
- [x] T006 Jest 설정 파일 생성 in webapi/jest.config.js

**Checkpoint**: 프로젝트 구조 완료

---

## Phase 2: Foundational (핵심 인프라 - 모든 Story의 전제조건)

**Purpose**: 모든 User Story가 의존하는 핵심 컴포넌트 구현

**⚠️ CRITICAL**: 이 Phase 완료 전에는 User Story 작업 불가

### IPC 및 스키마 로더

- [x] T007 IPC 스키마 로더 구현 in webapi/src/config/schema-loader.js (js-yaml 사용, config/ipc/ipc-schema.yaml 파싱)
- [x] T008 Unix Domain Socket IPC 클라이언트 구현 in webapi/src/ipc/client.js (reconnect 로직 포함)
- [x] T009 IPC 브리지 구현 in webapi/src/ipc/bridge.js (read/write/subscribe 메서드)

### 타입 검증 및 스키마

- [x] T010 Zod 스키마 생성기 구현 in webapi/src/config/schema-generator.js (IPC 타입 → Zod 스키마 변환)
- [x] T011 [P] 타입 검증 유틸리티 구현 in webapi/src/utils/type-validator.js (Vector3d, array<T,N> 검증)

### Middleware

- [x] T012 [P] 스키마 검증 middleware 구현 in webapi/src/middleware/schema-validator.js (key 존재 확인, 타입 검증, 권한 확인)
- [x] T013 [P] Rate limiter middleware 구현 in webapi/src/middleware/rate-limiter.js (Token Bucket, 초당 10-20 요청)
- [x] T014 [P] Error handler middleware 구현 in webapi/src/middleware/error-handler.js (오류 코드 매핑, 로그 기록)
- [x] T015 [P] CORS middleware 설정 in webapi/src/middleware/cors.js

### 로깅 및 모니터링

- [x] T016 [P] Logger 설정 in webapi/src/utils/logger.js (pino, journald 출력)
- [x] T017 [P] systemd notify 유틸리티 구현 in webapi/src/utils/systemd-notify.js (sd-notify 래퍼, Watchdog 지원)

### Mock 및 테스트 인프라

- [x] T018 [P] Mock IPC 서버 구현 in webapi/tests/mocks/ipc-mock.js (가짜 Datastore 데이터 제공)
- [x] T019 [P] Mock IPC 브리지 구현 in webapi/tests/mocks/ipc-bridge-mock.js (단위 테스트용)

**Checkpoint**: 핵심 인프라 완료 - User Story 구현 시작 가능

---

## Phase 3: User Story 1 - Datastore 데이터 실시간 모니터링 (Priority: P1) 🎯 MVP

**Goal**: HTTP GET API를 통해 Datastore 키를 읽어 JSON으로 반환

**Independent Test**: `curl http://localhost:3000/api/datastore/robot_position` 실행 시 로봇 위치 데이터 반환

**Why MVP**: 이 기능만으로도 시스템 상태를 파악할 수 있어 즉시 가치 제공

### 서버 초기화

- [x] T020 [US1] Fastify 서버 초기화 in webapi/src/server.js (플러그인 등록, middleware 적용)

### HTTP API 구현

- [x] T021 [US1] Datastore GET endpoint 구현 in webapi/src/routes/datastore.js (`/api/datastore/:key` 라우트)
- [x] T022 [US1] GET 요청 핸들러 구현 (IPC 브리지를 통해 Datastore 읽기, VersionedData 반환)
- [x] T023 [US1] 오류 처리 추가 (KEY_NOT_FOUND, PERMISSION_DENIED, SERVICE_UNAVAILABLE)

### 단위 테스트

- [x] T024 [P] [US1] schema-validator.test.js 작성 (키 존재 확인, 권한 검증)
- [x] T025 [P] [US1] ipc-bridge.test.js 작성 (read 메서드 테스트, mock 사용)

### 통합 테스트

- [x] T026 [US1] api.test.js 작성 in webapi/tests/integration/ (Supertest, Mock IPC 서버 사용)
- [x] T027 [US1] GET /api/datastore/:key 통합 테스트 (성공 케이스, 404, 403, 503)

**User Story 1 완료 기준**:
- ✅ `curl http://localhost:3000/api/datastore/robot_position` → 200 OK + JSON 데이터
- ✅ 존재하지 않는 키 요청 → 404 Not Found
- ✅ 읽기 권한 없는 키 요청 → 403 Forbidden
- ✅ MXRC Core 중지 상태 → 503 Service Unavailable

---

## Phase 4: User Story 4 - 시스템 상태 모니터링 (Priority: P4)

**Goal**: `/api/health` endpoint를 통해 MXRC Core 및 API 서버 상태 확인

**Independent Test**: `curl http://localhost:3000/api/health` 실행 시 모든 서비스 상태 반환

**Why P4 but early**: US1과 병렬 개발 가능하며, 운영 필수 기능

### Health Check 구현

- [ ] T028 [P] [US4] Health check endpoint 구현 in webapi/src/routes/health.js (`/api/health` 라우트)
- [ ] T029 [P] [US4] systemd 서비스 상태 조회 구현 (mxrc-rt, mxrc-nonrt 상태 확인)
- [ ] T030 [P] [US4] IPC 연결 상태 확인 추가 (latency 측정)
- [ ] T031 [P] [US4] Health status 계산 로직 구현 (healthy/degraded/unhealthy)

### 테스트

- [ ] T032 [P] [US4] health.test.js 작성 in webapi/tests/integration/ (모든 서비스 정상, 일부 중단, IPC 실패 케이스)

**User Story 4 완료 기준**:
- ✅ `curl http://localhost:3000/api/health` → 200 OK + JSON 상태
- ✅ RT 프로세스 중지 시 → `{"status": "degraded"}`
- ✅ IPC 연결 실패 시 → `{"status": "unhealthy"}`
- ✅ 응답 시간 < 100ms

---

## Phase 5: User Story 2 - 데이터 변경 알림 수신 (Priority: P2)

**Goal**: WebSocket을 통해 Datastore 키 구독 및 실시간 변경 알림 수신

**Independent Test**: WebSocket 클라이언트로 `robot_position` 구독 후, RT 프로세스가 값 업데이트 시 알림 수신

**Dependencies**: US1 완료 필요 (IPC 브리지 및 routes 재사용)

### WebSocket 서버 구현

- [ ] T033 [US2] Fastify WebSocket 플러그인 등록 in webapi/src/server.js (@fastify/websocket)
- [ ] T034 [US2] WebSocket 핸들러 구현 in webapi/src/routes/websocket.js (`/ws` 엔드포인트)
- [ ] T035 [US2] WebSocket 연결 관리자 구현 (connection 추가/제거, 구독 관리)

### 구독 및 알림

- [ ] T036 [US2] Subscribe 메시지 핸들러 구현 (키 검증, 권한 확인, IPC 브리지 subscribe 호출)
- [ ] T037 [US2] Unsubscribe 메시지 핸들러 구현
- [ ] T038 [US2] IPC 알림 수신 및 WebSocket 브로드캐스트 구현
- [ ] T039 [US2] Throttling 로직 추가 (hot keys 초당 100 업데이트, 일반 keys 초당 10 업데이트)

### 오류 처리

- [ ] T040 [US2] WebSocket 오류 메시지 전송 구현 (INVALID_MESSAGE, PERMISSION_DENIED, TOO_MANY_KEYS)
- [ ] T041 [US2] Rate limiting 추가 (Subscribe/Unsubscribe 초당 10 요청)

### 테스트

- [ ] T042 [US2] websocket.test.js 작성 in webapi/tests/integration/ (구독, 알림 수신, 구독 해제, 오류 케이스)

**User Story 2 완료 기준**:
- ✅ WebSocket 연결 후 `{"type":"subscribe","keys":["robot_position"]}` 전송 → 구독 확인
- ✅ RT 프로세스가 위치 업데이트 → 1초 이내 WebSocket 알림 수신
- ✅ 여러 키 동시 구독 → 변경된 키만 선택적 알림
- ✅ 최대 100개 동시 연결 지원

---

## Phase 6: User Story 3 - Datastore 제어 명령 전송 (Priority: P3)

**Goal**: HTTP PUT API를 통해 Non-RT 쓰기 가능한 Datastore 키에 값 설정

**Independent Test**: `curl -X PUT http://localhost:3000/api/datastore/ethercat_target_position -d '{"value":[1.57,0,3.14]}'` 실행 시 Datastore 업데이트

**Dependencies**: US1, US2 완료 필요 (쓰기 권한 검증 추가)

### HTTP API 구현

- [ ] T043 [US3] Datastore PUT endpoint 구현 in webapi/src/routes/datastore.js (`PUT /api/datastore/:key` 라우트)
- [ ] T044 [US3] PUT 요청 핸들러 구현 (스키마 검증, 쓰기 권한 확인, IPC 브리지를 통해 Datastore 쓰기)
- [ ] T045 [US3] 타입 검증 추가 (요청 body 타입 vs IPC 스키마 타입 일치 확인)

### 오류 처리

- [ ] T046 [US3] 쓰기 권한 오류 처리 (RT-only keys → 403 Forbidden)
- [ ] T047 [US3] 타입 불일치 오류 처리 (400 Bad Request, 예상 vs 받은 타입 정보 포함)

### 테스트

- [ ] T048 [US3] PUT /api/datastore/:key 통합 테스트 (성공 케이스, 403, 400)

**User Story 3 완료 기준**:
- ✅ Non-RT 쓰기 가능 키 업데이트 → 200 OK + 새 버전 번호
- ✅ RT 전용 키 쓰기 시도 → 403 Forbidden
- ✅ 타입 불일치 데이터 전송 → 400 Bad Request + 상세 오류 메시지
- ✅ 배열 크기 검증 (array<double, 64>는 정확히 64개 요소 필요)

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: 프로덕션 배포 준비 및 운영 최적화

### systemd 통합

- [ ] T049 [P] systemd 서비스 파일 생성 in systemd/mxrc-webapi.service (의존성, Watchdog, 자원 제한)
- [ ] T050 [P] systemd Watchdog 활성화 (30초마다 notify.watchdog() 호출)
- [ ] T051 [P] 서비스 시작 시 sd_notify("READY") 전송

### 문서 및 배포

- [ ] T052 [P] package.json scripts 추가 (dev, dev:mock, test, test:coverage, start)
- [ ] T053 [P] .env 프로덕션 설정 예시 추가
- [ ] T054 [P] CHANGELOG.md 작성 (v1.0.0 릴리스 노트)

### 성능 최적화

- [ ] T055 [P] IPC socket buffer 크기 조정 (SO_SNDBUF, SO_RCVBUF 256KB)
- [ ] T056 [P] Fastify logger level 조정 (프로덕션: warn)
- [ ] T057 [P] Hot keys 캐싱 추가 (TTL 100ms)

**모든 작업 완료 기준**:
- ✅ 모든 단위/통합 테스트 통과
- ✅ 테스트 커버리지 > 80%
- ✅ systemd 서비스 정상 시작 및 Watchdog 작동
- ✅ 메모리 사용량 < 128MB
- ✅ HTTP 성능 > 1,000 req/s (hello-world 시나리오)

---

## Parallel Execution Opportunities

### Setup + Foundational 단계
```bash
# 병렬 실행 가능한 작업들 (독립적인 파일)
T003 (.env.example) || T004 (.gitignore) || T005 (README.md)
T011 (type-validator) || T012 (schema-validator) || T013 (rate-limiter) || T014 (error-handler) || T015 (CORS) || T016 (logger) || T017 (systemd-notify) || T018 (ipc-mock) || T019 (ipc-bridge-mock)
```

### User Story 1 (MVP)
```bash
# 테스트를 병렬로 작성 (테스트 파일은 독립적)
T024 (schema-validator.test.js) || T025 (ipc-bridge.test.js)
```

### User Story 1 + User Story 4 병렬 개발
```bash
# US1과 US4는 완전히 독립적 - 동시 개발 가능
US1: T020-T027
US4: T028-T032

# 팀이 있다면:
# - Developer 1: US1 (MVP) 담당
# - Developer 2: US4 (health check) 담당
```

### Polish 단계
```bash
# 모든 polish 작업은 병렬 가능
T049 (systemd service) || T050 (watchdog) || T051 (sd_notify) || T052 (package.json) || T053 (.env) || T054 (CHANGELOG) || T055 (buffer size) || T056 (logger) || T057 (caching)
```

---

## Testing Strategy

### 단위 테스트 (Mock 사용)
- **IPC 브리지**: Mock IPC 서버 사용
- **Schema validator**: 가짜 스키마 데이터 사용
- **Middleware**: req/res mock 객체 사용

### 통합 테스트 (Supertest + Mock IPC 서버)
- **HTTP API**: Mock IPC 서버로 MXRC Core 시뮬레이션
- **WebSocket**: ws 라이브러리로 클라이언트 시뮬레이션

### 실제 환경 테스트 (선택적)
- **MXRC Core 연동**: 실제 RT 프로세스와 통합 테스트 (주의: RT 성능 영향 측정)

---

## Deployment Checklist

### Pre-Deployment
- [ ] 모든 테스트 통과
- [ ] 테스트 커버리지 확인
- [ ] README.md 및 quickstart.md 검토
- [ ] .env.example 업데이트

### Deployment
- [ ] systemd 서비스 파일 복사
- [ ] 서비스 enable 및 start
- [ ] journald 로그 확인
- [ ] health check endpoint 검증

### Post-Deployment
- [ ] 메모리 사용량 모니터링 (systemctl status mxrc-webapi)
- [ ] RT 프로세스 영향 측정 (1% 미만 확인)
- [ ] WebSocket 동시 연결 수 확인

---

## Progress Tracking

**Setup**: 0/6 완료
**Foundational**: 0/13 완료
**US1 (MVP)**: 0/8 완료 🎯
**US4**: 0/5 완료
**US2**: 0/10 완료
**US3**: 0/6 완료
**Polish**: 0/9 완료

**Total**: 0/57 완료 (0%)

---

**Last Updated**: 2025-01-24
**Next Step**: T001 프로젝트 디렉토리 생성부터 시작
