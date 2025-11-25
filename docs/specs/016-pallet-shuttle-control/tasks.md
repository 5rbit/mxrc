# Tasks: 팔렛 셔틀 제어 시스템

**Input**: Design documents from `/docs/specs/016-pallet-shuttle-control/`
**Status**: In Progress - Phase 3-4 Mostly Complete
**Progress**: 56/93 tasks completed (60.2%)
**Last Updated**: 2025-11-25
**Prerequisites**: plan.md, spec.md, research.md

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 작업 설명은 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Task, Action, test, model, service 등)
- 파일 경로와 코드는 원래대로 표기합니다

**예시**:
- ✅ 좋은 예: "Task 모델 생성 in src/models/task.cpp"
- ❌ 나쁜 예: "Create Task model in src/models/task.cpp"

---

**Tests**: 이 프로젝트는 TDD 원칙을 따르므로 각 구현 작업 전에 테스트를 작성합니다.

**Organization**: User Story별로 그룹화하여 각 Story를 독립적으로 구현 및 테스트할 수 있도록 합니다.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 병렬 실행 가능 (서로 다른 파일, 의존성 없음)
- **[Story]**: 이 작업이 속한 User Story (예: US1, US2, US3)
- 작업 설명에 정확한 파일 경로 포함

## Path Conventions

- **Single project**: `src/`, `tests/` at repository root
- 경로는 plan.md에 정의된 구조를 따름

---

## Phase 1: Setup (Shared Infrastructure) ✅

**Purpose**: 프로젝트 초기화 및 기본 구조 생성

- [X] T001 CMakeLists.txt에 새로운 디렉토리 추가 (src/core/control, src/core/alarm, src/robot/pallet_shuttle)
- [X] T002 [P] src/core/control/interfaces/ 디렉토리 생성
- [X] T003 [P] src/core/alarm/interfaces/ 디렉토리 생성
- [X] T004 [P] src/robot/pallet_shuttle/ 하위 디렉토리 생성 (control, actions, sequences, tasks, state, config)
- [X] T005 [P] tests/unit/core/control/ 디렉토리 생성
- [X] T006 [P] tests/unit/core/alarm/ 디렉토리 생성
- [X] T007 [P] tests/unit/robot/pallet_shuttle/ 하위 디렉토리 생성
- [X] T008 [P] tests/integration/robot/pallet_shuttle/ 디렉토리 생성
- [X] T009 Mock Driver 설정 파일 준비 in config/mock-driver.yaml

---

## Phase 2: Foundational (Blocking Prerequisites) ✅

**Purpose**: 모든 User Story가 의존하는 핵심 인프라 구축

**⚠️ CRITICAL**: 이 Phase가 완료되어야 User Story 작업을 시작할 수 있습니다

### 2.1 핵심 인터페이스 정의

- [X] T010 [P] IRobotController 인터페이스 정의 in src/core/control/interfaces/IRobotController.h
- [X] T011 [P] IBehaviorArbiter 인터페이스 정의 in src/core/control/interfaces/IBehaviorArbiter.h
- [X] T012 [P] ITaskQueue 인터페이스 정의 in src/core/control/interfaces/ITaskQueue.h
- [X] T013 [P] IAlarmManager 인터페이스 정의 in src/core/alarm/interfaces/IAlarmManager.h
- [X] T014 [P] IAlarmConfiguration 인터페이스 정의 in src/core/alarm/interfaces/IAlarmConfiguration.h

### 2.2 DTO 및 Enum 정의

- [X] T015 [P] Priority enum 정의 (5단계) in src/core/control/dto/Priority.h
- [X] T016 [P] ControlMode enum 정의 (9단계) in src/core/control/dto/ControlMode.h
- [X] T017 [P] BehaviorRequest 구조체 정의 in src/core/control/dto/BehaviorRequest.h
- [X] T018 [P] AlarmDto 정의 (심각도, 유형, 상태) in src/core/alarm/dto/AlarmDto.h
- [X] T019 [P] AlarmSeverity enum 정의 (Critical, Warning, Info) in src/core/alarm/dto/AlarmSeverity.h

