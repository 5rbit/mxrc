# Feature Specification: MXRC 아키텍처 개선 및 고도화

**Feature Branch**: `019-architecture-improvements`
**Created**: 2025-01-23
**Status**: Planning
**Progress**: 3/5 (Spec → Plan → Tasks → Implementation → Completed)
**Last Updated**: 2025-01-23
**Input**: User description: "docs/007-*.md 의 내용을 가지고 개선 및 고도화 진행"

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

### User Story 1 - IPC 계약 명시화 및 타입 안전성 보장 (Priority: P1)

개발자가 RT/Non-RT 프로세스 간 통신(DataStore 키, EventBus 이벤트)을 구현할 때, 명시적으로 정의된 스키마를 통해 컴파일 타임에 타입 안전성을 검증받고, 통신 계약을 명확히 이해할 수 있어야 합니다.

**Why this priority**: IPC 계약의 불명확성은 런타임 오류를 유발하며, 시스템의 안정성과 유지보수성에 직접적인 영향을 미칩니다. 이는 가장 기본적이면서도 중요한 개선 사항입니다.

**Independent Test**: 스키마 파일에서 자동 생성된 코드를 사용하여 DataStore 키 접근 및 EventBus 이벤트 발행/구독을 수행하고, 잘못된 키나 타입 사용 시 컴파일 에러가 발생하는지 검증할 수 있습니다.

**Acceptance Scenarios**:

1. **Given** 개발자가 새로운 DataStore 키를 추가하려고 할 때, **When** 스키마 파일에 키를 정의하고 코드를 생성하면, **Then** 타입 안전한 Accessor가 자동으로 생성되어 컴파일 타임에 타입 검증이 가능해야 합니다.
2. **Given** 개발자가 EventBus를 통해 새로운 이벤트 타입을 발행하려고 할 때, **When** 스키마에 정의되지 않은 이벤트를 사용하면, **Then** 컴파일 에러가 발생하여 런타임 오류를 사전에 방지해야 합니다.
3. **Given** 새로운 개발자가 프로젝트에 참여했을 때, **When** IPC 스키마 문서를 읽으면, **Then** 모든 DataStore 키와 EventBus 이벤트 타입, 데이터 구조, 사용 목적을 명확히 이해할 수 있어야 합니다.

---

### User Story 2 - DataStore Hot Key 성능 최적화 (Priority: P2)

시스템 개발자가 가장 빈번하게 접근되는 실시간 데이터(로봇 위치, 속도 등)에 대해 나노초 수준의 접근 성능(<60ns 읽기, <110ns 쓰기)을 확보할 수 있어야 합니다.

**Why this priority**: RT 프로세스의 성능 목표 달성은 실시간 제어 시스템의 핵심 요구사항이지만, 기본 IPC 계약이 먼저 확립된 후 최적화를 진행하는 것이 효율적입니다.

**Independent Test**: 벤치마크 테스트를 통해 Hot Key로 지정된 데이터에 대한 읽기/쓰기 작업이 목표 성능(<60ns/<110ns)을 만족하는지 측정할 수 있습니다.

**Acceptance Scenarios**:

1. **Given** 로봇의 현재 위치 데이터가 Hot Key로 지정되었을 때, **When** RT 프로세스에서 해당 데이터를 읽으면, **Then** 평균 읽기 시간이 60ns 미만이어야 합니다.
2. **Given** 모터 속도 데이터가 Hot Key로 지정되었을 때, **When** 1000번의 연속 쓰기 작업을 수행하면, **Then** 평균 쓰기 시간이 110ns 미만이고 99 percentile 시간이 150ns 미만이어야 합니다.
3. **Given** Hot Key 최적화가 적용된 상태에서, **When** RT 프로세스가 Major Cycle을 실행하면, **Then** DataStore 접근으로 인한 지연이 전체 Cycle Time의 5% 미만이어야 합니다.

---

### User Story 3 - EventBus 우선순위 및 정책 강화 (Priority: P3)

시스템 개발자가 안전 관련 이벤트(AvoidReq, AVOID_CLEAR 등)에 우선순위를 부여하고, 메시지 병합(Coalescing), 유효기간(TTL), 백프레셔(Backpressure) 정책을 적용하여 시스템의 안정성과 효율성을 높일 수 있어야 합니다.

**Why this priority**: EventBus의 고급 기능은 시스템 안정성을 더욱 향상시키지만, 기본 IPC 계약과 성능 최적화보다 우선순위가 낮습니다.

