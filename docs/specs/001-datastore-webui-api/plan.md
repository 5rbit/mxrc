# Implementation Plan: Datastore WebUI를 위한 Decoupled API 서버

**Branch**: `001-datastore-webui-api` | **Date**: 2025-01-24 | **Spec**: [spec.md](./spec.md)
**Status**: Planning
**Progress**: Phase 0 (Research) → Phase 1 (Design) → Phase 2 (Tasks)
**Last Updated**: 2025-01-24
**Input**: Feature specification from `/docs/specs/001-datastore-webui-api/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/commands/plan.md` for the execution workflow.

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Task, Action, Sequence, API, JSON, CMake 등)
- 일반 설명, 구현 계획, 설계 결정은 모두 한글로 작성합니다

**예시**:
- ✅ 좋은 예: "Task 계층에서 실행 모드를 관리합니다"
- ❌ 나쁜 예: "The Task layer manages execution modes"

---

## Summary

MXRC Core (C++)의 Datastore를 WebUI에서 접근할 수 있도록 Node.js 기반의 독립적인 API 서버를 구현합니다. API 서버는 IPC(공유 메모리)를 통해 Datastore와 통신하며, HTTP RESTful API 및 WebSocket을 통해 데이터 모니터링 및 제어 기능을 제공합니다. systemd 통합을 통해 안정적인 프로세스 관리를 보장합니다.

**핵심 접근 방식**: Decoupled Architecture - MXRC Core의 실시간성과 안정성을 보존하면서 Node.js의 웹 생태계를 활용한 분리된 프로세스 아키텍처

---

## Technical Context

**Language/Version**: Node.js 20 LTS (현재 안정 버전)
**Primary Dependencies**:
- Express.js (HTTP 서버) 또는 Fastify (고성능 대안 - NEEDS CLARIFICATION)
- ws (WebSocket 라이브러리)
- node-addon-api (C++ 공유 메모리 IPC 브리지용 네이티브 addon)
- js-yaml (IPC 스키마 파싱)
- systemd 통합용 라이브러리 (NEEDS CLARIFICATION: systemd-notify npm package 또는 직접 구현)

**Storage**:
- 공유 메모리 (MXRC Core의 Datastore, `tbb::concurrent_hash_map` 기반)
- IPC 스키마: `config/ipc/ipc-schema.yaml` (읽기 전용)

**Testing**:
- Jest (단위 테스트 및 통합 테스트)
- Supertest (HTTP API 테스트)
- mock IPC 인터페이스 (C++ Datastore 없이 테스트 가능)

**Target Platform**: Ubuntu 24.04 LTS (PREEMPT_RT), localhost 네트워크

**Project Type**: Backend API 서버 (Node.js single project)

**Performance Goals**:
- HTTP 요청 처리: 초당 1,000건 이상
- WebSocket 동시 연결: 최소 100개
- 응답 시간: Datastore 읽기 1초 이내, health check 100ms 이내
- 알림 지연: 0.1초 이내

**Constraints**:
- 메모리 사용량: 128MB 이하
- RT 프로세스 영향: 1% 미만 (IPC 오버헤드)
- 가용성: 99.9% (systemd 자동 재시작 포함)
- 타입 안전성: IPC 스키마 기반 검증 필수

**Scale/Scope**:
- IPC 스키마 키: 약 50-100개 (현재 `ipc-schema.yaml` 기준)
- 동시 사용자: 단일 사용자 (초기 버전)
- API endpoints: 약 20-30개 (CRUD + health check + WebSocket)

---

## Constitution Check (Phase 0 & Phase 1)

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Initial Check (Phase 0)

### ✅ Principle I: 계층적 아키텍처 원칙
**Status**: PASS (해당 없음)
**Rationale**: API 서버는 MXRC Core와 독립적인 프로세스이며, Core의 계층 구조를 변경하지 않습니다.

### ✅ Principle II: 인터페이스 기반 설계
**Status**: PASS
**Rationale**: API 서버는 IPC 브리지 인터페이스를 통해 Datastore에 접근하며, HTTP/WebSocket 인터페이스를 명확히 정의합니다.

### ✅ Principle III: RAII 원칙
**Status**: PASS (해당 없음)
**Rationale**: Node.js 프로젝트이므로 C++ RAII 원칙은 네이티브 addon 부분에만 적용됩니다. 네이티브 addon에서는 RAII를 준수합니다.

