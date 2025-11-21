# Tasks: Production Readiness

**Input**: Design documents from `/docs/specs/001-production-readiness/`
**Status**: In Progress
**Progress**: 0/97 tasks completed
**Last Updated**: 2025-11-21
**Prerequisites**: plan.md (✓), spec.md (✓), research.md (✓), data-model.md (✓), contracts/ (✓)

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 작업 설명은 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: CPU affinity, NUMA, test, model, service 등)
- 파일 경로와 코드는 원래대로 표기합니다

**예시**:
- ✅ 좋은 예: "CPU affinity 설정 클래스 생성 in src/core/rt/perf/CPUAffinityManager.cpp"
- ❌ 나쁜 예: "Create CPU affinity manager in src/core/rt/perf/CPUAffinityManager.cpp"

---

**Tests**: Tests are included as this is a production-readiness feature requiring comprehensive validation per Constitution (TDD principle).

**Organization**: Tasks are grouped by user story (P1-P4) to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1=P1, US2=P2, US3=P3, US4=P4)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization and configuration management

- [x] T001 CMake 설정 업데이트: libnuma, OpenTelemetry SDK 의존성 추가 in CMakeLists.txt
- [x] T002 [P] JSON 설정 파일 디렉토리 생성 및 템플릿 작성 in config/
- [x] T003 [P] systemd service 파일 디렉토리 생성 in systemd/
- [x] T004 [P] 테스트 디렉토리 구조 생성 (unit/, integration/) in tests/

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before ANY user story can be implemented

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

### 공통 인터페이스 정의

- [x] T005 [P] IHealthCheck 인터페이스 정의 in src/core/ha/HealthCheck.h
- [x] T006 [P] ITracer 인터페이스 정의 in src/core/tracing/TracerProvider.h
- [x] T007 [P] ILogFormatter 인터페이스 정의 in src/core/monitoring/StructuredLogger.h

### 기존 코드 확장 지점 준비

- [x] T008 RTExecutive에 초기화 hook 추가 (CPU affinity/NUMA 설정용) in src/core/rt/RTExecutive.cpp
- [ ] T009 EventBus에 observer pattern 지원 추가 (tracing용) in src/core/event/EventBus.cpp

### 설정 파일 로더

- [ ] T010 JSON 설정 파일 파서 구현 (nlohmann_json 사용) in src/core/config/ConfigLoader.h/cpp

**Checkpoint**: Foundation ready - user story implementation can now begin in parallel

---

## Phase 3: User Story 1 - 실시간 시스템 성능 최적화 (Priority: P1) 🎯 MVP

**Goal**: RT 프로세스가 전용 CPU 코어에서 실행되고 NUMA 메모리 지역성을 활용하여 예측 가능한 실시간 성능 달성

**Independent Test**: RT 프로세스를 설정된 CPU affinity와 NUMA 노드에서 실행하고, 10,000 사이클 동안 deadline miss rate < 0.01% 측정

**Success Criteria**:
- SC-001: Deadline miss rate < 0.01% (10,000 cycles)
- SC-002: Cycle time jitter 감소 50% (CPU isolation 적용 전 대비)
- SC-003: Local NUMA access > 95%
- SC-004: 평균 메모리 접근 지연시간 감소 30%

### 데이터 모델 (US1)

- [x] T011 [P] [US1] CPUAffinityConfig 구조체 정의 in src/core/rt/perf/CPUAffinityManager.h
- [x] T012 [P] [US1] NUMABindingConfig 구조체 정의 in src/core/rt/perf/NUMABinding.h

### CPU Affinity 구현 (US1)

- [x] T013 [US1] CPUAffinityManager 클래스 구현 (pthread_setaffinity_np 사용) in src/core/rt/perf/CPUAffinityManager.cpp
- [x] T014 [US1] CPU isolation 검증 함수 구현 (isolcpus/cgroups 확인) in src/core/rt/perf/CPUAffinityManager.cpp
- [x] T015 [US1] CPU affinity 설정 RAII guard 구현 in src/core/rt/perf/CPUAffinityManager.cpp
- [x] T016 [US1] JSON 설정 파일 로드 및 파싱 in src/core/rt/perf/CPUAffinityManager.cpp

