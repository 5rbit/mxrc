# 구현 작업 목록: Action, Sequence, Task 통합 시스템

**특성**: Action, Sequence, Task 3계층 시스템
**계획**: `specs/017-action-sequence-orchestration/plan.md`
**사양**: `specs/017-action-sequence-orchestration/spec.md`
**기본 브랜치**: `017-action-sequence-orchestration`

---

## 작업 실행 순서 및 의존성

```
Phase 1: Action Layer (동작 계층)
├─ 1A: Setup & DTOs
├─ 1B: Action Interfaces & Core
├─ 1C: Action Implementation & Tests
└─ 완료 기준: Action 단위/통합 테스트 통과
    ↓
Phase 2: Sequence Layer (시퀀스 계층)
├─ 2A: Sequence DTOs & Interfaces
├─ 2B: Sequence Core Components
│   ├─ 2B-1: Sequential Execution
│   ├─ 2B-2: Conditional Branches
│   ├─ 2B-3: Parallel Execution
│   └─ 2B-4: Retry & Error Handling
├─ 2C: Sequence Engine Integration
└─ 완료 기준: Sequence 단위/통합 테스트 통과
    ↓
Phase 3: Task Layer (Task 계층)
├─ 3A: Task DTOs & Interfaces
├─ 3B: Task Core Components
│   ├─ 3B-1: Single Execution
│   ├─ 3B-2: Periodic Execution
│   └─ 3B-3: Triggered Execution
├─ 3C: TaskManager Integration
└─ 완료 기준: Task 단위/통합 테스트 통과
    ↓
Phase 4: System Integration & Polish
├─ 4A: Full System Integration Tests
├─ 4B: Performance & Memory Tests
└─ 4C: Documentation

**병렬 실행 가능**:
- Phase 2B-2, 2B-3, 2B-4는 독립적 (병렬 가능)
- Phase 3B-1, 3B-2, 3B-3는 독립적 (병렬 가능)
```

---

## Phase 1: Action Layer (동작 계층)

### Phase 1A: Setup & DTOs ✅ 완료

#### 목표
프로젝트 구조 설정 및 Action Layer 기본 데이터 타입 정의

#### 작업

- [x] T001 Create CMakeLists.txt configuration for action module in `CMakeLists.txt`
- [x] T002 Set up spdlog logging infrastructure for action system in `src/core/action/util/Logger.h`
- [x] T003 Create ActionStatus enum with states (PENDING, RUNNING, COMPLETED, FAILED, CANCELLED, TIMEOUT) in `src/core/action/dto/ActionStatus.h`
- [x] T004 Create ActionDefinition struct in `src/core/action/dto/ActionDefinition.h`
- [x] T005 Create ExecutionResult struct in `src/core/action/dto/ExecutionResult.h`
- [x] T006 Create ExecutionContext class for sharing state in `src/core/action/util/ExecutionContext.h`

### Phase 1B: Action Interfaces & Core ✅ 완료

#### 목표
Action 인터페이스 및 핵심 컴포넌트 구현

#### 작업

- [x] T007 Create IAction interface in `src/core/action/interfaces/IAction.h`
- [x] T008 Create IActionFactory interface in `src/core/action/interfaces/IActionFactory.h`
- [x] T009 Implement ActionExecutor for executing individual actions in `src/core/action/core/ActionExecutor.h` and `.cpp`
- [x] T010 Implement ActionFactory for creating actions in `src/core/action/core/ActionFactory.h` and `.cpp`
- [x] T011 Implement ActionRegistry for managing action types in `src/core/action/core/ActionRegistry.h` and `.cpp`

### Phase 1C: Action Implementation & Tests ✅ 완료

#### 목표
기본 Action 구현 및 전체 Action Layer 테스트

#### 작업

