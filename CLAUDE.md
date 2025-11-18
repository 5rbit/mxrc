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

#### 권장 빌드 방법 (macOS, Homebrew)

macOS에서 Homebrew를 사용하여 `tbb`와 `googletest`를 설치한 경우, 아래의 명령어를 사용하면 의존성을 정확하게 찾아 빌드할 수 있습니다. 이 방법은 VSCode의 기본 빌드 태스크로도 설정되어 있습니다.

```bash
# TBB 및 googletest 경로를 지정하여 빌드
TBB_ROOT=$(brew --prefix tbb) && \
GTEST_ROOT=$(brew --prefix googletest) && \
mkdir -p build && \
cd build && \
cmake .. -DTBB_DIR=${TBB_ROOT}/lib/cmake/TBB -DCMAKE_PREFIX_PATH=${GTEST_ROOT} && \
make -j$(sysctl -n hw.ncpu)
```

#### 일반 빌드 방법 (Linux)

```bash
# 빌드 디렉토리 생성 및 빌드
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### 테스트 및 실행

```bash
# 메인 실행 파일 실행 (build 디렉토리 내부에서)
./mxrc

# 모든 테스트 실행 (build 디렉토리 내부에서)
./run_tests

# 특정 테스트 스위트 실행 (build 디렉토리 내부에서)
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
├── task/                            # Task Layer
│   ├── interfaces/
│   │   ├── ITask.h
│   │   ├── ITaskExecutor.h
│   │   └── ITriggerProvider.h
│   ├── core/
│   │   ├── TaskExecutor.{h,cpp}
│   │   ├── TaskRegistry.{h,cpp}
│   │   ├── PeriodicScheduler.{h,cpp}
│   │   ├── TriggerManager.{h,cpp}
│   │   └── TaskMonitor.{h,cpp}
│   └── dto/
│       ├── TaskDefinition.h
│       ├── TaskExecution.h
│       ├── TaskStatus.h
│       └── TaskExecutionMode.h
│
├── event/                          # Event Layer (NEW - Phase 019)
│   ├── interfaces/
│   │   ├── IEvent.h
│   │   └── IEventBus.h
│   ├── core/
│   │   ├── EventBus.{h,cpp}
│   │   └── SubscriptionManager.{h,cpp}
│   ├── dto/
│   │   ├── EventType.h
│   │   ├── EventBase.h
│   │   ├── ActionEvents.h
│   │   ├── SequenceEvents.h
│   │   ├── TaskEvents.h
│   │   └── DataStoreEvents.h
│   ├── util/
│   │   ├── EventFilter.h
│   │   ├── EventStats.h
│   │   ├── LockFreeQueue.h (SPSC)
│   │   └── MPSCLockFreeQueue.h (향후 최적화용)
│   └── adapters/
│       └── DataStoreEventAdapter.{h,cpp}
│
└── datastore/
    └── DataStore.{h,cpp}

tests/
├── unit/                           # 단위 테스트
│   ├── action/                     # 12 tests
│   ├── sequence/                   # 14 tests
│   ├── task/                       # 67 tests (Registry, Executor, Scheduler, Trigger, Monitor)
│   └── event/                      # 42+ tests (NEW)
│       ├── LockFreeQueue_test.cpp
│       ├── MPSCLockFreeQueue_test.cpp
│       ├── SubscriptionManager_test.cpp
│       ├── EventBus_test.cpp
│       └── DataStoreEventAdapter_test.cpp
└── integration/                    # 통합 테스트
    ├── action_integration_test.cpp
    ├── sequence_integration_test.cpp
    └── event/
        └── event_flow_test.cpp
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
- 개별 Action 실행 (동기/비동기)
- 타임아웃 관리 및 실시간 모니터링
- 실행 결과 수집
- 에러 처리
- 안전한 종료 및 리소스 정리
  - 소멸자에서 실행 중인 액션 자동 취소
  - 타임아웃 모니터링 스레드 안전 종료
  - `clearCompletedActions()`: 완료된 액션 상태 정리

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
- 안전한 종료 및 리소스 정리
  - 소멸자에서 실행 중인 시퀀스 자동 취소
  - `clearCompletedSequences()`: 완료된 시퀀스 상태 정리

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
- 안전한 종료 및 리소스 정리
  - `clearCompletedTasks()`: 완료된 태스크 상태 정리
