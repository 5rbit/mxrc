# MXRC 개발 가이드라인 (Gemini AI)

마지막 업데이트: 2025-11-15

## 프로젝트 개요

MXRC는 범용 로봇 제어 컨트롤러로, C++20으로 개발되며 계층적 아키텍처를 따릅니다.

## 활성 기술 스택

### 언어 및 빌드
- **언어**: C++20
- **빌드 시스템**: CMake 3.16+
- **컴파일러**: GCC 11+ or Clang 14+

### 라이브러리
- **로깅**: spdlog
- **테스트**: Google Test (GTest)
- **메모리 관리**: C++ STL (shared_ptr, unique_ptr)

### 개발 환경
- **OS**: Ubuntu 24.04 LTS PREEMPT_RT
- **IDE**: 자유 (VS Code, CLion 등)

## 프로젝트 구조

```text
mxrc/
├── src/core/
│   ├── action/              # Action Layer (기본 동작)
│   │   ├── interfaces/      # IAction, IActionFactory
│   │   ├── core/            # ActionExecutor, ActionFactory, ActionRegistry
│   │   ├── dto/             # ActionStatus, ActionDefinition
│   │   └── impl/            # DelayAction, MoveAction
│   │
│   ├── sequence/            # Sequence Layer (동작 조합)
│   │   ├── core/            # SequenceEngine, SequenceRegistry
│   │   │                    # ConditionEvaluator, RetryHandler
│   │   └── dto/             # SequenceDefinition, ConditionalBranch
│   │
│   └── task/                # Task Layer (실행 관리)
│       ├── interfaces/      # ITask, ITaskExecutor
│       ├── core/            # TaskExecutor, TaskRegistry
│       │                    # PeriodicScheduler, TriggerManager
│       └── dto/             # TaskDefinition, TaskExecution
│
├── tests/
│   ├── unit/                # 단위 테스트 (112 tests)
│   │   ├── action/          # 26 tests
│   │   ├── sequence/        # 33 tests
│   │   └── task/            # 53 tests
│   └── integration/         # 통합 테스트
│
├── specs/                   # 사양 문서
│   └── 017-action-sequence-orchestration/
│       ├── spec.md          # 기능 사양
│       ├── architecture.md  # 아키텍처 설계
│       ├── plan.md          # 구현 계획
│       └── tasks.md         # Task 목록
│
├── CMakeLists.txt
├── README.md               # 사용자 가이드
└── CLAUDE.md              # 상세 개발 가이드
```

## 빌드 및 테스트 명령어

### 빌드

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

### 테스트

```bash
# build 디렉토리로 이동
cd build

# 전체 테스트 실행
./run_tests

# 계층별 테스트
./run_tests --gtest_filter=ActionExecutor*
./run_tests --gtest_filter=SequenceEngine*
./run_tests --gtest_filter=TaskExecutor*

# 간략한 출력
./run_tests --gtest_brief=1
```

### 실행

```bash
# build 디렉토리로 이동
cd build

# 메인 프로그램 실행
./mxrc
```

## 코드 스타일 (C++)

### 네임스페이스
```cpp
// 계층별 네임스페이스
namespace mxrc::core::action { }
namespace mxrc::core::sequence { }
namespace mxrc::core::task { }
```

### 네이밍 규칙
- **클래스**: PascalCase (예: `ActionExecutor`, `SequenceEngine`)
- **함수/메서드**: camelCase (예: `execute()`, `getStatus()`)
- **변수**: snake_case (예: `action_id_`, `execution_time_`)
- **상수**: UPPER_SNAKE_CASE (예: `MAX_RETRIES`)
- **인터페이스**: I 접두사 (예: `IAction`, `ITask`)

### 헤더 가드
```cpp
#ifndef MXRC_CORE_ACTION_ACTION_EXECUTOR_H
#define MXRC_CORE_ACTION_ACTION_EXECUTOR_H
// ...
#endif
```