### NUMA Binding 구현 (US1)

- [x] T017 [US1] NUMABinding 클래스 구현 (libnuma 사용) in src/core/rt/perf/NUMABinding.cpp
- [x] T018 [US1] NUMA 메모리 정책 설정 (NUMA_LOCAL) in src/core/rt/perf/NUMABinding.cpp
- [x] T019 [US1] NUMA 노드 바인딩 검증 함수 in src/core/rt/perf/NUMABinding.cpp
- [x] T020 [US1] JSON 설정 파일 로드 및 파싱 in src/core/rt/perf/NUMABinding.cpp

### 성능 모니터링 (US1)

- [ ] T021 [US1] PerfMonitor 클래스 구현 (jitter, deadline miss 측정) in src/core/rt/perf/PerfMonitor.cpp
- [ ] T022 [US1] NUMA 메모리 접근 통계 수집 (/proc/[pid]/numa_maps 파싱) in src/core/rt/perf/PerfMonitor.cpp
- [ ] T023 [US1] Prometheus 메트릭 확장 (mxrc_rt_jitter_us, mxrc_numa_local_memory_percent) in src/core/rt/perf/PerfMonitor.cpp

### RTExecutive 통합 (US1)

- [ ] T024 [US1] RTExecutive 초기화 시 CPU affinity 설정 호출 in src/core/rt/RTExecutive.cpp
- [ ] T025 [US1] RTExecutive 초기화 시 NUMA binding 설정 호출 in src/core/rt/RTExecutive.cpp

### 설정 파일 (US1)

- [ ] T026 [P] [US1] config/cpu_affinity.json 템플릿 작성
- [ ] T027 [P] [US1] config/numa_binding.json 템플릿 작성

### Unit Tests (US1)

- [ ] T028 [P] [US1] CPUAffinityManager unit test in tests/unit/perf/CPUAffinityManager_test.cpp
- [ ] T029 [P] [US1] NUMABinding unit test in tests/unit/perf/NUMABinding_test.cpp
- [ ] T030 [P] [US1] PerfMonitor unit test in tests/unit/perf/PerfMonitor_test.cpp

### Integration Tests (US1)

- [ ] T031 [US1] CPU isolation 통합 테스트 (10,000 사이클, deadline miss rate 측정) in tests/integration/perf/cpu_isolation_test.cpp
- [ ] T032 [US1] NUMA 최적화 통합 테스트 (local access > 95% 검증) in tests/integration/perf/numa_optimization_test.cpp

**Checkpoint**: At this point, User Story 1 should be fully functional and testable independently. RT 성능이 목표치를 달성해야 함.

---

## Phase 4: User Story 2 - 시스템 고가용성 보장 (Priority: P2)

**Goal**: RT/Non-RT 프로세스 실패 시 자동 복구, 분산 환경에서 failover 지원

**Independent Test**: 실행 중인 RT 프로세스를 강제 종료하고 5초 이내 자동 재시작 및 상태 복구 확인

**Success Criteria**:
- SC-005: 프로세스 재시작 시간 < 5초
- SC-006: Failover 서비스 중단 시간 < 5초, 데이터 손실 없음
- SC-007: 30일 연속 운영 시 가용성 99.9% 이상
- SC-008: 단일 노드 장애 시 100ms 이내 작업 계속

### 데이터 모델 (US2)

- [ ] T033 [P] [US2] ProcessHealthStatus 구조체 정의 in src/core/ha/HealthCheck.h
- [ ] T034 [P] [US2] StateCheckpoint 구조체 정의 in src/core/ha/StateCheckpoint.h
- [ ] T035 [P] [US2] FailoverPolicy 구조체 정의 in src/core/ha/FailoverManager.h

### Health Check 구현 (US2)