- **테스트**: 19개 단위 테스트 통과

#### Task 실행 모드
```cpp
enum class TaskExecutionMode {
    ONCE,      // 단일 실행 (Phase 3B-1 완료)
    PERIODIC,  // 주기적 실행 (Phase 3B-2)
    TRIGGERED  // 트리거 기반 실행 (Phase 3B-3)
};
```

### Event Layer (Phase 019 - Phase 1-4 완료)

**목표**: 실시간 실행 상태 모니터링 및 DataStore 연동

#### EventBus
- 비동기 이벤트 처리 시스템 (SPSC Lock-Free Queue + Mutex)
- publish/subscribe/unsubscribe 기능
- 타입 기반 및 predicate 기반 필터링
- 구독자 예외 격리
- 큐 오버플로우 처리 (드롭 정책)
- **테스트**: 14개 단위 테스트 통과
- **동시성**: Mutex로 보호된 multi-producer 지원

#### SubscriptionManager
- 구독 등록/해제 관리
- 이벤트 타입별 구독자 조회
- 스레드 안전성 보장 (mutex)
- **테스트**: 5개 단위 테스트 통과

#### Lock-Free Queue
- SPSC(Single-Producer Single-Consumer) 패턴
- Ring buffer 기반 고성능 큐
- 10,000+ ops/sec 처리량
- **테스트**: 8개 단위 테스트 (성능 벤치마크 포함)

#### DataStoreEventAdapter
- DataStore ↔ EventBus 양방향 연동
- DataStore 변경 → EventBus 이벤트 발행
- EventBus 이벤트 → DataStore 자동 저장
- 순환 업데이트 방지 메커니즘
- **테스트**: 16+ 단위 테스트

#### 이벤트 타입
```cpp
namespace mxrc::core::event {
    // Action 이벤트
    - ActionStarted, ActionCompleted, ActionFailed, ActionCancelled, ActionTimeout

    // Sequence 이벤트
    - SequenceStarted, SequenceStepStarted, SequenceStepCompleted,
      SequenceCompleted, SequenceFailed, SequenceCancelled,
      SequencePaused, SequenceResumed, SequenceProgressUpdated

    // Task 이벤트
    - TaskStarted, TaskCompleted, TaskFailed, TaskCancelled,
      TaskScheduled, TaskProgressUpdated

    // DataStore 이벤트
    - DataStoreValueChanged
}
```

#### 이벤트 시스템 사용 예시
```cpp
// 1. EventBus 생성 및 시작
auto eventBus = std::make_shared<EventBus>(10000);  // 큐 용량 10,000
eventBus->start();

// 2. 이벤트 구독
auto subId = eventBus->subscribe(
    [](auto e) { return e->getType() == EventType::ACTION_COMPLETED; },
    [](std::shared_ptr<IEvent> event) {
        auto actionEvent = std::static_pointer_cast<ActionCompletedEvent>(event);
        spdlog::info("Action {} completed in {}ms",
                     actionEvent->actionId, actionEvent->duration);
    });

// 3. Executor에 EventBus 주입
auto executor = std::make_shared<ActionExecutor>(eventBus);

// 4. Action 실행 시 자동으로 이벤트 발행
auto action = std::make_shared<DelayAction>("test", 100);
executor->executeAsync(action, context);  // → ActionStarted 이벤트 발행
executor->waitForCompletion("test");       // → ActionCompleted 이벤트 발행

// 5. 구독 해제
eventBus->unsubscribe(subId);

// 6. EventBus 정지
eventBus->stop();
```