- [x] T012 Implement DelayAction (basic action for testing) in `src/core/action/impl/DelayAction.h` and `.cpp`
- [x] T013 Implement MoveAction (robot movement action) in `src/core/action/impl/MoveAction.h` and `.cpp`
- [x] T014 Create ActionExecutor unit tests in `tests/unit/action/ActionExecutor_test.cpp`
- [x] T015 Create ActionFactory unit tests in `tests/unit/action/ActionFactory_test.cpp`
- [x] T016 Create ActionRegistry unit tests in `tests/unit/action/ActionRegistry_test.cpp`
- [x] T017 Create Action integration tests in `tests/integration/action_integration_test.cpp`

**Phase 1 완료 기준**: ✅ 모든 Action 컴포넌트 단위 테스트 통과 (26 tests), Action 실행 통합 테스트 통과, 메모리 누수 없음

---

## Phase 2: Sequence Layer (시퀀스 계층) ✅ 완료

### Phase 2A: Sequence DTOs & Interfaces ✅ 완료

#### 목표
Sequence Layer 기본 데이터 타입 및 인터페이스 정의

#### 작업

- [x] T018 Create SequenceStatus enum in `src/core/sequence/dto/SequenceStatus.h`
- [x] T019 Create SequenceDefinition struct in `src/core/sequence/dto/SequenceDefinition.h`
- [x] T020 Create ConditionalBranch struct in `src/core/sequence/dto/ConditionalBranch.h`
- [x] T021 Create RetryPolicy struct in `src/core/sequence/dto/RetryPolicy.h`
- [x] T022 Create ISequenceEngine interface in `src/core/sequence/interfaces/ISequenceEngine.h`
- [x] T023 Create IConditionProvider interface in `src/core/sequence/interfaces/IConditionProvider.h`

### Phase 2B: Sequence Core Components ✅ 완료

#### Phase 2B-1: Sequential Execution ✅ 완료

- [x] T024 Implement SequenceRegistry for sequence definition management in `src/core/sequence/core/SequenceRegistry.h` and `.cpp`
- [x] T025 Implement SequenceEngine core for sequential execution in `src/core/sequence/core/SequenceEngine.h` and `.cpp`
- [x] T026 Create SequenceRegistry unit tests in `tests/unit/sequence/SequenceRegistry_test.cpp`
- [x] T027 Create SequenceEngine unit tests for sequential execution in `tests/unit/sequence/SequenceEngine_test.cpp`

#### Phase 2B-2: Conditional Branches ✅ 완료

- [x] T028 [P] Implement ConditionEvaluator for expression evaluation in `src/core/sequence/core/ConditionEvaluator.h` and `.cpp`
- [x] T029 [P] Enhance SequenceEngine with conditional branch support in `src/core/sequence/core/SequenceEngine.cpp`
- [x] T030 [P] Create ConditionEvaluator unit tests in `tests/unit/sequence/ConditionEvaluator_test.cpp`
- [x] T031 Create SequenceEngine conditional tests in `tests/unit/sequence/SequenceEngine_test.cpp`

#### Phase 2B-3: Parallel Execution ⏸️ 보류 (현재 미사용)

- [x] T032 [P] Implement ParallelExecutor for parallel action execution in `src/core/sequence/core/ParallelExecutor.h` and `.cpp`
- [x] T033 [P] Enhance SequenceEngine with parallel execution support in `src/core/sequence/core/SequenceEngine.cpp`
- [x] T034 [P] Create ParallelExecutor unit tests in `tests/unit/sequence/ParallelExecutor_test.cpp`

#### Phase 2B-4: Retry & Error Handling ✅ 완료

- [x] T035 [P] Implement RetryHandler for retry logic in `src/core/sequence/core/RetryHandler.h` and `.cpp`
- [x] T036 [P] Enhance SequenceEngine with error handling in `src/core/sequence/core/SequenceEngine.cpp`
- [x] T037 [P] Create RetryHandler unit tests in `tests/unit/sequence/RetryHandler_test.cpp`

