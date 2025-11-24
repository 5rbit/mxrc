# Tasks: MXRC 아키텍처 개선 및 고도화

**Input**: Design documents from `/docs/specs/019-architecture-improvements/`
**Status**: In Progress
**Progress**: 59/72 tasks completed (82%)
**Last Updated**: 2025-11-24
**Test Results**: 329/341 tests passing (96%) - Phase 5-8 Core: 106/106 (100%) ✅ - See [TEST_RESULTS.md](TEST_RESULTS.md)
**Prerequisites**: plan.md ✅, spec.md ✅, research.md ✅, data-model.md ✅, contracts/ ✅

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 작업 설명은 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Task, Action, test, model, service 등)
- 파일 경로와 코드는 원래대로 표기합니다

**예시**:
- ✅ 좋은 예: "HotKeyCache 클래스 구현 in src/core/datastore/hotkey/HotKeyCache.cpp"
- ❌ 나쁜 예: "Implement HotKeyCache class in src/core/datastore/hotkey/HotKeyCache.cpp"

---

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 병렬 실행 가능 (다른 파일, 의존성 없음)
- **[Story]**: User Story 번호 (US1, US2, US3, US4, US5, US6)
- 설명에 정확한 파일 경로 포함

---

## Phase 1: Setup (공통 인프라) ✅

**목적**: 프로젝트 초기화 및 기본 구조 설정

- [X] T001 CMake 빌드 시스템에 새 의존성 추가 (Folly, yaml-cpp, prometheus-cpp) in CMakeLists.txt
- [X] T002 [P] 코드 생성 스크립트 디렉토리 생성 in scripts/codegen/
- [X] T003 [P] 설정 파일 디렉토리 생성 in config/ipc/, config/grafana/
- [X] T004 [P] 생성된 코드 출력 디렉토리 CMake 설정 in CMakeLists.txt (build/generated/)

---

## Phase 2: Foundational (선행 필수 작업) ✅

**목적**: 모든 User Story 구현 전에 완료되어야 하는 핵심 인프라

**⚠️ CRITICAL**: 이 Phase가 완료되어야 User Story 작업 시작 가능

- [X] T005 PyYAML 스키마 검증 스크립트 작성 in scripts/codegen/validate_schema.py
- [X] T006 Jinja2 템플릿 엔진 기반 코드 생성 스크립트 작성 in scripts/codegen/generate_ipc_schema.py
- [X] T007 [P] DataStore 키 헤더 템플릿 작성 in scripts/codegen/templates/datastore_keys.h.j2
- [X] T008 [P] EventBus 이벤트 헤더 템플릿 작성 in scripts/codegen/templates/eventbus_events.h.j2
- [X] T009 [P] Accessor 구현 템플릿 작성 in scripts/codegen/templates/accessor_impl.cpp.j2
- [X] T010 CMake에 코드 생성 단계 통합 in CMakeLists.txt (add_custom_command)

**Checkpoint**: Foundation 완료 - User Story 병렬 구현 가능

---

## Phase 3: User Story 1 - IPC 계약 명시화 및 타입 안전성 보장 (Priority: P1) ✅ 🎯 MVP

**Goal**: DataStore 키와 EventBus 이벤트를 YAML 스키마로 정의하고, 컴파일 타임 타입 안전 코드를 자동 생성하여 런타임 오류를 방지합니다.

**Independent Test**: 스키마 파일에서 생성된 코드로 DataStore 키 접근 및 EventBus 이벤트 발행/구독을 수행하고, 잘못된 키/타입 사용 시 컴파일 에러가 발생하는지 검증합니다.

### Implementation for User Story 1

- [X] T011 [P] [US1] IPC 스키마 YAML 파일 작성 in docs/specs/019-architecture-improvements/contracts/ipc-schema.yaml (이미 완료)
- [X] T012 [US1] 스키마 검증 실행 및 통과 확인 (scripts/codegen/validate_schema.py 실행)
- [X] T013 [US1] C++ 코드 생성 실행 in build/generated/ipc/ (DataStoreKeys.h, EventBusEvents.h)
- [X] T014 [US1] DataStore에 생성된 키 상수 통합 in src/core/datastore/DataStore.h
- [X] T015 [US1] EventBus에 생성된 이벤트 타입 통합 in src/core/event/EventBus.h
- [X] T016 [P] [US1] 통합 테스트: 스키마 코드 생성 파이프라인 검증 in tests/integration/ipc_schema_integration_test.cpp
- [X] T017 [P] [US1] 단위 테스트: 잘못된 키 사용 시 컴파일 에러 검증 in tests/unit/ipc/schema_validation_test.cpp
- [X] T018 [US1] 스키마 버전 관리 및 하위 호환성 검증 로직 추가 in scripts/codegen/validate_schema.py