**중요 노트**:
- EventBus는 선택적 의존성: `eventBus=nullptr`이면 이벤트 발행 안 함
- publish는 논블로킹: 큐가 가득 차면 이벤트 드롭 (통계에 기록)
- 동시성 안전성: 여러 스레드에서 publish 가능 (mutex로 보호)
- **알려진 이슈**: `TROUBLESHOOTING.md` 참조 (SPSC→Mutex 전환 이력)

## 코드 작성 가이드

### 주석 작성 규칙

**기본 원칙**: 모든 주석은 한글로 작성하되, 기술 용어는 영어를 허용합니다.

**허용되는 기술 용어 예시**:
- 자료구조: `concurrent_hash_map`, `mutex`, `shared_ptr`, `weak_ptr`, `vector`, `map`
- 디자인 패턴: `Singleton`, `Observer`, `Factory`, `RAII`
- 시스템 개념: `thread-safe`, `lock-free`, `accessor`, `dangling pointer`
- 알고리즘: `hash`, `binary search`, `queue`

**올바른 주석 예시**:
```cpp
// ✅ concurrent_hash_map으로 고성능 스레드 안전 데이터 접근
tbb::concurrent_hash_map<std::string, SharedData> data_map_;

// ✅ weak_ptr 사용으로 dangling pointer 방지
std::vector<std::weak_ptr<Observer>> subscribers_;

// ✅ RAII 패턴으로 안전한 리소스 관리
std::lock_guard<std::mutex> lock(mutex_);
```

**잘못된 주석 예시**:
```cpp
// ❌ High-performance thread-safe data access using concurrent_hash_map
// (완전히 영어로 작성)

// ❌ 동시성 해시 맵을 사용하여 고성능 실을 안전한 데이터 접근
// (기술 용어를 억지로 번역)
```

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