- [ ] T036 [US2] HealthCheck HTTP 서버 구현 (기존 MetricsServer 확장) in src/core/ha/HealthCheck.cpp
- [ ] T037 [US2] GET /health endpoint 구현 (ProcessHealthStatus 반환) in src/core/ha/HealthCheck.cpp
- [ ] T038 [US2] GET /health/ready endpoint 구현 (Readiness probe) in src/core/ha/HealthCheck.cpp
- [ ] T039 [US2] GET /health/live endpoint 구현 (Liveness probe) in src/core/ha/HealthCheck.cpp
- [ ] T040 [US2] GET /health/details endpoint 구현 (상세 진단 정보) in src/core/ha/HealthCheck.cpp

### Process Monitoring (US2)

- [ ] T041 [US2] ProcessMonitor 클래스 구현 (주기적 health check) in src/core/ha/ProcessMonitor.cpp
- [ ] T042 [US2] systemd watchdog 통합 (sd_notify) in src/core/ha/ProcessMonitor.cpp
- [ ] T043 [US2] ProcessHealthStatus 업데이트 로직 (CPU, 메모리, deadline miss) in src/core/ha/ProcessMonitor.cpp
- [ ] T044 [US2] Health check 실패 감지 및 이벤트 발생 in src/core/ha/ProcessMonitor.cpp

### State Checkpoint (US2)

- [ ] T045 [US2] StateCheckpoint 클래스 구현 (선택적 직렬화) in src/core/ha/StateCheckpoint.cpp
- [ ] T046 [US2] RT state 직렬화 (Task/Sequence/Action 상태) in src/core/ha/StateCheckpoint.cpp
- [ ] T047 [US2] RTDataStore snapshot 직렬화 (선택적 키-값) in src/core/ha/StateCheckpoint.cpp
- [ ] T048 [US2] EventBus 큐 snapshot 직렬화 in src/core/ha/StateCheckpoint.cpp
- [ ] T049 [US2] Checkpoint 파일 저장 및 로드 (JSON 포맷) in src/core/ha/StateCheckpoint.cpp
- [ ] T050 [US2] Checkpoint 복구 로직 (failover 시 로드) in src/core/ha/StateCheckpoint.cpp

### Failover Manager (US2)

- [ ] T051 [US2] FailoverManager 클래스 구현 in src/core/ha/FailoverManager.cpp
- [ ] T052 [US2] Primary/standby 상태 관리 in src/core/ha/FailoverManager.cpp
- [ ] T053 [US2] Health check 실패 시 재시작 트리거 in src/core/ha/FailoverManager.cpp
- [ ] T054 [US2] 재시작 횟수 제한 로직 (max_restart_count, restart_window) in src/core/ha/FailoverManager.cpp
- [ ] T055 [US2] JSON 설정 파일 로드 (FailoverPolicy) in src/core/ha/FailoverManager.cpp

### Leader Election (US2)

- [ ] T056 [US2] LeaderElection 클래스 구현 (Bully 알고리즘) in src/core/ha/LeaderElection.cpp
- [ ] T057 [US2] Leader election 프로토콜 (heartbeat, 선출) in src/core/ha/LeaderElection.cpp
- [ ] T058 [US2] Split-brain 방지 (quorum 기반 합의) in src/core/ha/LeaderElection.cpp

### systemd Integration (US2)

- [ ] T059 [P] [US2] systemd/mxrc-rt.service 파일 작성 (WatchdogSec, Restart 설정)
- [ ] T060 [P] [US2] systemd/mxrc-nonrt.service 파일 작성
- [ ] T061 [P] [US2] systemd/mxrc-monitor.service 파일 작성 (별도 monitor 프로세스)

### 설정 파일 (US2)

- [ ] T062 [US2] config/failover_policy.json 템플릿 작성

### Unit Tests (US2)