### 2.3 Alarm 시스템 기본 구현

- [X] T020 Alarm 클래스 구현 in src/core/alarm/impl/Alarm.cpp
- [X] T021 AlarmConfiguration 클래스 구현 in src/core/alarm/impl/AlarmConfiguration.cpp (YAML 파싱)
- [X] T022 AlarmManager 클래스 구현 (기본 생성, 조회, 리셋) in src/core/alarm/impl/AlarmManager.cpp
- [X] T023 Alarm 설정 파일 스키마 정의 in docs/specs/016-pallet-shuttle-control/contracts/alarm-config-schema.json
- [X] T024 기본 Alarm 설정 파일 작성 in config/alarm-config.yaml (10종 Alarm 정의)

### 2.4 Behavior Arbitration 핵심 구현

- [X] T025 Custom Priority Queue 구현 (5개 독립 큐) in src/core/control/impl/BehaviorPriorityQueue.cpp
- [X] T026 BehaviorArbiter 클래스 구현 (우선순위 선택 알고리즘) in src/core/control/impl/BehaviorArbiter.cpp
- [X] T027 ControlMode 상태 전환 검증 함수 구현 in src/core/control/impl/BehaviorArbiter.cpp
- [X] T028 TaskQueue 클래스 구현 (우선순위 정렬) in src/core/control/impl/TaskQueue.cpp
- [X] T029 Behavior Arbitration 설정 파일 작성 in config/behavior-arbitration.yaml

### 2.5 Task Layer Suspend/Resume 확장

- [X] T030 ITaskExecutor에 pause(), resume() 메서드 확인/추가 in src/core/task/interfaces/ITaskExecutor.h
- [X] T031 TaskExecutor에서 pause/resume 구현 in src/core/task/core/TaskExecutor.cpp
- [X] T032 TaskPausedEvent, TaskResumedEvent 정의 in src/core/task/dto/TaskEvent.h
- [X] T033 EventBus에 Task 상태 이벤트 발행 통합 in src/core/task/core/TaskExecutor.cpp

**Checkpoint**: 기반 인프라 완료 - User Story 구현 시작 가능 ✅

---

## Phase 3: User Story 2 - Alarm 감지 및 대응 (Priority: P1) 🎯 (Partially Complete)

**Goal**: 비정상 상황 발생 시 즉시 Alarm을 발생시키고 적절한 대응 수행

**Independent Test**: 센서 시뮬레이션으로 다양한 Alarm 발생 후 DataStore 기록 및 대응 확인

**Why First**: Alarm 시스템은 모든 다른 User Story의 안전장치 역할을 하므로 가장 먼저 구현

### 테스트 작성 (TDD)

- [X] T034 [P] [US2] AlarmManager 단위 테스트 (생성, 조회, 리셋, 재발 추적, 심각도 상향) in tests/unit/core/alarm/AlarmManager_test.cpp ✅
- [X] T035 [P] [US2] AlarmConfiguration 단위 테스트 (YAML 파싱, 심각도 상향) in tests/unit/core/alarm/AlarmConfiguration_test.cpp ✅
- [X] T036 [P] [US2] Alarm 재발 빈도 추적 테스트 in tests/unit/core/alarm/AlarmManager_test.cpp ✅
- [ ] T037 [P] [US2] Critical Alarm 발생 시 즉시 중단 통합 테스트 in tests/integration/robot/pallet_shuttle/alarm_handling_test.cpp (Phase 5 의존)

### 구현