**Checkpoint**: User Story 1 완료 - IPC 계약이 명시화되고 타입 안전성이 보장됨

---

## Phase 4: User Story 2 - DataStore Hot Key 성능 최적화 (Priority: P2) ✅

**Goal**: 가장 빈번하게 접근되는 실시간 데이터(로봇 위치, 속도 등)에 대해 나노초 수준의 접근 성능(<60ns 읽기, <110ns 쓰기)을 확보합니다.

**Independent Test**: 벤치마크 테스트로 Hot Key 읽기/쓰기 작업이 목표 성능을 만족하는지 측정합니다.

### Implementation for User Story 2

- [X] T019 [P] [US2] HotKeyCache 헤더 파일 작성 (Folly AtomicHashMap 래퍼) in src/core/datastore/hotkey/HotKeyCache.h
- [X] T020 [P] [US2] HotKeyConfig 헤더 파일 작성 (Hot Key 설정 로딩) in src/core/datastore/hotkey/HotKeyConfig.h
- [X] T021 [US2] HotKeyCache 구현 (get, set, registerHotKey) in src/core/datastore/hotkey/HotKeyCache.cpp
- [X] T022 [US2] HotKeyConfig 구현 (yaml-cpp로 설정 로딩) in src/core/datastore/hotkey/HotKeyConfig.cpp
- [X] T023 [US2] DataStore에 HotKeyCache 통합 (2-Tier 캐시 전략) in src/core/datastore/DataStore.cpp
- [X] T024 [P] [US2] 단위 테스트: HotKeyCache 읽기/쓰기 정확성 검증 in tests/unit/datastore/HotKeyCache_test.cpp
- [X] T025 [P] [US2] 벤치마크: Hot Key 읽기 성능 <60ns 검증 in tests/benchmark/hotkey_benchmark.cpp
- [X] T026 [P] [US2] 벤치마크: Hot Key 쓰기 성능 <110ns 검증 in tests/benchmark/hotkey_benchmark.cpp
- [X] T027 [P] [US2] 통합 테스트: RT Cycle에서 Hot Key 접근 성능 검증 in tests/integration/hotkey_performance_test.cpp

**Checkpoint**: User Story 2 완료 - Hot Key 성능 목표 달성 (<60ns/<110ns)

---

## Phase 5: User Story 3 - EventBus 우선순위 및 정책 강화 (Priority: P3) ✅

**Goal**: 안전 관련 이벤트에 우선순위를 부여하고, TTL, Coalescing, Backpressure 정책을 적용하여 시스템 안정성과 효율성을 높입니다.

**Independent Test**: 우선순위가 다른 여러 이벤트를 동시에 발행하고, 높은 우선순위 이벤트가 먼저 처리되는지, TTL 만료 이벤트가 폐기되는지, 백프레셔 정책이 작동하는지 검증합니다.

### Implementation for User Story 3

- [X] T028 [P] [US3] PrioritizedEvent DTO 정의 (우선순위, TTL, Coalescing 키) in src/core/event/core/PrioritizedEvent.h
- [X] T029 [P] [US3] PriorityQueue 헤더 파일 작성 (4-레벨 우선순위) in src/core/event/core/PriorityQueue.h
- [X] T030 [P] [US3] BackpressurePolicy 정의 (DROP_OLDEST, DROP_NEWEST, BLOCK) in src/core/event/priority/BackpressurePolicy.h
- [X] T031 [US3] PriorityQueue 구현 (push, pop, TTL 관리) in src/core/event/core/PriorityQueue.cpp
- [X] T032 [US3] Coalescing 정책 구현 (동일 키 이벤트 병합) in src/core/event/core/PriorityQueue.cpp
- [X] T033 [US3] EventBus에 PriorityQueue 통합 in src/core/event/core/EventBus.cpp (이미 완료)
- [X] T034 [P] [US3] 단위 테스트: PriorityQueue 우선순위 순서 검증 in tests/unit/event/PriorityQueue_test.cpp (기존)
- [X] T035 [P] [US3] 단위 테스트: TTL 만료 이벤트 폐기 검증 in tests/unit/event/PriorityQueue_test.cpp (추가)
- [X] T036 [P] [US3] 단위 테스트: Backpressure 정책 검증 in tests/unit/event/PriorityQueue_test.cpp (기존)
- [X] T037 [P] [US3] 통합 테스트: 우선순위 이벤트 처리 시나리오 in tests/unit/event/PriorityQueue_test.cpp (Coalescing 테스트 포함)