### ✅ Principle IV: 메모리 안전성
**Status**: PASS
**Rationale**:
- Node.js: 가비지 컬렉션 기반 자동 메모리 관리
- 네이티브 addon: RAII 및 스마트 포인터 사용
- 통합 테스트에서 메모리 누수 검증

### ✅ Principle V: 테스트 주도 개발 (TDD)
**Status**: PASS
**Rationale**:
- Jest를 사용한 단위/통합 테스트
- mock IPC 인터페이스로 격리된 테스트 환경
- API endpoint별 테스트 케이스

### ✅ Principle VI: 실시간 성능 (Phase 1 Re-check)
**Status**: PASS
**Rationale**:
- **Phase 0**: IPC 메커니즘을 Unix Domain Socket으로 선정하여 낮은 latency 보장
- **Phase 1**: Rate limiting (초당 10-20 요청) 및 systemd cgroup 리소스 제한 (CPU 50%, Memory 512MB) 적용
- **결과**: RT 프로세스 영향 1% 미만 달성 가능

### ✅ Principle VII: 문서화 및 한글 사용
**Status**: PASS
**Rationale**:
- 모든 문서 한글 작성 (research.md, data-model.md, quickstart.md)
- 기술 용어만 영어 표기
- API 문서 (OpenAPI, WebSocket Protocol) 제공

---

### Phase 1 Re-check Summary

**Result**: 모든 원칙 충족 ✅

**Updated Decisions**:
1. IPC 메커니즘: Unix Domain Socket (성능/복잡도 균형)
2. HTTP 프레임워크: Fastify (4.5배 빠른 성능)
3. Rate Limiting: Token Bucket (초당 10-20 요청, RT 보호)
4. 메모리 제한: systemd cgroup으로 512MB 강제

**No Constitution Violations**: 프로젝트 원칙과 완전히 일치함

---

## Project Structure

### Documentation (this feature)

```text
docs/specs/001-datastore-webui-api/
├── spec.md              # Feature Specification (완료)
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (조사 결과)
├── data-model.md        # Phase 1 output (데이터 모델)
├── quickstart.md        # Phase 1 output (개발자 가이드)
├── contracts/           # Phase 1 output (API 스키마)
│   ├── openapi.yaml     # RESTful API 명세
│   └── websocket.md     # WebSocket 프로토콜
├── checklists/          # 검증 체크리스트
│   └── requirements.md  # Spec 품질 체크리스트 (완료)
└── tasks.md             # Phase 2 output (/speckit.tasks - NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
webapi/                  # Node.js API 서버 (신규 생성)
├── src/
│   ├── server.js        # Express/Fastify 서버 엔트리포인트
│   ├── config/
│   │   └── schema-loader.js  # IPC 스키마 로더
│   ├── ipc/
│   │   ├── bridge.js    # IPC 브리지 (네이티브 addon 래퍼)
│   │   └── native/      # C++ 네이티브 addon (공유 메모리 접근)
│   │       ├── binding.gyp
│   │       ├── ipc_bridge.cc
│   │       └── ipc_bridge.h
│   ├── routes/
│   │   ├── datastore.js # Datastore CRUD API
│   │   ├── health.js    # Health check endpoint
│   │   └── websocket.js # WebSocket 핸들러
│   ├── middleware/
│   │   ├── schema-validator.js  # 타입/권한 검증
│   │   ├── rate-limiter.js      # 요청 속도 제한
│   │   └── error-handler.js     # 오류 처리
│   └── utils/
│       ├── systemd-notify.js    # systemd Watchdog 통합
│       └── logger.js            # 로깅 (journald)
├── tests/
│   ├── unit/            # 단위 테스트
│   │   ├── schema-validator.test.js
│   │   ├── ipc-bridge.test.js
│   │   └── routes.test.js
│   ├── integration/     # 통합 테스트
│   │   ├── api.test.js
│   │   └── websocket.test.js
│   └── mocks/
│       └── ipc-mock.js  # IPC 브리지 mock
├── package.json
├── package-lock.json
└── README.md

systemd/                 # systemd 서비스 파일 (기존 디렉토리)
└── mxrc-webapi.service  # API 서버 서비스 정의 (신규 추가)

config/ipc/              # IPC 스키마 (기존)
└── ipc-schema.yaml      # 읽기 전용 참조
```