- [X] T038 [US2] Alarm 재발 빈도 추적 로직 구현 (sliding window) in src/core/alarm/impl/AlarmManager.cpp ✅
- [X] T039 [US2] Alarm 심각도 자동 상향 조정 로직 구현 in src/core/alarm/impl/AlarmManager.cpp ✅
- [ ] T040 [US2] DataStore Alarm 정보 저장 통합 in src/core/alarm/impl/AlarmManager.cpp (TODO 마커 존재)
- [ ] T041 [US2] EventBus Alarm 이벤트 발행 통합 in src/core/alarm/impl/AlarmManager.cpp (TODO 마커 존재)
- [X] T042 [US2] Critical Alarm 처리 흐름 구현 (즉시 중단) in src/core/control/impl/BehaviorArbiter.cpp ✅
- [ ] T043 [US2] Warning Alarm 처리 흐름 구현 (작업 완료 후 대기) in src/core/control/impl/BehaviorArbiter.cpp
- [ ] T044 [US2] Alarm 이력 조회 기능 구현 in src/core/alarm/impl/AlarmManager.cpp

**Checkpoint**: Alarm 시스템 부분 완료 - 핵심 기능 동작, TODO 마커 남아있음

---

## Phase 4: User Story 5 - 행동 의사 결정 및 모드 전환 (Priority: P1) 🎯 (Tests Complete)

**Goal**: 우선순위 기반 행동 선택 및 제어 모드 전환

**Independent Test**: 다양한 우선순위 행동 할당 후 올바른 순서로 처리되는지 확인

**Why Second**: 팔렛 운반 작업(US1)이 BehaviorArbiter 위에서 동작하므로 먼저 구축

### 테스트 작성 (TDD) ✅

- [X] T045 [P] [US5] BehaviorArbiter 우선순위 선택 단위 테스트 (11 tests) in tests/unit/core/control/BehaviorArbiter_test.cpp ✅
- [X] T046 [P] [US5] TaskQueue 우선순위 정렬 단위 테스트 (10 tests) in tests/unit/core/control/TaskQueue_test.cpp ✅
- [X] T047 [P] [US5] ControlMode 상태 전환 검증 테스트 (included in T045) ✅
- [ ] T048 [P] [US5] Behavior Arbitration 통합 테스트 (경쟁 상황) in tests/integration/robot/pallet_shuttle/behavior_arbitration_test.cpp

### 구현 ✅

- [X] T049 [US5] BehaviorArbiter tick() 메서드 구현 (100ms 주기) in src/core/control/impl/BehaviorArbiter.cpp ✅
- [X] T050 [US5] selectNextBehavior() 알고리즘 구현 in src/core/control/impl/BehaviorArbiter.cpp ✅
- [X] T051 [US5] shouldPreemptCurrentTask() 로직 구현 in src/core/control/impl/BehaviorArbiter.cpp ✅
- [X] T052 [US5] handleUrgentTask() 구현 (Suspend/Resume) in src/core/control/impl/BehaviorArbiter.cpp ✅
- [X] T053 [US5] handleEmergency() 구현 (즉시 중단) in src/core/control/impl/BehaviorArbiter.cpp ✅
- [X] T054 [US5] transitionTo() 메서드 구현 (ControlMode 전환) in src/core/control/impl/BehaviorArbiter.cpp ✅
- [ ] T055 [US5] DataStore에 제어 모드 상태 기록 in src/core/control/impl/BehaviorArbiter.cpp (TODO 마커)
- [X] T056 [US5] 동일 우선순위 부가 규칙 (FIFO) 구현 in src/core/control/impl/BehaviorPriorityQueue.cpp ✅

**Checkpoint**: Phase 4 거의 완료 - DataStore 통합만 남음 (T055)

---

## Phase 5: User Story 1 - 팔렛 픽업 및 배치 (Priority: P1) 🎯 MVP

**Goal**: 팔렛 픽업-이동-배치 기본 작업 수행

**Independent Test**: 픽업 위치(A) → 배치 위치(B) 단일 작업 완료 확인

### 테스트 작성 (TDD)

