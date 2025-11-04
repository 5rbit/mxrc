# 작업: Task & Mission Management (Task 및 임무 관리) 고도화

**입력**: `/specs/004-task-mission-management/`의 설계 문서
**전제 조건**: plan.md (필수), spec.md (사용자 스토리에 필요), research.md, data-model.md, contracts/

**테스트**: TDD 접근 방식에 따라 테스트 작업이 포함됩니다.

**구성**: 작업은 각 스토리를 독립적으로 구현하고 테스트할 수 있도록 사용자 스토리별로 그룹화됩니다.

## 형식: `[ID] [P?] [Story] 설명`

- **[P]**: 병렬 실행 가능 (다른 파일, 종속성 없음)
- **[Story]**: 이 작업이 속한 사용자 스토리 (예: US1, US2, US3)
- 설명에 정확한 파일 경로 포함

## 경로 규칙

- **단일 프로젝트**: 리포지토리 루트의 `src/`, `tests/`
- 아래 경로는 단일 프로젝트를 가정합니다 - plan.md 구조에 따라 조정하십시오.

---

## 1단계: 설정 (공유 인프라)

**목적**: 프로젝트 초기화 및 기본 구조 설정

- [x] T001 [P] BehaviorTree.CPP 라이브러리 의존성 추가 및 빌드 시스템 구성 (CMakeLists.txt)
- [x] T002 [P] spdlog 라이브러리 의존성 추가 및 로거 설정 (src/core/utils/Logger.h)
- [x] T003 [P] Google Test 프레임워크 설정 및 기본 테스트 환경 구성 (tests/)

---

## 2단계: 기반 (블로킹 전제 조건)

**목적**: 사용자 스토리 구현 전에 완료되어야 하는 핵심 인프라

**⚠️ 중요**: 이 단계가 완료될 때까지 사용자 스토리 작업을 시작할 수 없습니다.

- [x] T004 [P] `IDataStore.h` 인터페이스 정의 (specs/004-task-mission-management/contracts/IDataStore.h)
- [x] T005 [P] `IMissionManager.h` 인터페이스 정의 (specs/004-task-mission-management/contracts/IMissionManager.h)
- [x] T006 [P] `IOperatorInterface.h` 인터페이스 정의 (specs/004-task-mission-management/contracts/IOperatorInterface.h)
- [x] T007 [P] `TaskContext.h` 기본 구조체 정의 (src/core/task/TaskContext.h)
- [x] T008 [P] `AbstractTask.h` 추상 클래스 정의 (src/core/task/AbstractTask.h)

**체크포인트**: 기반 준비 완료 - 이제 사용자 스토리 구현을 병렬로 시작할 수 있습니다.

---

## 3단계: 사용자 스토리 1 - 독립적이고 견고한 Task 구현 및 관리 (우선순위: P1) 🎯 MVP

**목표**: 재사용 가능한 Task 단위를 구현하고, 명확한 생명주기 및 상태 관리를 통해 견고하게 동작하도록 합니다.

**독립 테스트**: 각 Task를 개별적으로 실행하고, 상태 전이가 올바르게 발생하는지, 오류 처리가 예상대로 동작하는지 검증합니다.

### 사용자 스토리 1에 대한 테스트 (TDD) ⚠️

- [x] T009 [P] [US1] `TaskFactory` 단위 테스트 작성 (tests/unit/task/TaskFactory_test.cpp)
- [x] T010 [P] [US1] `ResourceManager` 단위 테스트 작성 (tests/unit/task/ResourceManager_test.cpp)
- [x] T011 [P] [US1] `DriveToPosition` Task 단위 테스트 작성 (tests/unit/task/DriveToPosition_test.cpp)

### 사용자 스토리 1 구현

- [x] T012 [P] [US1] `TaskFactory.h` 및 `TaskFactory.cpp` 구현 (src/core/task/TaskFactory.h, src/core/task/TaskFactory.cpp)
- [x] T013 [P] [US1] `ResourceManager.h` 및 `ResourceManager.cpp` 구현 (src/core/task/ResourceManager.h, src/core/task/ResourceManager.cpp)
- [x] T014 [US1] `DriveToPosition` Task 예제 구현 (src/core/task/tasks/DriveToPosition.h, src/core/task/tasks/DriveToPosition.cpp)
- [x] T015 [US1] `LiftPallet` Task 예제 구현 (src/core/task/tasks/LiftPallet.h, src/core/task/tasks/LiftPallet.cpp)

**체크포인트**: 이 시점에서 사용자 스토리 1은 완전히 작동하고 독립적으로 테스트할 수 있어야 합니다.

---

## 4단계: 사용자 스토리 2 - 동적이고 유연한 Mission(워크플로우) 정의 및 실행 (우선순위: P1)

**목표**: Behavior Tree 기반으로 복잡한 Mission을 정의하고 실행하며, 동적으로 흐름을 제어합니다.