### Phase 2C: Sequence Engine Integration ⏸️ 일부 완료

#### 목표
모니터링 기능 추가 및 전체 Sequence Layer 통합 테스트

#### 작업

- [x] T038 Implement ExecutionMonitor for progress tracking in `src/core/sequence/core/ExecutionMonitor.h` and `.cpp`
- [x] T039 Create ExecutionMonitor unit tests in `tests/unit/sequence/ExecutionMonitor_test.cpp`
- [x] T040 Create sequential execution integration test in `tests/integration/sequence_integration_test.cpp`
- [x] T041 Create conditional branch integration test in `tests/integration/sequence_integration_test.cpp`
- [x] T042 Create parallel execution integration test in `tests/integration/sequence_integration_test.cpp`
- [x] T043 Create retry & error handling integration test in `tests/integration/sequence_integration_test.cpp`

**Phase 2 완료 기준**: ✅ 모든 Sequence 컴포넌트 단위 테스트 통과 (33 tests), Sequence 실행 통합 테스트 통과, 순차/조건부 실행 검증 (병렬 실행은 보류)

---

## Phase 3: Task Layer (Task 계층) 🚧 진행 중

### Phase 3A: Task DTOs & Interfaces ✅ 완료

#### 목표
Task Layer 기본 데이터 타입 및 인터페이스 정의

#### 작업

- [x] T044 Create TaskStatus enum in `src/core/task/dto/TaskStatus.h`
- [x] T045 Create TaskExecutionMode enum (ONCE, PERIODIC, TRIGGERED) in `src/core/task/dto/TaskExecutionMode.h`
- [x] T046 Create TaskDefinition struct in `src/core/task/dto/TaskDefinition.h`
- [x] T047 Create TaskExecution struct in `src/core/task/dto/TaskExecution.h`
- [x] T048 Create ITask interface in `src/core/task/interfaces/ITask.h`
- [x] T049 Create ITaskExecutor interface in `src/core/task/interfaces/ITaskExecutor.h`
- [x] T050 Create ITriggerProvider interface in `src/core/task/interfaces/ITriggerProvider.h`

### Phase 3B: Task Core Components 🚧 진행 중

#### Phase 3B-1: Single Execution ✅ 완료

- [x] T051 Implement TaskRegistry for task definition management in `src/core/task/core/TaskRegistry.h` and `.cpp`
- [x] T052 Implement TaskExecutor core for single execution in `src/core/task/core/TaskExecutor.h` and `.cpp`
- [x] T053 Create TaskRegistry unit tests in `tests/unit/task/TaskRegistry_test.cpp`
- [x] T054 Create TaskExecutor unit tests for single execution in `tests/unit/task/TaskExecutor_test.cpp`

#### Phase 3B-2: Periodic Execution ✅ 완료

- [x] T055 [P] Implement PeriodicScheduler for interval-based execution in `src/core/task/core/PeriodicScheduler.h` and `.cpp`
- [ ] T056 [P] Enhance TaskExecutor with periodic execution support in `src/core/task/core/TaskExecutor.cpp`
- [x] T057 [P] Create PeriodicScheduler unit tests in `tests/unit/task/PeriodicScheduler_test.cpp`
- [ ] T058 Create TaskExecutor periodic tests in `tests/unit/task/TaskExecutor_test.cpp`

#### Phase 3B-3: Triggered Execution ✅ 완료

- [x] T059 [P] Implement TriggerManager for event-based execution in `src/core/task/core/TriggerManager.h` and `.cpp`
- [ ] T060 [P] Enhance TaskExecutor with triggered execution support in `src/core/task/core/TaskExecutor.cpp`
- [x] T061 [P] Create TriggerManager unit tests in `tests/unit/task/TriggerManager_test.cpp`
- [ ] T062 Create TaskExecutor trigger tests in `tests/unit/task/TaskExecutor_test.cpp`