**Structure Decision**:
- **Node.js single project 구조** 선택 (Option 1 변형)
- 이유: API 서버는 독립적인 백엔드 서비스이며, Web UI (React)는 별도 기능으로 분리됨
- 위치: 프로젝트 루트에 `webapi/` 디렉토리 생성 (C++ 코드와 명확히 분리)
- 네이티브 addon: `src/ipc/native/`에 위치 (Node.js와 C++ 간 브리지)

---

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

해당 없음. 모든 Constitution 원칙이 충족되었습니다.

---

## Phase 0: Research & Decisions

### Research Tasks

다음 사항들에 대한 조사가 필요합니다:

1. **Node.js에서 C++ 공유 메모리 접근 방법** (NEEDS CLARIFICATION)
   - `node-addon-api`를 사용한 네이티브 addon 구현
   - `tbb::concurrent_hash_map` 접근 방법
   - `VersionedData<T>` 구조체 처리 방법
   - 대안: Unix Domain Socket 또는 Named Pipe (성능 비교)

2. **HTTP 프레임워크 선택: Express vs Fastify** (NEEDS CLARIFICATION)
   - 성능: Fastify가 더 빠름 (초당 1,000건 요청 처리 목표)
   - 생태계: Express가 더 성숙
   - 프로젝트 적합성 평가

3. **systemd 통합 방법** (NEEDS CLARIFICATION)
   - npm 패키지: `systemd-notify` 또는 `sd-notify`
   - 직접 구현: `sd_notify()` 함수 직접 호출 (네이티브 addon)
   - Watchdog 타이머 설정 방법

4. **WebSocket 라이브러리 선택**
   - `ws` (표준 라이브러리)
   - `socket.io` (더 많은 기능, 오버헤드 있음)
   - 적합성 평가

5. **IPC 스키마 타입 매핑**
   - YAML 타입 → JavaScript/TypeScript 타입 매핑
   - `Vector3d`, `array<double, 64>` 등 C++ 타입 처리
   - 타입 검증 로직

6. **Rate Limiting 전략**
   - 알고리즘: Token Bucket vs Sliding Window
   - 라이브러리: `express-rate-limit` vs 직접 구현
   - RT 프로세스 보호를 위한 적절한 제한값

### Research Output

**결과**: `research.md` 파일에 다음 내용 포함:
- 각 기술 선택에 대한 결정 및 근거
- 대안 검토 및 선택 이유
- 성능 벤치마크 (가능한 경우)
- 구현 패턴 및 예제 코드

---

## Phase 1: Design Artifacts

### 1. Data Model (`data-model.md`)

**핵심 엔티티**:

1. **DatastoreKey** (IPC 스키마 키)
   - 속성: name (string), type (string), access (object), description (string)
   - 관계: N/A (읽기 전용 스키마)
   - 출처: `config/ipc/ipc-schema.yaml`

2. **DatastoreValue** (Datastore 값)
   - 속성: key (string), value (any), version (uint64), timestamp (Date)
   - 관계: DatastoreKey를 참조
   - 상태: 동적 데이터 (공유 메모리에서 읽기)

3. **WebSocketSubscription** (구독 정보)
   - 속성: connectionId (string), subscribedKeys (array of string)
   - 관계: DatastoreKey 배열
   - 상태: 연결됨, 구독 중, 연결 해제됨

4. **HealthStatus** (시스템 상태)
   - 속성: status (enum: healthy/degraded/unhealthy), services (object), timestamp (Date)
   - 관계: N/A
   - 상태: 실시간 계산

### 2. API Contracts (`contracts/`)

**RESTful API** (`openapi.yaml`):