- [ ] T057 [P] [US1] MoveToPositionAction 단위 테스트 in tests/unit/robot/pallet_shuttle/actions/MoveToPositionActionTest.cpp
- [ ] T058 [P] [US1] PickPalletAction 단위 테스트 in tests/unit/robot/pallet_shuttle/actions/PickPalletActionTest.cpp
- [ ] T059 [P] [US1] PlacePalletAction 단위 테스트 in tests/unit/robot/pallet_shuttle/actions/PlacePalletActionTest.cpp
- [ ] T060 [P] [US1] PalletTransportSequence 단위 테스트 in tests/unit/robot/pallet_shuttle/sequences/PalletTransportSequenceTest.cpp
- [ ] T061 [P] [US1] PalletTransportTask 단위 테스트 in tests/unit/robot/pallet_shuttle/tasks/PalletTransportTaskTest.cpp
- [ ] T062 [US1] 기본 팔렛 운반 통합 테스트 in tests/integration/robot/pallet_shuttle/basic_transport_test.cpp

### 구현

- [ ] T063 [P] [US1] MoveToPositionAction 구현 (Mock Driver 사용) in src/robot/pallet_shuttle/actions/MoveToPositionAction.cpp
- [ ] T064 [P] [US1] PickPalletAction 구현 in src/robot/pallet_shuttle/actions/PickPalletAction.cpp
- [ ] T065 [P] [US1] PlacePalletAction 구현 in src/robot/pallet_shuttle/actions/PlacePalletAction.cpp
- [ ] T066 [US1] PalletTransportSequence 구현 (픽업→이동→배치) in src/robot/pallet_shuttle/sequences/PalletTransportSequence.cpp
- [ ] T067 [US1] PalletTransportTask 구현 in src/robot/pallet_shuttle/tasks/PalletTransportTask.cpp
- [ ] T068 [US1] PalletShuttleState 클래스 구현 (로봇 상태 관리) in src/robot/pallet_shuttle/state/PalletShuttleState.cpp
- [ ] T069 [US1] PalletShuttleController 구현 (IRobotController) in src/robot/pallet_shuttle/control/PalletShuttleController.cpp
- [ ] T070 [US1] PalletShuttleConfig 클래스 구현 (설정 파일 파싱) in src/robot/pallet_shuttle/config/PalletShuttleConfig.cpp
- [ ] T071 [US1] Sequence 실행 중 오류 처리 (Alarm 발생) in src/robot/pallet_shuttle/sequences/PalletTransportSequence.cpp

**Checkpoint**: MVP 완료 - 단일 팔렛 운반 작업 성공

---

## Phase 6: User Story 3 - 다중 팔렛 순차 처리 (Priority: P2)

**Goal**: 여러 팔렛 운반 작업을 작업 큐에서 순차 처리

**Independent Test**: 5개 작업 등록 후 순서대로 완료되는지 확인

### 테스트 작성 (TDD)

- [ ] T072 [P] [US3] TaskQueue 다중 작업 처리 단위 테스트 in tests/unit/core/control/TaskQueueTest.cpp
- [ ] T073 [P] [US3] 긴급 작업 삽입 테스트 in tests/unit/core/control/TaskQueueTest.cpp
- [ ] T074 [US3] 다중 작업 순차 처리 통합 테스트 in tests/integration/robot/pallet_shuttle/basic_transport_test.cpp

### 구현

- [ ] T075 [US3] TaskQueue에 여러 작업 추가 기능 구현 in src/core/control/impl/TaskQueue.cpp
- [ ] T076 [US3] TaskQueue에서 다음 작업 자동 시작 로직 in src/robot/pallet_shuttle/control/PalletShuttleController.cpp
- [ ] T077 [US3] 작업 실패 시 건너뛰기/재시도 정책 구현 in src/robot/pallet_shuttle/control/PalletShuttleController.cpp
- [ ] T078 [US3] TaskCompletedEvent 핸들러 구현 (다음 작업 시작) in src/robot/pallet_shuttle/control/PalletShuttleController.cpp