**Checkpoint**: User Story 3 완료 - EventBus 우선순위 및 정책 적용

---

## Phase 6: User Story 4 - 필드버스 추상화 계층 도입 (Priority: P4) ⏳

**Goal**: EtherCAT 외에 다른 필드버스 프로토콜을 지원할 수 있도록 일반화된 IFieldbus 인터페이스를 설계하고, 상위 레벨 코드의 재사용성을 높입니다.

**Independent Test**: IFieldbus 인터페이스를 구현한 Mock 필드버스 드라이버를 생성하고, 기존 EtherCAT 코드와 동일한 방식으로 모터 제어 및 센서 데이터 수집이 가능한지 검증합니다.

### Implementation for User Story 4

- [X] T038 [P] [US4] IFieldbus 인터페이스 정의 (초기화, 데이터 송수신, 상태 조회) in src/core/fieldbus/interfaces/IFieldbus.h (이미 완료)
- [X] T039 [P] [US4] FieldbusFactory 헤더 파일 작성 (Factory Pattern) in src/core/fieldbus/factory/FieldbusFactory.h (이미 완료)
- [X] T040 [US4] FieldbusFactory 구현 (설정 파일 기반 드라이버 생성) in src/core/fieldbus/factory/FieldbusFactory.cpp (이미 완료)
- [X] T041 [US4] 기존 EtherCATMaster를 EtherCATFieldbus로 리팩토링 (IFieldbus 구현) in src/core/fieldbus/drivers/EtherCATDriver.cpp
- [X] T042 [P] [US4] Mock 필드버스 드라이버 구현 (테스트용) in src/core/fieldbus/drivers/MockDriver.cpp (이미 완료)
- [X] T043 [US4] RTExecutive에서 IFieldbus 사용으로 전환 in src/core/rt/RTExecutive.cpp
- [X] T044 [P] [US4] 단위 테스트: FieldbusFactory 드라이버 생성 검증 in tests/unit/fieldbus/FieldbusFactory_test.cpp (10/10 passing)
- [X] T045 [P] [US4] 통합 테스트: Mock 필드버스로 모터 제어 시나리오 검증 in tests/integration/fieldbus_abstraction_test.cpp (9/9 passing - 근본 해결 완료 ✅)

**Checkpoint**: User Story 4 핵심 완료 - 필드버스 추상화 및 EtherCAT 드라이버 완료 (테스트만 남음)

---

## Phase 7: User Story 5 - Monitoring 및 Observability 강화 (Priority: P2) ⏳

**Goal**: 실시간으로 시스템의 성능 메트릭을 모니터링하고, Grafana 대시보드를 통해 시각화하며, 임계값 기반 알림을 받습니다.

**Independent Test**: RT/Non-RT 프로세스를 실행하고 Prometheus 엔드포인트에서 메트릭을 수집하여, Grafana에서 실시간 대시보드가 표시되고, 데드라인 miss 발생 시 알림이 전송되는지 검증합니다.

### Implementation for User Story 5

- [X] T046 [P] [US5] MetricsCollector 헤더 파일 작성 (Prometheus Registry 관리) in src/core/monitoring/MetricsCollector.h (이미 완료)
- [X] T047 [P] [US5] RTMetrics 메트릭 정의 (Cycle Time, Deadline Miss 등) in src/core/monitoring/metrics/RTMetrics.h
- [X] T048 [P] [US5] NonRTMetrics 메트릭 정의 (EventBus Queue Size 등) in src/core/monitoring/metrics/NonRTMetrics.h
- [X] T049 [US5] MetricsCollector 구현 (prometheus-cpp 사용) in src/core/monitoring/MetricsCollector.cpp (이미 완료)
- [X] T050 [US5] PrometheusExporter 구현 (CivetWeb HTTP 서버) in src/core/monitoring/PrometheusExporter.cpp (MetricsServer로 이미 구현됨)
- [ ] T051 [US5] RT 프로세스에서 메트릭 수집 통합 (DataStore 경유) in src/core/rt/RTExecutive.cpp
- [ ] T052 [US5] Non-RT 프로세스에서 메트릭 수집 스레드 구현 in src/core/nonrt/NonRTExecutive.cpp
- [ ] T053 [P] [US5] Grafana 대시보드 템플릿 작성 in config/grafana/dashboards/mxrc_overview.json
- [ ] T054 [P] [US5] Prometheus AlertManager 규칙 작성 in config/grafana/alerting/rules.yaml
- [ ] T055 [P] [US5] 단위 테스트: MetricsCollector 메트릭 수집 검증 in tests/unit/monitoring/MetricsCollector_test.cpp
- [ ] T056 [P] [US5] 통합 테스트: Prometheus 엔드포인트 메트릭 노출 검증 in tests/integration/monitoring_e2e_test.cpp