### RAII 원칙
```cpp
// Good: 스마트 포인터 사용
auto action = std::make_shared<DelayAction>(id, duration);

// Bad: 수동 메모리 관리
// DelayAction* action = new DelayAction(id, duration);
```

### 인터페이스 정의
```cpp
class IAction {
public:
    virtual ~IAction() = default;
    virtual void execute(ExecutionContext& context) = 0;
    virtual void cancel() = 0;
    virtual ActionStatus getStatus() const = 0;
};
```

## 아키텍처 개념

### 3계층 구조
```
Task Layer      → 실행 모드 관리 (ONCE, PERIODIC, TRIGGERED)
    ↓
Sequence Layer  → Action 조합 (순차, 조건부, 병렬)
    ↓
Action Layer    → 기본 동작 실행
```

### 주요 컴포넌트

#### Action Layer
- **ActionExecutor**: Action 실행 및 결과 수집
- **ActionFactory**: Action 인스턴스 생성 (플러그인 방식)
- **ActionRegistry**: Action 타입 등록 및 관리

#### Sequence Layer
- **SequenceEngine**: 시퀀스 실행 조율 (순차/조건/병렬)
- **SequenceRegistry**: 시퀀스 정의 저장 및 조회
- **ConditionEvaluator**: 조건식 평가 (==, !=, <, >, AND, OR)
- **RetryHandler**: 재시도 정책 관리

#### Task Layer
- **TaskExecutor**: Task 실행 및 상태 관리
- **TaskRegistry**: Task 정의 관리
- **PeriodicScheduler**: 주기적 실행 (개발 중)
- **TriggerManager**: 이벤트 트리거 (개발 중)

## 테스트 작성 가이드

### 테스트 구조
```cpp
#include "gtest/gtest.h"

namespace mxrc::core::action {

class ComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 초기화
    }

    void TearDown() override {
        // 정리
    }
};

TEST_F(ComponentTest, TestScenario) {
    // Given - 준비
    // When - 실행
    // Then - 검증
    EXPECT_EQ(expected, actual);
}

}
```

### 테스트 명명
- 파일: `ComponentName_test.cpp`
- 클래스: `ComponentNameTest`
- 케이스: 시나리오 설명 (예: `ExecuteActionSuccessfully`)

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

---

## ⚠️ 반-패턴 (Anti-Patterns) 및 피해야 할 설계

### 1. ❌ Singleton 패턴 사용 (메모리 누수 위험)

**문제 있는 코드:**
```cpp
class DataStore {
private:
    static DataStore* instance_ = nullptr;  // ❌ 동적 할당
    static std::mutex mutex_;

public:
    static DataStore& getInstance() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (instance_ == nullptr) {
            instance_ = new DataStore();    // ❌ 누수! delete 없음
        }
        return *instance_;
    }
};

// 테스트에서도 문제
auto& ds = DataStore::getInstance();  // ❌ 상태 격리 불가
```

**문제점:**
- 메모리 누수 (동적 할당 후 해제 안 함)
- 테스트 상태 격리 불가능
- 전역 뮤텍스로 인한 성능 병목
- 의존성이 숨겨짐

**✅ 올바른 방법:**
```cpp
class DataStore : public std::enable_shared_from_this<DataStore> {
public:
    static std::shared_ptr<DataStore> create() {
        static std::shared_ptr<DataStore> instance =
            std::make_shared<DataStore>();  // ✓ 안전한 할당
        return instance;
    }

private:
    DataStore() = default;
};

// 사용
auto ds = DataStore::create();  // ✓ shared_ptr로 관리
```

---

### 2. ❌ raw pointer 기반 Observer 패턴 (이슈 #003의 원인)

**문제 있는 코드:**
```cpp
class Notifier {
private:
    std::vector<Observer*> subscribers_;  // ❌ dangling pointer 위험

    void notify(const SharedData& data) override {
        for (Observer* obs : subscribers_) {
            obs->onDataChanged(data);      // ❌ NULL 포인터 접근 가능
        }
    }
};

// 사용
class DataStore {
    void subscribe(Observer* observer) {  // ❌ raw pointer
        // observer가 파괴되어도 이 포인터가 남음
    }
};
```

