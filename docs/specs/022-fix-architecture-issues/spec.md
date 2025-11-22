# Feature Specification: 아키텍처 안정성 개선

**Feature Branch**: `022-fix-architecture-issues`
**Created**: 2025-01-22
**Status**: Tasks Ready
**Progress**: 3/5 (Spec ✅ → Plan ✅ → Tasks ✅ → Implementation → Completed)
**Last Updated**: 2025-01-22
**Input**: User description: "Architecture improvements from research 006: fix DataStore God Object, EventBus stability, HA split-brain, and systemd race condition"

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: DataStore, EventBus, systemd, IPC 등)
- 일반 설명, 요구사항, 시나리오는 모두 한글로 작성합니다

**예시**:
- ✅ 좋은 예: "시스템은 DataStore 접근 패턴을 개선해야 합니다"
- ❌ 나쁜 예: "System should improve DataStore access patterns"

---

## 개요

Research 문서 006에서 식별된 아키텍처의 4가지 핵심 문제점을 해결하여 시스템의 안정성, 유지보수성, 실시간성을 개선합니다.

**해결할 문제**:
1. **DataStore God Object 문제**: 암묵적 결합 및 데이터 불일치
2. **EventBus 안정성**: 이벤트 과부하 및 백프레셔 부재
3. **HA 스플릿 브레인**: 프로세스 간 하트비트 실패 시나리오
4. **Systemd 시작 순서**: RT/Non-RT 프로세스 경쟁 상태 (CRITICAL)

---

## User Scenarios & Testing

### User Story 1 - Systemd 시작 순서 경쟁 상태 해결 (Priority: P1)

**시나리오**: 시스템 관리자가 시스템을 부팅하거나 서비스를 재시작할 때, RT와 Non-RT 프로세스가 항상 올바른 순서로 시작되어 안정적으로 통신 채널을 설정해야 합니다.

**Why this priority**: 현재 시스템 시작이 타이밍에 따라 실패하는 치명적인 버그입니다. 프로덕션 환경에서 절대 용납될 수 없으며, 다른 모든 기능의 전제 조건입니다.

**Independent Test**: systemd 서비스를 여러 번 재시작하여 100% 성공률을 검증할 수 있습니다. 이는 독립적으로 테스트 가능하며 즉각적인 가치를 제공합니다.

**Acceptance Scenarios**:

1. **Given** 시스템이 부팅 중이거나 서비스가 재시작되는 상황, **When** systemd가 mxrc 서비스들을 시작할 때, **Then** RT 프로세스가 먼저 시작되어 공유 메모리를 생성하고, Non-RT 프로세스가 그 다음에 시작되어 공유 메모리를 성공적으로 열어야 함
2. **Given** RT 프로세스가 시작된 상태, **When** Non-RT 프로세스가 초기화를 시도할 때, **Then** 공유 메모리가 존재하지 않더라도 재시도 로직을 통해 최대 5초 동안 연결을 시도하고 성공해야 함
3. **Given** 서비스가 10번 연속 재시작되는 상황, **When** 각 재시작마다 프로세스 시작 순서가 실행될 때, **Then** 모든 재시작이 100% 성공하고 통신 채널이 정상적으로 설정되어야 함

---

### User Story 2 - DataStore 접근 패턴 개선 (Priority: P2)

**시나리오**: 개발자가 특정 데이터 도메인(센서 데이터, 로봇 상태 등)에 접근할 때, 명확한 인터페이스를 통해 필요한 데이터만 접근하고, 데이터 변경의 영향 범위를 쉽게 파악할 수 있어야 합니다.

**Why this priority**: DataStore God Object 문제는 장기적인 유지보수성과 확장성에 영향을 미칩니다. P1 문제 해결 후 우선적으로 개선해야 할 구조적 문제입니다.

