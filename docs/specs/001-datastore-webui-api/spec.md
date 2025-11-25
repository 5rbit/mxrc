# Feature Specification: Datastore WebUI를 위한 Decoupled API 서버

**Feature Branch**: `001-datastore-webui-api`
**Created**: 2025-01-24
**Status**: In Progress
**Progress**: 3/5 (Spec → Plan → Tasks → Implementation → Completed)
**Last Updated**: 2025-01-24
**Input**: User description: "docs/research/008-*.md 구현 하고싶어"

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Task, Action, Sequence, API, JSON 등)
- 일반 설명, 요구사항, 시나리오는 모두 한글로 작성합니다

**예시**:
- ✅ 좋은 예: "사용자는 Task를 생성할 수 있어야 합니다"
- ❌ 나쁜 예: "User can create a task"

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Datastore 데이터 실시간 모니터링 (Priority: P1)

시스템 관리자는 웹 브라우저를 통해 MXRC Core의 Datastore에 저장된 로봇 상태 데이터(위치, 속도, 센서 값 등)를 실시간으로 모니터링합니다.

**Why this priority**: Datastore 가시성 확보는 시스템 운영 및 디버깅의 가장 기본적인 요구사항입니다. 이 기능만으로도 시스템 상태를 파악할 수 있는 독립적인 가치를 제공합니다.

**Independent Test**: 웹 브라우저에서 특정 URL에 접속하여 현재 로봇 위치(`robot_position`), 속도(`robot_velocity`) 등의 실시간 데이터를 확인할 수 있으면 테스트 완료입니다.

**Acceptance Scenarios**:

1. **Given** MXRC Core가 실행 중이고 API 서버가 시작된 상태, **When** 관리자가 웹 브라우저에서 `/api/datastore/robot_position` endpoint에 접속, **Then** 현재 로봇 위치 데이터가 JSON 형식으로 표시됨
2. **Given** RT 프로세스가 Datastore에 새로운 센서 데이터를 기록한 상태, **When** 관리자가 `/api/datastore/ethercat_sensor_position` endpoint를 조회, **Then** 최신 센서 위치 데이터(최대 64축)가 반환됨
3. **Given** API 서버가 실행 중, **When** MXRC Core가 중지된 상태에서 데이터를 요청, **Then** 적절한 오류 메시지와 함께 503 Service Unavailable 응답을 받음

---

### User Story 2 - 데이터 변경 알림 수신 (Priority: P2)

시스템 관리자는 WebSocket을 통해 Datastore의 특정 키(예: `robot_position`)에 대한 변경 알림을 실시간으로 수신하여 UI를 자동으로 업데이트합니다.

**Why this priority**: 실시간 모니터링의 효율성을 극대화합니다. Polling 방식 대비 네트워크 부하를 줄이고 즉각적인 데이터 반영이 가능합니다.

**Independent Test**: WebSocket 연결 후 특정 키를 구독하고, RT 프로세스에서 해당 키를 업데이트했을 때 WebSocket을 통해 변경 알림을 받으면 테스트 완료입니다.

**Acceptance Scenarios**:

1. **Given** 관리자가 WebSocket 연결을 수립한 상태, **When** `robot_position` 키를 구독하고 RT 프로세스가 위치를 업데이트, **Then** 1초 이내에 WebSocket을 통해 변경된 위치 데이터를 수신함
2. **Given** 여러 키(`robot_position`, `robot_velocity`)를 동시에 구독한 상태, **When** 어느 하나라도 업데이트됨, **Then** 변경된 키와 값만 선택적으로 알림 수신
3. **Given** WebSocket 연결이 끊어진 상태, **When** 재연결 시도, **Then** 이전 구독 정보 없이 새로운 세션으로 재연결됨

---

### User Story 3 - Datastore 제어 명령 전송 (Priority: P3)

시스템 관리자는 Web UI를 통해 Non-RT 프로세스가 쓰기 권한을 가진 Datastore 키(예: `ethercat_target_position`)에 대해 값을 설정합니다.