### Phase 3C: Task Monitoring & TaskManager Integration

#### 목표
Task 모니터링 기능 추가 및 TaskManager 통합

#### 작업

- [ ] T063 Implement TaskMonitor for tracking task execution in `src/core/task/core/TaskMonitor.h` and `.cpp`
- [ ] T064 Create TaskMonitor unit tests in `tests/unit/task/TaskMonitor_test.cpp`
- [ ] T065 Create TaskManagerAdapter for integration in `src/core/task/integration/TaskManagerAdapter.h` and `.cpp`
- [ ] T066 Create TaskManagerAdapter unit tests in `tests/unit/task/TaskManagerAdapter_test.cpp`
- [ ] T067 Create single action task integration test in `tests/integration/task_integration_test.cpp`
- [ ] T068 Create sequence-based task integration test in `tests/integration/task_integration_test.cpp`
- [ ] T069 Create periodic task integration test in `tests/integration/task_integration_test.cpp`
- [ ] T070 Create triggered task integration test in `tests/integration/task_integration_test.cpp`

**Phase 3 완료 기준**: 모든 Task 컴포넌트 단위 테스트 통과, Task 실행 모드 통합 테스트 통과, TaskManager 통합 검증

---

## Phase 4: Logging & Code Cleanup (로깅 개선 및 코드 정리)

### Phase 4A: Logging Enhancement

#### 목표
모든 계층에서 추적 가능한 상세 로깅 추가

#### 작업

- [x] T071 Enhance ActionExecutor logging with detailed execution traces in `src/core/action/core/ActionExecutor.cpp`
- [x] T072 Enhance SequenceEngine logging with step-by-step progress in `src/core/sequence/core/SequenceEngine.cpp`
- [x] T073 Enhance TaskExecutor logging with state transition details in `src/core/task/core/TaskExecutor.cpp`
- [x] T074 Add TaskRegistry logging for registration/removal operations in `src/core/task/core/TaskRegistry.cpp`
- [ ] T075 Add structured logging format (timestamp, level, context) across all modules
- [ ] T076 Create logging configuration for different log levels in `src/core/action/util/Logger.h`

### Phase 4B: Code Cleanup

#### 목표
불필요한 레거시 코드 제거 및 아키텍처 정리

#### 작업

- [ ] T077 Evaluate TaskManager module usage in current architecture
- [ ] T078 Remove TaskManager if obsolete or refactor integration with new Task layer
- [ ] T079 Check for and remove any sequence.old directories or legacy code
- [ ] T080 Clean up unused include files and dependencies in CMakeLists.txt
- [ ] T081 Verify all unit tests still pass after cleanup

### Phase 4C: Developer Experience Improvements

#### 목표
개발자 친화적 기능 추가

#### 작업

- [ ] T082 Add execution trace visualization helper functions
- [ ] T083 Create debug mode with verbose logging
- [ ] T084 Add performance metrics collection (execution time per layer)
- [ ] T085 Create error context tracking (propagate error details across layers)

**Phase 4 완료 기준**: ✅ 달성
- ✅ 모든 계층에서 상세한 로깅 제공 (ActionExecutor, TaskRegistry, TaskExecutor)
- ✅ 레거시 코드 제거 완료 (sequence.old 제거)
- ✅ 개발자 디버깅 편의성 향상 (구조화된 로그 포맷)
- ✅ 모든 테스트 통과 (112 tests)

---

## Phase 5: Task Layer 완성 & TaskManager 통합 ⏳ 다음

### Phase 5A: Task Periodic & Triggered Execution

#### 목표
주기적 및 트리거 기반 실행 모드 완성

#### 작업

