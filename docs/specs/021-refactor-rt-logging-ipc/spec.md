# Feature Specification: RT/Non-RT 아키텍처 및 로깅 시스템 개선

**Feature Branch**: `021-refactor-rt-logging-ipc`
**Created**: 2025-11-21
**Status**: Draft
**Progress**: 0/5 (Spec → Plan → Tasks → Implementation → Completed)
**Last Updated**: 2025-11-21
**Input**: User description: "docs/research/005-*.md 참고하여 작성 브랜치는 현재 브랜치 및 상태에서 이어서 진"

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

### User Story 1 - 통합 로그를 통한 신속한 디버깅 (Priority: P1)

개발자가 RT와 Non-RT 프로세스에 걸친 복잡한 문제를 디버깅해야 하는 상황에서, 두 프로세스의 모든 로그를 시간 순으로 정렬된 단일 뷰에서 확인할 수 있습니다. 이를 통해 문제의 원인을 신속하게 파악하고 해결 시간을 단축합니다.

**Why this priority**: 분산된 로그는 시스템의 동작을 이해하는 데 가장 큰 장애물 중 하나입니다. 로그 통합은 유지보수성과 개발 생산성을 직접적으로 향상시키는 가장 중요한 기능입니다.

**Independent Test**: RT 프로세스와 Non-RT 프로세스에서 각각 로그를 발생시킨 후, 최종 로그 파일 또는 출력에서 두 로그가 JSON 형식으로, 시간 순서에 맞게 통합되어 출력되는지 확인합니다.

**Acceptance Scenarios**:

1. **Given** RT 프로세스와 Non-RT 프로세스가 실행 중일 때,
   **When** 두 프로세스에서 거의 동시에 로그가 발생하면,
   **Then** 최종 로그 저장소에는 두 로그가 타임스탬프 순서에 따라 정확하게 기록된다.
2. **Given** 로그 파일이 JSON 형식으로 기록되고 있을 때,
   **When** 로그 뷰어를 사용하여 'source' 필드로 필터링하면,
   **Then** 특정 프로세스(RT 또는 Non-RT)의 로그만 정확히 표시된다.

---

### User Story 2 - 로깅 부하에도 안정적인 실시간 성능 보장 (Priority: P1)

시스템에 높은 로깅 부하가 발생하는 상황에서도 RT 프로세스는 로깅 작업으로 인한 지연 없이 정해진 시간(deadline) 안에 모든 실시간 Task를 완료합니다.

**Why this priority**: RT 프로세스의 결정론적 실행 시간 보장은 시스템의 핵심 요구사항입니다. 로깅과 같은 부가 기능이 이 요구사항을 저해해서는 안 됩니다.

**Independent Test**: RT 프로세스가 주기적인 Task를 수행하도록 설정하고, 동시에 대량의 로그(초당 수천 건)를 발생시키도록 합니다. Task 실행 주기의 지연 시간(jitter)을 측정하여 허용치 이내인지 확인합니다.

**Acceptance Scenarios**:

1. **Given** RT 프로세스가 1ms 주기로 Task를 실행 중일 때,
   **When** 초당 10,000건의 로그 메시지를 생성하도록 요청하면,
   **Then** Task 실행 주기의 최대 지연 시간은 10μs(마이크로초)를 초과하지 않는다.

---

### User Story 3 - 프로세스 장애 감지 및 시스템 안정 상태 전환 (Priority: P2)

Non-RT 프로세스가 예기치 않게 중단되었을 때, RT 프로세스는 Heartbeat 메커니즘을 통해 수백 ms 내에 장애를 감지하고, 이 상황을 시스템에 알린 후 사전에 정의된 안전 상태로 진입합니다.

**Why this priority**: 한 프로세스의 장애가 전체 시스템의 불안정으로 이어지는 것을 방지하고, 시스템이 예측 가능한 방식으로 안전하게 대응하도록 보장합니다.