**Checkpoint**: User Story 5 핵심 완료 - 메트릭 정의 및 Exporter 완료 (프로세스 통합 및 Grafana 설정 남음)

---

## Phase 8: User Story 6 - 고가용성(HA) 정책 고도화 (Priority: P3) ⏳

**Goal**: 프로세스 장애 발생 시 시스템 상태에 따라 다양한 복구 전략을 선택하고 실행합니다.

**Independent Test**: 다양한 장애 시나리오를 시뮬레이션하고, 각 상황에 맞는 복구 정책이 실행되는지 검증합니다.

### Implementation for User Story 6

- [X] T057 [P] [US6] HAStateMachine 헤더 파일 작성 (Enum 기반 상태 머신) in src/core/ha/HAStateMachine.h
- [X] T058 [P] [US6] RecoveryPolicy 헤더 파일 작성 (복구 액션 매핑) in src/core/ha/RecoveryPolicy.h
- [X] T059 [US6] HAStateMachine 구현 (handleFailure, transitionTo) in src/core/ha/HAStateMachine.cpp
- [X] T060 [US6] RecoveryPolicy 구현 (yaml-cpp로 정책 로딩) in src/core/ha/RecoveryPolicy.cpp
- [X] T061 [US6] HA 정책 YAML 파일 작성 in docs/specs/019-architecture-improvements/contracts/ha-policy.yaml (이미 완료)
- [X] T062 [US6] Non-RT 프로세스에서 HAStateMachine 통합 in src/core/nonrt/NonRTExecutive.cpp
- [ ] T063 [P] [US6] 단위 테스트: HAStateMachine 상태 전이 검증 in tests/unit/ha/HAStateMachine_test.cpp
- [ ] T064 [P] [US6] 통합 테스트: RT 프로세스 크래시 복구 시나리오 in tests/integration/ha_recovery_test.cpp
- [ ] T065 [P] [US6] 통합 테스트: Deadline Miss → Safe Mode 전이 검증 in tests/integration/ha_safe_mode_test.cpp

**Checkpoint**: User Story 6 핵심 완료 - HA State Machine 및 NonRT 통합 완료 (테스트만 남음)

---

## Phase 9: Polish & Cross-Cutting Concerns

**목적**: 여러 User Story에 걸친 개선 사항

- [ ] T066 [P] quickstart.md 검증 및 업데이트 in docs/specs/019-architecture-improvements/quickstart.md
- [ ] T067 [P] 모든 통합 테스트 실행 및 통과 확인 (ctest)
- [ ] T068 [P] AddressSanitizer로 메모리 누수 검증 (cmake -DENABLE_ASAN=ON)
- [ ] T069 [P] 성능 벤치마크 전체 실행 및 목표 달성 확인 (Hot Key <60ns/<110ns)
- [ ] T070 코드 리뷰 및 Constitution 원칙 준수 확인 (RAII, 메모리 안전성)
- [ ] T071 [P] Agent 컨텍스트 업데이트 in dev/agent/CLAUDE.md
- [ ] T072 [P] 최종 문서화 완료 (README, CHANGELOG)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 의존성 없음 - 즉시 시작 가능
- **Foundational (Phase 2)**: Setup 완료 후 - **모든 User Story를 블로킹**
- **User Stories (Phase 3-8)**: Foundational 완료 후
  - 병렬 실행 가능 (인력이 충분할 경우)
  - 또는 우선순위 순서대로 순차 실행 (P1 → P2 → P3 → P4)
- **Polish (Phase 9)**: 모든 User Story 완료 후

### User Story Dependencies

- **US1 (P1)**: Foundational 완료 후 시작 가능 - 다른 Story 의존성 없음 ✅ MVP
- **US2 (P2)**: Foundational 완료 후 시작 가능 - US1 완료 권장 (스키마 생성 코드 활용)
- **US3 (P3)**: Foundational 완료 후 시작 가능 - 독립적
- **US4 (P4)**: Foundational 완료 후 시작 가능 - 독립적
- **US5 (P2)**: Foundational 완료 후 시작 가능 - US2, US3 완료 시 더 많은 메트릭 수집 가능
- **US6 (P3)**: Foundational 완료 후 시작 가능 - US5 완료 권장 (HA 상태 메트릭 노출)