**문제점:**
- Observer 파괴 후 dangling pointer 남음
- 멀티스레드 환경에서 경쟁 상태
- NULL 포인터 역참조 세그멘테이션 폴트
- 생명주기 관리 불명확

**✅ 올바른 방법:**
```cpp
class Notifier {
private:
    std::vector<std::weak_ptr<Observer>> subscribers_;  // ✓ 안전함

    void notify(const SharedData& data) override {
        std::lock_guard<std::mutex> lock(mutex_);

        // 파괴된 observer 자동 정리
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

// 사용 (shared_ptr 필수!)
std::shared_ptr<Observer> obs = std::make_shared<MyObserver>();
notifier->subscribe(obs);  // ✓ weak_ptr로 변환됨
```

---

### 3. ❌ 전역 뮤텍스 사용 (성능 병목, 이슈 #002와 연관)

**문제 있는 코드:**
```cpp
class DataStore {
private:
    std::map<std::string, SharedData> data_map_;
    static std::mutex global_mutex_;  // ❌ 모든 연산 직렬화

public:
    void set(...) {
        std::lock_guard<std::mutex> lock(global_mutex_);  // ❌ 블로킹
        data_map_[id] = data;
        notify();  // ❌ 이벤트 처리도 블로킹됨
    }

    SharedData get(...) {
        std::lock_guard<std::mutex> lock(global_mutex_);  // ❌ 블로킹
        return data_map_[id];
    }
};

// 결과: EventBus 디스패치 스레드가 블로킹되면 이벤트 처리 전체 중단
```

**문제점:**
- 모든 스레드가 단일 뮤텍스 대기
- 10배 이상 성능 저하
- 이벤트 처리 지연 (연쇄 블로킹)
- 확장성 최악

**✅ 올바른 방법:**
```cpp
#include <tbb/concurrent_hash_map.h>

class DataStore {
private:
    // ✓ 내부 세분화된 락 (fine-grained locking)
    tbb::concurrent_hash_map<std::string, SharedData> data_map_;

public:
    void set(...) {
        // ✓ 락이 필요 없음! concurrent_hash_map이 내부 관리
        typename tbb::concurrent_hash_map<...>::accessor acc;
        data_map_.insert(acc, id);
        acc->second = data;

        // 별도 락으로 notify 처리
        notifySubscribers(data);  // 논블로킹
    }
};

// 개선 효과: 1000ms → 100ms (10배 향상)
```

---

### 4. ❌ new/delete 직접 사용 (메모리 관리 실패)

**문제 있는 코드:**
```cpp
// 1. 메모리 누수
class Factory {
public:
    IAction* createAction(...) {
        return new DelayAction(...);  // ❌ 호출자가 delete 해야 함
                                      // ❌ 하지만 누군가 깜빡할 수 있음
    }
};

// 2. 불일치하는 할당/해제
void process() {
    int* arr = new int[100];      // ❌ 배열
    delete arr;                   // ❌ delete (new[]가 아님) → 힙 손상!
}

// 3. 예외 안전성 문제
void setup() {
    MyObject* obj = new MyObject();
    doSomething();  // ❌ 예외 발생 → delete 호출 안 됨
    delete obj;
}
```

**✅ 올바른 방법:**
```cpp
// 1. shared_ptr/unique_ptr 사용
class Factory {
public:
    std::shared_ptr<IAction> createAction(...) {
        return std::make_shared<DelayAction>(...);  // ✓ 자동 해제
    }
};

// 2. 배열도 스마트 포인터
void process() {
    std::vector<int> arr(100);  // ✓ RAII 방식
    // 또는
    std::unique_ptr<int[]> arr(new int[100]);  // ✓ 맞는 할당/해제
}

// 3. 예외 안전성 보장
void setup() {
    auto obj = std::make_unique<MyObject>();
    doSomething();  // ✓ 예외 발생해도 자동 정리
    // delete 필요 없음
}
```

---