- [ ] T063 [P] [US2] ProcessMonitor unit test in tests/unit/ha/ProcessMonitor_test.cpp
- [ ] T064 [P] [US2] FailoverManager unit test in tests/unit/ha/FailoverManager_test.cpp
- [ ] T065 [P] [US2] LeaderElection unit test in tests/unit/ha/LeaderElection_test.cpp
- [ ] T066 [P] [US2] StateCheckpoint unit test (직렬화/역직렬화) in tests/unit/ha/StateCheckpoint_test.cpp

### Integration Tests (US2)

- [ ] T067 [US2] 프로세스 재시작 통합 테스트 (5초 이내 복구 검증) in tests/integration/ha/process_restart_test.cpp
- [ ] T068 [US2] Failover 통합 테스트 (primary → standby 전환) in tests/integration/ha/failover_test.cpp
- [ ] T069 [US2] Split-brain 방지 테스트 (network partition 시뮬레이션) in tests/integration/ha/split_brain_test.cpp

**Checkpoint**: At this point, User Stories 1 AND 2 should both work independently. 고가용성 목표치 달성.

---

## Phase 5: User Story 3 - 통합 로깅 및 분석 (Priority: P3)

**Goal**: 모든 프로세스의 로그가 structured format으로 중앙 집중화, 실시간 검색/필터링/시각화 가능

**Independent Test**: RT/Non-RT 프로세스에서 발생한 로그 이벤트가 1초 이내 중앙 로그 시스템에 전송되고 Kibana에서 검색 가능 확인

**Success Criteria**:
- SC-009: 로그 전송 지연 < 1초
- SC-010: 로그 검색 쿼리 평균 1초 이내 결과 반환 (24시간 데이터)
- SC-013: 문제 발생 시 원인 10분 이내 식별 가능

### 데이터 모델 (US3)

- [ ] T070 [US3] StructuredLogEvent 구조체 정의 (ECS 스키마) in src/core/monitoring/StructuredLogger.h

### Structured Logging 구현 (US3)

- [ ] T071 [US3] StructuredLogger 클래스 구현 (spdlog 확장) in src/core/monitoring/StructuredLogger.cpp
- [ ] T072 [US3] Custom JSON formatter 구현 (ECS 호환) in src/core/monitoring/StructuredLogger.cpp
- [ ] T073 [US3] Thread-local storage에 trace ID 저장 (log correlation) in src/core/monitoring/StructuredLogger.cpp
- [ ] T074 [US3] Ring buffer 구현 (overrun_oldest 정책) in src/core/monitoring/StructuredLogger.cpp
- [ ] T075 [US3] 비동기 로깅 설정 (spdlog async_logger) in src/core/monitoring/StructuredLogger.cpp

### 로그 필드 추가 (US3)

- [ ] T076 [US3] ECS 필드 매핑 (@timestamp, log.level, process.name 등) in src/core/monitoring/StructuredLogger.cpp
- [ ] T077 [US3] MXRC 커스텀 필드 추가 (mxrc.task_id, mxrc.cycle_time_us) in src/core/monitoring/StructuredLogger.cpp
- [ ] T078 [US3] trace.id, span.id 자동 주입 (TraceContext 연동) in src/core/monitoring/StructuredLogger.cpp

### 기존 코드 통합 (US3)

- [ ] T079 [US3] 기존 spdlog 로거를 StructuredLogger로 교체 in src/core/rt/RTExecutive.cpp
- [ ] T080 [US3] 중요 이벤트 자동 로깅 (deadline miss, failover) in src/core/rt/RTExecutive.cpp

### Unit Tests (US3)

- [ ] T081 [P] [US3] StructuredLogger unit test (JSON 포맷 검증) in tests/unit/monitoring/StructuredLogger_test.cpp
- [ ] T082 [P] [US3] Custom formatter unit test (ECS 스키마 호환성) in tests/unit/monitoring/StructuredLogger_test.cpp

### Integration Tests (US3)

- [ ] T083 [US3] 로그 전송 통합 테스트 (1초 이내 전송 검증, Filebeat 사용) in tests/integration/monitoring/logging_integration_test.cpp

