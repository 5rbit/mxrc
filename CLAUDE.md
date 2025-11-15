# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 프로젝트 개요

MXRC는 어떤 로봇도 제어할 수 있는 범용 로봇 제어 컨트롤러입니다. C++20으로 개발되며, CMake 빌드 시스템을 사용하고, Linux Ubuntu 24.04 LTS PREEMPT_RT 환경에서 실시간 성능을 목표로 합니다.

**핵심 성능 요구사항:**
- OOP/인터페이스 기반 설계로 모듈 간 상호 의존성(Chaining) 최소화
- 구동 중심이 아닌, 고성능 아키텍처에 초점
- RAII 원칙 필수 적용으로 리소스 누수 방지 및 견고성 확보
- 구성 요소는 추적 가능한 상세 로그 기록
- 모든 모듈에 대한 철저한 단위 테스트 수행
- 실패의 경우도 단위 테스트에 포함하여 진행
- 명확한 상태 머신 구현 및 실시간 진행률(Progress) 보고 기능

## 빌드 및 테스트

### 빌드 명령어

```bash
# 빌드 설정 및 빌드
mkdir -p build
cd build
cmake ..
make

# 메인 실행 파일 실행
./mxrc

# 모든 테스트 실행 (112 tests)
./run_tests

# 특정 테스트 스위트 실행
./run_tests --gtest_filter=ActionExecutor*
./run_tests --gtest_filter=SequenceEngine*
./run_tests --gtest_filter=TaskExecutor*
```

## 시스템 아키텍처

MXRC는 계층적 아키텍처를 따르며, 각 계층은 명확한 책임과 독립적인 테스트 가능성을 가집니다.

### 아키텍처 계층

```
┌─────────────────────────────────────────────────────────┐
│         Task Management Layer                           │
│  Task 생명주기 관리 및 실행 모드 제어                   │
├─────────────────────────────────────────────────────────┤
│ • TaskExecutor (단일/주기적/트리거 실행)                │
│ • TaskRegistry (Task 정의 관리)                         │
│ • PeriodicScheduler (주기적 실행 스케줄러)              │
│ • TriggerManager (이벤트 트리거 관리)                   │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│         Sequence Orchestration Layer                    │
│  여러 Action의 순차/조건부/병렬 실행 조율                │
├─────────────────────────────────────────────────────────┤
│ • SequenceEngine (시퀀스 실행 엔진)                     │
│ • SequenceRegistry (시퀀스 정의 관리)                   │
│ • ConditionEvaluator (조건 평가)                       │
│ • RetryHandler (재시도 처리)                           │
│ • ExecutionContext (실행 컨텍스트)                     │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│         Action Execution Layer                          │
│  로봇 동작의 기본 정의 및 실행                           │
├─────────────────────────────────────────────────────────┤
│ • IAction (동작 인터페이스)                             │
│ • ActionExecutor (개별 동작 실행)                       │
│ • ActionFactory (동작 생성 팩토리)                      │
│ • ActionRegistry (동작 타입 등록)                       │
└─────────────────────────────────────────────────────────┘
```

### 계층별 책임

| 계층 | 모듈 | 주요 책임 | 상태 관리 |
|------|------|---------|---------|
| Task | `task/` | Task 정의, 등록, 실행 모드 관리 | TaskStatus |
| Sequence | `sequence/` | Action 순서 정의, 조건/병렬 실행 | SequenceStatus |
| Action | `action/` | 개별 Action 실행, 타임아웃 관리 | ActionStatus |

## 디렉토리 구조

```
src/core/
├── action/                          # Action Layer
│   ├── interfaces/
│   │   ├── IAction.h
│   │   └── IActionFactory.h
│   ├── core/
│   │   ├── ActionExecutor.{h,cpp}
│   │   ├── ActionFactory.{h,cpp}
│   │   └── ActionRegistry.{h,cpp}
│   ├── dto/
│   │   ├── ActionStatus.h
│   │   └── ActionDefinition.h
│   ├── impl/                       # 기본 Action 구현
│   │   ├── DelayAction.{h,cpp}
│   │   └── MoveAction.{h,cpp}
│   └── util/
│       ├── ExecutionContext.h
│       └── Logger.h
│
├── sequence/                        # Sequence Layer
│   ├── core/
│   │   ├── SequenceEngine.{h,cpp}
│   │   ├── SequenceRegistry.{h,cpp}
│   │   ├── ConditionEvaluator.{h,cpp}
│   │   └── RetryHandler.{h,cpp}
│   └── dto/
│       ├── SequenceDefinition.h
│       ├── SequenceStatus.h
│       ├── ConditionalBranch.h
│       └── RetryPolicy.h
│
└── task/                            # Task Layer
    ├── interfaces/
    │   ├── ITask.h
    │   ├── ITaskExecutor.h
    │   └── ITriggerProvider.h
    ├── core/
    │   ├── TaskExecutor.{h,cpp}
    │   ├── TaskRegistry.{h,cpp}
    │   ├── PeriodicScheduler.{h,cpp}
    │   └── TriggerManager.{h,cpp}
    └── dto/
        ├── TaskDefinition.h
        ├── TaskExecution.h
        ├── TaskStatus.h
        └── TaskExecutionMode.h

tests/
├── unit/                           # 단위 테스트
│   ├── action/                     # 26 tests
│   ├── sequence/                   # 33 tests
│   └── task/                       # 53 tests (TaskRegistry, TaskCoreExecutor 포함)
└── integration/                    # 통합 테스트
    ├── action_integration_test.cpp
    └── sequence_integration_test.cpp
```