### 5. ❌ 동기화 없는 멀티스레드 접근

**문제 있는 코드:**
```cpp
class SharedData {
public:
    int value = 0;
    std::string name = "";
};

class Container {
private:
    SharedData data_;  // ❌ 동기화 없음

public:
    void update(int v) {
        data_.value = v;    // ❌ Thread #1
        data_.name = "..."; // ❌ Thread #2와 경쟁
    }

    int getValue() {
        return data_.value;  // ❌ 경쟁 상태
    }
};

// 결과: 예측 불가능한 버그, 데이터 손상
```

**✅ 올바른 방법:**
```cpp
class Container {
private:
    SharedData data_;
    std::mutex mutex_;  // ✓ 동기화 추가

public:
    void update(int v) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.value = v;
        data_.name = "...";  // ✓ 원자적 업데이트
    }

    int getValue() {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_.value;  // ✓ 동기화됨
    }
};

// 또는 TBB 사용
#include <tbb/concurrent_hash_map.h>
tbb::concurrent_hash_map<std::string, SharedData> data_;
// ✓ 내부 동기화 제공
```

---

### 6. ❌ 순환 참조 (메모리 누수)

**문제 있는 코드:**
```cpp
class Node : public std::enable_shared_from_this<Node> {
public:
    std::shared_ptr<Node> next;  // ✓ 정상
    std::shared_ptr<Node> prev;  // ❌ 순환 참조!
};

// 사용
auto node1 = std::make_shared<Node>();
auto node2 = std::make_shared<Node>();
node1->next = node2;
node2->prev = node1;  // ❌ 순환 참조 → 메모리 누수!
// node1과 node2의 참조 계수가 절대 0이 될 수 없음
```

**✅ 올바른 방법:**
```cpp
class Node {
public:
    std::shared_ptr<Node> next;      // ✓ 앞으로만 가능
    std::weak_ptr<Node> prev;        // ✓ weak_ptr로 역참조
};

// 사용
auto node1 = std::make_shared<Node>();
auto node2 = std::make_shared<Node>();
node1->next = node2;
node2->prev = node1;  // ✓ 순환 참조 방지

// prev 사용
if (auto p = node2->prev.lock()) {
    // p가 유효한 경우만 사용
}
```

---

### 7. ❌ 예외 발생 후 리소스 정리 누락

**문제 있는 코드:**
```cpp
void processData() {
    EventBus* eventBus = new EventBus();

    try {
        eventBus->start();
        doSomethingRisky();  // ❌ 예외 발생!
    } catch (...) {
        // ❌ eventBus->stop() 호출 안 됨
    }
    delete eventBus;  // ❌ 도달하지 않음 → 누수!
}
```

**✅ 올바른 방법:**
```cpp
void processData() {
    auto eventBus = std::make_shared<EventBus>();

    eventBus->start();
    try {
        doSomethingRisky();  // ✓ 예외 발생해도
    } catch (...) {
        // 자동으로 소멸자 호출
        throw;  // 예외 전파
    }
    // ✓ 자동 cleanup (RAII)
}

// 또는 RAII 래퍼
class EventBusGuard {
private:
    std::shared_ptr<EventBus> bus_;

public:
    EventBusGuard(std::shared_ptr<EventBus> b) : bus_(b) {
        bus_->start();  // ✓ 시작
    }

    ~EventBusGuard() {
        bus_->stop();   // ✓ 항상 정리 (예외 상황에서도)
    }
};
```

---

### 8. ❌ 테스트에서 Singleton 직접 사용 (상태 격리 불가)

**문제 있는 코드:**
```cpp
TEST_F(DataStoreTest, Test1) {
    DataStore& ds = DataStore::getInstance();  // ❌ 전역 상태
    ds.set("key", value, type);
}

TEST_F(DataStoreTest, Test2) {
    DataStore& ds = DataStore::getInstance();
    // ❌ Test1의 데이터가 남아있음 → 테스트 상호 간섭!
}
```