**Why this priority**: 모니터링 이후 제어 기능을 추가하여 시스템 관리 효율성을 높입니다. 안전성을 위해 RT 프로세스 전용 키는 제외합니다.

**Independent Test**: Web UI에서 목표 위치값을 입력하고 전송 버튼을 클릭했을 때, API 서버를 통해 Datastore의 해당 키가 업데이트되면 테스트 완료입니다.

**Acceptance Scenarios**:

1. **Given** 관리자가 Web UI의 제어 패널에 접속한 상태, **When** `ethercat_target_position[0]`에 1.57(rad) 값을 입력하고 전송, **Then** Datastore의 해당 키가 업데이트되고 성공 응답(200 OK)을 받음
2. **Given** RT 전용 쓰기 키(`robot_position`)에 대한 쓰기 요청, **When** API 서버가 요청을 수신, **Then** 403 Forbidden 오류와 함께 "RT-only write key" 메시지 반환
3. **Given** 잘못된 타입의 데이터 전송(예: Vector3d 키에 문자열 전송), **When** API 서버가 요청을 검증, **Then** 400 Bad Request 오류와 함께 타입 불일치 메시지 반환

---

### User Story 4 - 시스템 상태 모니터링 (Priority: P4)

시스템 관리자는 Web UI를 통해 MXRC Core 프로세스(RT/Non-RT)의 실행 상태, API 서버의 헬스 체크, systemd 서비스 상태를 확인합니다.

**Why this priority**: 운영 가시성을 제공하여 시스템 장애를 조기에 감지할 수 있습니다.

**Independent Test**: `/api/health` endpoint 접속 시 모든 서비스의 상태 정보가 반환되면 테스트 완료입니다.

**Acceptance Scenarios**:

1. **Given** 모든 서비스가 정상 실행 중, **When** `/api/health` endpoint를 조회, **Then** `{"status": "healthy", "mxrc_rt": "running", "mxrc_nonrt": "running", "api_server": "running"}` 형식의 응답을 받음
2. **Given** MXRC RT 프로세스가 중지된 상태, **When** health check 요청, **Then** `{"status": "degraded", "mxrc_rt": "stopped", ...}` 응답을 받음
3. **Given** API 서버가 IPC 연결에 실패한 상태, **When** health check 요청, **Then** `{"status": "unhealthy", "error": "IPC connection failed"}` 응답을 받음

---

### Edge Cases

- **MXRC Core 프로세스가 재시작되는 경우**: API 서버는 IPC 연결을 자동으로 재수립하고, 재연결 실패 시 적절한 오류 응답을 반환해야 합니다.
- **다수의 동시 WebSocket 연결**: 시스템은 최소 100개의 동시 WebSocket 연결을 처리할 수 있어야 하며, 연결 수 제한 초과 시 새로운 연결을 거부해야 합니다.
- **Datastore 키가 존재하지 않는 경우**: API는 404 Not Found 응답과 함께 명확한 오류 메시지를 반환해야 합니다.
- **API 서버가 과부하 상태인 경우**: 응답 시간이 5초를 초과하면 타임아웃 오류를 반환하고, 추가 요청은 429 Too Many Requests로 제한해야 합니다.
- **IPC 스키마와 실제 Datastore 타입 불일치**: API 서버 시작 시 스키마 검증을 수행하고, 불일치 발견 시 경고 로그를 기록하고 해당 키 접근을 차단해야 합니다.