namespace mxrc::core::event {
    // Event 계층 코드
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

## Git 커밋 메시지 가이드

### 기본 원칙

**중요**: 모든 커밋 메시지는 **한글**로 작성합니다.

### 커밋 메시지 형식

```
<타입>(<범위>): <제목>

<본문>

<푸터>
```

#### 타입 (Type)
- `feat`: 새로운 기능 추가
- `fix`: 버그 수정
- `refactor`: 코드 리팩토링 (기능 변경 없음)
- `docs`: 문서 수정
- `test`: 테스트 코드 추가/수정
- `chore`: 빌드, 설정 파일 수정
- `style`: 코드 포맷팅, 세미콜론 누락 등
- `perf`: 성능 개선

#### 범위 (Scope)
변경된 모듈이나 컴포넌트 (예: action, sequence, task, event, datastore)

#### 제목 (Subject)
- 50자 이내로 작성
- 명령형으로 작성 ("수정함" ❌, "수정" ✅)
- 마침표 없음
- 한글로 작성

#### 본문 (Body)
- 72자마다 줄바꿈
- **무엇을, 왜** 변경했는지 설명
- "어떻게"는 코드가 설명하므로 생략 가능
- 한글로 작성

#### 푸터 (Footer)
- 관련 이슈 번호 (선택사항)
- 예: `관련 이슈: #003`

### 금지 사항

**❌ 절대 하지 말 것:**
1. **AI/Claude가 작성했다는 언급 금지**
   - "Claude가 검토함", "AI가 작성함" 등의 표현 사용 금지
   - "🤖 Generated with Claude Code" 같은 푸터 사용 금지

2. **영어 커밋 메시지 금지**
   - 모든 커밋 메시지는 한글로 작성
   - 코드 예시나 기술 용어는 예외

### 올바른 예시

```
fix(action): ActionExecutor 소멸자 뮤텍스 데드락 해결

문제 상황:
- unlock/lock 패턴으로 인한 데드락 발생
- state 참조가 무효화되어 크래시 가능성

해결 방법:
- RAII 패턴으로 스레드를 먼저 수집
- 락 없이 안전하게 join 수행

테스트:
- ActionExecutor 기본 테스트 통과
- 소멸자 안정성 테스트 통과

관련 이슈: #003
```

### 잘못된 예시

```
❌ fix(action): Fix ActionExecutor destructor mutex deadlock
   (영어 사용 금지)

❌ fix(action): ActionExecutor 소멸자 수정

   Claude Code가 검토하고 수정함
   🤖 Generated with Claude Code
   (AI 언급 금지)

❌ fix: 버그 수정
   (범위 누락, 제목 불명확)
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
- **Action Layer**: 12 tests (기본 기능 + 종료 안정성)
- **Sequence Layer**: 14 tests (기본 기능 + 종료 안정성)
- **Task Layer**: 67 tests (Registry 12 + Executor 19 + Scheduler 9 + Trigger 12 + Monitor 15)
- **Event Layer**: 42+ tests (NEW - Phase 019)
  - LockFreeQueue: 8 tests (성능 벤치마크 포함)
  - MPSCLockFreeQueue: 3 tests (향후 최적화용)
  - SubscriptionManager: 5 tests
  - EventBus: 14 tests
  - DataStoreEventAdapter: 16+ tests
  - 통합 테스트: 5+ tests (event_flow_test.cpp)
- **통합 테스트**: 4 tests (action, sequence)
- **팩토리/레지스트리**: 18 tests
- **전체**: 157+ tests

#### 종료 안정성 테스트
- ActionExecutor: 소멸자, 상태 정리, 타임아웃 스레드 관리, 동시성, 메모리 누수 방지
- SequenceEngine: 소멸자, 상태 정리, 동시성, 메모리 누수 방지
- TaskExecutor: 상태 정리, 실패/취소 처리, 메모리 누수 방지
- EventBus: 디스패치 스레드 안전 종료, 남은 이벤트 처리

## 📝 메모리 관련 테스트 필수 사항

### 1. 객체 생명주기 및 포인터 유효성 검증
*   **NULL 포인터 접근 방지**: 모든 포인터 변수 사용 전에 NULL 검사(`if (ptr != nullptr)`)를 철저히 수행해야 합니다.
*   **댕글링 포인터(Dangling Pointer) 방지**: 객체가 파괴된 후에도 해당 메모리 주소를 가리키는 포인터가 남아있지 않도록 `std::shared_ptr`이나 `std::weak_ptr` 같은 스마트 포인터를 사용하여 객체 생명주기를 관리해야 합니다.
*   **초기화 보장**: 클래스 멤버 변수 중 포인터는 반드시 생성자에서 `nullptr` 또는 유효한 객체 주소로 초기화되어야 합니다.

### 2. 동시성 및 스레드 안전성 테스트
*   **경합 조건(Race Condition) 검사**: 멀티스레드 환경에서 공유 자원에 대한 읽기/쓰기 접근이 동시에 발생하지 않도록 락 메커니즘(`std::mutex`, TBB 동시성 컨테이너 등)이 올바르게 적용되었는지 테스트해야 합니다.
*   **락 효율성 및 데드락 방지**: 락의 범위가 최소화되었는지 확인하고, 여러 스레드가 서로의 락 해제를 기다리는 **교착 상태(Deadlock)**가 발생하지 않도록 테스트해야 합니다.
*   **스레드 세이프티(Thread Safety) 보장**: `TBB::tbb`와 같이 동시성 라이브러리를 사용할 경우, 해당 라이브러리의 함수가 스레드 안전하게 사용되고 있는지 확인해야 합니다.

### 3. 메모리 할당 및 누수(Leak) 검사

프로그램이 종료되거나 특정 기능이 완료된 후, 할당된 메모리가 올바르게 해제되었는지 전문 도구를 사용하여 확인해야 합니다.

*   **힙 오염(Heap Corruption) 방지**: `new`/`delete` 또는 `malloc`/`free` 쌍이 일치하는지, 배열 할당/해제 시 `new[]`/`delete[]`가 올바르게 사용되었는지 검증합니다.

*   **Valgrind (Ubuntu)**:
    *   **설치**: `sudo apt-get install valgrind`
    *   **사용법**: Valgrind는 메모리 누수, 유효하지 않은 메모리 접근 등 다양한 오류를 동적으로 분석합니다.
    ```bash
    # Valgrind로 테스트 실행
    valgrind --leak-check=full --show-leak-kinds=all ./build/run_tests --gtest_filter=<YourTest>
    ```

*   **Address Sanitizer (ASan)**:
    *   **사용법**: 컴파일 시 `-fsanitize=address` 플래그를 추가하여 빌드합니다. ASan은 런타임에 메모리 오류를 매우 빠르게 감지합니다.
    *   **CMake 설정 (`CMakeLists.txt`):**
        ```cmake
        # 디버그 빌드 시 Address Sanitizer 활성화
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_compile_options(mxrc PRIVATE -fsanitize=address)
            target_link_libraries(mxrc PRIVATE -fsanitize=address)
        endif()
        ```
    *   이후 평소처럼 테스트를 실행하면, 메모리 오류 발생 시 상세한 리포트가 출력됩니다.

### 4. 에지 케이스 (Edge Case) 테스트
*   **동시 초기화/파괴**: 멀티스레드가 동시성 자료구조를 초기화하거나 파괴하려고 시도할 때 프로그램이 충돌하지 않는지 테스트해야 합니다.
*   **경계 조건**: `0` 또는 시스템이 허용하는 `max_allowed_parallelism` 등의 경계 값에서 TBB가 올바르게 작동하는지 확인해야 합니다.

## 설계 원칙

### 1. RAII (Resource Acquisition Is Initialization)
- 모든 리소스는 생성자에서 할당, 소멸자에서 해제
- `std::shared_ptr`, `std::unique_ptr` 사용
- 수동 메모리 관리 금지
- **특히 중요**: 소멸자에서 완전한 정리 필수 (멀티스레드 환경 고려)

### 2. 인터페이스 기반 설계
- 모든 확장 지점에 인터페이스 사용
- 의존성 주입 (Dependency Injection) 선호
- 느슨한 결합 (Loose Coupling)
- **권장**: Singleton 패턴보다 shared_ptr 기반 생성 메서드 사용

### 3. Singleton 패턴 지양 및 shared_ptr 기반 DI 채택

#### ❌ Singleton 패턴의 문제점

```cpp
// 피해야 할 패턴
class DataStore {
    static DataStore* instance_ = nullptr;  // ❌ 메모리 누수