**✅ 올바른 방법:**
```cpp
class DataStoreTest : public ::testing::Test {
protected:
    std::shared_ptr<DataStore> ds_;

    void SetUp() override {
        ds_ = DataStore::create();  // ✓ 각 테스트마다 새 인스턴스
    }
};

TEST_F(DataStoreTest, Test1) {
    ds_->set("key", value, type);  // ✓ 격리됨
}

TEST_F(DataStoreTest, Test2) {
    // ✓ Test1의 데이터 없음
}
```

---

## 요약: 피해야 할 5가지

| 안티패턴 | 문제 | 권장 방법 |
|---------|------|---------|
| Singleton (new) | 메모리 누수 | shared_ptr::create() |
| raw pointer Observer | Dangling pointer | weak_ptr |
| 전역 뮤텍스 | 성능 병목 | TBB concurrent_hash_map |
| new/delete 직접 | 메모리 누수 | 스마트 포인터 |
| 예외 안전성 미흡 | 리소스 누수 | RAII 원칙 |

## 최근 변경 사항

### Phase 3B-1: Task Single Execution (2025-11-15)
**추가된 기능:**
- TaskRegistry 구현 (Task 정의 등록 및 조회)
- TaskExecutor ONCE 모드 구현 (단일 실행)
- TaskCoreExecutor 단위 테스트 14개 추가
- TaskRegistry 단위 테스트 12개 추가

**테스트 현황:**
- 전체: 112 tests (모두 통과 ✅)
- Action: 26 tests
- Sequence: 33 tests
- Task: 53 tests

### Phase 2A-2G: Sequence Layer (2025-11-14)
**추가된 기능:**
- SequenceEngine 구현 (순차/조건부/병렬 실행)
- ConditionEvaluator 구현 (조건식 평가)
- RetryHandler 구현 (재시도 정책)
- 시퀀스 템플릿 지원

**테스트 현황:**
- 33 tests 추가 (모두 통과)

### Phase 1: Action Layer (2025-11-13)
**추가된 기능:**
- IAction 인터페이스 정의
- ActionExecutor, ActionFactory, ActionRegistry 구현
- DelayAction, MoveAction 기본 구현
- ExecutionContext (Action 간 데이터 공유)

**테스트 현황:**
- 26 tests 추가 (모두 통과)

## 개발 프로세스

### 1. 사양 확인
```bash
# 관련 사양 문서 확인
specs/017-action-sequence-orchestration/spec.md
specs/017-action-sequence-orchestration/architecture.md
```

### 2. 테스트 작성 (TDD)
```bash
# 테스트 작성
tests/unit/<layer>/<Component>_test.cpp
```

### 3. 구현
```bash
# 헤더 및 구현 작성
src/core/<layer>/core/<Component>.h
src/core/<layer>/core/<Component>.cpp
```

### 4. 빌드 및 테스트
```bash
cd build
make -j$(nproc)
./run_tests --gtest_filter=<Component>*
```

### 5. 통합
```bash
# 전체 테스트 확인
./run_tests
```

## 다음 단계

### 020-refactor-datastore-locking 
- 
## 참고 문서

- **README.md**: 프로젝트 개요 및 사용 가이드
- **CLAUDE.md**: 상세 개발 가이드 및 API 문서
- **architecture.md**: 전체 사양 및 아키텍처

## 문제 해결

### 빌드 오류
```bash
# 빌드 디렉토리 삭제 후 재빌드
rm -rf build
mkdir build && cd build
cmake .. && make -j$(nproc)
```

### 테스트 실패
```bash
# 특정 테스트만 실행하여 디버깅
./run_tests --gtest_filter=<FailedTest>

# 상세 출력
./run_tests --gtest_verbose
```

### 의존성 문제
```bash
# Ubuntu에서 의존성 재설치
sudo apt-get update
sudo apt-get install --reinstall libspdlog-dev libgtest-dev
```

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

<!-- 수동 추가 시작 -->
<!-- 프로젝트별 추가 설정이나 노트를 여기에 작성 -->
<!-- 수동 추가 끝 -->