**Independent Test**: 우선순위가 다른 여러 이벤트를 동시에 발행하고, 높은 우선순위 이벤트가 먼저 처리되는지, TTL이 만료된 이벤트가 폐기되는지, 큐가 가득 찬 상태에서 백프레셔 정책이 작동하는지 검증할 수 있습니다.

**Acceptance Scenarios**:

1. **Given** AvoidReq 이벤트(우선순위 HIGH)와 일반 상태 업데이트 이벤트(우선순위 NORMAL)가 동시에 발행되었을 때, **When** EventBus가 이벤트를 처리하면, **Then** AvoidReq 이벤트가 먼저 처리되어야 합니다.
2. **Given** 상태 업데이트 이벤트에 100ms TTL이 설정되었을 때, **When** 이벤트가 큐에서 150ms 동안 대기한 후 처리되려고 하면, **Then** 해당 이벤트는 폐기되고 처리되지 않아야 합니다.
3. **Given** EventBus 큐 크기가 1000으로 제한되었을 때, **When** 큐가 가득 찬 상태에서 새 이벤트가 발행되면, **Then** 백프레셔 정책(DROP_OLDEST 또는 BLOCK)에 따라 처리되어야 합니다.

---

### User Story 4 - 필드버스 추상화 계층 도입 (Priority: P4)

시스템 아키텍트가 EtherCAT 외에 다른 필드버스 프로토콜(EtherNet/IP, CANopen 등)을 지원할 수 있도록 일반화된 필드버스 인터페이스를 설계하고, 상위 레벨 코드의 재사용성을 높일 수 있어야 합니다.

**Why this priority**: 필드버스 확장성은 장기적인 시스템 유연성을 제공하지만, 현재 EtherCAT 구현이 안정적으로 작동하고 있으므로 우선순위가 낮습니다.

**Independent Test**: IFieldbus 인터페이스를 구현한 Mock 필드버스 드라이버를 생성하고, 기존 EtherCAT 코드와 동일한 방식으로 모터 제어 및 센서 데이터 수집이 가능한지 검증할 수 있습니다.

**Acceptance Scenarios**:

1. **Given** IFieldbus 인터페이스가 정의되었을 때, **When** 개발자가 새로운 CANopen 드라이버를 구현하면, **Then** 상위 레벨의 모터 제어 로직을 변경하지 않고 CANopen을 사용할 수 있어야 합니다.
2. **Given** EtherCAT과 Mock 필드버스 드라이버가 모두 IFieldbus를 구현했을 때, **When** 설정 파일에서 필드버스 타입을 변경하면, **Then** 런타임에 적절한 드라이버가 로드되어 동작해야 합니다.

---

### User Story 5 - Monitoring 및 Observability 강화 (Priority: P2)

운영자와 개발자가 실시간으로 시스템의 성능 메트릭(Cycle Time, Deadline Miss, Queue Size 등)을 모니터링하고, Grafana 대시보드를 통해 시각화하며, 임계값 기반 알림을 받을 수 있어야 합니다.

**Why this priority**: 모니터링은 시스템 안정성 확보와 문제 진단에 필수적이지만, 핵심 IPC 개선 이후에 구현하는 것이 효율적입니다.

**Independent Test**: RT/Non-RT 프로세스를 실행하고 Prometheus 엔드포인트에서 메트릭을 수집하여, Grafana에서 실시간 대시보드가 표시되고, 데드라인 miss 발생 시 알림이 전송되는지 검증할 수 있습니다.

**Acceptance Scenarios**:

1. **Given** RT 프로세스가 실행 중일 때, **When** Prometheus가 `/metrics` 엔드포인트를 스크랩하면, **Then** Cycle Time, Deadline Miss, RTDataStore 메트릭이 Prometheus 포맷으로 노출되어야 합니다.
2. **Given** Grafana 대시보드가 설정되었을 때, **When** RT 프로세스에서 Deadline Miss가 발생하면, **Then** 대시보드에 실시간으로 표시되고, 알림 규칙에 따라 알림이 전송되어야 합니다.
3. **Given** 시스템이 정상 동작 중일 때, **When** 운영자가 Grafana 대시보드를 확인하면, **Then** RT/Non-RT 프로세스의 CPU 사용률, 메모리 사용량, EventBus 큐 크기, Task 성공률 등을 한눈에 파악할 수 있어야 합니다.

---

### User Story 6 - 고가용성(HA) 정책 고도화 (Priority: P3)