**Independent Test**: 특정 도메인 Accessor(예: SensorDataAccessor)를 구현하고, 해당 Accessor를 통해서만 데이터에 접근하도록 강제하는 단위 테스트로 검증 가능합니다.

**Acceptance Scenarios**:

1. **Given** 센서 데이터에 접근해야 하는 모듈, **When** 센서 데이터 읽기를 시도할 때, **Then** SensorDataAccessor 인터페이스를 통해서만 접근 가능하고 다른 도메인 데이터는 접근 불가능해야 함
2. **Given** 데이터 스키마 변경이 필요한 상황, **When** 특정 도메인의 데이터 구조를 변경할 때, **Then** 해당 도메인 Accessor만 수정하면 되고, 영향 받는 모듈을 컴파일 타임에 식별 가능해야 함
3. **Given** RT 프로세스가 여러 상태 값을 업데이트하는 상황, **When** Non-RT 프로세스가 동시에 데이터를 읽을 때, **Then** 버전 번호를 통해 일관된 데이터 스냅샷을 읽거나, 불일치를 감지하고 재시도해야 함

---

### User Story 3 - EventBus 안정성 강화 (Priority: P3)

**시나리오**: RT 프로세스가 대량의 이벤트를 생성하는 장애 상황에서도, 중요한 이벤트는 유실되지 않고 Non-RT 프로세스에 전달되어야 하며, RT 프로세스의 실시간성은 유지되어야 합니다.

**Why this priority**: 정상 동작 시에는 문제가 없지만, 장애 상황에서의 안정성을 보장합니다. P1, P2 해결 후 진행 가능합니다.

**Independent Test**: 의도적으로 이벤트 폭주 상황을 만들고, 중요 이벤트의 전달률과 RT 프로세스의 지연 시간을 측정하여 검증 가능합니다.

**Acceptance Scenarios**:

1. **Given** RT 프로세스가 초당 10,000개의 로그 이벤트를 생성하는 상황, **When** EventBus 큐가 90% 이상 찼을 때, **Then** 낮은 우선순위 이벤트(일반 로그)부터 버리고 높은 우선순위 이벤트(오류, 상태 변경)는 100% 전달되어야 함
2. **Given** 동일한 센서 연결 끊김 이벤트가 1ms마다 발생하는 상황, **When** EventBus가 이를 처리할 때, **Then** Throttling 메커니즘을 통해 1초에 최대 1개로 제한하여 전송해야 함
3. **Given** EventBus 큐가 가득 찬 상황, **When** RT 프로세스가 새 이벤트를 보내려 할 때, **Then** RT 프로세스는 블로킹되지 않고 즉시 반환되어야 하며, RT 사이클 지연이 10μs 이하여야 함

---

### User Story 4 - HA 스플릿 브레인 방지 (Priority: P4)

**시나리오**: RT와 Non-RT 프로세스 간 통신 채널(IPC)에만 문제가 생기고 두 프로세스는 정상 동작 중인 상황에서, 시스템이 예측 가능한 복구 절차를 수행해야 합니다.

**Why this priority**: 매우 드물게 발생하는 edge case이지만, 발생 시 치명적일 수 있습니다. 기본 안정성 확보 후 마지막 단계로 개선합니다.

**Independent Test**: 네트워크 파티션 시뮬레이션을 통해 IPC 채널만 차단하고, systemd Watchdog의 동작을 검증할 수 있습니다.

**Acceptance Scenarios**:

1. **Given** RT와 Non-RT 프로세스 모두 정상 동작 중, **When** IPC 채널만 장애가 발생하여 하트비트가 5초 이상 끊긴 경우, **Then** systemd Watchdog가 두 프로세스를 모두 재시작하고, 재시작 후 정상 통신이 복구되어야 함
2. **Given** RT 프로세스만 실제로 비정상 종료된 상황, **When** Non-RT 프로세스가 하트비트 실패를 감지할 때, **Then** systemd가 RT 프로세스만 재시작하고, Non-RT는 재시작 완료를 기다린 후 재연결해야 함
3. **Given** 스플릿 브레인 발생 시나리오, **When** 두 프로세스가 서로를 비정상으로 판단할 때, **Then** systemd가 제3의 감시자 역할을 하여 정확한 복구 정책(전체 재시작)을 실행해야 함