**Checkpoint**: 다중 작업 처리 완료 - 작업 큐 관리 검증

---

## Phase 7: User Story 4 - 상태 모니터링 및 진행 추적 (Priority: P2)

**Goal**: 실시간 로봇 상태 및 작업 진행률 제공

**Independent Test**: 상태 조회 API 호출 후 실제 로봇 상태와 일치 확인

### 테스트 작성 (TDD)

- [ ] T079 [P] [US4] PalletShuttleState 상태 조회 테스트 in tests/unit/robot/pallet_shuttle/state/PalletShuttleStateTest.cpp
- [ ] T080 [P] [US4] 작업 진행률 계산 테스트 in tests/unit/robot/pallet_shuttle/tasks/PalletTransportTaskTest.cpp

### 구현

- [ ] T081 [US4] DataStore 실시간 상태 업데이트 in src/robot/pallet_shuttle/state/PalletShuttleState.cpp
- [ ] T082 [US4] 작업 진행률 추적 로직 구현 in src/robot/pallet_shuttle/tasks/PalletTransportTask.cpp
- [ ] T083 [US4] Alarm 목록 조회 기능 구현 in src/core/alarm/impl/AlarmManager.cpp
- [ ] T084 [US4] 작업 큐 상태 조회 기능 구현 in src/core/control/impl/TaskQueue.cpp

**Checkpoint**: 상태 모니터링 완료 - 실시간 정보 제공 검증

---

## Phase 8: User Story 6 - 주기적 상태 점검 및 예방 정비 (Priority: P3)

**Goal**: 주기적 하드웨어 상태 점검 및 예방 정비 알림

**Independent Test**: 주기적 점검 Task 실행 후 결과 기록 확인

### 테스트 작성 (TDD)

- [ ] T085 [P] [US6] SafetyCheckSequence 단위 테스트 in tests/unit/robot/pallet_shuttle/sequences/SafetyCheckSequenceTest.cpp
- [ ] T086 [P] [US6] PERIODIC 모드 Task 실행 테스트 in tests/unit/core/task/TaskExecutorTest.cpp

### 구현

- [ ] T087 [US6] SafetyCheckSequence 구현 (센서 점검) in src/robot/pallet_shuttle/sequences/SafetyCheckSequence.cpp
- [ ] T088 [US6] PERIODIC 모드 Task 스케줄링 in src/core/task/core/TaskExecutor.cpp
- [ ] T089 [US6] 부품 마모도 임계값 체크 로직 in src/robot/pallet_shuttle/sequences/SafetyCheckSequence.cpp
- [ ] T090 [US6] Info 수준 Alarm 발생 (예방 정비) in src/robot/pallet_shuttle/sequences/SafetyCheckSequence.cpp

**Checkpoint**: 주기적 점검 완료 - 예방 정비 알림 검증

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: 전체 시스템 품질 향상

- [ ] T091 [P] AddressSanitizer로 모든 테스트 메모리 검증
- [ ] T092 성능 프로파일링 (Critical Alarm < 100ms, DataStore 기록 < 50ms)
- [ ] T093 End-to-End 통합 테스트 (모든 User Story 시나리오) in tests/integration/robot/pallet_shuttle/end_to_end_test.cpp

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 즉시 시작 가능
- **Foundational (Phase 2)**: Setup 완료 후 → **모든 User Story를 차단**
- **User Stories (Phase 3-8)**: Foundational 완료 후 시작 가능
  - 병렬 실행 가능 (팀 역량에 따라)
  - 또는 우선순위 순서대로: US2 → US5 → US1 → US3 → US4 → US6
- **Polish (Phase 9)**: 모든 원하는 User Story 완료 후

### User Story Dependencies

