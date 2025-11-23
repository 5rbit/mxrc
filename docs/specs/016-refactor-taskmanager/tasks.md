# 작업 목록: TaskManager 모듈 리팩터링

**브랜치**: `016-refactor-taskmanager` | **날짜**: 2025-11-13 | **사양서**: [spec.md](./spec.md)

## 구현 전략

본 리팩터링은 사용자 스토리(US) 단위로 진행되며, 각 사용자 스토리는 독립적으로 테스트 가능한 기능 개선을 나타냅니다. MVP(최소 실행 가능 제품)는 **US1** 완료 시점으로, `TaskManager`의 핵심 책임 분리가 이루어진 상태입니다. 이후 점진적으로 역할을 명확히 하고(US2), 상태 관리를 강화하며(US3), Command Pattern을 확대(US4)합니다.

---

## Phase 1: 기반 인터페이스 및 파일 구조 설정

*이 단계에서는 실제 구현에 앞서 필요한 헤더 파일과 테스트 파일의 기본 구조를 생성하여 병렬 개발을 준비합니다.*

- [X] T001 [P] `TaskDefinitionRegistry.h` 헤더 파일 생성 in `src/core/taskmanager/`
- [X] T002 [P] `TaskExecutor.h` 헤더 파일 생성 in `src/core/taskmanager/`
- [X] T003 [P] `Task.h` 신규 추상 베이스 클래스 헤더 파일 생성 in `src/core/taskmanager/`
- [X] T004 [P] `CancelTaskCommand.h` 헤더 파일 생성 in `src/core/taskmanager/commands/`
- [X] T005 [P] `PauseTaskCommand.h` 헤더 파일 생성 in `src/core/taskmanager/commands/`
- [X] T006 [P] `TaskDefinitionRegistry_test.cpp` 테스트 파일 생성 in `tests/unit/task/`
- [X] T007 [P] `TaskExecutor_test.cpp` 테스트 파일 생성 in `tests/unit/task/`

---

## Phase 2: [US1] `TaskManager` 책임 분리

*목표: `TaskManager`의 God Object 문제를 해결하기 위해 책임을 `TaskDefinitionRegistry`와 `TaskExecutor`로 분리합니다.*
*독립 테스트: `TaskDefinitionRegistry`는 Task 등록/생성 기능, `TaskExecutor`는 Task 실행/취소 기능을 독립적으로 테스트할 수 있습니다.*

- [X] T008 [US1] `TaskDefinitionRegistry.cpp` 구현: Task 등록 및 생성 로직 in `src/core/taskmanager/`
- [X] T009 [US1] `TaskExecutor.cpp` 구현: Task 실행 및 생명주기 관리 로직 in `src/core/taskmanager/`
- [X] T010 [US1] `TaskDefinitionRegistry_test.cpp` 단위 테스트 작성 in `tests/unit/task/`
- [X] T011 [US1] `TaskExecutor_test.cpp` 단위 테스트 작성 in `tests/unit/task/`
- [X] T012 [US1] `TaskManager.h` 및 `TaskManager.cpp` 리팩터링: `TaskDefinitionRegistry`와 `TaskExecutor`를 사용하도록 변경 in `src/core/taskmanager/`
- [X] T013 [US1] `TaskManager_test.cpp` 리팩터링: 변경된 `TaskManager`의 동작 검증 in `tests/unit/task/`

---

## Phase 3: [US2] `Task`와 `ITask` 역할 명확화

*목표: `Task`와 `ITask`의 역할을 통합하여 데이터와 행위를 함께 갖는 단일 객체 모델을 만듭니다.*
*독립 테스트: `Task` 파생 클래스 객체만으로 `execute`, `cancel` 등의 행위와 상태 조회가 모두 가능함을 테스트할 수 있습니다.*

- [X] T014 [US2] `interfaces/ITask.h` 수정: `toDto()`, `getStatus()` 등 데이터 조회 메서드 추가 in `src/core/taskmanager/interfaces/`
- [X] T015 [US2] `Task.h` 및 `Task.cpp` 구현: `ITask`를 구현하는 추상 베이스 클래스로 구현 in `src/core/taskmanager/`
- [X] T016 [P] [US2] `tasks/DriveForwardTask.h` 및 `.cpp` 리팩터링: 새로운 `Task` 베이스 클래스 상속 in `src/core/taskmanager/tasks/`
- [X] T017 [P] [US2] `tasks/DummyTask.h` 및 `.cpp` 리팩터링: 새로운 `Task` 베이스 클래스 상속 in `src/core/taskmanager/tasks/`
- [X] T018 [P] [US2] `tasks/InspectAreaTask.h` 및 `.cpp` 리팩터링: 새로운 `Task` 베이스 클래스 상속 in `src/core/taskmanager/tasks/`
- [X] T019 [US2] `factory/TaskFactory.h` 및 `.cpp` 리팩터링: `TaskDefinitionRegistry`와 통합하거나 새로운 구조에 맞게 수정 in `src/core/taskmanager/factory/`
- [X] T020 [US2] `TaskFactory_test.cpp` 수정: 변경된 팩토리 로직 검증 in `tests/unit/task/`