**독립 테스트**: JSON/YAML 형식의 Mission 정의 파일을 로드하여, Behavior Tree가 의도한 대로 Task를 실행하는지 검증합니다.

### 사용자 스토리 2에 대한 테스트 (TDD) ⚠️

- [x] T016 [P] [US2] `MissionManager` 단위 테스트 작성 (tests/unit/task/MissionManager_test.cpp)
- [x] T017 [P] [US2] Behavior Tree 파서 및 실행기 단위 테스트 작성 (tests/unit/task/BehaviorTree_test.cpp)
- [x] T018 [P] [US2] `TaskScheduler` 단위 테스트 작성 (tests/unit/task/TaskScheduler_test.cpp)

### 사용자 스토리 2 구현

- [x] T019 [US2] `MissionManager.h` 및 `MissionManager.cpp` 구현 (src/core/task/MissionManager.h, src/core/task/MissionManager.cpp)
- [x] T020 [US2] BehaviorTree.CPP와 연동하여 Behavior Tree 실행 로직 구현 (T019에 포함)
- [x] T021 [P] [US2] Mission 정의 파일(JSON/YAML) 파서 및 유효성 검사기 구현 (src/core/task/MissionParser.h, src/core/task/MissionParser.cpp)
- [x] T022 [P] [US2] `TaskScheduler.h` 및 `TaskScheduler.cpp` 구현 (src/core/task/TaskScheduler.h, src/core/task/TaskScheduler.cpp)
- [x] T023 [P] [US2] `TaskDependencyManager.h` 및 `TaskDependencyManager.cpp` 구현 (src/core/task/TaskDependencyManager.h, src/core/task/TaskDependencyManager.cpp)

**체크포인트**: 이 시점에서 사용자 스토리 1과 2는 모두 독립적으로 작동해야 합니다.

---

## 5단계: 사용자 스토리 4 - 신뢰할 수 있는 Mission 및 Task 운영 (우선순위: P1)

**목표**: Mission/Task의 실행 기록을 추적하고, 시스템 장애 시 상태를 복구하여 운영의 신뢰성을 보장합니다.

**독립 테스트**: Mission 실행 중 중요한 이벤트가 로그에 기록되는지, 시스템 재시작 후 Mission 상태가 복구되는지 검증합니다.

### 사용자 스토리 4에 대한 테스트 (TDD) ⚠️

- [ ] T025 [P] [US4] `DataStore` 연동 및 상태 복구 통합 테스트 작성 (tests/integration/task/DataStoreRecovery_test.cpp)

### 사용자 스토리 4 구현

- [x] T027 [US4] `MissionManager`에 `IDataStore` 인터페이스를 통해 Mission/Task 상태 저장 로직 추가 (src/core/task/MissionManager.cpp)
- [x] T028 [US4] 시스템 재시작 시 `IDataStore` 인터페이스를 통해 Mission 상태를 복구하는 로직 구현 (T027에 포함)


**체크포인트**: 이제 모든 사용자 스토리 1, 2, 4가 독립적으로 작동해야 합니다.

---

## 6단계: 사용자 스토리 3 - Mission 및 Task 실행 상태 실시간 모니터링 및 제어 (우선순위: P2)

**목표**: 운영자가 Mission/Task의 상태를 실시간으로 모니터링하고, 일시 중지/재개/취소 등 제어 명령을 내릴 수 있도록 합니다.

**독립 테스트**: 외부 인터페이스를 통해 Mission 상태를 조회하고, 제어 명령을 내렸을 때 시스템이 올바르게 반응하는지 검증합니다.

### 사용자 스토리 3에 대한 테스트 (TDD) ⚠️

- [x] T030 [P] [US3] `OperatorInterface` 단위 테스트 작성 (tests/unit/task/OperatorInterface_test.cpp)

### 사용자 스토리 3 구현

- [x] T031 [US3] `OperatorInterface.h` 및 `OperatorInterface.cpp` 구현 (src/core/task/OperatorInterface.h, src/core/task/OperatorInterface.cpp)
- [x] T032 [US3] `MissionManager`에 실시간 상태 보고 및 제어 명령 처리 로직 추가 (src/core/task/MissionManager.cpp)
- [x] T033 [US3] `DataStore`에서 Task 상태 이력을 조회하는 기능 구현 (src/core/datastore/DataStore.cpp)

**체크포인트**: 이제 모든 사용자 스토리가 독립적으로 작동해야 합니다.

---

## 7단계: 폴리싱 및 교차 관심사

**목적**: 여러 사용자 스토리에 영향을 미치는 개선 사항