---

### Edge Cases

- RT 프로세스가 공유 메모리를 생성하기 전에 Non-RT가 시작되면? (재시도 로직으로 대기)
- EventBus 큐가 가득 찬 상태에서 높은 우선순위 이벤트가 들어오면? (낮은 우선순위 이벤트 버림)
- DataStore 버전 불일치가 3회 연속 감지되면? (오류 로깅 후 최신 버전 강제 사용)
- systemd Watchdog 타임아웃이 RT 사이클보다 짧게 설정되면? (설정 검증 및 경고)
- 두 프로세스가 동시에 systemd에 의해 재시작되면? (순차 재시작 보장)

## Requirements

### Functional Requirements

**Systemd 시작 순서 (P1)**:
- **FR-001**: systemd 서비스 종속성은 RT 프로세스가 Non-RT 프로세스보다 먼저 시작되도록 설정되어야 함 (mxrc-rt.service Before=mxrc-nonrt.service)
- **FR-002**: Non-RT 프로세스는 공유 메모리 연결 실패 시 최대 5초 동안 100ms 간격으로 재시도해야 함
- **FR-003**: RT 프로세스는 공유 메모리 생성 후 systemd에 READY 신호를 보내야 함

**DataStore 접근 패턴 (P2)**:
- **FR-004**: 각 데이터 도메인(Sensor, RobotState, TaskStatus 등)별로 전용 Accessor 인터페이스가 정의되어야 함
- **FR-005**: DataStore의 모든 데이터 업데이트는 버전 번호(또는 시퀀스 번호)를 함께 증가시켜야 함
- **FR-006**: 데이터 읽기 시 버전 번호를 함께 반환하여 일관성 검증이 가능해야 함
- **FR-007**: 데이터 스키마(키, 타입, 유효 범위)는 specs/contracts/data-contracts.md에 문서화되어야 함

**EventBus 안정성 (P3)**:
- **FR-008**: EventBus는 3단계 우선순위 큐(CRITICAL, NORMAL, DEBUG)를 지원해야 함
- **FR-009**: 큐가 80% 이상 찼을 때 DEBUG 레벨 이벤트부터 버리기 시작해야 함
- **FR-010**: 동일한 이벤트 타입이 100ms 이내에 반복 발생 시 1개로 병합(Coalescing)해야 함
- **FR-011**: RT 프로세스는 이벤트 전송 시 절대 블로킹되지 않아야 함 (non-blocking push)

**HA 스플릿 브레인 방지 (P4)**:
- **FR-012**: systemd Watchdog는 RT 프로세스의 하트비트를 30초 주기로 확인해야 함
- **FR-013**: RT 프로세스는 10초마다 sd_notify()를 통해 하트비트를 전송해야 함
- **FR-014**: Watchdog 타임아웃 발생 시 systemd가 자동으로 프로세스를 재시작해야 함
- **FR-015**: IPC 채널 장애 감지 시 로그에 명확한 오류 메시지와 복구 절차를 기록해야 함

### Key Entities

- **DataAccessor**: 특정 데이터 도메인에 대한 타입 안전 접근 인터페이스
  - 속성: 도메인 이름, 허용된 키 목록, 데이터 타입 정의
  - 관계: DataStore에 대한 제한된 뷰(view) 제공

- **VersionedData**: 버전 번호를 포함한 데이터 래퍼
  - 속성: 데이터 값, 버전 번호(uint64_t), 타임스탬프
  - 관계: DataStore의 모든 엔트리는 VersionedData로 래핑됨