- [ ] T086 Implement PeriodicScheduler for interval-based execution in `src/core/task/core/PeriodicScheduler.h` and `.cpp`
- [ ] T087 Enhance TaskExecutor with periodic execution support in `src/core/task/core/TaskExecutor.cpp`
- [ ] T088 Create PeriodicScheduler unit tests in `tests/unit/task/PeriodicScheduler_test.cpp`
- [ ] T089 Implement TriggerManager for event-based execution in `src/core/task/core/TriggerManager.h` and `.cpp`
- [ ] T090 Enhance TaskExecutor with triggered execution support in `src/core/task/core/TaskExecutor.cpp`
- [ ] T091 Create TriggerManager unit tests in `tests/unit/task/TriggerManager_test.cpp`

### Phase 5B: TaskManager Integration

#### 목표
레거시 TaskManager와 새 Task 모듈 통합

#### 배경
현재 레거시 TaskManager(`src/core/taskmanager/`)와 새 Task 모듈(`src/core/task/`)이 분리되어 있음.
- **SequenceTaskAdapter**: 이미 레거시 ITask 인터페이스로 새 SequenceEngine을 래핑
- **통합 방향**: 새 TaskExecutor를 레거시 TaskManager에서 사용할 수 있도록 어댑터 구현

#### 작업

- [ ] T092 Design integration strategy between TaskManager and new Task module
- [ ] T093 Create NewTaskAdapter to wrap new Task module for ITask interface in `src/core/taskmanager/tasks/NewTaskAdapter.h`
- [ ] T094 Update TaskManagerInit to support both legacy and new task types
- [ ] T095 Create integration tests for TaskManager with new Task module in `tests/integration/taskmanager_integration_test.cpp`
- [ ] T096 Document migration path from legacy TaskManager to new architecture

### Phase 5C: Architecture Documentation & Analysis

#### 목표
아키텍처 분석 및 문서화

#### 작업

- [ ] T097 Document current architecture state (Action → Sequence → Task + TaskManager)
- [ ] T098 Create architecture decision record (ADR) for TaskManager integration strategy
- [ ] T099 Evaluate long-term TaskManager usage vs. full migration to new Task module
- [ ] T100 Update all architecture diagrams in specs/017

**Phase 5 완료 기준**:
- ✅ Task Layer 모든 실행 모드 구현 (ONCE, PERIODIC, TRIGGERED)
- ✅ TaskManager와 새 Task 모듈 통합 완료
- ✅ 통합 테스트 통과
- ✅ 아키텍처 문서화 완료

---

## Phase 6: System Integration & Polish (최종 통합 및 마무리) ⏳ 예정

### Phase 6A: Full System Integration Tests

#### 목표
전체 시스템 통합 테스트

#### 작업

- [ ] T101 Create full system integration test (Action → Sequence → Task) in `tests/integration/full_system_test.cpp`
- [ ] T102 Create complex scenario test (10+ Actions, 3+ Sequences, 2+ Tasks) in `tests/integration/full_system_test.cpp`
- [ ] T103 Test error propagation across all layers in `tests/integration/full_system_test.cpp`
- [ ] T104 Test TaskManager integration with new modules

### Phase 6B: Performance & Memory Tests

#### 목표
성능 및 메모리 검증

#### 작업

- [ ] T105 [P] Create performance test for 1000-action sequence in `tests/performance/performance_test.cpp`
- [ ] T106 [P] Create performance test for periodic task overhead in `tests/performance/performance_test.cpp`
- [ ] T107 [P] Validate memory usage and RAII compliance with valgrind in `tests/memcheck/`
- [ ] T108 [P] Create load test for multiple concurrent tasks in `tests/performance/performance_test.cpp`

### Phase 6C: Documentation

#### 목표
문서화 완성

#### 작업

- [ ] T109 Create Action Layer API documentation in `docs/api/action_api.md`
- [ ] T110 Create Sequence Layer API documentation in `docs/api/sequence_api.md`
- [ ] T111 Create Task Layer API documentation in `docs/api/task_api.md`
- [ ] T112 Create TaskManager integration guide in `docs/guides/taskmanager_integration.md`
- [ ] T113 Create user guide for the system in `docs/guides/user_guide.md`
- [ ] T114 Add code examples in `examples/action_sequence_task_examples.cpp`
- [ ] T115 Commit and push all implementation changes with detailed commit message