시스템 관리자가 프로세스 장애 발생 시 단순 재시작을 넘어, 시스템 상태에 따라 '안전 모드 진입', '부분 기능 재시작', '경고 후 수동 개입 대기' 등 다양한 복구 전략을 선택하고 실행할 수 있어야 합니다.

**Why this priority**: 고급 HA 정책은 시스템 견고성을 크게 향상시키지만, 기본 모니터링 인프라가 구축된 후 구현하는 것이 효율적입니다.

**Independent Test**: 다양한 장애 시나리오(RT 프로세스 크래시, Non-RT 하트비트 타임아웃, DataStore 접근 실패 등)를 시뮬레이션하고, 각 상황에 맞는 복구 정책이 실행되는지 검증할 수 있습니다.

**Acceptance Scenarios**:

1. **Given** RT 프로세스가 3회 연속 Deadline Miss를 기록했을 때, **When** HA Agent가 이를 감지하면, **Then** 시스템은 안전 모드로 진입하고 모든 모터를 정지시켜야 합니다.
2. **Given** Non-RT 프로세스가 응답하지 않을 때, **When** RT 프로세스의 하트비트 모니터가 타임아웃을 감지하면, **Then** 설정된 정책(재시작, 안전 모드, 수동 개입 대기)에 따라 처리되어야 합니다.
3. **Given** 경미한 오류(일시적 센서 통신 실패)가 발생했을 때, **When** HA Agent가 이를 감지하면, **Then** 시스템은 정상 동작을 유지하면서 경고 로그만 기록해야 합니다.

---

### Edge Cases

- IPC 스키마가 변경되었을 때 기존 코드와의 호환성은 어떻게 보장됩니까?
- Hot Key로 지정된 데이터가 너무 많아져서 메모리 제약을 초과하면 어떻게 처리됩니까?
- EventBus에서 우선순위가 동일한 여러 이벤트가 동시에 발행되면 처리 순서는 어떻게 결정됩니까?
- 필드버스 드라이버 전환 중에 모터 제어 명령이 유실되지 않도록 어떻게 보장됩니까?
- Prometheus 엔드포인트가 과도한 스크랩 요청으로 인해 RT 성능에 영향을 미치면 어떻게 대응됩니까?
- HA 복구 정책 실행 중에 또 다른 장애가 발생하면 어떻게 처리됩니까?

## Requirements *(mandatory)*

### Functional Requirements

#### IPC 계약 명시화 (US1)
- **FR-001**: 시스템은 DataStore의 모든 키를 스키마 파일(YAML 또는 JSON)에 명시적으로 정의해야 합니다.
- **FR-002**: 시스템은 EventBus의 모든 이벤트 타입과 페이로드 구조를 스키마 파일에 명시적으로 정의해야 합니다.
- **FR-003**: 시스템은 스키마 파일로부터 C++ 타입 안전 코드(Accessor, Event Struct 등)를 자동 생성해야 합니다.
- **FR-004**: 시스템은 스키마 변경 시 버전 관리를 지원하고, 하위 호환성 검증 기능을 제공해야 합니다.

#### DataStore 성능 최적화 (US2)
- **FR-005**: 시스템은 Hot Key로 지정된 데이터에 대해 Lock-Free 자료구조를 사용하여 최적화된 접근을 제공해야 합니다.
- **FR-006**: 시스템은 Hot Key 읽기 작업에서 평균 60ns 미만, 쓰기 작업에서 평균 110ns 미만의 성능을 보장해야 합니다.
- **FR-007**: 시스템은 VersionedData의 일관성 검증 로직을 캡슐화하여 `read_consistent()` 메서드로 제공해야 합니다.

#### EventBus 고급 기능 (US3)
- **FR-008**: 시스템은 이벤트에 우선순위(LOW, NORMAL, HIGH, CRITICAL)를 설정할 수 있어야 합니다.
- **FR-009**: 시스템은 이벤트에 TTL(Time-To-Live)을 설정하고, 만료된 이벤트를 자동으로 폐기해야 합니다.
- **FR-010**: 시스템은 메시지 병합(Coalescing) 정책을 지원하여 동일 타입의 연속 이벤트를 병합할 수 있어야 합니다.
- **FR-011**: 시스템은 큐 오버플로우 시 백프레셔 정책(DROP_OLDEST, DROP_NEWEST, BLOCK)을 적용해야 합니다.