- **PrioritizedEvent**: 우선순위가 부여된 이벤트
  - 속성: 이벤트 타입, 페이로드, 우선순위(CRITICAL/NORMAL/DEBUG), 타임스탬프
  - 관계: EventBus 큐에 저장되며, 우선순위에 따라 처리 순서 결정

## Success Criteria

### Measurable Outcomes

**안정성**:
- **SC-001**: 시스템 재시작 시 프로세스 시작 성공률이 100%여야 함 (현재: 타이밍 의존적)
- **SC-002**: 24시간 연속 운영 시 RT 프로세스의 평균 가동 시간이 99.9% 이상이어야 함
- **SC-003**: IPC 채널 장애 발생 시 평균 복구 시간이 10초 이하여야 함

**성능**:
- **SC-004**: EventBus 큐가 90% 찼을 때도 RT 프로세스의 사이클 지터가 10μs 이하여야 함
- **SC-005**: DataStore 버전 불일치 감지 및 재시도에 소요되는 시간이 평균 1ms 이하여야 함

**유지보수성**:
- **SC-006**: 데이터 스키마 변경 시 영향 받는 모듈 수가 평균 30% 감소해야 함 (Accessor 도입 효과)
- **SC-007**: 새로운 데이터 도메인 추가 시 필요한 코드 변경이 5개 파일 이하여야 함

**관측성**:
- **SC-008**: 모든 아키텍처 관련 이벤트(시작 순서, 버전 불일치, 큐 포화 등)가 로그에 기록되어야 함
- **SC-009**: 시스템 상태를 실시간으로 확인할 수 있는 메트릭이 Prometheus로 노출되어야 함

---

## Assumptions

다음과 같은 전제 조건을 가정합니다:

1. **PREEMPT_RT 커널 환경**: 시스템은 PREEMPT_RT 패치가 적용된 커널에서 실행됩니다
2. **systemd 버전**: systemd v240 이상이 설치되어 있으며, Type=notify, WatchdogSec, Before/After 지시자를 지원합니다
3. **CPU 격리**: RT 전용 CPU 코어(예: core 2-3)가 격리되어 있고, RT 프로세스만 해당 코어에서 실행됩니다
4. **공유 메모리**: `/dev/shm` 또는 POSIX 공유 메모리를 사용할 수 있는 권한이 있습니다
5. **Boost.Lockfree**: Boost 라이브러리 1.65 이상이 설치되어 있으며, lock-free SPSC 큐를 사용할 수 있습니다
6. **기존 IPC 구조**: RT/Non-RT 프로세스 간 공유 메모리 기반 IPC가 이미 구현되어 있습니다 (specs/021)
7. **Prometheus 통합**: Prometheus exporter가 이미 구성되어 있으며, 새로운 메트릭을 추가할 수 있습니다

---

## Scope

### In Scope

**P1 - Systemd 시작 순서 경쟁 상태 해결**:
- ✅ mxrc-rt.service Before 지시자 추가
- ✅ mxrc-nonrt.service 공유 메모리 연결 재시도 로직 구현
- ✅ RT 프로세스 sd_notify(READY=1) 호출 추가
- ✅ 시작 순서 검증 통합 테스트 작성

**P2 - DataStore 접근 패턴 개선**:
- ✅ 도메인별 Accessor 인터페이스 설계 및 구현 (SensorDataAccessor, RobotStateAccessor, TaskStatusAccessor)
- ✅ VersionedData 래퍼 클래스 구현
- ✅ 버전 불일치 감지 및 재시도 로직
- ✅ 데이터 스키마 문서화 (specs/contracts/data-contracts.md)
- ✅ 기존 코드를 Accessor 패턴으로 점진적 마이그레이션

**P3 - EventBus 안정성 강화**:
- ✅ 3단계 우선순위 큐 구현 (CRITICAL, NORMAL, DEBUG)
- ✅ 백프레셔 및 드롭 정책 구현
- ✅ 이벤트 병합(Coalescing) 및 Throttling 메커니즘
- ✅ Non-blocking push 보장
- ✅ EventBus 과부하 시나리오 테스트