    static DataStore& getInstance() {
        if (instance_ == nullptr) {
            instance_ = new DataStore();    // ❌ 해제 불가능
        }
        return *instance_;
    }
};
```

**문제점**:
- 메모리 누수 (동적 할당 후 해제 안 함)
- 테스트 격리 어려움
- 전역 상태로 인한 뮤텍스 병목
- 의존성이 명시적이지 않음

#### ✅ 권장 패턴: shared_ptr 기반 static 팩토리

```cpp
// 권장 패턴
class DataStore : public std::enable_shared_from_this<DataStore> {
public:
    static std::shared_ptr<DataStore> create() {
        static std::shared_ptr<DataStore> instance =
            std::make_shared<DataStore>();  // ✓ 안전한 할당
        return instance;                      // ✓ 자동 해제
    }

    DataStore(const DataStore&) = delete;     // ✓ 복사 방지
    DataStore& operator=(const DataStore&) = delete;

private:
    DataStore() = default;
};

// 사용
auto ds = DataStore::create();  // shared_ptr로 안전하게 관리
```

**장점**:
- 메모리 자동 관리 (shared_ptr)
- Singleton 특성 유지
- 테스트 친화적
- 의존성 명시적 (DI 용이)

### 4. Observer 패턴에서 weak_ptr 사용 필수

#### ❌ 위험한 패턴 (이슈 #003 원인)

```cpp
class MapNotifier : public Notifier {
private:
    std::vector<Observer*> subscribers_;  // ❌ dangling pointer 위험