**Checkpoint**: All user stories 1-3 should now be independently functional. 로그 중앙 집중화 완료.

---

## Phase 6: User Story 4 - 분산 트레이싱 및 성능 분석 (Priority: P4)

**Goal**: 요청이 RT/Non-RT 프로세스를 거치는 전체 경로 추적, 각 구간 처리 시간 및 병목 지점 식별

**Independent Test**: EventBus를 통해 전송된 이벤트에 trace ID 부여, RT → Non-RT → RT 경로의 전체 처리 시간 Jaeger UI에서 확인

**Success Criteria**:
- SC-011: Trace ID의 전체 경로를 2초 이내 시각화
- SC-012: 트레이싱 오버헤드 < 5% (RT cycle time 기준)
- SC-014: 성능 병목 자동 식별 및 알림

### 데이터 모델 (US4)

- [ ] T084 [P] [US4] TraceContext 구조체 정의 (W3C Trace Context) in src/core/tracing/SpanContext.h
- [ ] T085 [P] [US4] Span 구조체 정의 (operation, start/end time, status) in src/core/tracing/SpanContext.h

### OpenTelemetry 통합 (US4)

- [ ] T086 [US4] TracerProvider 클래스 구현 (OpenTelemetry 초기화) in src/core/tracing/TracerProvider.cpp
- [ ] T087 [US4] OTLP Exporter 설정 (Jaeger 연동) in src/core/tracing/TracerProvider.cpp
- [ ] T088 [US4] Sampling strategy 구현 (head-based, configurable rate) in src/core/tracing/TracerProvider.cpp
- [ ] T089 [US4] JSON 설정 파일 로드 (sampling rate, exporter endpoint) in src/core/tracing/TracerProvider.cpp

### Span Management (US4)

- [ ] T090 [US4] SpanContext 클래스 구현 (trace ID, span ID 관리) in src/core/tracing/SpanContext.cpp
- [ ] T091 [US4] RAII Span Guard 구현 (자동 시작/종료) in src/core/tracing/SpanContext.cpp
- [ ] T092 [US4] Thread-local storage에 TraceContext 저장 in src/core/tracing/SpanContext.cpp
- [ ] T093 [US4] W3C Trace Context 직렬화/역직렬화 (header propagation) in src/core/tracing/SpanContext.cpp

### EventBus 계측 (US4)

- [ ] T094 [US4] EventBusTracer 클래스 구현 (observer pattern) in src/core/tracing/EventBusTracer.cpp
- [ ] T095 [US4] EventBus에 tracer observer 등록 in src/core/event/EventBus.cpp
- [ ] T096 [US4] 이벤트 publish 시 span 생성 in src/core/tracing/EventBusTracer.cpp
- [ ] T097 [US4] 이벤트 전파 시 TraceContext 주입 in src/core/tracing/EventBusTracer.cpp

### RT Cycle 트레이싱 (US4)

- [ ] T098 [US4] RTCycleTracer 클래스 구현 in src/core/tracing/RTCycleTracer.cpp
- [ ] T099 [US4] RT cycle 시작/종료 시 span 생성 in src/core/tracing/RTCycleTracer.cpp
- [ ] T100 [US4] Task/Sequence/Action 실행 span 생성 in src/core/tracing/RTCycleTracer.cpp

### 설정 파일 (US4)

- [ ] T101 [US4] config/tracing_config.json 템플릿 작성

### Unit Tests (US4)

- [ ] T102 [P] [US4] TracerProvider unit test in tests/unit/tracing/TracerProvider_test.cpp
- [ ] T103 [P] [US4] SpanContext unit test (W3C 스키마 검증) in tests/unit/tracing/SpanContext_test.cpp
- [ ] T104 [P] [US4] EventBusTracer unit test in tests/unit/tracing/EventBusTracer_test.cpp

### Integration Tests (US4)