- **User Story 2 (Alarm)**: Foundational 이후 독립 실행 가능 → 가장 먼저 구현 권장
- **User Story 5 (Behavior)**: Foundational 이후 독립 실행 가능 → US1보다 먼저 구현 권장
- **User Story 1 (팔렛 운반)**: US2, US5 완료 후 → MVP 완성
- **User Story 3 (다중 작업)**: US1 완료 후 → TaskQueue 활용
- **User Story 4 (모니터링)**: US1 완료 후 → 상태 정보 노출
- **User Story 6 (주기 점검)**: US1 완료 후 → Sequence 재사용

### Within Each User Story

- 테스트 작성 → 테스트 실패 확인 → 구현 (TDD)
- 인터페이스/DTO → 구현 클래스
- Action → Sequence → Task
- 핵심 구현 → 통합

### Parallel Opportunities

- Setup 모든 [P] 작업 병렬
- Foundational의 모든 [P] 작업 병렬 (T010-T019)
- Foundational 완료 후 여러 User Story 병렬 (팀 역량 허용 시)
- 각 User Story 내 테스트들 병렬 ([P] 표시)
- 각 User Story 내 Action 구현들 병렬 ([P] 표시)

---

## Parallel Example: User Story 1

```bash
# User Story 1 테스트를 모두 병렬로 실행:
Task: "MoveToPositionAction 단위 테스트 in tests/unit/robot/pallet_shuttle/actions/MoveToPositionActionTest.cpp"
Task: "PickPalletAction 단위 테스트 in tests/unit/robot/pallet_shuttle/actions/PickPalletActionTest.cpp"
Task: "PlacePalletAction 단위 테스트 in tests/unit/robot/pallet_shuttle/actions/PlacePalletActionTest.cpp"

# User Story 1 Action 구현을 모두 병렬로 실행:
Task: "MoveToPositionAction 구현 in src/robot/pallet_shuttle/actions/MoveToPositionAction.cpp"
Task: "PickPalletAction 구현 in src/robot/pallet_shuttle/actions/PickPalletAction.cpp"
Task: "PlacePalletAction 구현 in src/robot/pallet_shuttle/actions/PlacePalletAction.cpp"
```

---

## Implementation Strategy

### MVP First (US2 + US5 + US1)

1. Phase 1: Setup 완료
2. Phase 2: Foundational 완료 (CRITICAL)
3. Phase 3: US2 (Alarm) 완료
4. Phase 4: US5 (Behavior) 완료
5. Phase 5: US1 (팔렛 운반) 완료
6. **STOP and VALIDATE**: MVP 독립 테스트
7. 배포/데모 준비

### Incremental Delivery

1. Setup + Foundational → 기반 완료
2. US2 → Alarm 시스템 검증 → 배포/데모
3. US5 → Behavior 시스템 검증 → 배포/데모
4. US1 → MVP 완성 → 배포/데모 (핵심 가치 제공!)
5. US3 → 다중 작업 처리 추가 → 배포/데모
6. US4 → 모니터링 추가 → 배포/데모
7. US6 → 예방 정비 추가 → 배포/데모
8. 각 Story는 이전 Story를 손상시키지 않고 가치 추가

### Parallel Team Strategy

여러 개발자가 있는 경우:

1. 팀이 Setup + Foundational을 함께 완료
2. Foundational 완료 후:
   - Developer A: US2 (Alarm)
   - Developer B: US5 (Behavior)
   - Developer C: US1 (팔렛 운반) - US2, US5 일부 완료 후 시작 가능
3. Story들이 독립적으로 완료 및 통합

---

## Notes

- [P] 작업 = 서로 다른 파일, 의존성 없음
- [Story] 레이블로 작업을 특정 User Story에 매핑
- 각 User Story는 독립적으로 완료 및 테스트 가능
- TDD: 테스트 실패 확인 후 구현
- 각 작업 또는 논리적 그룹 후 커밋
- 각 Checkpoint에서 Story를 독립적으로 검증
- 피해야 할 것: 모호한 작업, 파일 충돌, Story 독립성을 해치는 의존성