---

## Phase 4: [US3] 상태 관리 로직 개선

*목표: Task의 상태를 외부에서 변경할 수 없도록 캡슐화를 강화합니다.*
*독립 테스트: Task의 public API를 통해서는 상태를 직접 변경할 수 없으며, `execute()`나 `cancel()` 호출 시에만 내부 상태가 변경됨을 단위 테스트로 검증합니다.*

- [X] T021 [US3] `interfaces/ITaskManager.h` 리팩터링: `updateTaskStatus`, `updateTaskProgress` 등 상태 변경 메서드 제거 in `src/core/taskmanager/interfaces/`
- [X] T022 [US3] `TaskManager.cpp` 리팩터링: 제거된 상태 변경 메서드 호출 로직 삭제 in `src/core/taskmanager/`
- [X] T023 [US3] 모든 Task 구현 파일(`DriveForwardTask.cpp` 등) 검토 및 수정: `execute`, `cancel` 내에서 상태(`status_`, `progress_`)를 직접 관리하도록 보장 in `src/core/taskmanager/tasks/`
- [X] T024 [US3] `TaskManager_test.cpp` 및 각 Task 테스트 파일 수정: 상태 변경이 Task 내부적으로 올바르게 처리되는지 검증하는 테스트 추가

---

## Phase 5: [US4] Command Pattern 활용 확대

*목표: `Cancel`, `Pause` 등 Task 관련 조작을 Command Pattern으로 구현하여 확장성을 확보합니다.*
*독립 테스트: `CancelTaskCommand` 등 각 Command 객체를 생성하고 실행했을 때, `TaskManager`를 통해 의도한 작업이 수행되는지 테스트할 수 있습니다.*

- [X] T025 [P] [US4] `commands/CancelTaskCommand.cpp` 구현 in `src/core/taskmanager/commands/`
- [X] T026 [P] [US4] `commands/PauseTaskCommand.cpp` 구현 in `src/core/taskmanager/commands/`
- [X] T027 [US4] `TaskManager.cpp` 수정: `executeCommand` 메서드에서 새로운 Command들을 처리하는 로직 추가 in `src/core/taskmanager/`
- [X] T028 [US4] `operator_interface/OperatorInterface.cpp` 수정: `cancelTask` 등의 메서드에서 새로운 Command를 생성하여 사용하도록 변경 in `src/core/taskmanager/operator_interface/`
- [X] T029 [US4] `OperatorInterface_test.cpp` 수정: 새로운 Command 사용 로직 검증 in `tests/unit/task/`

---

## Phase 6: 최종 통합 및 폴리싱

- [X] T030 전체 빌드 및 모든 단위/통합 테스트 실행 (`ctest`)
- [ ] T031 [P] 모든 신규 및 수정된 파일에 대한 Doxygen 주석 리뷰 및 보강
- [ ] T032 [P] `README.md` 또는 관련 개발 문서에 리팩터링된 아키텍처 반영
- [ ] T033 최종 코드 정적 분석(cppcheck, clang-tidy) 및 MISRA C++ 준수 여부 확인

## 의존성

- **US2**는 **US1**에 의존합니다 (`TaskExecutor`가 `ITask`를 사용).
- **US3**는 **US2**에 의존합니다 (통합된 `ITask`의 상태 관리).
- **US4**는 **US1**에 의존합니다 (`TaskManager`가 Command를 처리).

**실행 순서**: US1 → US2 → US3 → US4

## 병렬 실행 예시

- **Phase 2 (US1)**:
  - 개발자 A: `TaskDefinitionRegistry` 구현 및 테스트 (T008, T010)
  - 개발자 B: `TaskExecutor` 구현 및 테스트 (T009, T011)
- **Phase 3 (US2)**:
  - 여러 개발자가 각자 다른 Task 클래스(`DriveForwardTask`, `DummyTask` 등)를 동시에 리팩터링할 수 있습니다 (T016, T017, T018).
- **Phase 5 (US4)**:
  - 개발자 A: `CancelTaskCommand` 구현 (T025)
  - 개발자 B: `PauseTaskCommand` 구현 (T026)