#### 필드버스 추상화 (US4)
- **FR-012**: 시스템은 일반화된 IFieldbus 인터페이스를 정의하여 다양한 필드버스 프로토콜을 지원할 수 있어야 합니다.
- **FR-013**: 시스템은 EtherCAT, CANopen, EtherNet/IP 등의 필드버스 프로토콜을 플러그인 방식으로 추가할 수 있어야 합니다.
- **FR-014**: 시스템은 설정 파일을 통해 런타임에 사용할 필드버스 드라이버를 선택할 수 있어야 합니다.

#### Monitoring 강화 (US5)
- **FR-015**: 시스템은 RT/Non-RT 프로세스의 성능 메트릭을 Prometheus 포맷으로 노출해야 합니다.
- **FR-016**: 시스템은 Cycle Time, Deadline Miss, Queue Size, Task 성공률 등의 핵심 메트릭을 수집해야 합니다.
- **FR-017**: 시스템은 Grafana 대시보드 템플릿을 제공하여 메트릭 시각화를 지원해야 합니다.
- **FR-018**: 시스템은 임계값 기반 알림 규칙을 Prometheus AlertManager와 통합하여 제공해야 합니다.

#### 고가용성 고도화 (US6)
- **FR-019**: 시스템은 장애 유형에 따라 다양한 복구 전략(재시작, 안전 모드, 부분 복구, 수동 개입)을 선택할 수 있어야 합니다.
- **FR-020**: 시스템은 복구 정책을 상태 머신으로 구현하여 복잡한 장애 시나리오에 대응해야 합니다.
- **FR-021**: 시스템은 복구 작업 이력을 로그로 기록하고, 관리자에게 알림을 제공해야 합니다.

### Key Entities

- **IPC Schema**: DataStore 키와 EventBus 이벤트의 타입, 구조, 목적을 정의하는 명세서입니다. YAML 또는 JSON 형식으로 작성되며, 코드 생성 도구의 입력으로 사용됩니다.
- **Hot Key**: 가장 빈번하게 접근되는 DataStore 키로, Lock-Free 자료구조를 사용하여 최적화됩니다. 예: 로봇 위치, 모터 속도 등.
- **Prioritized Event**: 우선순위(LOW, NORMAL, HIGH, CRITICAL), TTL, Coalescing 정책 등의 메타데이터를 포함하는 EventBus 이벤트입니다.
- **IFieldbus Interface**: 필드버스 프로토콜의 공통 기능(초기화, 데이터 송수신, 상태 조회 등)을 정의하는 추상 인터페이스입니다.
- **Metrics Collector**: RT/Non-RT 프로세스의 성능 메트릭을 수집하고 Prometheus 포맷으로 노출하는 컴포넌트입니다.
- **HA State Machine**: 장애 감지부터 복구 완료까지의 과정을 상태 전이로 표현하는 상태 머신입니다. 각 상태에서 수행할 복구 작업을 정의합니다.

## Success Criteria *(mandatory)*

### Measurable Outcomes

#### IPC 계약 명시화 (US1)
- **SC-001**: 모든 DataStore 키와 EventBus 이벤트가 스키마 파일에 문서화되어, 새로운 개발자가 30분 이내에 IPC 구조를 이해할 수 있어야 합니다.
- **SC-002**: 스키마에서 자동 생성된 코드를 사용할 때, 잘못된 키나 타입 사용 시 100% 컴파일 에러가 발생하여 런타임 오류를 방지해야 합니다.

#### DataStore 성능 최적화 (US2)
- **SC-003**: Hot Key 읽기 작업의 평균 성능이 60ns 미만, 99 percentile이 100ns 미만이어야 합니다.
- **SC-004**: Hot Key 쓰기 작업의 평균 성능이 110ns 미만, 99 percentile이 150ns 미만이어야 합니다.
- **SC-005**: RT 프로세스의 Major Cycle Time에서 DataStore 접근 시간이 차지하는 비율이 5% 미만이어야 합니다.

#### EventBus 고급 기능 (US3)
- **SC-006**: 우선순위가 높은 이벤트가 우선순위가 낮은 이벤트보다 평균 50% 빠르게 처리되어야 합니다.
- **SC-007**: TTL이 설정된 이벤트 중 만료된 이벤트의 폐기율이 100%여야 합니다.
- **SC-008**: 큐 오버플로우 상황에서 백프레셔 정책이 100% 정확하게 적용되어야 합니다.

#### 필드버스 추상화 (US4)
- **SC-009**: IFieldbus 인터페이스를 구현한 새로운 필드버스 드라이버를 2시간 이내에 추가할 수 있어야 합니다.
- **SC-010**: 상위 레벨 모터 제어 코드의 80% 이상이 필드버스 드라이버 변경 시에도 재사용 가능해야 합니다.