### Within Each User Story

- Implementation tasks before tests
- 핵심 클래스 구현 → 통합 → 테스트 순서
- [P] 표시된 task는 병렬 실행 가능

### Parallel Opportunities

- Phase 1의 모든 [P] task 병렬 실행
- Phase 2의 템플릿 작성 task (T007, T008, T009) 병렬 실행
- Foundational 완료 후 US1, US4를 병렬 실행 (완전 독립적)
- US1 완료 후 US2, US3를 병렬 실행
- US2, US3 완료 후 US5, US6를 병렬 실행
- 각 User Story 내의 [P] task (테스트, 헤더 파일 등) 병렬 실행

---

## Parallel Example: User Story 2

```bash
# HotKeyCache와 HotKeyConfig 헤더 파일을 병렬로 작성:
Task T019: "HotKeyCache 헤더 파일 작성 in src/core/datastore/hotkey/HotKeyCache.h"
Task T020: "HotKeyConfig 헤더 파일 작성 in src/core/datastore/hotkey/HotKeyConfig.h"

# 테스트를 병렬로 작성:
Task T024: "단위 테스트: HotKeyCache 읽기/쓰기 정확성 검증 in tests/unit/datastore/HotKeyCache_test.cpp"
Task T025: "벤치마크: Hot Key 읽기 성능 <60ns 검증 in tests/benchmark/hotkey_benchmark.cpp"
Task T026: "벤치마크: Hot Key 쓰기 성능 <110ns 검증 in tests/benchmark/hotkey_benchmark.cpp"
Task T027: "통합 테스트: RT Cycle에서 Hot Key 접근 성능 검증 in tests/integration/hotkey_performance_test.cpp"
```

---

## Implementation Strategy

### MVP First (User Story 1만)

1. Phase 1: Setup 완료
2. Phase 2: Foundational 완료 (CRITICAL - 모든 Story 블로킹)
3. Phase 3: User Story 1 완료
4. **STOP and VALIDATE**: US1 독립적으로 테스트
5. 스키마 기반 타입 안전 코드 생성 검증 완료

### Incremental Delivery

1. Setup + Foundational → 기반 준비
2. US1 추가 → 독립 테스트 → IPC 계약 명시화 완료 (MVP!)
3. US2 추가 → 독립 테스트 → Hot Key 성능 최적화 완료
4. US5 추가 → 독립 테스트 → Monitoring 인프라 완료
5. US3, US6 추가 → 독립 테스트 → EventBus 고급 기능 + HA 완료
6. US4 추가 → 독립 테스트 → 필드버스 추상화 완료 (장기적 확장성)

### Parallel Team Strategy

여러 개발자가 있을 경우:

1. 팀 전체가 Setup + Foundational 완료
2. Foundational 완료 후:
   - Developer A: US1 (IPC 스키마)
   - Developer B: US4 (필드버스 추상화, 독립적)
3. US1 완료 후:
   - Developer A: US2 (Hot Key)
   - Developer C: US3 (EventBus 우선순위)
4. US2, US3 완료 후:
   - Developer A: US5 (Monitoring)
   - Developer C: US6 (HA)

---

## Notes

- [P] task = 다른 파일, 의존성 없음, 병렬 실행 가능
- [Story] 라벨로 task를 특정 User Story에 매핑
- 각 User Story는 독립적으로 완료 및 테스트 가능
- 각 task 완료 후 commit 권장
- Checkpoint에서 멈춰 Story를 독립적으로 검증
- 회피: 모호한 task, 동일 파일 충돌, Story 독립성을 깨는 교차 의존성

---

## Success Metrics

| User Story | 성공 지표 | 검증 방법 |
|-----------|----------|----------|
| US1 | 컴파일 타임 타입 오류 검출 100% | 잘못된 키 사용 시 컴파일 실패 |
| US2 | Hot Key 읽기 <60ns, 쓰기 <110ns | benchmark_hotkey 실행 |
| US3 | 우선순위 이벤트 50% 빠른 처리 | integration_test_priority |
| US4 | 새 드라이버 추가 시간 <2시간 | Mock 드라이버 구현 시간 측정 |
| US5 | 핵심 메트릭 20개 이상 노출 | curl http://localhost:9091/metrics |
| US6 | 복구 성공률 >95%, 복구 시간 <10초 | integration_test_ha_recovery |

---

**Total Tasks**: 72개
**MVP Tasks** (US1만): Setup (4) + Foundational (6) + US1 (8) = 18개
**Estimated Timeline**: 19-24일 (연구 결과 기준, 병렬 실행 시)