    void notify(const SharedData& data) override {
        for (Observer* obs : subscribers_) {
            obs->onDataChanged(data);      // ❌ NULL 포인터 가능
        }
    }
};
```

**문제점**:
- Observer 파괴 후에도 raw pointer 남음
- 멀티스레드 환경에서 경쟁 상태 발생
- NULL 포인터 역참조 세그멘테이션 폴트

#### ✅ 권장 패턴: weak_ptr 기반

```cpp
class MapNotifier : public Notifier {
private:
    std::vector<std::weak_ptr<Observer>> subscribers_;  // ✓ 안전함
    std::mutex mutex_;

    void notify(const SharedData& data) override {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto it = subscribers_.begin(); it != subscribers_.end(); ) {
            if (auto obs = it->lock()) {       // ✓ 자동 NULL 체크
                obs->onDataChanged(data);
                ++it;
            } else {
                it = subscribers_.erase(it);   // ✓ 자동 정리
            }
        }
    }
};

// Observer 등록 (shared_ptr 필수)
notifier->subscribe(std::make_shared<MyObserver>());  // ✓ 안전함
```

**장점**:
- NULL 포인터 자동 감지
- 파괴된 객체 자동 정리
- 멀티스레드 안전
- 메모리 누수 방지

### 5. 동시성 설계: 전역 락 ❌, 세분화된 락 ✅

#### ❌ 전역 락의 문제 (성능 병목)

```cpp
class DataStore {
    std::map<std::string, SharedData> data_map_;
    static std::mutex mutex_;  // ❌ 모든 연산 직렬화

    void set(...) {
        std::lock_guard<std::mutex> lock(mutex_);  // ❌ 블로킹
        data_map_[id] = data;
    }
};
```

**문제점**:
- 모든 스레드가 단일 뮤텍스 대기
- EventBus 디스패치 스레드 블로킹
- 연쇄 성능 저하

#### ✅ 권장 패턴: 동시성 해시 맵 (oneTBB)

```cpp
#include <tbb/concurrent_hash_map.h>

class DataStore {
private:
    tbb::concurrent_hash_map<std::string, SharedData> data_map_;
    // ✓ 내부 세분화된 락, 전역 lock_guard 불필요