---

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: 시스템은 MXRC Core(C++)와 **독립적인 프로세스**로 동작하는 Node.js 기반 API 서버를 제공해야 합니다
- **FR-002**: API 서버는 IPC 메커니즘(공유 메모리 기반)을 통해 MXRC Core의 Datastore에 접근해야 합니다
- **FR-003**: API 서버는 HTTP RESTful API를 통해 Datastore의 읽기 및 쓰기 기능을 제공해야 합니다
- **FR-004**: API 서버는 WebSocket을 통해 Datastore 키의 변경 알림을 실시간으로 전송해야 합니다
- **FR-005**: 시스템은 `config/ipc/ipc-schema.yaml`에 정의된 접근 권한(`rt_read`, `rt_write`, `nonrt_read`, `nonrt_write`)을 준수해야 합니다
- **FR-006**: API 서버는 IPC 스키마에 정의되지 않은 키에 대한 접근 요청을 거부해야 합니다
- **FR-007**: API 서버는 타입 안전성을 보장해야 하며, 스키마에 정의된 타입과 일치하지 않는 데이터 쓰기 요청을 거부해야 합니다
- **FR-008**: API 서버는 systemd 서비스(`mxrc-webapi.service`)로 등록되어 systemd에 의해 관리되어야 합니다
- **FR-009**: systemd 서비스는 `mxrc-rt.service` 의존성을 가져야 하며, RT 프로세스 시작 후에만 API 서버가 시작되어야 합니다
- **FR-010**: API 서버는 systemd Watchdog 기능을 사용하여 활성 상태를 보고해야 합니다
- **FR-011**: API 서버는 모든 로그를 systemd-journald에 기록해야 합니다
- **FR-012**: 시스템은 health check endpoint(`/api/health`)를 제공하여 MXRC Core와의 연결 상태를 보고해야 합니다
- **FR-013**: API 서버는 CORS(Cross-Origin Resource Sharing)를 지원하여 다양한 출처의 Web UI 접근을 허용해야 합니다
- **FR-014**: API 서버는 비정상 종료 시 systemd에 의해 자동으로 재시작되어야 합니다
- **FR-015**: 시스템은 API 서버와 MXRC Core 간의 IPC 연결 실패 시 자동 재연결을 시도해야 합니다

### Key Entities

- **API 서버 (Node.js 프로세스)**: MXRC Core와 독립적으로 실행되는 중간 계층 서버. IPC를 통해 Datastore에 접근하고 HTTP/WebSocket 인터페이스를 제공합니다.
- **IPC 브리지**: API 서버 내부에서 MXRC Core의 공유 메모리 기반 Datastore와 통신하는 컴포넌트. 읽기/쓰기 작업 및 변경 알림 구독을 처리합니다.
- **HTTP Handler**: RESTful API endpoint를 처리하는 컴포넌트. 요청 검증, 스키마 확인, Datastore 접근 제어를 수행합니다.
- **WebSocket Manager**: WebSocket 연결을 관리하고 Datastore 변경 알림을 구독자에게 브로드캐스트하는 컴포넌트.
- **Schema Validator**: `ipc-schema.yaml`을 로드하여 API 요청의 타입 및 접근 권한을 검증하는 컴포넌트.
- **Systemd Service Unit (`mxrc-webapi.service`)**: API 서버를 관리하는 systemd 서비스 정의. 의존성, 재시작 정책, Watchdog 설정을 포함합니다.

---

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 관리자는 웹 브라우저를 통해 Datastore의 모든 읽기 가능 키에 5초 이내에 접근할 수 있어야 합니다
- **SC-002**: Datastore 키 변경 발생 시, 구독 중인 모든 WebSocket 클라이언트는 1초 이내에 알림을 수신해야 합니다
- **SC-003**: API 서버는 초당 최소 1,000건의 HTTP 요청을 처리할 수 있어야 합니다
- **SC-004**: API 서버는 최소 100개의 동시 WebSocket 연결을 안정적으로 유지할 수 있어야 합니다
- **SC-005**: MXRC Core가 실행 중일 때 API 서버의 가용성은 99.9% 이상이어야 합니다
- **SC-006**: API 서버의 메모리 사용량은 512MB를 초과하지 않아야 합니다
- **SC-007**: IPC 통신 오버헤드는 MXRC Core의 RT 성능에 1% 이상의 영향을 주지 않아야 합니다
- **SC-008**: API 서버 비정상 종료 시 systemd에 의해 10초 이내에 재시작되어야 합니다
- **SC-009**: 타입 불일치 또는 권한 위반 요청의 95% 이상이 명확한 오류 메시지와 함께 거부되어야 합니다
- **SC-010**: health check endpoint는 500ms 이내에 응답해야 합니다