- [ ] T105 [US4] End-to-end 트레이싱 통합 테스트 (RT → Non-RT 전체 경로) in tests/integration/tracing/end_to_end_tracing_test.cpp
- [ ] T106 [US4] 트레이싱 오버헤드 측정 테스트 (< 5% 검증) in tests/integration/tracing/tracing_overhead_test.cpp

**Checkpoint**: All user stories should now be independently functional. 전체 시스템 observability 완성.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: 전체 시스템 통합 및 최적화

### 통합 테스트

- [ ] T107 [P] quickstart.md 가이드 전체 시나리오 검증 (P1-P4 순차 실행)
- [ ] T108 부하 테스트 (10,000 사이클 deadline miss rate < 0.01% 검증)
- [ ] T109 Failover 시나리오 전체 테스트 (부하 중 재시작)

### 문서화

- [ ] T110 [P] API 문서 업데이트 (health-check-api.yaml 기반) in docs/
- [ ] T111 [P] 설정 파일 가이드 작성 (모든 JSON 설정 설명) in docs/
- [ ] T112 [P] 트러블슈팅 가이드 업데이트 (quickstart.md 기반) in docs/

### 성능 최적화

- [ ] T113 RT 경로 프로파일링 (로깅/트레이싱 오버헤드 최소화)
- [ ] T114 메모리 사용량 최적화 (checkpoint 크기 최소화)

### 보안 강화

- [ ] T115 [P] Health check endpoint 인증 추가 (선택 사항)
- [ ] T116 [P] 설정 파일 권한 검증 (민감 정보 보호)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Stories (Phase 3-6)**: All depend on Foundational phase completion
  - User stories can then proceed in parallel (if staffed)
  - Or sequentially in priority order (P1 → P2 → P3 → P4)
- **Polish (Phase 7)**: Depends on all user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational (Phase 2) - No dependencies on other stories
- **User Story 2 (P2)**: Can start after Foundational (Phase 2) - No dependencies on US1 (독립적 테스트 가능)
- **User Story 3 (P3)**: Can start after Foundational (Phase 2) - May integrate with US4 (trace ID 로깅) but independently testable
- **User Story 4 (P4)**: Can start after Foundational (Phase 2) - No dependencies on US1/US2/US3 (독립적 테스트 가능)

### Within Each User Story

- 데이터 모델 먼저 정의 (병렬 가능)
- 클래스 구현은 데이터 모델 의존
- 통합 작업은 클래스 구현 완료 후
- Unit tests는 구현과 병렬 가능 (TDD 권장)
- Integration tests는 모든 구현 완료 후

### Parallel Opportunities

- Phase 1: T002, T003, T004 병렬 실행 가능
- Phase 2: T005, T006, T007 병렬 실행 가능 (인터페이스 정의)
- US1: T011, T012 병렬 (데이터 모델), T028, T029, T030 병렬 (unit tests)
- US2: T033, T034, T035 병렬 (데이터 모델), T059, T060, T061 병렬 (systemd), T063-T066 병렬 (unit tests)
- US3: T081, T082 병렬 (unit tests)
- US4: T084, T085 병렬 (데이터 모델), T102, T103, T104 병렬 (unit tests)
- Phase 7: T107, T110, T111, T112, T115, T116 병렬 가능

---

## Parallel Example: User Story 1 (CPU Isolation & NUMA)