## 핵심 컴포넌트

### Action Layer (Phase 1 완료)

#### IAction 인터페이스
```cpp
namespace mxrc::core::action {
    class IAction {
    public:
        virtual ~IAction() = default;
        virtual std::string getId() const = 0;
        virtual std::string getType() const = 0;
        virtual void execute(ExecutionContext& context) = 0;
        virtual void cancel() = 0;
        virtual ActionStatus getStatus() const = 0;
    };
}
```

#### ActionExecutor
- 개별 Action 실행
- 타임아웃 관리
- 실행 결과 수집
- 에러 처리

#### ActionFactory
- Action 타입별 생성 함수 등록 (`registerFactory`)
- 파라미터 기반 Action 인스턴스 생성
- 플러그인 방식 확장 지원

### Sequence Layer (Phase 2 완료)

#### SequenceEngine
- 시퀀스 실행 제어 (시작, 일시정지, 재개, 취소)
- 순차/조건부/병렬 실행 조율
- Action 간 데이터 전달 관리
- 진행률 추적

#### SequenceRegistry
- 시퀀스 정의 등록 및 조회 (`registerDefinition`, `getDefinition`)
- 스레드 안전성 보장

#### ConditionEvaluator
- 조건식 평가 (==, !=, <, >, <=, >=)
- 논리 연산 (AND, OR, NOT)
- 이전 Action 결과 참조

### Task Layer (Phase 3B-1 완료)

#### TaskRegistry
- Task 정의 등록 및 관리
- Task 타입 구분 (ACTION, SEQUENCE)
- 스레드 안전한 등록/조회
- **테스트**: 12개 단위 테스트 통과

#### TaskExecutor
- Task 실행 모드 관리 (ONCE, PERIODIC, TRIGGERED)
- 단일 Action 기반 Task 실행
- Sequence 기반 Task 실행
- Task 상태 관리 및 제어 (cancel, pause, resume)
- **테스트**: 14개 단위 테스트 통과

#### Task 실행 모드
```cpp
enum class TaskExecutionMode {
    ONCE,      // 단일 실행 (Phase 3B-1 완료)
    PERIODIC,  // 주기적 실행 (Phase 3B-2)
    TRIGGERED  // 트리거 기반 실행 (Phase 3B-3)
};
```

## 코드 작성 가이드

### 네임스페이스

모든 코드는 중첩된 네임스페이스를 사용:

```cpp
namespace mxrc::core::action {
    // Action 계층 코드
}

namespace mxrc::core::sequence {
    // Sequence 계층 코드
}

namespace mxrc::core::task {
    // Task 계층 코드
}
```

### Action 구현

새로운 Action 타입을 구현할 때:

```cpp
class MyAction : public IAction {
public:
    MyAction(const std::string& id, long duration)
        : id_(id), duration_(duration) {}

    void execute(ExecutionContext& context) override {
        // 작업 수행
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_));

        // 결과 저장
        context.setActionResult(id_, "completed");
    }

    void cancel() override {
        status_ = ActionStatus::CANCELLED;
    }

    ActionStatus getStatus() const override {
        return status_;
    }

    std::string getId() const override { return id_; }
    std::string getType() const override { return "MyAction"; }

private:
    std::string id_;
    long duration_;
    ActionStatus status_ = ActionStatus::IDLE;
};

// ActionFactory에 등록
factory->registerFactory("MyAction", [](const std::string& id, const auto& params) {
    long duration = 100;
    auto it = params.find("duration");
    if (it != params.end()) {
        duration = std::stol(it->second);
    }
    return std::make_shared<MyAction>(id, duration);
});
```

### Sequence 정의