- [x] T034 [P] docs/task/README.md 문서 업데이트
- [x] T035 [US5] MissionManager에 Mission 정의 및 Task 실행 결과 저장/로드 기능 추가 (src/core/task/MissionManager.cpp)
- [x] T036 [US5] DataStore에 Mission 정의 및 Task 실행 결과 영속화 로직 구현 (src/core/datastore/DataStore.cpp)
- [x] T037 [P] [US6] MissionManager 오류 복구 관련 단위 테스트 작성 (tests/unit/task/MissionManager_Recovery_test.cpp)
- [x] T038 [US6] MissionManager에 오류 발생 시 Mission/Task 상태 복구 로직 구현 (src/core/task/MissionManager.cpp)
- [ ] T039 [US6] Behavior Tree 노드에 오류 처리 및 복구 메커니즘 통합 (예: Fallback, Retry 노드 활용)
- [ ] T040 [US4] Mission 제어 및 상태 보고 통신 채널 암호화 및 메시지 무결성 구현 (FR-019)

---

## 종속성 및 실행 순서

### 단계 종속성

- **설정 (1단계)**: 종속성 없음 - 즉시 시작 가능
- **기반 (2단계)**: 설정 완료에 의존 - 모든 사용자 스토리를 차단합니다.
- **사용자 스토리 (3단계 이상)**: 모두 기반 단계 완료에 의존
  - 그 후 사용자 스토리를 병렬로 진행할 수 있습니다 (인력이 있는 경우).
  - 또는 우선순위 순서대로 순차적으로 (P1 → P2 → P3)

### 사용자 스토리 종속성

- **사용자 스토리 1 (P1)**: 기반 (2단계) 이후 시작 가능 - 다른 스토리에 대한 종속성 없음
- **사용자 스토리 2 (P1)**: US1 완료에 의존
- **사용자 스토리 4 (P1)**: US2 완료에 의존
- **사용자 스토리 3 (P2)**: US4 완료에 의존

### 각 사용자 스토리 내

- 테스트 (포함된 경우)는 구현 전에 작성되고 실패해야 합니다.
- 서비스 전 모델
- 엔드포인트 전 서비스
- 통합 전 핵심 구현
- 다음 우선순위로 이동하기 전에 스토리 완료

### 병렬 기회

- [P]로 표시된 모든 설정 작업을 병렬로 실행할 수 있습니다.
- [P]로 표시된 모든 기반 작업을 병렬로 실행할 수 있습니다 (2단계 내).
- 기반 단계가 완료되면 모든 사용자 스토리를 병렬로 시작할 수 있습니다 (팀 용량이 허용하는 경우).
- [P]로 표시된 사용자 스토리에 대한 모든 테스트를 병렬로 실행할 수 있습니다.
- [P]로 표시된 스토리 내의 모델을 병렬로 실행할 수 있습니다.
- 다른 개발자가 다른 사용자 스토리를 병렬로 작업할 수 있습니다.

---

## 병렬 예: 사용자 스토리 1

```bash
# 사용자 스토리 1에 대한 모든 테스트를 함께 시작 (테스트가 요청된 경우):
Task: "tests/unit/datastore/DataStore_test.cpp에서 TaskFactory 단위 테스트 작성"
Task: "tests/unit/task/ResourceManager_test.cpp에서 ResourceManager 단위 테스트 작성"
Task: "tests/unit/task/DriveToPosition_test.cpp에서 DriveToPosition Task 단위 테스트 작성"

# 사용자 스토리 1에 대한 모든 구현을 함께 시작:
Task: "src/core/task/TaskFactory.h, src/core/task/TaskFactory.cpp에서 TaskFactory.h 및 TaskFactory.cpp 구현"
Task: "src/core/task/ResourceManager.h, src/core/task/ResourceManager.cpp에서 ResourceManager.h 및 ResourceManager.cpp 구현"
```

---

## 구현 전략

### MVP 우선 (사용자 스토리 1만)

1.  1단계: 설정 완료
2.  2단계: 기반 완료 (중요 - 모든 스토리를 차단)
3.  3단계: 사용자 스토리 1 완료
4.  **중지 및 확인**: 사용자 스토리 1을 독립적으로 테스트
5.  준비가 되면 배포/데모

### 증분 제공

1.  설정 + 기반 완료 → 기반 준비 완료
2.  사용자 스토리 1 추가 → 독립적으로 테스트 → 배포/데모 (MVP!)
3.  사용자 스토리 2 추가 → 독립적으로 테스트 → 배포/데모
4.  사용자 스토리 4 추가 → 독립적으로 테스트 → 배포/데모
5.  사용자 스토리 3 추가 → 독립적으로 테스트 → 배포/데모
6.  각 스토리는 이전 스토리를 깨지 않고 가치를 더합니다.

### 병렬 팀 전략

여러 개발자와 함께:

1.  팀이 함께 설정 + 기반을 완료합니다.
2.  기반이 완료되면:
    - 개발자 A: 사용자 스토리 1
    - 개발자 B: 사용자 스토리 2 (US1 완료 후)
    - 개발자 C: 사용자 스토리 4 (US2 완료 후)
3.  스토리가 독립적으로 완료되고 통합됩니다.