```bash
# Launch data models together:
Task T011: "CPUAffinityConfig 구조체 정의"
Task T012: "NUMABindingConfig 구조체 정의"

# Launch unit tests together (after implementation):
Task T028: "CPUAffinityManager unit test"
Task T029: "NUMABinding unit test"
Task T030: "PerfMonitor unit test"

# Launch config files together:
Task T026: "config/cpu_affinity.json 템플릿 작성"
Task T027: "config/numa_binding.json 템플릿 작성"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001-T004)
2. Complete Phase 2: Foundational (T005-T010) - **CRITICAL**
3. Complete Phase 3: User Story 1 (T011-T032)
4. **STOP and VALIDATE**:
   - Run integration tests (T031, T032)
   - Verify deadline miss rate < 0.01%
   - Verify NUMA local access > 95%
   - Test independently without US2/US3/US4
5. Deploy/demo if ready

### Incremental Delivery

1. **Foundation** (Setup + Foundational) → Infrastructure ready
2. **P1: Performance Optimization** (US1) → Test independently → **Deploy/Demo (MVP!)**
   - RT 성능 최적화 완료
   - Production 배포 가능한 최소 기능
3. **P2: High Availability** (US2) → Test independently → Deploy/Demo
   - 자동 복구 및 failover 추가
   - Production 안정성 향상
4. **P3: Structured Logging** (US3) → Test independently → Deploy/Demo
   - 로그 중앙 집중화
   - 운영 효율성 향상
5. **P4: Distributed Tracing** (US4) → Test independently → Deploy/Demo
   - 성능 분석 도구 추가
   - 심층 디버깅 가능
6. **Polish** (Phase 7) → Final validation → Production release

Each story adds value without breaking previous stories.

### Parallel Team Strategy

With multiple developers after Foundational phase:

1. **Team completes Setup + Foundational together** (T001-T010)
2. Once Foundational is done:
   - **Developer A**: User Story 1 (P1) - CPU/NUMA optimization
   - **Developer B**: User Story 2 (P2) - High availability
   - **Developer C**: User Story 3 (P3) - Structured logging
   - **Developer D**: User Story 4 (P4) - Distributed tracing
3. Stories complete and integrate independently
4. Team reconvenes for Phase 7 (Polish)

---

## Task Count Summary

- **Phase 1 (Setup)**: 4 tasks
- **Phase 2 (Foundational)**: 6 tasks (BLOCKING)
- **Phase 3 (US1 - Performance)**: 22 tasks
- **Phase 4 (US2 - High Availability)**: 37 tasks
- **Phase 5 (US3 - Structured Logging)**: 14 tasks
- **Phase 6 (US4 - Distributed Tracing)**: 23 tasks
- **Phase 7 (Polish)**: 10 tasks

**Total**: 116 tasks

### Per User Story:
- **US1 (P1)**: 22 tasks - Most critical, MVP scope
- **US2 (P2)**: 37 tasks - Most complex (failover, leader election, state checkpoint)
- **US3 (P3)**: 14 tasks - Simplest, extends existing spdlog
- **US4 (P4)**: 23 tasks - OpenTelemetry integration

### Parallel Opportunities Identified:
- Phase 1: 3 parallel tasks
- Phase 2: 3 parallel tasks (interfaces)
- US1: 7 parallel tasks
- US2: 11 parallel tasks
- US3: 2 parallel tasks
- US4: 5 parallel tasks
- Phase 7: 6 parallel tasks

**Total Parallel Tasks**: 37 tasks (32% of total)

### Suggested MVP Scope:
**Phase 1 + Phase 2 + Phase 3 (US1 only)** = 32 tasks

이 MVP로 production-grade RT 성능을 달성하고, 이후 US2-US4를 순차적으로 추가하여 고가용성, 로깅, 트레이싱 기능을 완성합니다.

---

## Notes

- **[P] tasks**: 다른 파일, 의존성 없음 → 병렬 실행 가능
- **[Story] label**: User story 추적용 (US1=P1, US2=P2, US3=P3, US4=P4)
- **각 user story는 독립적으로 완성 및 테스트 가능**
- **TDD 원칙**: Unit test를 먼저 작성하고 실패 확인 후 구현 (Constitution 준수)
- **Checkpoint에서 독립 검증**: 각 user story 완료 시 성공 기준 달성 확인
- **Commit 전략**: 각 task 또는 논리적 그룹 완료 후 commit
- **피해야 할 것**: 모호한 작업, 같은 파일 충돌, user story 간 강한 결합

---

**Format Validation**: ✅ All 116 tasks follow the strict checklist format with checkboxes, task IDs, [P]/[Story] labels, and file paths.