**P4 - HA 스플릿 브레인 방지**:
- ✅ systemd Watchdog 설정 검증 및 문서화
- ✅ 하트비트 전송 로직 구현
- ✅ IPC 채널 장애 감지 및 로깅
- ✅ 복구 절차 문서화

**공통**:
- ✅ Prometheus 메트릭 추가 (버전 불일치 카운터, 이벤트 드롭 카운터, 큐 사용률 등)
- ✅ 통합 테스트 작성 (각 우선순위별로 독립 테스트 가능)
- ✅ 아키텍처 문서 업데이트 (docs/architecture/architecture.md)

### Out of Scope

**이번 Feature에서 제외되는 항목**:
- ❌ DataStore 전체 재설계 또는 다른 저장소로 마이그레이션
- ❌ RT/Non-RT 프로세스 아키텍처 자체의 변경
- ❌ 새로운 IPC 메커니즘 도입 (공유 메모리 → 다른 방식)
- ❌ EventBus를 메시지 큐 시스템(RabbitMQ, ZeroMQ 등)으로 대체
- ❌ Multi-master HA 또는 3-node 클러스터 구성
- ❌ 네트워크 기반 분산 시스템으로 확장
- ❌ 기존 코드의 전면 리팩토링 (점진적 개선만 수행)
- ❌ GUI 또는 CLI 도구 개발

**향후 Feature로 고려 가능**:
- 🔜 DataStore를 완전히 domain-driven 설계로 재구성 (Feature 023)
- 🔜 분산 트레이싱을 위한 OpenTelemetry 통합 (Feature 024)
- 🔜 HA 확장: 3-node Raft 합의 알고리즘 도입 (Feature 025)

---

## Dependencies

### 선행 요구사항 (필수 완료)

1. **Feature 018 - systemd 프로세스 관리 통합** (완료됨)
   - mxrc-rt.service 및 mxrc-nonrt.service 정의
   - systemd Watchdog 기본 설정
   - Type=notify 지원

2. **Feature 021 - RT/Non-RT IPC 리팩토링** (가정: 완료됨)
   - 공유 메모리 기반 IPC 구현
   - Boost.Lockfree SPSC 큐 사용
   - EventBus 기본 구조

3. **PREEMPT_RT 환경 설정**
   - PREEMPT_RT 커널 설치
   - CPU 격리 설정 (isolcpus, nohz_full, rcu_nocbs)
   - systemd 설치 (v240+)

### 외부 종속성

- **라이브러리**:
  - Boost.Lockfree (v1.65+) - 우선순위 큐 및 lock-free 자료구조
  - libsystemd (v240+) - sd_notify() API
  - spdlog (v1.x) - 구조화된 로깅
  - Prometheus C++ client (v1.x) - 메트릭 노출

- **시스템 구성**:
  - /dev/shm 또는 POSIX 공유 메모리 지원
  - systemd 서비스 관리 권한
  - Prometheus 서버 (메트릭 수집용)

### 팀 간 종속성

- **DevOps 팀**: systemd 서비스 설정 검토 및 배포 파이프라인 업데이트
- **QA 팀**: 통합 테스트 시나리오 검증 (특히 P1 시작 순서 테스트)
- **아키텍처 팀**: DataStore Accessor 패턴 설계 리뷰

---

## Risks

### 리스크 1: Systemd 서비스 순서 변경으로 인한 부작용 (P1)

**영향도**: High
**발생 가능성**: Medium

**설명**: mxrc-rt.service에 `Before=mxrc-nonrt.service`를 추가하면 기존에 다른 의존성을 가진 서비스들과 충돌할 수 있습니다.