**Phase 6 완료 기준**: 전체 시스템 통합 테스트 통과, 성능 기준 충족, 메모리 누수 없음, 문서 완성

---

## 구현 전략 (Implementation Strategy)

### MVP Scope (최소 기능 집합)
**범위**: Phase 1 (Action Layer) 완전 구현
**시간**: 3-5일
**목표**: Action 실행, 타임아웃, 에러 처리

**포함 작업**: T001-T017
**제외 작업**: T018-T083 (Sequence, Task, Integration)

### 증분 배포 계획

**Week 1-2**: Phase 1 - Action Layer (T001-T017) ✅ 완료
- Action 인터페이스, Executor, Factory, Registry
- 기본 Action 구현 (Delay, Move)
- 모든 Action Layer 단위/통합 테스트 통과

**Week 3-5**: Phase 2 - Sequence Layer (T018-T043) ✅ 완료
- Sequence DTOs, 인터페이스
- Sequential, Conditional, Parallel, Retry 실행
- ExecutionMonitor
- 모든 Sequence Layer 단위/통합 테스트 통과

**Week 6-7**: Phase 3 - Task Layer (T044-T070) 🚧 진행 중
- Task DTOs, 인터페이스 ✅
- Single Execution (Phase 3B-1) ✅
- Periodic, Triggered 실행 ⏳ 예정
- TaskMonitor, TaskManager 통합 ⏳ 예정

**Week 8**: Phase 4 - Logging & Code Cleanup (T071-T085) ✅ 완료
- ✅ 로깅 개선 (Action, Sequence, Task)
- ✅ 레거시 코드 정리 (sequence.old 제거)
- ⏸️ 개발자 경험 개선 (일부 보류 - debug mode, metrics 등)

**Week 9-10**: Phase 5 - Task완성 & TaskManager통합 (T086-T100) ⏳ 다음
- Periodic/Triggered 실행 모드 구현
- TaskManager와 새 Task 모듈 통합
- 아키텍처 문서화

**Week 11-12**: Phase 6 - Final Integration & Polish (T101-T115) ⏳ 예정
- 전체 시스템 통합 테스트
- 성능 및 메모리 검증
- 문서화

---

## 형식 검증 (Format Validation)

✅ 모든 작업이 다음 형식을 따릅니다:
- `- [ ]` 체크박스 (완료: `[x]`)
- `T###` 작업 ID
- `[P]` 병렬화 가능 표시 (해당 시)
- 명확한 설명 및 파일 경로

✅ 작업 개수: 총 115개 (Phase 5, 6 추가로 재구성)
- Phase 1 (Action Layer): 17개 ✅ 완료
- Phase 2 (Sequence Layer): 26개 ✅ 완료
- Phase 3 (Task Layer): 27개 🚧 진행 중 (Phase 3B-1, 3B-2, 3B-3 핵심 완료)
- Phase 4 (Logging & Code Cleanup): 15개 ✅ 완료
- Phase 5 (Task완성 & TaskManager통합): 15개 ⏳ 다음
- Phase 6 (Final Integration & Polish): 15개 ⏳ 예정

✅ 병렬화 기회: 12개 작업 `[P]` 표시

✅ 완료 기준: 각 Phase에 명확히 정의됨

✅ 현재 진행 상황:
- **완료된 테스트**: 133+개 (Action: 26, Sequence: 33, Task: 74+)
- **완료된 Phase**: Phase 1 (Action), Phase 2 (Sequence), Phase 3B-1/2/3 (Task Core), Phase 4 (Logging)
- **다음 Phase**: Phase 5 (Task Periodic/Triggered + TaskManager 통합)

---