    void set(...) {
        typename tbb::concurrent_hash_map<...>::accessor acc;
        data_map_.insert(acc, id);
        acc->second = data;
        // ✓ 자동으로 안전한 동시성 처리
    }
};
```

**개선 효과**:
- 10배 성능 향상 (1000ms → 100ms)
- 이벤트 처리 지연 5배 감소
- 메모리 누수 제거
- NULL 포인터 위험 완전 제거

### 6. 단계적 구현
- Phase 1: Action Layer → Phase 2: Sequence Layer → Phase 3: Task Layer
- 각 계층은 이전 계층 위에 구축
- 독립적 테스트 가능

### 7. 스레드 안전성
- Registry 클래스들은 `std::mutex`로 보호
- 상태 접근은 동기화됨
- Logger는 thread-safe
- **새로운 원칙**: 전역 락 대신 세분화된 락 또는 concurrent 자료구조 사용

### 8. 모듈의 독립성과 책임(SRP)
- 더 성숙한 아키텍처를 위한 끊임 없는 제안과 발전

### 9. 메모리 안전성 검증

프로젝트는 다음 도구로 메모리 안전성을 보장합니다:

- **AddressSanitizer**: 컴파일 시 `-fsanitize=address` 적용
- **Valgrind**: 메모리 누수 탐지
  ```bash
  valgrind --leak-check=full --show-leak-kinds=all ./run_tests
  ```
- **스마트 포인터 필수**: 모든 동적 할당은 `shared_ptr` 또는 `unique_ptr`로 관리
- **weak_ptr 활용**: Observer 패턴이나 순환 참조 방지

## 현재 진행 상황

### 완료된 Phase
- ✅ **Phase 017**: Action & Sequence & Task Layer 완료
  - Action Layer (12 tests)
  - Sequence Layer (14 tests)
  - Task Layer (67 tests)

- ✅ **Phase 019**: Event-Enhanced Hybrid Architecture (진행 중)
  - **Phase 1**: 프로젝트 Setup ✅
  - **Phase 2**: 기반 인프라 구축 (EventBus 핵심) ✅
    - Lock-Free Queue (SPSC) 구현
    - SubscriptionManager 구현
    - EventBus 구현 (14 tests)
  - **Phase 3**: User Story 1 - 실시간 실행 상태 모니터링 ✅
    - Action/Sequence/Task 이벤트 DTO 정의
    - Executor들에 EventBus 통합
    - 통합 테스트 (event_flow_test.cpp)
  - **Phase 4**: User Story 2 - DataStore와 EventBus 연동 ✅
    - DataStoreEventAdapter 구현
    - 양방향 연동 (DataStore ↔ EventBus)
    - 순환 업데이트 방지
    - 단위 테스트 16+ tests

### 다음 단계 (Phase 019 계속)
- **Phase 5**: User Story 3 - 확장 가능한 모니터링 컴포넌트 (P2)
  - ExecutionTimeCollector 예제
  - StateTransitionLogger 예제
  - 외부 모니터링 시스템 통합 예제

- **Phase 6**: Polish & 최적화
  - MPSC Lock-Free Queue 완성 (현재 SPSC + Mutex)
  - 성능 벤치마크 (오버헤드 <5%, 지연 <10ms)
  - 메모리 프로파일링
  - API 문서화

### 알려진 이슈
- ⚠️ **SPSC→Mutex 전환**: `TROUBLESHOOTING.md` 이슈 #001 참조
  - Multi-producer 환경에서 SPSC 큐 사용으로 인한 크래시 해결
  - 향후 MPSC Lock-Free Queue로 최적화 예정

## 문제 해결

### 크리티컬 이슈 (크래시, 세그멘테이션 폴트) 대응
프로그램 실행 중 크래시 또는 세그멘테이션 폴트와 같은 크리티컬 이슈가 발생하는 경우, 다음 절차를 따릅니다.

1.  **`lldb`를 이용한 버그 식별**:
    *   디버거(`lldb`)를 사용하여 크래시가 발생한 지점의 스택 트레이스, 레지스터 상태, 메모리 정보를 수집합니다.
    *   자세한 방법은 `docs/debugging_with_lldb.md` 문서를 참고하세요.

2.  **이슈 파일 작성**:
    *   `/issue` 디렉토리에 새로운 이슈 파일을 생성합니다.
    *   이슈 파일은 `docs/templete/issue.md` 템플릿 양식을 따라 작성합니다.

3.  **로그 첨부**:
    *   작성된 이슈 파일에 `lldb`를 통해 수집한 로그, 백트레이스, 분석 내용 등을 상세히 첨부합니다.

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
   - `specs/<기능번호>-<기능명>/spec.md`
   - `specs/<기능번호>-<기능명>/architecture.md`
   - `specs/<기능번호>-<기능명>/tasks.md`

## 참고 자료

- 전체 아키텍처: 프로젝트 루트의 `architecture.md`
- 구현 계획: `specs/<기능번호>-<기능명>/plan.md`
- Task 목록: `specs/<기능번호>-<기능명>/tasks.md`
- README: 프로젝트 루트의 `README.md`