---

## Scope & Boundaries

### In Scope
- Node.js 기반 API 서버 구현
- IPC를 통한 Datastore 읽기/쓰기 기능
- RESTful API 및 WebSocket 인터페이스
- `ipc-schema.yaml` 기반 타입 안전성 및 접근 제어
- systemd 통합 및 서비스 관리
- health check 및 모니터링 기능

### Out of Scope
- Web UI (React) 구현은 별도 기능으로 분리
- 사용자 인증 및 권한 관리(추후 보안 강화 시 추가 검토)
- Datastore 데이터의 영구 저장(로깅/분석 기능은 별도 구현)
- API 서버의 로드 밸런싱 및 클러스터링
- HTTPS/TLS 암호화(초기 버전은 로컬 네트워크 환경 가정)

---

## Assumptions

- MXRC Core의 Datastore는 공유 메모리 기반 IPC를 통해 외부 프로세스의 접근을 허용합니다
- Node.js 환경에서 공유 메모리 접근이 가능한 네이티브 모듈(예: node-addon-api) 또는 IPC 라이브러리를 사용할 수 있습니다
- API 서버는 MXRC Core와 동일한 시스템(localhost)에서 실행됩니다
- systemd가 설치되어 있고 서비스 관리 권한이 있습니다
- 초기 버전은 단일 사용자 환경을 가정하며, 다중 사용자 접근 제어는 추후 추가됩니다
- Web UI는 별도의 정적 파일 서버를 통해 제공되거나, API 서버가 정적 파일 제공 기능을 포함할 수 있습니다(구현 단계에서 결정)

---

## Dependencies

- **MXRC Core (C++)**: Datastore 및 IPC 인터페이스 제공
- **IPC Schema (`config/ipc/ipc-schema.yaml`)**: API 서버가 참조하는 타입 및 권한 정의
- **systemd**: 프로세스 관리 및 모니터링
- **Node.js Runtime**: API 서버 실행 환경
- **공유 메모리 라이브러리**: C++ Datastore와의 IPC 통신

---

## Risks & Mitigation

### Risk 1: IPC 통신 안정성
- **위험**: 공유 메모리 접근 시 동기화 문제로 인한 데이터 손상 또는 크래시
- **완화**: Datastore의 `VersionedData` 및 `tbb::concurrent_hash_map` 메커니즘을 활용하여 읽기 일관성 보장. 쓰기는 Non-RT 허용 키에만 제한.

### Risk 2: API 서버 부하로 인한 RT 프로세스 영향
- **위험**: API 서버의 과도한 IPC 요청이 RT 프로세스 성능을 저하시킬 수 있음
- **완화**: 프로세스 수준 분리 및 systemd cgroup 리소스 제한 적용. API 요청 속도 제한(rate limiting) 구현.

### Risk 3: Node.js 비동기 모델과 공유 메모리 접근 충돌
- **위험**: Node.js의 이벤트 루프와 동기적 공유 메모리 접근 간의 충돌
- **완화**: 네이티브 addon을 사용하여 비동기 I/O로 래핑하거나, worker thread를 활용한 격리.

### Risk 4: 스키마 불일치
- **위험**: IPC 스키마와 실제 Datastore 타입 간 불일치로 인한 런타임 오류
- **완화**: API 서버 시작 시 스키마 검증 단계 추가. CI/CD 파이프라인에서 스키마 일치성 자동 검증.

---

## Open Questions

없음. 모든 주요 설계 결정은 research 문서(008-datastore-webui-architecture-proposal.md)를 기반으로 합니다.