```cpp
// 1. 순차 실행
SequenceDefinition seqDef("seq1", "Sequential Actions");
seqDef.addStep(ActionStep("step1", "Delay").addParameter("duration", "100"));
seqDef.addStep(ActionStep("step2", "Move").addParameter("x", "10"));
registry.registerDefinition(seqDef);

// 2. 조건부 분기
ConditionalBranch branch;
branch.id = "check_value";
branch.condition = "value > 5";
branch.thenActions = {"action_high"};
branch.elseActions = {"action_low"};
seqDef.addConditionalBranch("step1", branch);

// 3. ExecutionContext 사용
context.setVariable("input", 42);
auto value = context.getVariable("input");
context.setActionResult(actionId, result);
```

### Task 정의 및 실행

```cpp
// 1. 단일 Action Task
TaskDefinition taskDef("task1", "Single Action Task");
taskDef.setWork("Delay")  // Action 타입
       .setOnceMode();

// 2. Sequence 기반 Task
TaskDefinition taskDef("task2", "Sequence Task");
taskDef.setWorkSequence("seq1")
       .setOnceMode();

// 3. Task 실행
ExecutionContext context;
auto result = taskExecutor->execute(taskDef, context);

// 4. Task 상태 확인
auto status = taskExecutor->getStatus("task1");
auto progress = taskExecutor->getProgress("task1");
```

## 테스트 규칙

### 테스트 구조
```cpp
// <ComponentName>_test.cpp
#include "gtest/gtest.h"
#include "core/<layer>/<component>.h"

namespace mxrc::core::<layer> {

class ComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 설정
    }

    void TearDown() override {
        // 정리
    }
};

TEST_F(ComponentTest, TestScenario) {
    // Given
    // When
    // Then
    EXPECT_EQ(expected, actual);
}

}
```

### 테스트 명명 규칙
- 테스트 파일: `<ComponentName>_test.cpp`
- 테스트 클래스: `<ComponentName>Test`
- 테스트 케이스: 시나리오를 명확히 설명하는 이름
- Mock 클래스: `Mock<ClassName>`

### 테스트 커버리지
- **Action Layer**: 26 tests
- **Sequence Layer**: 33 tests
- **Task Layer**: 53 tests
- **전체**: 112 tests (모두 통과)

## 설계 원칙

### 1. RAII (Resource Acquisition Is Initialization)
- 모든 리소스는 생성자에서 할당, 소멸자에서 해제
- `std::shared_ptr`, `std::unique_ptr` 사용
- 수동 메모리 관리 금지

### 2. 인터페이스 기반 설계
- 모든 확장 지점에 인터페이스 사용
- 의존성 주입 (Dependency Injection)
- 느슨한 결합 (Loose Coupling)

### 3. 단계적 구현
- Phase 1: Action Layer → Phase 2: Sequence Layer → Phase 3: Task Layer
- 각 계층은 이전 계층 위에 구축
- 독립적 테스트 가능

### 4. 스레드 안전성
- Registry 클래스들은 `std::mutex`로 보호
- 상태 접근은 동기화됨
- Logger는 thread-safe

## 현재 진행 상황

### 완료된 Phase
- ✅ **Phase 1**: Action Layer 완료 (26 tests)
- ✅ **Phase 2A-2G**: Sequence Layer 완료 (33 tests)
- ✅ **Phase 3B-1**: Task Core Components - Single Execution (26 tests)
  - TaskRegistry 구현 및 테스트 (12 tests)
  - TaskExecutor ONCE 모드 구현 및 테스트 (14 tests)

### 다음 단계
- **Phase 3B-2**: Periodic Execution (주기적 실행)
  - PeriodicScheduler 구현
  - TaskExecutor periodic 모드 확장
- **Phase 3B-3**: Triggered Execution (트리거 기반 실행)
  - TriggerManager 구현
  - TaskExecutor triggered 모드 확장

## 의존성

- **spdlog**: 로깅 프레임워크
- **GTest**: 단위 테스트 프레임워크
- **CMake**: 빌드 시스템 (최소 버전 3.16)

Ubuntu에서 의존성 설치:
```bash
sudo apt-get install libspdlog-dev libgtest-dev cmake
```

## 사양 주도 개발

이 프로젝트는 사양 우선 접근 방식을 따릅니다:

1. 각 기능은 `specs/<기능번호>-<기능명>/`에 사양을 가짐
2. 사양에는 사용자 스토리, 인수 기준, 기능적 요구사항 포함
3. 관련 사양 참조:
   - `specs/017-action-sequence-orchestration/spec.md`
   - `specs/017-action-sequence-orchestration/architecture.md`
   - `specs/017-action-sequence-orchestration/tasks.md`

## 참고 자료

- 전체 아키텍처: `specs/017-action-sequence-orchestration/architecture.md`
- 구현 계획: `specs/017-action-sequence-orchestration/plan.md`
- Task 목록: `specs/017-action-sequence-orchestration/tasks.md`
- README: 프로젝트 루트의 `README.md`