**Independent Test**: 실행 중인 Non-RT 프로세스를 강제 종료(`kill -9`)한 후, RT 프로세스가 지정된 시간 내에 'critical' 레벨의 장애 감지 로그를 기록하는지 확인합니다.

**Acceptance Scenarios**:

1. **Given** RT와 Non-RT 프로세스가 Heartbeat를 교환하며 정상 실행 중일 때,
   **When** Non-RT 프로세스가 응답하지 않으면,
   **Then** RT 프로세스는 500ms 이내에 'Non-RT process heartbeat lost' 로그를 기록한다.

---

### Edge Cases

- **공유 메모리 큐가 가득 찼을 때**: RT 프로세스는 오래된 로그를 덮어쓰거나 폐기하고, 이 사실을 알리는 특수 로그(카운터 등)를 남겨야 합니다.
- **프로세스 시작 순서**: Non-RT 프로세스가 먼저 시작되지 않으면 RT 프로세스는 어떻게 동작해야 하는가? (예: Non-RT가 준비될 때까지 대기)
- **Heartbeat 신호 유실**: 네트워크나 시스템의 일시적인 문제로 Heartbeat 신호가 유실될 경우, 몇 회까지 허용할 것인가?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: RT 프로세스는 로그 기록을 위해 파일 I/O 또는 기타 블로킹 시스템 콜을 절대 호출해서는 안 된다.
- **FR-002**: RT 프로세스는 모든 로그 메시지를 실시간-안전(real-time safe) 락-프리(lock-free) 공유 메모리 링 버퍼에 기록해야 한다.
- **FR-003**: Non-RT 프로세스는 공유 메모리 버퍼에서 로그 레코드를 읽어와 파일 시스템과 같은 영구 저장소에 비동기적으로 기록해야 한다.
- **FR-004**: 시스템에서 생성되는 모든 로그는 타임스탬프, 로그 레벨, 소스(프로세스/모듈), 메시지를 포함하는 JSON 형식이어야 한다.
- **FR-005**: RT 프로세스와 Non-RT 프로세스 간의 통신은 Lock-Free 공유 메모리 큐를 통해 이루어져야 한다.
- **FR-006**: RT와 Non-RT 프로세스는 주기적으로(예: 100ms 마다) 서로에게 Heartbeat 신호를 교환해야 한다.
- **FR-007**: 한 프로세스가 5회 연속으로 Heartbeat 신호를 받지 못하면, 해당 상황을 'critical' 레벨의 로그로 기록하고 사전에 정의된 안전 상태로 전환을 시도해야 한다.
- **FR-008**: 운영 중 Non-RT 프로세스는 IPC를 통해 RT 프로세스 내 특정 모듈의 로그 레벨을 동적으로 변경할 수 있어야 한다.

### Key Entities

- **Log Record**: 타임스탬프, 로그 레벨, 소스, 메시지 및 추가적인 컨텍스트 데이터를 포함하는 구조화된 데이터 엔티티.
- **IPC Message**: Log Record, Heartbeat 신호, 설정 변경 커맨드 등 프로세스 간 통신을 위한 데이터 패킷.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 로깅 작업으로 인한 RT 프로세스의 태스크 마감 시간 위반(deadline miss) 발생률이 0%로 측정된다.
- **SC-002**: 문제 해결 시, RT 및 Non-RT 프로세스에서 발생한 모든 로그를 단일 통합 뷰에서 시간 순으로 조회할 수 있다.
- **SC-003**: 프로세스(RT 또는 Non-RT)의 응답 없음(unresponsive) 상태를 500ms 이내에 감지하여 로그로 기록할 수 있다.
- **SC-004**: 시스템 재시작 없이, 운영 중인 시스템의 특정 모듈 로그 레벨을 'INFO'에서 'DEBUG'로 변경하는 기능이 성공적으로 동작한다.