#### Monitoring 강화 (US5)
- **SC-011**: RT/Non-RT 프로세스의 핵심 메트릭 20개 이상이 Prometheus 엔드포인트를 통해 노출되어야 합니다.
- **SC-012**: Grafana 대시보드에서 시스템 상태를 10초 이내에 파악할 수 있어야 합니다.
- **SC-013**: 데드라인 miss 발생 시 5초 이내에 알림이 전송되어야 합니다.

#### 고가용성 고도화 (US6)
- **SC-014**: 장애 발생 시 적절한 복구 전략이 10초 이내에 실행되어야 합니다.
- **SC-015**: 복구 성공률이 95% 이상이어야 하며, 실패 시 안전 모드로 진입하여 시스템 손상을 방지해야 합니다.
- **SC-016**: 관리자가 복구 작업 이력을 통해 장애 원인을 15분 이내에 파악할 수 있어야 합니다.

## Scope

### In Scope
- IPC 스키마 정의 및 코드 생성 도구 개발
- DataStore Hot Key 최적화 구현
- EventBus 우선순위, TTL, Coalescing, Backpressure 기능 추가
- IFieldbus 인터페이스 설계 및 EtherCAT 드라이버 리팩토링
- Prometheus/Grafana 기반 모니터링 인프라 구축
- HA 상태 머신 설계 및 복구 정책 구현

### Out of Scope
- Bag 데이터 분석 및 시각화 도구 개발 (별도 Feature로 진행)
- ExecutionContext 구조화 (향후 Feature로 진행)
- 다중 필드버스 동시 운영 (Phase 1에서는 단일 필드버스만 지원)
- 고급 머신러닝 기반 이상 탐지 (Monitoring의 추가 고도화 기능)

## Assumptions

- 현재 시스템은 EtherCAT 기반으로 안정적으로 동작하고 있으며, 필드버스 추상화는 기존 기능을 유지하면서 점진적으로 진행됩니다.
- Prometheus와 Grafana는 별도 서버에 설치되어 있으며, MXRC 프로세스는 메트릭만 노출합니다.
- IPC 스키마는 YAML 형식을 기본으로 하며, 필요 시 JSON도 지원합니다.
- Hot Key는 최대 32개로 제한하며, 각 Hot Key의 데이터 크기는 512 bytes 이하로 가정합니다 (64축 모터 및 64개 IO 모듈 지원).
- HA 복구 정책은 설정 파일을 통해 커스터마이징 가능하며, 기본 정책은 안전 우선 원칙을 따릅니다.

## Dependencies

- 기존 DataStore, EventBus, EtherCAT 모듈의 안정적인 동작
- Prometheus, Grafana의 외부 설치 및 설정
- C++ 코드 생성 도구 (예: Jinja2 템플릿 엔진, Python 스크립트)
- YAML/JSON 파싱 라이브러리 (예: yaml-cpp, nlohmann/json)

## Constraints

- RT 프로세스의 성능에 영향을 주지 않도록 모니터링 오버헤드는 1% 미만으로 제한
- IPC 스키마 변경은 하위 호환성을 최대한 유지하며, 호환성이 깨지는 변경은 메이저 버전 업데이트로 관리
- Hot Key 최적화는 메모리 사용량 증가가 10MB 미만이어야 함
- HA 복구 작업은 시스템 안전을 최우선으로 하며, 데이터 손실 방지가 성능보다 우선

## Risks

- **IPC 스키마 도입으로 인한 초기 학습 곡선**: 개발자가 스키마 정의 및 코드 생성 도구 사용법을 익히는 데 시간이 필요할 수 있습니다. (완화: 상세한 문서 및 예제 제공)
- **Hot Key 최적화의 복잡성**: Lock-Free 자료구조는 구현이 복잡하고 버그 발생 시 디버깅이 어려울 수 있습니다. (완화: 충분한 단위 테스트 및 벤치마크)
- **필드버스 추상화의 성능 오버헤드**: 추상화 계층이 추가되면서 성능 저하가 발생할 수 있습니다. (완화: 가상 함수 최소화, 인라인 최적화)
- **HA 상태 머신의 복잡한 시나리오 처리**: 예상치 못한 장애 조합에서 상태 머신이 제대로 동작하지 않을 수 있습니다. (완화: 광범위한 시뮬레이션 테스트)