**완화 전략**:
- systemd 종속성 그래프를 사전에 분석 (`systemd-analyze dot`)
- 테스트 환경에서 10회 이상 재부팅 테스트 수행
- 롤백 계획 수립 (기존 서비스 파일 백업)

### 리스크 2: DataStore Accessor 도입으로 인한 성능 오버헤드 (P2)

**영향도**: Medium
**발생 가능성**: Low

**설명**: Accessor 레이어 추가로 인해 데이터 접근 지연이 증가할 수 있습니다 (특히 RT 경로).

**완화 전략**:
- Accessor는 인라인 함수로 구현하여 컴파일러 최적화 유도
- 버전 체크 오버헤드를 10ns 이하로 제한 (벤치마크 테스트)
- RT 경로에서는 버전 체크를 선택적으로 비활성화 가능하도록 설계

### 리스크 3: EventBus 우선순위 큐 구현 복잡도 (P3)

**영향도**: Medium
**발생 가능성**: Medium

**설명**: 3단계 우선순위 큐와 백프레셔 로직이 복잡하여 예상치 못한 버그가 발생할 수 있습니다.

**완화 전략**:
- Boost.Lockfree의 검증된 자료구조 사용
- 단위 테스트에서 모든 우선순위 조합 테스트
- 초기에는 2단계 우선순위(CRITICAL, NORMAL)만 구현 후 점진적 확장

### 리스크 4: 기존 코드 마이그레이션 비용 (P2, P3)

**영향도**: High
**발생 가능성**: High

**설명**: 기존 코드를 Accessor 패턴으로 변경하는 작업이 예상보다 오래 걸릴 수 있습니다.

**완화 전략**:
- 점진적 마이그레이션 전략: 새 코드는 Accessor 사용, 기존 코드는 호환성 유지
- 컴파일러 경고를 통해 직접 접근을 deprecated 처리
- 마이그레이션 우선순위 결정: 자주 변경되는 모듈부터 시작

---

## Related Documents

### 기술 연구

- [Research 006 - Architecture Analysis & Improvements](../../research/006-architecture-analysis-improvements.md)
  - 본 Feature의 근거가 된 아키텍처 분석 문서
  - DataStore God Object, EventBus 안정성, HA 스플릿 브레인, systemd 경쟁 상태 문제 식별

### 선행 Feature 명세

- [Feature 018 - systemd 프로세스 관리 통합](../018-systemd-process-management/)
  - systemd 서비스 파일 정의
  - Watchdog, Resource limits, CPU affinity 설정
  - INSTALL.md 및 quickstart.md 참조

- [Feature 021 - RT/Non-RT 로깅 및 IPC 리팩토링](../021-refactor-rt-logging-ipc/)
  - 공유 메모리 IPC 구조
  - Boost.Lockfree SPSC 큐 사용
  - EventBus 기본 설계

### 아키텍처 문서

- [Architecture Overview](../../architecture/architecture.md)
  - 시스템 전체 아키텍처 다이어그램
  - RT/Non-RT 프로세스 분리 설계
  - IPC 메커니즘 설명

### 계약 및 스키마

- [Data Contracts](../../specs/contracts/data-contracts.md) (본 Feature에서 작성 예정)
  - DataStore 스키마 정의
  - 도메인별 데이터 타입 및 유효 범위
  - Accessor 인터페이스 명세

### 외부 참조

- [systemd.service(5)](https://www.freedesktop.org/software/systemd/man/systemd.service.html)
  - Type=notify, Before, After 지시자 설명
- [sd_notify(3)](https://www.freedesktop.org/software/systemd/man/sd_notify.html)
  - READY=1, WATCHDOG=1 프로토콜
- [Boost.Lockfree Documentation](https://www.boost.org/doc/libs/1_65_0/doc/html/lockfree.html)
  - SPSC/MPMC 큐 API 및 성능 특성

---

**End of Specification**

**다음 단계**: `/speckit.plan` 명령으로 구현 계획 수립