```yaml
openapi: 3.0.0
info:
  title: MXRC Datastore WebAPI
  version: 1.0.0
paths:
  /api/datastore/{key}:
    get:
      summary: Datastore 키 읽기
      parameters:
        - name: key
          in: path
          required: true
          schema:
            type: string
      responses:
        200:
          description: 성공
          content:
            application/json:
              schema:
                type: object
                properties:
                  key: { type: string }
                  value: { type: any }
                  version: { type: integer }
                  timestamp: { type: string, format: date-time }
        403:
          description: 읽기 권한 없음
        404:
          description: 키가 존재하지 않음
        503:
          description: MXRC Core 연결 실패

    put:
      summary: Datastore 키 쓰기 (Non-RT 허용 키만)
      parameters:
        - name: key
          in: path
          required: true
          schema:
            type: string
      requestBody:
        required: true
        content:
          application/json:
            schema:
              type: object
              properties:
                value: { type: any }
      responses:
        200:
          description: 성공
        400:
          description: 타입 불일치
        403:
          description: 쓰기 권한 없음
        404:
          description: 키가 존재하지 않음

  /api/health:
    get:
      summary: Health check
      responses:
        200:
          description: 성공
          content:
            application/json:
              schema:
                type: object
                properties:
                  status: { type: string, enum: [healthy, degraded, unhealthy] }
                  mxrc_rt: { type: string }
                  mxrc_nonrt: { type: string }
                  api_server: { type: string }
                  timestamp: { type: string, format: date-time }
```

**WebSocket Protocol** (`websocket.md`):

```markdown
# WebSocket Protocol

## Connection
- URL: `ws://localhost:PORT/ws`
- Subprotocol: `datastore-v1`

## Messages (JSON format)

### Client → Server

#### Subscribe
```json
{
  "type": "subscribe",
  "keys": ["robot_position", "robot_velocity"]
}
```

#### Unsubscribe
```json
{
  "type": "unsubscribe",
  "keys": ["robot_position"]
}
```

### Server → Client

#### Update Notification
```json
{
  "type": "update",
  "key": "robot_position",
  "value": [1.0, 2.0, 3.0],
  "version": 12345,
  "timestamp": "2025-01-24T10:00:00Z"
}
```

#### Error
```json
{
  "type": "error",
  "code": "PERMISSION_DENIED",
  "message": "Read permission denied for key 'internal_state'"
}
```
```

### 3. Quickstart Guide (`quickstart.md`)

**내용**:
1. **환경 설정**
   - Node.js 20 LTS 설치
   - 의존성 설치: `npm install`
   - 네이티브 addon 빌드: `npm run build:native`

2. **개발 서버 실행**
   - 로컬 개발: `npm run dev` (nodemon 사용)
   - MXRC Core mock: `npm run dev:mock` (IPC mock 사용)

3. **테스트 실행**
   - 전체 테스트: `npm test`
   - 단위 테스트: `npm run test:unit`
   - 통합 테스트: `npm run test:integration`
   - 커버리지: `npm run test:coverage`

4. **프로덕션 배포**
   - systemd 서비스 등록: `sudo systemctl enable mxrc-webapi`
   - 서비스 시작: `sudo systemctl start mxrc-webapi`
   - 로그 확인: `journalctl -u mxrc-webapi -f`

5. **API 테스트**
   - curl 예제
   - WebSocket 클라이언트 예제 (wscat)

---

## Phase 2: Task Breakdown

**Note**: 이 섹션은 `/speckit.tasks` 명령으로 생성됩니다. Phase 1 완료 후 실행하세요.

작업 목록은 `tasks.md` 파일에 생성됩니다.

---

## Next Steps

1. ✅ **Phase 0 완료**: research.md 생성 (모든 NEEDS CLARIFICATION 해결)
2. ✅ **Phase 1 완료**: data-model.md, contracts/, quickstart.md 생성
3. ✅ **Phase 2 완료**: tasks.md 생성 (57개 작업, User Story별 구성)
4. ⬜ **Implementation 시작**: tasks.md의 작업 순서대로 구현 시작

---

## Phase 1 Deliverables

✅ **Completed**:
- `research.md`: 기술 스택 선정 및 조사 결과
- `data-model.md`: 5개 핵심 엔티티 정의
- `contracts/openapi.yaml`: RESTful API 명세
- `contracts/websocket.md`: WebSocket 프로토콜 정의
- `quickstart.md`: 개발자 가이드

## Phase 2 Deliverables

✅ **Completed**:
- `tasks.md`: 57개 구현 작업 (User Story별 구성)
  - Setup: 6 tasks
  - Foundational: 13 tasks
  - US1 (MVP): 8 tasks
  - US4: 5 tasks
  - US2: 10 tasks
  - US3: 6 tasks
  - Polish: 9 tasks

---

**Last Updated**: 2025-01-24
**Status**: Phase 2 완료, 구현 대기
