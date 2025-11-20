# 실시간 시스템을 위한 Task Layer 아키텍처 전환 분석

## 문서 정보
- **작성일**: 2025-11-20
- **작성자**: Claude Code (Serena MCP)
- **목적**: MXRC 프로젝트의 RT/Non-RT 영역 분리 및 실시간 보장 아키텍처 설계 분석
- **대상**: Task, Action, Sequence Layer의 대규모 아키텍처 전환

---

## 1. 현재 아키텍처 분석

### 1.1 현재 Task Layer 구조

#### 주요 컴포넌트
```
TaskExecutor (src/core/task/core/TaskExecutor.h:26-119)
├── ActionFactory (동적 생성)
├── ActionExecutor (동기/비동기 실행)
├── SequenceEngine (시퀀스 조율)
└── EventBus (이벤트 발행)

PeriodicScheduler (src/core/task/core/PeriodicScheduler.h)
├── std::thread 기반 주기 실행
├── std::mutex 동기화
└── sleep_for() 기반 타이밍
```

#### 실행 모드
- **ONCE**: 단일 실행 (완료 ✅)
- **PERIODIC**: 주기적 실행 (std::thread + sleep)
- **TRIGGERED**: 이벤트 트리거 실행

### 1.2 실시간성 위배 요소

#### ❌ 치명적 문제점

1. **스레드 기반 설계**
   ```cpp
   // PeriodicScheduler.cpp:60-62
   newInfo->thread = std::make_unique<std::thread>(
       &PeriodicScheduler::runSchedule, this, newInfo.get()
   );
   ```
   - **문제**: 일반 std::thread는 우선순위 제어 불가
   - **영향**: 실시간 데드라인 보장 불가
   - **PREEMPT_RT**: 실시간 스케줄링 정책(SCHED_FIFO/RR) 미사용

2. **동적 메모리 할당**
   ```cpp
   // ActionExecutor.cpp:70-80
   ExecutionResult ActionExecutor::execute(...) {
       std::string actionId = executeAsync(action, context, timeout);
       // std::make_shared, std::unique_ptr 등 동적 할당
   }
   ```
   - **문제**: 런타임 메모리 할당으로 인한 비결정적 지연
   - **영향**: Worst-case Execution Time (WCET) 예측 불가

3. **뮤텍스 기반 동기화**
   ```cpp
   // TaskExecutor.h:99
   mutable std::mutex stateMutex_;
   std::map<std::string, TaskState> states_;
   ```
   - **문제**: Priority Inversion 발생 가능
   - **영향**: 고우선순위 태스크가 저우선순위 태스크에 블록될 수 있음
   - **해결책**: Priority Inheritance Protocol 필요

4. **sleep_for() 타이밍**
   ```cpp
   // PeriodicScheduler.cpp:170-172
   auto sleepTime = info->interval - elapsed;
   if (sleepTime.count() > 0 && info->running) {
       std::this_thread::sleep_for(sleepTime);
   }
   ```
   - **문제**: 정확한 주기 보장 불가 (drift 누적)
   - **영향**: Jitter 증가, 주기성 위배

5. **예외 처리**
   ```cpp
   // PeriodicScheduler.cpp:151-162
   try {
       info->callback(context);
   } catch (const std::exception& e) {
       Logger::get()->error(...);
   }
   ```
   - **문제**: 예외 처리는 비결정적 오버헤드
   - **실시간 원칙**: "No exceptions in RT code"

### 1.3 현재 아키텍처의 강점

✅ **유지해야 할 설계**

1. **계층적 분리**: Action → Sequence → Task
2. **인터페이스 기반**: 확장성 우수
3. **RAII 원칙**: 리소스 관리 명확
4. **이벤트 기반**: EventBus 통합
5. **테스트 가버리지**: 195개 단위 테스트

---

## 2. RT/Non-RT 영역 분리 설계

### 2.1 아키텍처 개요

```
┌─────────────────────────────────────────────────────────────┐
│                   Application Layer (Non-RT)                │
│  - Task 정의 및 등록                                         │
│  - 시퀀스 구성                                                │
│  - 로깅, 모니터링                                             │
│  - EventBus 구독                                             │
└─────────────────────────────────────────────────────────────┘
                            ↓ (Command Queue)
┌─────────────────────────────────────────────────────────────┐
│              RT/Non-RT Boundary (Lock-Free Queue)           │
│  - 명령 큐 (SPSC/MPSC)                                       │
│  - 상태 공유 메모리 (Double Buffering)                       │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                  Real-Time Executive (RT)                   │
│  - Cyclic Executive 루프                                     │
│  - 고정 주기 스케줄링 (1ms, 10ms, 100ms)                     │
│  - Action 실행 (사전 할당 메모리)                             │
│  - 상태 머신 실행                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 영역별 책임

#### Non-RT 영역 (일반 스레드, SCHED_OTHER)

**책임:**
- Task 정의 생성 및 검증
- 복잡한 시퀀스 구성 (조건부, 병렬)
- 로깅 및 디버깅
- 파일 I/O, 네트워크 통신
- EventBus 이벤트 발행/구독
- DataStore 백그라운드 정리

**허용되는 작업:**
- 동적 메모리 할당
- 뮤텍스/락 사용
- 예외 처리
- STL 컨테이너 사용
- 블로킹 I/O

**컴포넌트:**
- `TaskRegistry` (정의 관리)
- `SequenceRegistry` (시퀀스 관리)
- `ActionFactory` (Action 생성)
- `EventBus` (비동기 이벤트)
- `DataStore` (상태 영속화)
- `Logger` (비동기 로깅)

#### RT 영역 (실시간 스레드, SCHED_FIFO)

**책임:**
- 주기적 Action 실행
- 타이밍 보장 (< 1ms 지터)
- 결정적 실행 (WCET 보장)
- 센서 읽기, 액추에이터 제어
- 상태 머신 전이

**금지 사항:**
- ❌ 동적 메모리 할당 (malloc, new, std::make_shared)
- ❌ 뮤텍스/락 (Priority Inversion)
- ❌ 예외 throw/catch
- ❌ STL 동적 컨테이너 (std::map, std::vector growth)
- ❌ 블로킹 I/O
- ❌ 로깅 (비동기 로거만 허용)

**허용되는 작업:**
- Lock-free 자료구조
- 사전 할당된 메모리 풀
- 고정 크기 배열
- Atomic 연산
- Memory-mapped I/O

**컴포넌트:**
- `RTExecutive` (Cyclic Executive)
- `RTActionExecutor` (고정 메모리 풀)
- `RTStateMachine` (상태 전이)
- `LockFreeQueue` (명령/상태 큐)

---

## 3. Cyclic Executive 패턴 적용

### 3.1 Cyclic Executive 개념

**정의:**
> 고정된 주기마다 반복 실행되는 루프 기반 스케줄러로, 태스크들이 정해진 슬롯에서 순차적으로 실행됨

**특징:**
- ✅ 단순하고 예측 가능
- ✅ 오버헤드 최소 (컨텍스트 스위칭 없음)
- ✅ WCET 분석 용이
- ⚠️ 유연성 낮음 (정적 스케줄)
- ⚠️ CPU 활용률 최적화 어려움

### 3.2 MXRC에 적합한 Cyclic Executive 설계

#### Major Cycle / Minor Cycle 구조

```
Major Cycle: 100ms (LCM of all periods)
├── Minor Cycle 1: 0-10ms    [1ms Task] x10
│   ├── 0-1ms:   RT Action Slot 1 (센서 읽기)
│   ├── 1-2ms:   RT Action Slot 2 (제어 계산)
│   ├── 2-3ms:   RT Action Slot 3 (액추에이터 명령)
│   ├── ...
│   └── 9-10ms:  RT Action Slot 10
│
├── Minor Cycle 2: 10-20ms   [10ms Task] x1 + [1ms Task] x10
│   ├── 10-11ms: Medium Priority Action
│   ├── 11-12ms: RT Action Slot 11
│   └── ...
│
└── Minor Cycle 10: 90-100ms [100ms Task] x1
    ├── 90-91ms: Low Priority Action
    └── ...
```

#### RT Executive 구현 예시

```cpp
// src/core/rt/RTExecutive.h
class RTExecutive {
public:
    static constexpr size_t MAJOR_CYCLE_MS = 100;
    static constexpr size_t MINOR_CYCLE_MS = 1;
    static constexpr size_t SLOTS_PER_MAJOR = MAJOR_CYCLE_MS / MINOR_CYCLE_MS;

    struct RTActionSlot {
        ActionID action_id;
        ExecutionFunction exec_func;  // 함수 포인터
        uint32_t period_slots;        // 몇 슬롯마다 실행?
        uint32_t next_slot;           // 다음 실행 슬롯
    };

    void run() {
        setPriority(SCHED_FIFO, 90);  // 최고 우선순위
        pinToCPU(1);                  // CPU 코어 고정

        while (running_) {
            auto start = getMonotonicTime();

            // 현재 슬롯의 액션들 실행
            executeSlot(current_slot_);

            // 다음 슬롯으로
            current_slot_ = (current_slot_ + 1) % SLOTS_PER_MAJOR;

            // 정확한 주기 대기 (clock_nanosleep)
            waitUntilNextCycle(start, MINOR_CYCLE_MS);
        }
    }

private:
    void executeSlot(uint32_t slot) {
        for (auto& action_slot : schedule_[slot]) {
            if (slot == action_slot.next_slot) {
                action_slot.exec_func(context_);
                action_slot.next_slot += action_slot.period_slots;
            }
        }
    }

    std::array<std::vector<RTActionSlot>, SLOTS_PER_MAJOR> schedule_;
    RTContext context_;  // 사전 할당된 실행 컨텍스트
    uint32_t current_slot_{0};
    std::atomic<bool> running_{true};
};
```

### 3.3 현재 구조에서의 적용 방법

#### Option 1: Hybrid Approach (권장 ⭐)

```
Non-RT Thread (SCHED_OTHER)          RT Thread (SCHED_FIFO)
─────────────────────────────        ──────────────────────────
TaskExecutor                          RTExecutive
  │                                     │
  ├─ execute(Task, ONCE)                │
  │    └─> Immediate execution          │
  │                                      │
  ├─ execute(Task, PERIODIC)            ├─ Cyclic Loop (1ms)
  │    └─> Register to RT ──────────>   │   └─ execute RT Actions
  │         via CommandQueue             │
  │                                      │
  └─ EventBus.subscribe()               └─> State updates
       └─ Async callbacks                   via Lock-Free Queue
```

**구현 단계:**

1. **Phase 1**: RTExecutive 추가 (새 컴포넌트)
   - Cyclic executive 루프
   - 고정 주기 Action 실행
   - 기존 TaskExecutor와 병행

2. **Phase 2**: Action을 RT/Non-RT로 분류
   ```cpp
   enum class ActionRTClass {
       SOFT_RT,      // Non-RT 영역 (기존 방식)
       HARD_RT_1MS,  // 1ms 주기 보장
       HARD_RT_10MS, // 10ms 주기 보장
   };
   ```

3. **Phase 3**: 통신 인터페이스 구현
   - Lock-free command queue (MPSC)
   - Shared state memory (double buffering)

#### Option 2: Full Replacement (급진적)

- 기존 TaskExecutor 완전 교체
- ❌ 리스크 높음
- ❌ 테스트 재작성 필요
- ✅ 아키텍처 일관성

---

## 4. 상태 머신 패턴 통합

### 4.1 상태 머신의 필요성

**로봇 제어 시나리오:**
```
IDLE → INITIALIZING → READY → RUNNING → STOPPING → IDLE
               ↓                  ↓
            ERROR ←──────────────┘
```

각 상태에서 허용되는 Action이 다름:
- IDLE: 센서 초기화만
- READY: 이동 명령 허용
- RUNNING: 모든 제어 명령
- ERROR: 안전 정지만

### 4.2 상태 머신 + Cyclic Executive 통합

```cpp
// src/core/rt/RTStateMachine.h
class RTStateMachine {
public:
    enum class State {
        IDLE,
        INITIALIZING,
        READY,
        RUNNING,
        STOPPING,
        ERROR
    };

    struct Transition {
        State from;
        State to;
        std::function<bool()> guard;      // 전이 조건
        std::function<void()> action;     // 전이 시 실행
    };

    void tick() {
        // 현재 상태의 Entry Action 실행
        if (state_changed_) {
            entry_actions_[current_state_]();
            state_changed_ = false;
        }

        // 주기적 Action 실행 (state-dependent)
        cyclic_actions_[current_state_]();

        // 전이 조건 체크
        for (const auto& trans : transitions_) {
            if (trans.from == current_state_ && trans.guard()) {
                trans.action();  // 전이 액션
                current_state_ = trans.to;
                state_changed_ = true;
                break;
            }
        }
    }

private:
    State current_state_{State::IDLE};
    bool state_changed_{false};

    std::array<std::function<void()>, NUM_STATES> entry_actions_;
    std::array<std::function<void()>, NUM_STATES> cyclic_actions_;
    std::vector<Transition> transitions_;
};
```

### 4.3 Action을 상태 머신 컨텍스트로 전달

**기존 방식:**
```cpp
// TaskExecutor가 Action을 직접 실행
taskExecutor->execute(moveAction, context);
```

**상태 머신 통합 방식:**
```cpp
// Action을 상태에 바인딩
stateMachine.registerCyclicAction(State::RUNNING, [&]() {
    if (rtExecutive.shouldExecute("move_action")) {
        moveAction->execute(context);
    }
});

// RT 루프에서 상태 머신 tick
void RTExecutive::run() {
    while (running_) {
        stateMachine.tick();  // 상태에 따라 Action 실행
        waitNextCycle();
    }
}
```

**장점:**
- ✅ 상태 기반 Action 필터링
- ✅ 안전성 향상 (잘못된 상태에서 위험 Action 방지)
- ✅ 로봇 모드 전환 명확화

---

## 5. 효율성 및 완성도 평가

### 5.1 효율성 분석

#### 메모리 효율성

| 항목 | 현재 (Dynamic) | RT (Static) | 개선율 |
|------|----------------|-------------|--------|
| Action 객체 | std::shared_ptr (힙) | 고정 풀 (사전 할당) | **메모리 단편화 제거** |
| TaskState 맵 | std::map (동적 확장) | 고정 배열 [MAX_TASKS] | **O(log n) → O(1)** |
| 이벤트 큐 | EventBus (mutex) | Lock-Free Queue | **락 경합 제거** |

**예상 메모리:**
```
RT 메모리 풀 크기 =
    (Action Pool: 100 actions × 256 bytes) +
    (Context Pool: 10 contexts × 1KB) +
    (Command Queue: 1024 slots × 64 bytes)
  = 25.6KB + 10KB + 64KB
  = ~100KB (사전 할당, 고정)
```

#### 실행 시간 효율성

| 작업 | 현재 (Non-RT) | RT (Cyclic) | 지연 개선 |
|------|---------------|-------------|-----------|
| Action 실행 | ~10-100μs (가변) | < 10μs (고정) | **WCET 보장** |
| 스케줄링 오버헤드 | ~50μs (스레드 전환) | < 1μs (함수 호출) | **50배 개선** |
| Jitter | ±5ms (sleep drift) | ±10μs (clock_nanosleep) | **500배 개선** |

#### CPU 활용률

**현재:**
- 다중 스레드 → 컨텍스트 스위칭 오버헤드 높음
- 캐시 미스 빈번

**RT Cyclic:**
- 단일 스레드 → 캐시 효율 극대화
- CPU 코어 고정 → 마이그레이션 제거
- ⚠️ 단점: 단일 코어만 사용 (멀티코어 활용 불가)

### 5.2 완성도 평가

#### Cyclic Executive 방식

| 평가 항목 | 점수 | 설명 |
|----------|------|------|
| **실시간 보장** | ⭐⭐⭐⭐⭐ | WCET 분석 가능, 데드라인 보장 |
| **구현 복잡도** | ⭐⭐⭐ | 단순하지만 스케줄 설계 필요 |
| **유연성** | ⭐⭐ | 정적 스케줄, 동적 추가 어려움 |
| **확장성** | ⭐⭐ | 새 Task 추가 시 스케줄 재계산 |
| **멀티코어 활용** | ⭐ | 단일 코어 (개선: Partitioned Cyclic) |
| **기존 코드 재사용** | ⭐⭐⭐⭐ | Action 인터페이스 그대로 사용 |

**총평:**
- ✅ **실시간성**: 최고 수준 보장
- ✅ **안정성**: 검증된 패턴
- ⚠️ **유연성**: 동적 Task 추가 제한적
- ⚠️ **복잡도**: 스케줄 계산 툴 필요

#### 상태 머신 통합

| 평가 항목 | 점수 | 설명 |
|----------|------|------|
| **안전성** | ⭐⭐⭐⭐⭐ | 잘못된 상태 전이 방지 |
| **가독성** | ⭐⭐⭐⭐ | 로봇 동작 흐름 명확 |
| **테스트 용이성** | ⭐⭐⭐⭐⭐ | 상태별 단위 테스트 |
| **실시간 오버헤드** | ⭐⭐⭐⭐ | 함수 포인터 호출만 |
| **기존 통합** | ⭐⭐⭐ | TaskExecutor 일부 수정 필요 |

### 5.3 종합 권장사항

#### 추천 아키텍처: **Hybrid RT/Non-RT + Cyclic Executive + State Machine**

```
Phase 1: RT 인프라 구축 (2주)
├── RTExecutive 구현 (Cyclic loop)
├── Lock-free Queue (MPSC)
└── Memory Pool Allocator

Phase 2: RT Action 마이그레이션 (2주)
├── 기존 Action을 RT/Non-RT 분류
├── RTActionExecutor 구현
└── 통신 인터페이스 통합

Phase 3: 상태 머신 통합 (1주)
├── RTStateMachine 구현
├── State-Action 바인딩
└── 전이 로직 구현

Phase 4: 검증 및 최적화 (2주)
├── 실시간 성능 측정
├── WCET 분석
└── Jitter 최소화 튜닝
```

#### 장점

✅ **기존 코드 재사용**: Action 인터페이스 유지
✅ **점진적 마이그레이션**: Non-RT → RT 단계적 전환
✅ **실시간 보장**: PREEMPT_RT + Cyclic Executive
✅ **안전성**: 상태 머신 기반 제어
✅ **테스트 가능**: 195개 기존 테스트 유지

#### 단점 및 해결책

⚠️ **동적 Task 추가 제한**
- 해결: Non-RT 영역에서 스케줄 재계산 후 RT 반영

⚠️ **멀티코어 미활용**
- 해결: Partitioned Cyclic Executive (각 코어에 독립 실행)

⚠️ **개발 복잡도 증가**
- 해결: 스케줄 생성 도구 개발, 명확한 문서화

---

## 6. 구현 예시 코드

### 6.1 RTExecutive 핵심 구조

```cpp
// src/core/rt/RTExecutive.h
#pragma once
#include <array>
#include <atomic>
#include <functional>
#include <time.h>
#include <pthread.h>

namespace mxrc::core::rt {

class RTExecutive {
public:
    static constexpr size_t MAJOR_CYCLE_MS = 100;
    static constexpr size_t MINOR_CYCLE_MS = 1;
    static constexpr size_t NUM_SLOTS = MAJOR_CYCLE_MS / MINOR_CYCLE_MS;

    using RTActionFunc = std::function<void(RTContext&)>;

    struct RTActionSlot {
        const char* name;           // 액션 이름 (디버깅용, 정적 문자열)
        RTActionFunc func;          // 실행 함수
        uint16_t period_slots;      // 주기 (슬롯 단위)
        uint16_t offset_slot;       // 시작 오프셋
        uint16_t next_exec_slot;    // 다음 실행 슬롯
        uint32_t exec_count;        // 실행 횟수
        uint64_t total_exec_ns;     // 총 실행 시간 (나노초)
        uint64_t max_exec_ns;       // 최대 실행 시간
    };

    RTExecutive();
    ~RTExecutive();

    // RT 스레드 시작/정지
    void start();
    void stop();

    // Action 등록 (Non-RT 스레드에서 호출)
    bool registerAction(const char* name, RTActionFunc func,
                       uint16_t period_ms, uint16_t offset_ms = 0);

    // 통계 조회 (Non-RT 스레드에서 호출)
    struct Statistics {
        uint64_t total_cycles;
        uint64_t missed_deadlines;
        uint64_t max_cycle_time_ns;
        double avg_cpu_usage;
    };
    Statistics getStatistics() const;

private:
    void rtThreadFunc();
    void executeSlot(uint32_t slot);
    void waitUntilNextCycle(timespec start);

    pthread_t rt_thread_;
    std::atomic<bool> running_{false};

    // RT 데이터 (캐시 라인 정렬)
    alignas(64) std::array<std::vector<RTActionSlot>, NUM_SLOTS> schedule_;
    alignas(64) RTContext context_;  // 사전 할당된 실행 컨텍스트
    alignas(64) uint32_t current_slot_{0};

    // 통계 (atomic)
    std::atomic<uint64_t> total_cycles_{0};
    std::atomic<uint64_t> missed_deadlines_{0};
    std::atomic<uint64_t> max_cycle_ns_{0};
};

} // namespace mxrc::core::rt
```

### 6.2 RTExecutive 구현

```cpp
// src/core/rt/RTExecutive.cpp
#include "RTExecutive.h"
#include <sched.h>
#include <sys/mman.h>

namespace mxrc::core::rt {

RTExecutive::RTExecutive() {
    // 메모리 사전 할당 (페이지 폴트 방지)
    for (auto& slot_actions : schedule_) {
        slot_actions.reserve(10);  // 슬롯당 최대 10개 액션
    }
}

RTExecutive::~RTExecutive() {
    stop();
}

void RTExecutive::start() {
    if (running_.load()) return;

    running_.store(true);

    // RT 스레드 생성
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    // 스케줄링 정책: SCHED_FIFO (실시간)
    struct sched_param param;
    param.sched_priority = 90;  // 높은 우선순위
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &param);

    // 메모리 락 (스왑 방지)
    mlockall(MCL_CURRENT | MCL_FUTURE);

    pthread_create(&rt_thread_, &attr,
                   [](void* arg) -> void* {
                       static_cast<RTExecutive*>(arg)->rtThreadFunc();
                       return nullptr;
                   }, this);

    pthread_attr_destroy(&attr);
}

void RTExecutive::stop() {
    if (!running_.load()) return;

    running_.store(false);
    pthread_join(rt_thread_, nullptr);
    munlockall();
}

bool RTExecutive::registerAction(const char* name, RTActionFunc func,
                                 uint16_t period_ms, uint16_t offset_ms) {
    if (period_ms == 0 || period_ms % MINOR_CYCLE_MS != 0) {
        return false;  // 주기는 minor cycle의 배수여야 함
    }

    RTActionSlot slot;
    slot.name = name;
    slot.func = func;
    slot.period_slots = period_ms / MINOR_CYCLE_MS;
    slot.offset_slot = offset_ms / MINOR_CYCLE_MS;
    slot.next_exec_slot = slot.offset_slot;
    slot.exec_count = 0;
    slot.total_exec_ns = 0;
    slot.max_exec_ns = 0;

    // 모든 실행 슬롯에 등록
    for (uint16_t s = slot.offset_slot; s < NUM_SLOTS; s += slot.period_slots) {
        schedule_[s].push_back(slot);
    }

    return true;
}

void RTExecutive::rtThreadFunc() {
    // CPU 코어 1번에 고정 (코어 0은 시스템용)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    timespec cycle_start;
    clock_gettime(CLOCK_MONOTONIC, &cycle_start);

    while (running_.load()) {
        // 현재 슬롯의 액션 실행
        executeSlot(current_slot_);

        // 다음 슬롯으로
        current_slot_ = (current_slot_ + 1) % NUM_SLOTS;
        if (current_slot_ == 0) {
            total_cycles_++;
        }

        // 다음 사이클까지 정확히 대기
        waitUntilNextCycle(cycle_start);
        clock_gettime(CLOCK_MONOTONIC, &cycle_start);
    }
}

void RTExecutive::executeSlot(uint32_t slot) {
    timespec slot_start;
    clock_gettime(CLOCK_MONOTONIC, &slot_start);

    for (auto& action : schedule_[slot]) {
        if (slot == action.next_exec_slot) {
            timespec action_start;
            clock_gettime(CLOCK_MONOTONIC, &action_start);

            // 액션 실행
            action.func(context_);

            timespec action_end;
            clock_gettime(CLOCK_MONOTONIC, &action_end);

            // 실행 시간 측정
            uint64_t exec_ns = (action_end.tv_sec - action_start.tv_sec) * 1000000000ULL +
                              (action_end.tv_nsec - action_start.tv_nsec);

            action.exec_count++;
            action.total_exec_ns += exec_ns;
            if (exec_ns > action.max_exec_ns) {
                action.max_exec_ns = exec_ns;
            }

            // 다음 실행 슬롯 계산
            action.next_exec_slot = (action.next_exec_slot + action.period_slots) % NUM_SLOTS;
        }
    }

    // 슬롯 오버런 체크
    timespec slot_end;
    clock_gettime(CLOCK_MONOTONIC, &slot_end);
    uint64_t slot_duration_ns = (slot_end.tv_sec - slot_start.tv_sec) * 1000000000ULL +
                                (slot_end.tv_nsec - slot_start.tv_nsec);

    if (slot_duration_ns > MINOR_CYCLE_MS * 1000000ULL) {
        missed_deadlines_++;
    }

    if (slot_duration_ns > max_cycle_ns_.load()) {
        max_cycle_ns_.store(slot_duration_ns);
    }
}

void RTExecutive::waitUntilNextCycle(timespec start) {
    timespec next;
    next.tv_sec = start.tv_sec;
    next.tv_nsec = start.tv_nsec + MINOR_CYCLE_MS * 1000000;  // 1ms

    // 나노초 오버플로우 처리
    if (next.tv_nsec >= 1000000000) {
        next.tv_sec++;
        next.tv_nsec -= 1000000000;
    }

    // 절대 시간 기반 대기 (drift 방지)
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, nullptr);
}

RTExecutive::Statistics RTExecutive::getStatistics() const {
    Statistics stats;
    stats.total_cycles = total_cycles_.load();
    stats.missed_deadlines = missed_deadlines_.load();
    stats.max_cycle_time_ns = max_cycle_ns_.load();

    // CPU 사용률 계산 (평균 실행 시간 / 사이클 시간)
    uint64_t total_exec_ns = 0;
    for (const auto& slot_actions : schedule_) {
        for (const auto& action : slot_actions) {
            total_exec_ns += action.total_exec_ns;
        }
    }

    uint64_t total_time_ns = total_cycles_.load() * MAJOR_CYCLE_MS * 1000000ULL;
    stats.avg_cpu_usage = total_time_ns > 0 ?
        (double)total_exec_ns / total_time_ns : 0.0;

    return stats;
}

} // namespace mxrc::core::rt
```

### 6.3 사용 예시

```cpp
// 예제: RTExecutive 사용
#include "RTExecutive.h"

int main() {
    mxrc::core::rt::RTExecutive rtExec;

    // 1ms 주기 액션 등록 (센서 읽기)
    rtExec.registerAction("read_sensor", [](RTContext& ctx) {
        // 센서 데이터 읽기 (< 50μs)
        ctx.sensor_value = readIMU();
    }, 1);  // 1ms period

    // 10ms 주기 액션 등록 (제어 계산)
    rtExec.registerAction("control_loop", [](RTContext& ctx) {
        // PID 제어 계산 (< 100μs)
        ctx.control_output = pidController.compute(ctx.sensor_value);
    }, 10);  // 10ms period

    // 100ms 주기 액션 등록 (상태 업데이트)
    rtExec.registerAction("state_update", [](RTContext& ctx) {
        // 상태 정보 업데이트 (< 200μs)
        updateStateMachine(ctx);
    }, 100);  // 100ms period

    // RT 스레드 시작
    rtExec.start();

    // Non-RT 작업 (로깅, 모니터링)
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        auto stats = rtExec.getStatistics();
        std::cout << "Cycles: " << stats.total_cycles
                  << ", Missed: " << stats.missed_deadlines
                  << ", CPU: " << stats.avg_cpu_usage * 100 << "%"
                  << std::endl;
    }

    rtExec.stop();
    return 0;
}
```

---

## 7. 결론

### 7.1 최종 권장사항

**MXRC 프로젝트의 실시간 보장을 위한 최적 아키텍처:**

1. **Hybrid RT/Non-RT 분리** ⭐⭐⭐⭐⭐
   - RTExecutive (Cyclic Executive) 도입
   - 기존 TaskExecutor는 Non-RT 작업 담당
   - Lock-free Queue로 통신

2. **상태 머신 통합** ⭐⭐⭐⭐⭐
   - 로봇 모드별 Action 제어
   - 안전성 및 가독성 극대화

3. **점진적 마이그레이션** ⭐⭐⭐⭐⭐
   - 기존 코드 최대한 재사용
   - Action 인터페이스 유지
   - 테스트 호환성 보장

### 7.2 효율성 요약

| 항목 | 개선 효과 |
|------|-----------|
| **Jitter** | ±5ms → ±10μs (500배 개선) |
| **WCET** | 비결정적 → 분석 가능 |
| **메모리** | 동적 할당 → 100KB 고정 |
| **CPU 오버헤드** | 스레드 전환 50μs → 함수 호출 1μs |
| **실시간 보장** | 불가능 → 하드 실시간 가능 |

### 7.3 구현 로드맵

```
총 예상 기간: 7주

Week 1-2: RTExecutive 인프라
├── Cyclic Executive 구현
├── pthread_setschedparam (SCHED_FIFO)
├── clock_nanosleep 타이밍
└── 성능 측정 및 검증

Week 3-4: RT/Non-RT 통신
├── Lock-Free Queue (MPSC)
├── Double Buffering 상태 공유
├── CommandQueue 프로토콜
└── 통합 테스트

Week 5: 상태 머신
├── RTStateMachine 구현
├── State-Action 바인딩
└── 전이 로직 및 테스트

Week 6-7: 마이그레이션 및 검증
├── 기존 Action을 RTAction으로 변환
├── 195개 단위 테스트 업데이트
├── 실시간 성능 벤치마크
└── 문서화 및 릴리스
```

### 7.4 리스크 및 대응

| 리스크 | 영향 | 대응 방안 |
|--------|------|----------|
| **개발 기간 증가** | 높음 | 점진적 도입, 기존 코드 재사용 |
| **테스트 재작성** | 중간 | Mock RTContext, 시뮬레이션 모드 |
| **WCET 분석 어려움** | 중간 | 측정 도구 개발, Conservative 추정 |
| **멀티코어 미활용** | 낮음 | Phase 2에서 Partitioned Executive |

---

## 8. RT/Non-RT 장애 격리 (Fault Isolation)

### 8.1 핵심 질문: "한쪽이 다운되어도 다른 쪽은 살아있을 수 있는가?"

**결론: ✅ 가능하지만 방법에 따라 격리 수준이 다름**

### 8.2 격리 수준 비교

#### Option 1: 스레드 분리 (같은 프로세스) ⭐⭐⭐

```
┌──────────────────────────────────────┐
│         MXRC Process (PID 1234)      │
│                                      │
│  ┌────────────┐    ┌─────────────┐  │
│  │ Non-RT     │    │ RT Thread   │  │
│  │ Thread     │    │ (SCHED_FIFO)│  │
│  │ (SCHED_    │◄──►│             │  │
│  │  OTHER)    │    │ Cyclic Exec │  │
│  └────────────┘    └─────────────┘  │
│         │                  │         │
│         └──────┬───────────┘         │
│                │                     │
│        Shared Memory Space           │
│        (Lock-Free Queue)             │
└──────────────────────────────────────┘
```

**장점:**
- ✅ 구현 간단
- ✅ 통신 오버헤드 최소 (공유 메모리)
- ✅ 컨텍스트 공유 용이

**장애 격리 수준: ❌❌❌ 낮음**

| 장애 유형 | Non-RT 다운 | RT 생존? | 설명 |
|----------|-------------|----------|------|
| **세그먼테이션 폴트** | Non-RT 스레드에서 발생 | ❌ **불가능** | 프로세스 전체 크래시 |
| **메모리 릭** | Non-RT에서 메모리 고갈 | ❌ **불가능** | OOM Killer가 프로세스 전체 종료 |
| **무한 루프** | Non-RT 스레드 행(hang) | ✅ **가능** | RT 스레드는 계속 실행 (다른 CPU 코어) |
| **데드락** | Non-RT 뮤텍스 데드락 | ✅ **가능** | RT는 Lock-Free 구조 사용 시 |
| **예외 미처리** | Non-RT에서 std::terminate | ❌ **불가능** | 프로세스 전체 종료 |

**예시 시나리오:**
```cpp
// Non-RT 스레드에서 크래시 발생
void nonRTTask() {
    int* ptr = nullptr;
    *ptr = 42;  // ❌ Segmentation Fault
    // → 전체 프로세스 종료 → RT 스레드도 죽음
}
```

#### Option 2: 프로세스 분리 ⭐⭐⭐⭐⭐

```
┌────────────────────┐         ┌────────────────────┐
│  Non-RT Process    │         │  RT Process        │
│  (PID 1234)        │         │  (PID 5678)        │
│  SCHED_OTHER       │         │  SCHED_FIFO        │
│                    │         │                    │
│  • TaskRegistry    │         │  • RTExecutive     │
│  • EventBus        │         │  • Cyclic Loop     │
│  • Logger          │         │  • Action Pool     │
│  • DataStore       │         │  • State Machine   │
└────────────────────┘         └────────────────────┘
         │                              │
         └──────────IPC 통신─────────────┘
              (Shared Memory + Semaphore)
                   or
              (Unix Domain Socket)
```

**장점:**
- ✅ **완벽한 격리**: 한쪽 크래시해도 다른 쪽 무관
- ✅ **메모리 격리**: 각 프로세스 독립 메모리 공간
- ✅ **권한 분리**: RT는 root, Non-RT는 일반 사용자
- ✅ **독립 재시작**: Non-RT 다운 시 RT는 계속 실행 후 재연결

**장애 격리 수준: ✅✅✅ 높음**

| 장애 유형 | Non-RT 다운 | RT 생존? | 설명 |
|----------|-------------|----------|------|
| **세그먼테이션 폴트** | Non-RT 프로세스 크래시 | ✅ **가능** | 각 프로세스 독립 메모리 |
| **메모리 릭** | Non-RT OOM | ✅ **가능** | RT 프로세스는 영향 없음 |
| **무한 루프** | Non-RT 행 | ✅ **가능** | RT는 독립 실행 |
| **데드락** | Non-RT 데드락 | ✅ **가능** | 완전 독립 |
| **예외 미처리** | Non-RT terminate | ✅ **가능** | RT 프로세스 무관 |

**예시 시나리오:**
```cpp
// Non-RT 프로세스 크래시
void nonRTProcess() {
    int* ptr = nullptr;
    *ptr = 42;  // ❌ Segmentation Fault
    // → Non-RT 프로세스만 종료
    // → RT 프로세스는 계속 실행! ✅
}
```

#### Option 3: 하이퍼바이저/컨테이너 분리 ⭐⭐⭐⭐⭐ (최고 수준)

```
┌─────────────────────────────────────────────────┐
│           Host OS (Ubuntu 24.04 PREEMPT_RT)     │
├─────────────────────────────────────────────────┤
│  Docker/LXC Container 1    │  Docker/LXC Container 2 │
│  Non-RT Services           │  RT Executive           │
│  • EventBus, Logger        │  • Cyclic Loop          │
│  • DataStore, UI           │  • 1ms Control          │
│  (cgroup: cpu.shares=512)  │  (cgroup: cpu.shares=1024) │
│  (cpuset: 0,2,3)          │  (cpuset: 1)  ← 전용 코어   │
└─────────────────────────────────────────────────┘
```

**장점:**
- ✅ **최상위 격리**: 커널 레벨 리소스 격리
- ✅ **CPU 격리**: RT 전용 코어 할당 (cpuset)
- ✅ **메모리 제한**: cgroup memory limit
- ✅ **네트워크 격리**: 독립 네트워크 네임스페이스

**단점:**
- ⚠️ 오버헤드 증가 (컨테이너 오버헤드 ~1-3%)
- ⚠️ 복잡도 증가

### 8.3 장애 시나리오별 생존 가능성

#### 시나리오 1: Non-RT 크래시, RT 계속 실행

**프로세스 분리 방식:**
```cpp
// RT 프로세스 (독립 실행)
int main_rt() {
    RTExecutive rtExec;

    // 주기적 안전 동작 실행
    rtExec.registerAction("safety_stop", [](RTContext& ctx) {
        // Non-RT 연결 체크
        if (!ctx.nonrt_alive) {
            // 안전 정지 모드로 전환
            robot.emergencyStop();
        }
    }, 10);  // 10ms마다 체크

    rtExec.start();  // ← Non-RT 죽어도 계속 실행
    return 0;
}

// Non-RT 프로세스 (크래시 가능)
int main_nonrt() {
    TaskExecutor executor;

    // 복잡한 계획, 로깅 등
    // 크래시 발생 시 RT는 영향 없음

    return 0;
}
```

**결과:**
```
Timeline:
T=0s:    Both RT & Non-RT running
T=10s:   Non-RT crashes (segfault)
         → RT detects via heartbeat timeout
T=10.01s: RT enters SAFE_MODE
         → Continue emergency stop action
         → Robot safely halts
T=15s:   Watchdog restarts Non-RT process
T=20s:   Non-RT reconnects to RT
         → RT exits SAFE_MODE
         → Normal operation resumes
```

#### 시나리오 2: RT 크래시 → 치명적 (반드시 방지)

**RT는 절대 크래시하면 안됨!**

방어 메커니즘:
1. **No Dynamic Allocation**: RT 코드에서 new/malloc 금지
2. **No Exceptions**: RT 영역에서 try/catch 금지
3. **Bounds Checking**: 배열 접근 전 체크
4. **Watchdog**: RT 무응답 시 시스템 리셋

```cpp
// RT Watchdog (하드웨어 or 커널 레벨)
void setupRTWatchdog() {
    int fd = open("/dev/watchdog", O_WRONLY);

    // RT 루프에서 주기적으로 kick
    while (running) {
        executeSlot();

        // Watchdog kick (RT 살아있음 알림)
        write(fd, "\0", 1);  // ← 이게 안오면 시스템 리셋

        waitNextCycle();
    }
}
```

#### 시나리오 3: 통신 단절

**프로세스 간 통신 실패 시:**

```cpp
// RT 프로세스: 통신 타임아웃 처리
class RTExecutive {
    void checkNonRTConnection() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - last_nonrt_message_;

        if (elapsed > TIMEOUT_THRESHOLD) {
            // Non-RT 다운 감지
            state_machine_.transition(State::SAFE_MODE);

            // 안전 동작 실행
            executeEmergencyActions();

            // Non-RT 재시작 요청 (systemd)
            system("systemctl restart mxrc-nonrt.service");
        }
    }
};
```

### 8.4 권장 아키텍처: Dual-Process with Watchdog

```
┌──────────────────────────────────────────────────────┐
│                  System Architecture                 │
├──────────────────────────────────────────────────────┤
│                                                      │
│  ┌────────────────────┐      ┌──────────────────┐   │
│  │  Non-RT Process    │      │  RT Process      │   │
│  │  (mxrc-nonrt)      │      │  (mxrc-rt)       │   │
│  │                    │      │                  │   │
│  │  SCHED_OTHER       │      │  SCHED_FIFO 90   │   │
│  │  CPU 0,2,3         │      │  CPU 1 (isolated)│   │
│  │  User: mxrc        │      │  User: root      │   │
│  └────────────────────┘      └──────────────────┘   │
│           │                           │              │
│           │    Shared Memory          │              │
│           │    (Lock-Free Queue)      │              │
│           └───────────┬───────────────┘              │
│                       │                              │
│  ┌────────────────────▼──────────────────────────┐   │
│  │     Watchdog Manager (systemd)               │   │
│  │  • Monitors both processes                   │   │
│  │  • Restarts Non-RT if crashes                │   │
│  │  • Emergency shutdown if RT crashes          │   │
│  └──────────────────────────────────────────────┘   │
│                       │                              │
│  ┌────────────────────▼──────────────────────────┐   │
│  │     Hardware Watchdog                        │   │
│  │  • Monitors RT heartbeat                     │   │
│  │  • System reset if no kick in 100ms          │   │
│  └──────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────┘
```

### 8.5 구현: 프로세스 분리 + Heartbeat

#### Non-RT 프로세스

```cpp
// mxrc-nonrt/main.cpp
int main() {
    // 공유 메모리 열기
    int shm_fd = shm_open("/mxrc_comm", O_RDWR, 0666);
    SharedMemory* comm = (SharedMemory*)mmap(
        nullptr, sizeof(SharedMemory),
        PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0
    );

    TaskExecutor executor;

    while (true) {
        try {
            // 복잡한 비즈니스 로직
            executor.planTrajectory();
            executor.updateUI();

            // RT에게 살아있음 알림
            comm->nonrt_heartbeat = getCurrentTimeMs();

            // RT로부터 명령 수신
            while (auto cmd = comm->rt_to_nonrt_queue.pop()) {
                handleCommand(cmd);
            }

        } catch (std::exception& e) {
            // Non-RT 예외는 여기서 처리
            // RT에는 영향 없음
            Logger::error("Non-RT error: {}", e.what());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}
```

#### RT 프로세스

```cpp
// mxrc-rt/main.cpp
int main() {
    // 실시간 우선순위 설정
    struct sched_param param;
    param.sched_priority = 90;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    // 메모리 락
    mlockall(MCL_CURRENT | MCL_FUTURE);

    // 공유 메모리 생성
    int shm_fd = shm_open("/mxrc_comm", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(SharedMemory));
    SharedMemory* comm = (SharedMemory*)mmap(
        nullptr, sizeof(SharedMemory),
        PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0
    );

    RTExecutive rtExec;

    // Non-RT 감시 액션 등록
    rtExec.registerAction("monitor_nonrt", [comm](RTContext& ctx) {
        auto now = getCurrentTimeMs();
        auto elapsed = now - comm->nonrt_heartbeat;

        if (elapsed > 500) {  // 500ms 타임아웃
            // Non-RT 다운 감지 → 안전 모드
            ctx.state_machine->transition(State::SAFE_MODE);
            comm->rt_to_nonrt_queue.push(Command::RESTART);
        }
    }, 100);  // 100ms마다 체크

    // 제어 루프 (절대 크래시 안되도록 방어 코드)
    rtExec.registerAction("control_loop", [](RTContext& ctx) {
        // 모든 배열 접근 전 경계 체크
        if (ctx.sensor_idx < MAX_SENSORS) {
            ctx.sensor_data = readSensor(ctx.sensor_idx);
        }

        // 예외 없이 에러 코드로 처리
        int error_code = pidController.compute(ctx.sensor_data, &ctx.output);
        if (error_code != 0) {
            ctx.error_count++;
            // 로깅 없이 복구 시도
        }
    }, 1);  // 1ms 주기

    rtExec.start();  // ← Non-RT 죽어도 계속 실행

    // Watchdog kick
    setupHardwareWatchdog();

    return 0;
}
```

#### Systemd 서비스 설정

```ini
# /etc/systemd/system/mxrc-rt.service
[Unit]
Description=MXRC Real-Time Process
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/mxrc-rt
Restart=on-failure
RestartSec=1s

# 실시간 우선순위 부여
LimitRTPRIO=99
LimitMEMLOCK=infinity

# 리소스 격리
CPUAffinity=1
Nice=-20

[Install]
WantedBy=multi-user.target
```

```ini
# /etc/systemd/system/mxrc-nonrt.service
[Unit]
Description=MXRC Non-Real-Time Process
After=mxrc-rt.service

[Service]
Type=simple
ExecStart=/usr/local/bin/mxrc-nonrt
Restart=always
RestartSec=2s

# 자동 재시작 (크래시 시)
StartLimitInterval=0

# Non-RT는 일반 우선순위
CPUAffinity=0 2 3

[Install]
WantedBy=multi-user.target
```

### 8.6 장애 격리 수준 요약

| 방법 | 격리 수준 | 통신 오버헤드 | 복잡도 | 권장도 |
|------|----------|--------------|--------|--------|
| **스레드 분리** | ⭐⭐ (낮음) | 최소 (~ns) | 낮음 | ⚠️ 실시간 시스템에 부적합 |
| **프로세스 분리** | ⭐⭐⭐⭐⭐ (높음) | 중간 (~μs) | 중간 | ✅ **권장** |
| **컨테이너** | ⭐⭐⭐⭐⭐ (최고) | 약간 높음 | 높음 | ✅ 대규모 시스템 |
| **VM 분리** | ⭐⭐⭐⭐⭐ (최고) | 높음 (~ms) | 매우 높음 | ⚠️ 오버헤드 과다 |

### 8.7 결론

**질문: "한쪽이 다운되어도 다른 쪽은 살아있을 수 있는가?"**

**답변: ✅ 가능합니다. 단, 프로세스 분리 필수!**

#### 권장 설계

1. **RT 프로세스** (절대 크래시 불가)
   - SCHED_FIFO, 최고 우선순위
   - 동적 할당 금지, 예외 금지
   - Cyclic Executive
   - 하드웨어 Watchdog 감시

2. **Non-RT 프로세스** (크래시 허용, 자동 재시작)
   - SCHED_OTHER, 일반 우선순위
   - 복잡한 로직, 로깅, UI
   - Systemd가 자동 재시작
   - RT는 Non-RT 다운 감지 → 안전 모드

3. **통신**
   - Shared Memory (Lock-Free Queue)
   - Heartbeat 교환 (100ms)
   - Timeout 시 격리 모드

#### 장점

✅ **Non-RT 크래시 시**: RT는 안전 모드로 전환, 로봇 정지, Non-RT 재시작 대기
✅ **RT 크래시 시**: 하드웨어 Watchdog → 시스템 전체 리셋 (최후 수단)
✅ **통신 단절 시**: RT는 마지막 명령 유지 또는 안전 정지
✅ **점진적 복구**: Non-RT 재시작 → 재연결 → 정상 운영 재개

---

## 9. DataStore 통합: RT/Non-RT 이중 구조

### 9.1 현재 DataStore 아키텍처 분석

#### 기존 DataStore 구조 (src/core/datastore/DataStore.h:86-173)

```cpp
class DataStore {
    // Facade 패턴: 4개 Manager에 위임
    tbb::concurrent_hash_map<std::string, SharedData> data_map_;  // 동적 해시맵
    std::map<std::string, Notifier*> notifiers_;                   // 동적 맵
    std::unique_ptr<ExpirationManager> expiration_manager_;
    std::unique_ptr<AccessControlManager> access_control_manager_;
    std::unique_ptr<MetricsCollector> metrics_collector_;
    std::unique_ptr<LogManager> log_manager_;
    std::mutex mutex_;  // 전역 뮤텍스
};
```

#### 실시간성 위배 요소

| 문제점 | 현재 구현 | 실시간 영향 |
|--------|----------|------------|
| **동적 메모리** | `tbb::concurrent_hash_map` (동적 확장) | 예측 불가능한 할당 지연 |
| **뮤텍스** | `std::mutex` | Priority Inversion 가능 |
| **std::any** | `SharedData::value` (동적 타입) | any_cast 오버헤드 |
| **Observer 패턴** | 동적 구독자 리스트 | 알림 시 불확정 지연 |
| **예외** | `get<T>()` throws exception | 예외 처리 오버헤드 |

### 9.2 DataStore 분리 전략

#### Option A: 완전 분리 (Dual DataStore) ⭐⭐⭐⭐⭐ 권장

```
┌─────────────────────────────────────────────────────────┐
│                  Data Architecture                      │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌──────────────────┐         ┌──────────────────┐     │
│  │  Non-RT Process  │         │  RT Process      │     │
│  ├──────────────────┤         ├──────────────────┤     │
│  │                  │         │                  │     │
│  │  DataStore       │         │  RTDataStore     │     │
│  │  (기존)          │         │  (고정 메모리)    │     │
│  │                  │         │                  │     │
│  │  • 동적 할당 OK  │         │  • 사전 할당     │     │
│  │  • TBB hashmap   │         │  • 고정 배열     │     │
│  │  • Observer      │         │  • Lock-Free     │     │
│  │  • 로깅/영속화   │         │  • No Exception  │     │
│  │                  │         │                  │     │
│  └──────────────────┘         └──────────────────┘     │
│           │                            │                │
│           │     Shared Memory          │                │
│           │    (Data Sync Region)      │                │
│           └────────────┬───────────────┘                │
│                        │                                │
│              Data Synchronization                       │
│              (Periodic Copy: 10ms)                      │
│              RT → Non-RT (Status)                       │
│              Non-RT → RT (Commands)                     │
└─────────────────────────────────────────────────────────┘
```

**설계 원칙:**

1. **RT DataStore**: 실시간 제어 데이터만 (센서, 제어 출력, 상태)
2. **Non-RT DataStore**: 복잡한 데이터 (로그, 설정, 통계, 이벤트)
3. **데이터 동기화**: 주기적 복사 (RT → Non-RT: 상태 공유, Non-RT → RT: 명령 전달)

#### Option B: 하이브리드 (단일 DataStore + RT 캐시)

```
┌─────────────────────────────────────────────────────┐
│  Non-RT Process                                     │
│  ┌───────────────────────────────────────────────┐  │
│  │  DataStore (Master)                           │  │
│  │  • 모든 데이터의 원본                         │  │
│  │  • 영속화, 로깅                               │  │
│  └───────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
                         ↕ (Shared Memory)
┌─────────────────────────────────────────────────────┐
│  RT Process                                         │
│  ┌───────────────────────────────────────────────┐  │
│  │  RT Cache (Read-Only Mirror)                  │  │
│  │  • 자주 접근하는 데이터만 복사                │  │
│  │  • 고정 크기 배열                             │  │
│  │  • Double Buffering                           │  │
│  └───────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
```

⚠️ **단점**: Non-RT DataStore 크래시 시 RT는 stale data만 가지고 동작

### 9.3 권장 아키텍처: Dual DataStore with Sync

#### 9.3.1 RTDataStore 설계 (RT 프로세스 전용)

```cpp
// src/core/rt/RTDataStore.h
namespace mxrc::core::rt {

/**
 * @brief 실시간 데이터 저장소 (고정 메모리, Lock-Free)
 *
 * 특징:
 * - 사전 할당된 고정 크기 배열
 * - Lock-Free 원자적 접근
 * - 예외 없음 (에러 코드 반환)
 * - 타입 안전성 (템플릿 특수화)
 */
class RTDataStore {
public:
    // 데이터 키 (컴파일 타임 상수)
    enum class Key : uint16_t {
        // 센서 데이터 (1ms 갱신)
        SENSOR_POSITION_X = 0,
        SENSOR_POSITION_Y = 1,
        SENSOR_VELOCITY = 2,
        SENSOR_FORCE = 3,

        // 제어 출력 (1ms 갱신)
        CONTROL_MOTOR_TORQUE = 100,
        CONTROL_SERVO_ANGLE = 101,

        // 상태 (10ms 갱신)
        STATE_ROBOT_MODE = 200,  // IDLE, RUNNING, ERROR
        STATE_TASK_ID = 201,
        STATE_PROGRESS = 202,

        // 파라미터 (Non-RT → RT, 100ms 갱신)
        PARAM_MAX_VELOCITY = 300,
        PARAM_PID_KP = 301,
        PARAM_PID_KI = 302,
        PARAM_PID_KD = 303,

        MAX_KEYS = 512  // 고정 크기
    };

    // 데이터 타입 (union으로 고정 크기)
    union RTValue {
        int32_t i32;
        float f32;
        double f64;
        uint64_t u64;
        char str[32];  // 짧은 문자열
    };

    struct RTData {
        RTValue value;
        uint64_t timestamp_ns;  // 나노초 타임스탬프
        uint32_t sequence;      // 갱신 시퀀스 번호
        uint8_t valid;          // 0=invalid, 1=valid
        uint8_t padding[3];
    };

    RTDataStore();
    ~RTDataStore() = default;

    // 데이터 쓰기 (에러 코드 반환, 예외 없음)
    int set_i32(Key key, int32_t value);
    int set_f32(Key key, float value);
    int set_f64(Key key, double value);

    // 데이터 읽기 (에러 코드 반환, 출력 파라미터)
    int get_i32(Key key, int32_t* out_value) const;
    int get_f32(Key key, float* out_value) const;
    int get_f64(Key key, double* out_value) const;

    // 타임스탬프 체크 (데이터 신선도)
    bool is_fresh(Key key, uint64_t max_age_ns) const;

    // 통계 (Lock-Free atomic)
    uint64_t get_total_reads() const { return total_reads_.load(); }
    uint64_t get_total_writes() const { return total_writes_.load(); }

private:
    // 고정 크기 배열 (사전 할당)
    alignas(64) std::array<RTData, static_cast<size_t>(Key::MAX_KEYS)> data_;

    // 통계 (atomic)
    alignas(64) std::atomic<uint64_t> total_reads_{0};
    alignas(64) std::atomic<uint64_t> total_writes_{0};

    // 타임스탬프 헬퍼
    uint64_t get_current_time_ns() const;
};

} // namespace mxrc::core::rt
```

#### 9.3.2 RTDataStore 구현

```cpp
// src/core/rt/RTDataStore.cpp
#include "RTDataStore.h"
#include <time.h>
#include <cstring>

namespace mxrc::core::rt {

RTDataStore::RTDataStore() {
    // 모든 데이터 초기화 (메모리 사전 할당)
    for (auto& d : data_) {
        std::memset(&d, 0, sizeof(RTData));
        d.valid = 0;  // 초기 상태: invalid
    }
}

int RTDataStore::set_i32(Key key, int32_t value) {
    if (key >= Key::MAX_KEYS) return -1;  // EINVAL

    size_t idx = static_cast<size_t>(key);
    RTData& d = data_[idx];

    // Atomic write (단일 워드 쓰기는 원자적)
    d.value.i32 = value;
    d.timestamp_ns = get_current_time_ns();
    d.sequence++;
    d.valid = 1;

    total_writes_.fetch_add(1, std::memory_order_relaxed);
    return 0;  // Success
}

int RTDataStore::get_i32(Key key, int32_t* out_value) const {
    if (key >= Key::MAX_KEYS) return -1;  // EINVAL
    if (!out_value) return -2;             // ENULL

    size_t idx = static_cast<size_t>(key);
    const RTData& d = data_[idx];

    if (!d.valid) return -3;  // ENODATA

    // Atomic read
    *out_value = d.value.i32;
    total_reads_.fetch_add(1, std::memory_order_relaxed);
    return 0;  // Success
}

int RTDataStore::set_f32(Key key, float value) {
    if (key >= Key::MAX_KEYS) return -1;

    size_t idx = static_cast<size_t>(key);
    RTData& d = data_[idx];

    d.value.f32 = value;
    d.timestamp_ns = get_current_time_ns();
    d.sequence++;
    d.valid = 1;

    total_writes_.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int RTDataStore::get_f32(Key key, float* out_value) const {
    if (key >= Key::MAX_KEYS) return -1;
    if (!out_value) return -2;

    size_t idx = static_cast<size_t>(key);
    const RTData& d = data_[idx];

    if (!d.valid) return -3;

    *out_value = d.value.f32;
    total_reads_.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

bool RTDataStore::is_fresh(Key key, uint64_t max_age_ns) const {
    if (key >= Key::MAX_KEYS) return false;

    size_t idx = static_cast<size_t>(key);
    const RTData& d = data_[idx];

    if (!d.valid) return false;

    uint64_t now = get_current_time_ns();
    uint64_t age = now - d.timestamp_ns;
    return age <= max_age_ns;
}

uint64_t RTDataStore::get_current_time_ns() const {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

} // namespace mxrc::core::rt
```

#### 9.3.3 데이터 동기화 메커니즘

```cpp
// src/core/sync/DataSynchronizer.h
/**
 * @brief RT ↔ Non-RT 데이터 동기화
 *
 * 역할:
 * 1. RT → Non-RT: 상태 정보 복사 (10ms 주기)
 * 2. Non-RT → RT: 파라미터/명령 전달 (100ms 주기)
 */
class DataSynchronizer {
public:
    struct SharedMemoryRegion {
        // RT → Non-RT (상태 공유)
        struct {
            int32_t robot_mode;
            float position_x;
            float position_y;
            float velocity;
            uint64_t timestamp_ns;
            uint32_t sequence;
        } rt_to_nonrt;

        // Non-RT → RT (파라미터)
        struct {
            float max_velocity;
            float pid_kp;
            float pid_ki;
            float pid_kd;
            uint64_t timestamp_ns;
            uint32_t sequence;
        } nonrt_to_rt;

        // Heartbeat
        std::atomic<uint64_t> rt_heartbeat_ns;
        std::atomic<uint64_t> nonrt_heartbeat_ns;
    };

    // RT 프로세스에서 호출
    static void syncFromRTDataStore(
        RTDataStore* rt_ds,
        SharedMemoryRegion* shm
    ) {
        // RT 데이터 → 공유 메모리 (10ms마다)
        int32_t mode;
        if (rt_ds->get_i32(RTDataStore::Key::STATE_ROBOT_MODE, &mode) == 0) {
            shm->rt_to_nonrt.robot_mode = mode;
        }

        float pos_x, pos_y, vel;
        rt_ds->get_f32(RTDataStore::Key::SENSOR_POSITION_X, &pos_x);
        rt_ds->get_f32(RTDataStore::Key::SENSOR_POSITION_Y, &pos_y);
        rt_ds->get_f32(RTDataStore::Key::SENSOR_VELOCITY, &vel);

        shm->rt_to_nonrt.position_x = pos_x;
        shm->rt_to_nonrt.position_y = pos_y;
        shm->rt_to_nonrt.velocity = vel;
        shm->rt_to_nonrt.timestamp_ns = getCurrentTimeNs();
        shm->rt_to_nonrt.sequence++;

        // Heartbeat 갱신
        shm->rt_heartbeat_ns.store(getCurrentTimeNs());
    }

    // Non-RT 프로세스에서 호출
    static void syncToRTDataStore(
        DataStore* non_rt_ds,
        SharedMemoryRegion* shm
    ) {
        // 공유 메모리 → RT 데이터 (100ms마다)
        float max_vel = non_rt_ds->get<float>("max_velocity");
        float kp = non_rt_ds->get<float>("pid_kp");
        float ki = non_rt_ds->get<float>("pid_ki");
        float kd = non_rt_ds->get<float>("pid_kd");

        shm->nonrt_to_rt.max_velocity = max_vel;
        shm->nonrt_to_rt.pid_kp = kp;
        shm->nonrt_to_rt.pid_ki = ki;
        shm->nonrt_to_rt.pid_kd = kd;
        shm->nonrt_to_rt.timestamp_ns = getCurrentTimeNs();
        shm->nonrt_to_rt.sequence++;

        // Heartbeat 갱신
        shm->nonrt_heartbeat_ns.store(getCurrentTimeNs());
    }

    // RT 프로세스: 공유 메모리 → RTDataStore
    static void applyNonRTParams(
        RTDataStore* rt_ds,
        SharedMemoryRegion* shm
    ) {
        // Non-RT 파라미터 적용 (100ms마다)
        rt_ds->set_f32(RTDataStore::Key::PARAM_MAX_VELOCITY,
                       shm->nonrt_to_rt.max_velocity);
        rt_ds->set_f32(RTDataStore::Key::PARAM_PID_KP,
                       shm->nonrt_to_rt.pid_kp);
        rt_ds->set_f32(RTDataStore::Key::PARAM_PID_KI,
                       shm->nonrt_to_rt.pid_ki);
        rt_ds->set_f32(RTDataStore::Key::PARAM_PID_KD,
                       shm->nonrt_to_rt.pid_kd);
    }

    // Non-RT 프로세스: 공유 메모리 → DataStore
    static void applyRTStatus(
        DataStore* non_rt_ds,
        SharedMemoryRegion* shm
    ) {
        // RT 상태 적용 (10ms마다)
        non_rt_ds->set("robot_mode", shm->rt_to_nonrt.robot_mode,
                       DataType::RobotMode);
        non_rt_ds->set("position_x", shm->rt_to_nonrt.position_x,
                       DataType::InterfaceData);
        non_rt_ds->set("position_y", shm->rt_to_nonrt.position_y,
                       DataType::InterfaceData);
        non_rt_ds->set("velocity", shm->rt_to_nonrt.velocity,
                       DataType::InterfaceData);
    }

private:
    static uint64_t getCurrentTimeNs();
};
```

### 9.4 완전한 시스템 아키텍처

```
┌────────────────────────────────────────────────────────────────────┐
│                        MXRC 통합 시스템                            │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  ┌──────────────────────────┐      ┌───────────────────────────┐  │
│  │  Non-RT Process          │      │  RT Process               │  │
│  │  (mxrc-nonrt)            │      │  (mxrc-rt)                │  │
│  │  PID: 1234               │      │  PID: 5678                │  │
│  │  SCHED_OTHER             │      │  SCHED_FIFO 90            │  │
│  │  CPU: 0,2,3              │      │  CPU: 1 (isolated)        │  │
│  ├──────────────────────────┤      ├───────────────────────────┤  │
│  │                          │      │                           │  │
│  │  ┌──────────────────┐    │      │  ┌───────────────────┐   │  │
│  │  │  TaskExecutor    │    │      │  │  RTExecutive      │   │  │
│  │  │  • 경로 계획     │    │      │  │  • Cyclic Loop    │   │  │
│  │  │  • Sequence      │    │      │  │  • 1ms Actions    │   │  │
│  │  └──────────────────┘    │      │  └───────────────────┘   │  │
│  │           │              │      │           │               │  │
│  │  ┌────────▼─────────┐    │      │  ┌────────▼──────────┐   │  │
│  │  │  DataStore       │    │      │  │  RTDataStore      │   │  │
│  │  │  (Master)        │    │      │  │  (RT Cache)       │   │  │
│  │  ├──────────────────┤    │      │  ├───────────────────┤   │  │
│  │  │ • TBB HashMap    │    │      │  │ • Fixed Array[512]│   │  │
│  │  │ • 동적 할당      │    │      │  │ • Lock-Free       │   │  │
│  │  │ • Observer       │    │      │  │ • No Exception    │   │  │
│  │  │ • 로깅/영속화    │    │      │  │ • Atomic Access   │   │  │
│  │  │ • EventBus 통합  │    │      │  │                   │   │  │
│  │  └──────────────────┘    │      │  └───────────────────┘   │  │
│  │           │              │      │           │               │  │
│  │  ┌────────▼─────────┐    │      │  ┌────────▼──────────┐   │  │
│  │  │  EventBus        │    │      │  │  State Machine    │   │  │
│  │  │  Logger          │    │      │  │  Watchdog         │   │  │
│  │  └──────────────────┘    │      │  └───────────────────┘   │  │
│  └──────────────────────────┘      └───────────────────────────┘  │
│              │                                   │                 │
│              │      Shared Memory Region         │                 │
│              │      (DataSynchronizer)           │                 │
│              └───────────────┬───────────────────┘                 │
│                              │                                     │
│                  ┌───────────▼──────────┐                          │
│                  │  Sync Region (mmap)  │                          │
│                  ├──────────────────────┤                          │
│                  │  RT → Non-RT         │                          │
│                  │  • robot_mode        │  10ms 주기               │
│                  │  • position (x,y)    │                          │
│                  │  • velocity          │                          │
│                  │  • timestamp         │                          │
│                  ├──────────────────────┤                          │
│                  │  Non-RT → RT         │                          │
│                  │  • max_velocity      │  100ms 주기              │
│                  │  • PID params        │                          │
│                  │  • task_commands     │                          │
│                  ├──────────────────────┤                          │
│                  │  Heartbeat           │                          │
│                  │  • rt_heartbeat_ns   │                          │
│                  │  • nonrt_heartbeat_ns│                          │
│                  └──────────────────────┘                          │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

### 9.5 데이터 흐름 및 동기화

#### 9.5.1 RT → Non-RT 흐름 (상태 공유)

```
┌─────────────────────────────────────────────────────┐
│  RT Process (1ms Cycle)                             │
│                                                     │
│  T=0ms:  read_sensor() → RTDataStore.set()         │
│          (SENSOR_POSITION_X = 10.5)                 │
│                                                     │
│  T=10ms: DataSynchronizer.syncFromRTDataStore()    │
│          RTDataStore → SharedMemory                 │
│          (10ms마다 상태 복사)                        │
└─────────────────────────────────────────────────────┘
                          ↓
              Shared Memory (Position: 10.5)
                          ↓
┌─────────────────────────────────────────────────────┐
│  Non-RT Process (10ms Polling)                      │
│                                                     │
│  T=10ms: DataSynchronizer.applyRTStatus()          │
│          SharedMemory → DataStore                   │
│          DataStore.set("position_x", 10.5)          │
│                                                     │
│  T=10ms: EventBus.publish(PositionChanged)         │
│          → UI 업데이트                               │
│          → Logger 기록                               │
│          → DataStore.saveState()                    │
└─────────────────────────────────────────────────────┘
```

#### 9.5.2 Non-RT → RT 흐름 (파라미터/명령)

```
┌─────────────────────────────────────────────────────┐
│  Non-RT Process                                     │
│                                                     │
│  User: "Set max velocity to 2.0 m/s"               │
│  ↓                                                  │
│  DataStore.set("max_velocity", 2.0)                │
│  ↓                                                  │
│  T=100ms: DataSynchronizer.syncToRTDataStore()     │
│           DataStore → SharedMemory                  │
│           (100ms마다 파라미터 복사)                  │
└─────────────────────────────────────────────────────┘
                          ↓
              Shared Memory (max_vel: 2.0)
                          ↓
┌─────────────────────────────────────────────────────┐
│  RT Process (100ms Check)                           │
│                                                     │
│  T=100ms: DataSynchronizer.applyNonRTParams()      │
│           SharedMemory → RTDataStore                │
│           RTDataStore.set(PARAM_MAX_VELOCITY, 2.0)  │
│                                                     │
│  T=101ms: control_loop()                           │
│           RTDataStore.get(PARAM_MAX_VELOCITY, &val) │
│           clamp(velocity, val)  ← 새 파라미터 적용  │
└─────────────────────────────────────────────────────┘
```

### 9.6 구현 예시: 통합 메인 함수

#### RT 프로세스 (mxrc-rt/main.cpp)

```cpp
#include "rt/RTExecutive.h"
#include "rt/RTDataStore.h"
#include "sync/DataSynchronizer.h"
#include <sys/mman.h>

int main() {
    // 실시간 설정
    setPriority(SCHED_FIFO, 90);
    mlockall(MCL_CURRENT | MCL_FUTURE);

    // RTDataStore 생성 (고정 메모리)
    RTDataStore rt_ds;

    // 공유 메모리 생성
    int shm_fd = shm_open("/mxrc_data_sync", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(DataSynchronizer::SharedMemoryRegion));
    auto* shm = static_cast<DataSynchronizer::SharedMemoryRegion*>(
        mmap(nullptr, sizeof(DataSynchronizer::SharedMemoryRegion),
             PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)
    );

    RTExecutive rtExec;

    // 1ms: 센서 읽기 → RTDataStore
    rtExec.registerAction("sensor_read", [&rt_ds](RTContext& ctx) {
        float position = readSensor();  // 하드웨어 읽기
        rt_ds.set_f32(RTDataStore::Key::SENSOR_POSITION_X, position);
    }, 1);

    // 1ms: 제어 계산 (RTDataStore 사용)
    rtExec.registerAction("control_loop", [&rt_ds](RTContext& ctx) {
        float pos, vel, max_vel;
        rt_ds.get_f32(RTDataStore::Key::SENSOR_POSITION_X, &pos);
        rt_ds.get_f32(RTDataStore::Key::SENSOR_VELOCITY, &vel);
        rt_ds.get_f32(RTDataStore::Key::PARAM_MAX_VELOCITY, &max_vel);

        float torque = pidController.compute(pos, vel, max_vel);
        rt_ds.set_f32(RTDataStore::Key::CONTROL_MOTOR_TORQUE, torque);

        writeActuator(torque);  // 하드웨어 쓰기
    }, 1);

    // 10ms: RT → Non-RT 동기화
    rtExec.registerAction("sync_to_nonrt", [&rt_ds, shm](RTContext& ctx) {
        DataSynchronizer::syncFromRTDataStore(&rt_ds, shm);
    }, 10);

    // 100ms: Non-RT → RT 동기화
    rtExec.registerAction("sync_from_nonrt", [&rt_ds, shm](RTContext& ctx) {
        DataSynchronizer::applyNonRTParams(&rt_ds, shm);

        // Non-RT 살아있는지 체크
        uint64_t nonrt_hb = shm->nonrt_heartbeat_ns.load();
        uint64_t now = getCurrentTimeNs();
        if (now - nonrt_hb > 500'000'000ULL) {  // 500ms timeout
            ctx.state_machine->transition(State::SAFE_MODE);
        }
    }, 100);

    rtExec.start();
    setupHardwareWatchdog();

    // RT 루프 블로킹
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
```

#### Non-RT 프로세스 (mxrc-nonrt/main.cpp)

```cpp
#include "core/task/core/TaskExecutor.h"
#include "core/datastore/DataStore.h"
#include "core/event/core/EventBus.h"
#include "sync/DataSynchronizer.h"
#include <sys/mman.h>

int main() {
    // Non-RT DataStore 생성 (기존 코드 재사용)
    auto dataStore = DataStore::create();
    auto eventBus = std::make_shared<EventBus>();
    TaskExecutor executor(...);

    // 공유 메모리 열기
    int shm_fd = shm_open("/mxrc_data_sync", O_RDWR, 0666);
    auto* shm = static_cast<DataSynchronizer::SharedMemoryRegion*>(
        mmap(nullptr, sizeof(DataSynchronizer::SharedMemoryRegion),
             PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)
    );

    // 파라미터 초기화
    dataStore->set("max_velocity", 1.0f, DataType::Para);
    dataStore->set("pid_kp", 0.5f, DataType::Para);
    dataStore->set("pid_ki", 0.1f, DataType::Para);
    dataStore->set("pid_kd", 0.05f, DataType::Para);

    // 주기적 동기화 스레드
    std::thread sync_thread([dataStore, shm]() {
        while (true) {
            // Non-RT → RT (100ms)
            DataSynchronizer::syncToRTDataStore(dataStore.get(), shm);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    std::thread status_thread([dataStore, shm, eventBus]() {
        while (true) {
            // RT → Non-RT (10ms)
            DataSynchronizer::applyRTStatus(dataStore.get(), shm);

            // 변경 사항 이벤트 발행
            auto robot_mode = dataStore->get<int32_t>("robot_mode");
            eventBus->publish(std::make_shared<RobotModeChangedEvent>(robot_mode));

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // 메인 루프 (Task 실행, UI 업데이트 등)
    while (true) {
        try {
            // Task 실행
            executor.update();

            // UI 업데이트
            ui.updatePositionDisplay(dataStore->get<float>("position_x"));

            // Heartbeat
            shm->nonrt_heartbeat_ns.store(getCurrentTimeNs());

        } catch (std::exception& e) {
            // Non-RT 예외는 여기서 처리, RT에 영향 없음
            spdlog::error("Non-RT error: {}", e.what());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    sync_thread.join();
    status_thread.join();
    return 0;
}
```

### 9.7 DataStore 통합의 장점

| 측면 | 장점 | 구체적 개선 |
|------|------|------------|
| **실시간성** | ✅ RT 데이터는 고정 메모리, Lock-Free | WCET 보장, Jitter < 1μs |
| **격리성** | ✅ Non-RT 크래시해도 RT DataStore 무관 | 안전성 극대화 |
| **확장성** | ✅ 기존 DataStore 코드 재사용 | 개발 비용 최소화 |
| **관찰성** | ✅ Non-RT에서 로깅, 모니터링, 영속화 | EventBus, Observer 활용 |
| **유지보수** | ✅ 명확한 역할 분리 | RT/Non-RT 데이터 경계 명확 |

### 9.8 메모리 사용량 분석

```
RT 프로세스:
├─ RTDataStore: 512 keys × 64 bytes  = 32KB (고정)
├─ RTExecutive: Action slots         = 10KB
├─ Stack + Code                      = 2MB
└─ SharedMemory (mmap)               = 4KB
   ──────────────────────────────────────
   Total RT Memory:                   ~2.1MB (고정, 락됨)

Non-RT 프로세스:
├─ DataStore (TBB hashmap)           = ~100KB - 10MB (동적)
├─ EventBus queue                    = ~1MB
├─ Logger buffer                     = ~10MB
├─ TaskExecutor                      = ~1MB
└─ SharedMemory (mmap)               = 4KB
   ──────────────────────────────────────
   Total Non-RT Memory:               ~15-30MB (동적)
```

### 9.9 데이터 일관성 보장

#### 문제: Torn Reads/Writes

```cpp
// ❌ 문제 상황: 64비트 데이터의 비원자적 접근
struct Data {
    double value;  // 8 bytes, 32비트 시스템에서 2번의 워드 접근
};

// Thread 1: Write
data.value = 3.14159;  // 상위 4바이트, 하위 4바이트 순차 쓰기

// Thread 2: Read (동시)
double val = data.value;  // Torn read: 섞인 값 읽을 수 있음
```

#### 해결책: Atomic 또는 Sequence Number

```cpp
// ✅ 해결 1: std::atomic (단일 값)
std::atomic<double> value;

// ✅ 해결 2: Sequence Number (복합 구조체)
struct RTData {
    RTValue value;
    uint32_t sequence_start;
    // ... data fields ...
    uint32_t sequence_end;
};

// Writer
void write(float val) {
    data.sequence_start++;
    data.value.f32 = val;  // 실제 쓰기
    data.sequence_end = data.sequence_start;  // 완료 표시
}

// Reader (일관성 체크)
float read() {
    uint32_t seq1, seq2;
    float val;
    do {
        seq1 = data.sequence_start;
        val = data.value.f32;  // 읽기
        seq2 = data.sequence_end;
    } while (seq1 != seq2);  // 불일치 시 재시도
    return val;
}
```

### 9.10 결론

**DataStore를 RT/Non-RT 아키텍처에 통합하는 최적 방안:**

1. **Dual DataStore** (권장 ⭐⭐⭐⭐⭐)
   - RT: RTDataStore (고정 배열, Lock-Free, 예외 없음)
   - Non-RT: DataStore (기존 TBB, 동적 할당, Observer)

2. **주기적 동기화**
   - RT → Non-RT: 10ms (상태 공유)
   - Non-RT → RT: 100ms (파라미터 갱신)

3. **완벽한 격리**
   - Non-RT 크래시 → RT는 마지막 파라미터로 안전 동작
   - RT 크래시 → 하드웨어 Watchdog → 시스템 리셋

4. **기존 코드 재사용**
   - DataStore 66개 테스트 그대로 유지
   - EventBus, Observer 패턴 활용

5. **점진적 도입**
   - Phase 1: RTDataStore 구현 (2주)
   - Phase 2: 동기화 메커니즘 (1주)
   - Phase 3: 통합 테스트 (1주)

---

## 10. 동적 주기 설정: Configurable Cyclic Executive

### 10.1 현재 문제점

**기존 Cyclic Executive의 한계:**

```cpp
// 현재: 고정된 주기만 사용 가능
class RTExecutive {
    static constexpr size_t MAJOR_CYCLE_MS = 100;  // 고정
    static constexpr size_t MINOR_CYCLE_MS = 1;    // 고정
    static constexpr size_t SLOTS_PER_MAJOR = 100; // 고정
};

// 1ms, 10ms, 100ms 주기만 사용 가능
rtExec.registerAction("sensor", callback, 1);   // ✅ OK
rtExec.registerAction("control", callback, 5);  // ❌ 5ms는 불가능!
rtExec.registerAction("logging", callback, 50); // ❌ 50ms는 불가능!
```

**문제점:**
- ❌ 임의의 주기 추가 불가 (1, 10, 100ms만 가능)
- ❌ 새 주기 추가 시 코드 재컴파일 필요
- ❌ 로봇별 커스터마이징 어려움
- ❌ 실행 시점 주기 변경 불가

### 10.2 해결 방안: LCM 기반 동적 스케줄링

**핵심 아이디어:**
1. **설정 파일**로 원하는 주기들 정의
2. **LCM(최소공배수)** 계산으로 Major Cycle 자동 결정
3. **GCD(최대공약수)** 계산으로 Minor Cycle 자동 결정
4. **빌드 타임** 또는 **런타임** 스케줄 생성

#### 설정 파일 예시 (JSON)

```json
// config/rt_schedule.json
{
  "periods_ms": [1, 5, 10, 20, 50, 100],  // 원하는 주기들
  "actions": [
    {
      "name": "sensor_read",
      "period_ms": 1,
      "wcet_us": 50,
      "priority": "HIGH"
    },
    {
      "name": "control_loop",
      "period_ms": 5,
      "wcet_us": 200,
      "priority": "HIGH"
    },
    {
      "name": "state_machine",
      "period_ms": 10,
      "wcet_us": 100,
      "priority": "MEDIUM"
    },
    {
      "name": "sync_to_nonrt",
      "period_ms": 20,
      "wcet_us": 150,
      "priority": "MEDIUM"
    },
    {
      "name": "diagnostics",
      "period_ms": 50,
      "wcet_us": 300,
      "priority": "LOW"
    },
    {
      "name": "sync_from_nonrt",
      "period_ms": 100,
      "wcet_us": 500,
      "priority": "LOW"
    }
  ]
}
```

### 10.3 LCM/GCD 기반 스케줄 계산

```cpp
// src/core/rt/ScheduleCalculator.h
#include <numeric>
#include <algorithm>

class ScheduleCalculator {
public:
    struct ScheduleParams {
        uint32_t minor_cycle_ms;  // GCD of all periods
        uint32_t major_cycle_ms;  // LCM of all periods
        uint32_t num_slots;       // major / minor
    };

    /**
     * @brief 주기들의 GCD, LCM 계산
     * @param periods_ms 사용하려는 모든 주기 (ms 단위)
     * @return 계산된 스케줄 파라미터
     */
    static ScheduleParams calculate(const std::vector<uint32_t>& periods_ms) {
        if (periods_ms.empty()) {
            return {1, 100, 100};  // 기본값
        }

        // GCD: 모든 주기의 최대공약수 → Minor Cycle
        uint32_t minor = periods_ms[0];
        for (size_t i = 1; i < periods_ms.size(); ++i) {
            minor = std::gcd(minor, periods_ms[i]);
        }

        // LCM: 모든 주기의 최소공배수 → Major Cycle
        uint32_t major = periods_ms[0];
        for (size_t i = 1; i < periods_ms.size(); ++i) {
            major = std::lcm(major, periods_ms[i]);
        }

        // 안전성 체크
        if (major > MAX_MAJOR_CYCLE_MS) {
            // LCM이 너무 크면 경고
            Logger::get()->warn(
                "[ScheduleCalculator] Major cycle {}ms exceeds max {}ms. "
                "Consider adjusting periods.",
                major, MAX_MAJOR_CYCLE_MS
            );
        }

        return {
            .minor_cycle_ms = minor,
            .major_cycle_ms = major,
            .num_slots = major / minor
        };
    }

private:
    static constexpr uint32_t MAX_MAJOR_CYCLE_MS = 1000;  // 1초 이하 권장
};
```

**예시 계산:**

```cpp
// 예: periods = [1, 5, 10, 20, 50, 100]
// GCD(1, 5, 10, 20, 50, 100) = 1  → Minor Cycle = 1ms
// LCM(1, 5, 10, 20, 50, 100) = 100 → Major Cycle = 100ms
// Slots = 100 / 1 = 100개

// 예: periods = [2, 4, 8, 16]
// GCD(2, 4, 8, 16) = 2   → Minor Cycle = 2ms
// LCM(2, 4, 8, 16) = 16  → Major Cycle = 16ms
// Slots = 16 / 2 = 8개
```

### 10.4 동적 스케줄 생성 구현

#### Option 1: 빌드 타임 생성 (권장 ⭐⭐⭐⭐⭐)

**장점:**
- ✅ 실행 시점 오버헤드 없음
- ✅ 컴파일 타임 검증 가능
- ✅ 메모리 사용량 최소화

**구현:**

```cpp
// tools/schedule_generator.cpp (빌드 도구)
#include <nlohmann/json.hpp>
#include <fstream>

class ScheduleGenerator {
public:
    void generateFromConfig(const std::string& config_path,
                           const std::string& output_path) {
        // 1. JSON 읽기
        std::ifstream file(config_path);
        nlohmann::json config = nlohmann::json::parse(file);

        auto periods = config["periods_ms"].get<std::vector<uint32_t>>();
        auto actions = config["actions"];

        // 2. LCM/GCD 계산
        auto params = ScheduleCalculator::calculate(periods);

        // 3. C++ 헤더 파일 생성
        std::ofstream out(output_path);
        out << "// Auto-generated from " << config_path << "\n";
        out << "#ifndef MXRC_RT_GENERATED_SCHEDULE_H\n";
        out << "#define MXRC_RT_GENERATED_SCHEDULE_H\n\n";
        out << "namespace mxrc::core::rt {\n\n";

        out << "constexpr uint32_t MINOR_CYCLE_MS = " << params.minor_cycle_ms << ";\n";
        out << "constexpr uint32_t MAJOR_CYCLE_MS = " << params.major_cycle_ms << ";\n";
        out << "constexpr uint32_t NUM_SLOTS = " << params.num_slots << ";\n\n";

        // 4. Action 스케줄 매핑
        out << "struct ActionSchedule {\n";
        out << "    const char* name;\n";
        out << "    uint32_t period_ms;\n";
        out << "    uint32_t period_slots;\n";
        out << "};\n\n";

        out << "constexpr ActionSchedule ACTIONS[] = {\n";
        for (const auto& action : actions) {
            std::string name = action["name"];
            uint32_t period_ms = action["period_ms"];
            uint32_t period_slots = period_ms / params.minor_cycle_ms;

            out << "    {\"" << name << "\", "
                << period_ms << ", " << period_slots << "},\n";
        }
        out << "};\n\n";

        out << "} // namespace mxrc::core::rt\n";
        out << "#endif // MXRC_RT_GENERATED_SCHEDULE_H\n";

        std::cout << "Generated schedule:\n";
        std::cout << "  Minor Cycle: " << params.minor_cycle_ms << "ms\n";
        std::cout << "  Major Cycle: " << params.major_cycle_ms << "ms\n";
        std::cout << "  Num Slots: " << params.num_slots << "\n";
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: schedule_generator <config.json> <output.h>\n";
        return 1;
    }

    ScheduleGenerator gen;
    gen.generateFromConfig(argv[1], argv[2]);
    return 0;
}
```

**CMakeLists.txt 통합:**

```cmake
# CMakeLists.txt
# 1. 스케줄 생성기 빌드
add_executable(schedule_generator tools/schedule_generator.cpp)
target_link_libraries(schedule_generator PRIVATE nlohmann_json::nlohmann_json)

# 2. 빌드 시점에 스케줄 생성
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/generated/RTSchedule.h
    COMMAND schedule_generator
            ${CMAKE_SOURCE_DIR}/config/rt_schedule.json
            ${CMAKE_BINARY_DIR}/generated/RTSchedule.h
    DEPENDS ${CMAKE_SOURCE_DIR}/config/rt_schedule.json
    COMMENT "Generating RT schedule from config..."
)

# 3. 생성된 헤더를 메인 타겟에 포함
add_custom_target(generate_schedule
    DEPENDS ${CMAKE_BINARY_DIR}/generated/RTSchedule.h)
add_dependencies(mxrc generate_schedule)

target_include_directories(mxrc PRIVATE ${CMAKE_BINARY_DIR}/generated)
```

**생성된 헤더 파일 예시:**

```cpp
// generated/RTSchedule.h (자동 생성됨)
#ifndef MXRC_RT_GENERATED_SCHEDULE_H
#define MXRC_RT_GENERATED_SCHEDULE_H

namespace mxrc::core::rt {

constexpr uint32_t MINOR_CYCLE_MS = 1;
constexpr uint32_t MAJOR_CYCLE_MS = 100;
constexpr uint32_t NUM_SLOTS = 100;

struct ActionSchedule {
    const char* name;
    uint32_t period_ms;
    uint32_t period_slots;
};

constexpr ActionSchedule ACTIONS[] = {
    {"sensor_read", 1, 1},
    {"control_loop", 5, 5},
    {"state_machine", 10, 10},
    {"sync_to_nonrt", 20, 20},
    {"diagnostics", 50, 50},
    {"sync_from_nonrt", 100, 100},
};

} // namespace mxrc::core::rt
#endif
```

#### Option 2: 런타임 동적 생성 (유연성 필요 시)

**장점:**
- ✅ 실행 중 주기 변경 가능
- ✅ 설정 파일 수정 시 재빌드 불필요

**단점:**
- ⚠️ 초기화 시간 증가
- ⚠️ 메모리 오버헤드

**구현:**

```cpp
// src/core/rt/DynamicRTExecutive.h
class DynamicRTExecutive {
public:
    /**
     * @brief JSON 설정 파일로부터 스케줄 로드
     * @note 초기화 단계(Non-RT)에서만 호출!
     */
    int loadScheduleFromConfig(const std::string& config_path) {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            return -1;
        }

        nlohmann::json config = nlohmann::json::parse(file);
        auto periods = config["periods_ms"].get<std::vector<uint32_t>>();

        // LCM/GCD 계산
        auto params = ScheduleCalculator::calculate(periods);

        minor_cycle_ms_ = params.minor_cycle_ms;
        major_cycle_ms_ = params.major_cycle_ms;
        num_slots_ = params.num_slots;

        // 스케줄 테이블 사전 할당
        schedule_table_.resize(num_slots_);

        Logger::get()->info(
            "[DynamicRTExecutive] Schedule loaded: minor={}ms, major={}ms, slots={}",
            minor_cycle_ms_, major_cycle_ms_, num_slots_
        );

        return 0;
    }

    /**
     * @brief Action 등록
     * @param name Action 이름
     * @param period_ms 실행 주기 (ms)
     * @param callback 실행 함수
     */
    int registerAction(
        const std::string& name,
        uint32_t period_ms,
        std::function<void(RTContext&)> callback
    ) {
        // 주기가 minor cycle의 배수인지 확인
        if (period_ms % minor_cycle_ms_ != 0) {
            Logger::get()->error(
                "[DynamicRTExecutive] Period {}ms is not multiple of minor cycle {}ms",
                period_ms, minor_cycle_ms_
            );
            return -1;
        }

        uint32_t period_slots = period_ms / minor_cycle_ms_;

        // 첫 실행 슬롯에 등록
        RTActionSlot slot{
            .name = name,
            .callback = callback,
            .period_slots = period_slots,
            .next_slot = 0  // 0번 슬롯부터 시작
        };

        schedule_table_[0].push_back(slot);

        Logger::get()->info(
            "[DynamicRTExecutive] Registered action '{}' with period {}ms (every {} slots)",
            name, period_ms, period_slots
        );

        return 0;
    }

    /**
     * @brief RT 루프 시작
     */
    void start() {
        setPriority(SCHED_FIFO, 90);
        mlockall(MCL_CURRENT | MCL_FUTURE);

        uint32_t current_slot = 0;

        while (running_) {
            auto cycle_start = getMonotonicTimeNs();

            // 현재 슬롯의 모든 액션 실행
            for (auto& action_slot : schedule_table_[current_slot]) {
                if (current_slot % action_slot.period_slots == action_slot.next_slot % action_slot.period_slots) {
                    action_slot.callback(context_);
                }
            }

            // 다음 슬롯
            current_slot = (current_slot + 1) % num_slots_;

            // 정확한 주기 대기
            waitUntilNextCycle(cycle_start, minor_cycle_ms_ * 1'000'000ULL);
        }
    }

private:
    struct RTActionSlot {
        std::string name;
        std::function<void(RTContext&)> callback;
        uint32_t period_slots;
        uint32_t next_slot;
    };

    uint32_t minor_cycle_ms_{1};
    uint32_t major_cycle_ms_{100};
    uint32_t num_slots_{100};

    std::vector<std::vector<RTActionSlot>> schedule_table_;
    RTContext context_;
    std::atomic<bool> running_{true};
};
```

### 10.5 사용 예시

#### 빌드 타임 스케줄 사용

```cpp
// src/rt_main.cpp
#include "generated/RTSchedule.h"  // 자동 생성된 헤더
#include "core/rt/RTExecutive.h"

int main() {
    using namespace mxrc::core::rt;

    // 1. 생성된 파라미터로 Executive 초기화
    RTExecutive rtExec(MINOR_CYCLE_MS, MAJOR_CYCLE_MS);

    // 2. 생성된 스케줄에 따라 Action 등록
    for (const auto& action : ACTIONS) {
        rtExec.registerAction(
            action.name,
            [action](RTContext& ctx) {
                // Action 구현
                if (std::strcmp(action.name, "sensor_read") == 0) {
                    readSensors(ctx);
                } else if (std::strcmp(action.name, "control_loop") == 0) {
                    runControlLoop(ctx);
                }
                // ...
            },
            action.period_ms
        );
    }

    // 3. RT 루프 시작
    rtExec.start();
}
```

#### 런타임 동적 스케줄 사용

```cpp
// src/rt_main.cpp
int main() {
    DynamicRTExecutive rtExec;

    // 1. 설정 파일에서 스케줄 로드 (초기화 단계)
    if (rtExec.loadScheduleFromConfig("config/rt_schedule.json") != 0) {
        std::cerr << "Failed to load schedule\n";
        return 1;
    }

    // 2. Action 등록
    rtExec.registerAction("sensor_read", 1, [](RTContext& ctx) {
        // 1ms마다 센서 읽기
        float pos = readSensor();
        ctx.datastore->set_f32(Key::SENSOR_POSITION_X, pos);
    });

    rtExec.registerAction("control_loop", 5, [](RTContext& ctx) {
        // 5ms마다 제어 루프
        float pos, vel;
        ctx.datastore->get_f32(Key::SENSOR_POSITION_X, &pos);
        float torque = pidController.compute(pos, vel);
        writeActuator(torque);
    });

    rtExec.registerAction("sync_to_nonrt", 20, [](RTContext& ctx) {
        // 20ms마다 Non-RT 동기화
        DataSynchronizer::syncToNonRT(ctx.datastore, ctx.shm);
    });

    // 3. RT 루프 시작
    rtExec.start();
}
```

### 10.6 주기 설정 검증 도구

```cpp
// tools/validate_schedule.cpp
class ScheduleValidator {
public:
    struct ValidationResult {
        bool valid;
        std::string error_message;
        uint32_t cpu_utilization_percent;
    };

    /**
     * @brief 스케줄 유효성 검증
     */
    static ValidationResult validate(const std::string& config_path) {
        std::ifstream file(config_path);
        nlohmann::json config = nlohmann::json::parse(file);

        auto periods = config["periods_ms"].get<std::vector<uint32_t>>();
        auto actions = config["actions"];

        // 1. LCM/GCD 계산
        auto params = ScheduleCalculator::calculate(periods);

        // 2. Major Cycle 크기 체크
        if (params.major_cycle_ms > 1000) {
            return {false, "Major cycle exceeds 1000ms", 0};
        }

        // 3. CPU 사용률 계산 (Liu & Layland)
        double utilization = 0.0;
        for (const auto& action : actions) {
            uint32_t period_ms = action["period_ms"];
            uint32_t wcet_us = action["wcet_us"];
            utilization += (wcet_us / 1000.0) / period_ms;
        }

        uint32_t cpu_percent = static_cast<uint32_t>(utilization * 100);

        // 4. 스케줄 가능성 체크 (Cyclic Executive는 100% 사용 가능하지만 70% 권장)
        if (cpu_percent > 70) {
            return {
                false,
                fmt::format("CPU utilization {}% exceeds recommended 70%", cpu_percent),
                cpu_percent
            };
        }

        // 5. WCET 검증
        for (const auto& action : actions) {
            uint32_t wcet_us = action["wcet_us"];
            uint32_t period_ms = action["period_ms"];

            if (wcet_us > period_ms * 1000) {
                return {
                    false,
                    fmt::format("Action '{}' WCET {}us exceeds period {}ms",
                               action["name"].get<std::string>(), wcet_us, period_ms),
                    cpu_percent
                };
            }
        }

        return {true, "Schedule is valid", cpu_percent};
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: validate_schedule <config.json>\n";
        return 1;
    }

    auto result = ScheduleValidator::validate(argv[1]);

    if (result.valid) {
        std::cout << "✅ Schedule is VALID\n";
        std::cout << "   CPU Utilization: " << result.cpu_utilization_percent << "%\n";
        return 0;
    } else {
        std::cerr << "❌ Schedule is INVALID\n";
        std::cerr << "   Error: " << result.error_message << "\n";
        return 1;
    }
}
```

### 10.7 실행 시점 주기 변경 (고급)

**요구사항:** 실행 중에도 Action 주기를 변경하고 싶은 경우

**제약사항:**
- ⚠️ RT 루프 실행 중에는 스케줄 변경 **불가**
- ✅ Non-RT에서 변경 요청 → RT는 다음 Major Cycle에 적용

**구현:**

```cpp
class AdaptiveRTExecutive {
public:
    /**
     * @brief 주기 변경 요청 (Non-RT에서 호출)
     */
    void requestPeriodChange(const std::string& action_name, uint32_t new_period_ms) {
        PeriodChangeRequest req{
            .action_name = action_name,
            .new_period_ms = new_period_ms,
            .timestamp_ns = getMonotonicTimeNs()
        };

        // Lock-Free Queue에 요청 삽입
        period_change_queue_.push(req);

        Logger::get()->info(
            "[AdaptiveRTExecutive] Period change requested for '{}': {}ms",
            action_name, new_period_ms
        );
    }

private:
    struct PeriodChangeRequest {
        std::string action_name;
        uint32_t new_period_ms;
        uint64_t timestamp_ns;
    };

    /**
     * @brief RT 루프 (Major Cycle 끝에서 변경 적용)
     */
    void rtLoop() {
        while (running_) {
            // Major Cycle 실행
            for (uint32_t slot = 0; slot < num_slots_; ++slot) {
                executeSlot(slot);
                waitNextMinorCycle();
            }

            // Major Cycle 끝: 주기 변경 요청 처리
            applyPendingChanges();
        }
    }

    void applyPendingChanges() {
        PeriodChangeRequest req;
        while (period_change_queue_.pop(req)) {
            // Action 찾기
            for (auto& slot_list : schedule_table_) {
                for (auto& action : slot_list) {
                    if (action.name == req.action_name) {
                        // 새 주기 적용
                        action.period_slots = req.new_period_ms / minor_cycle_ms_;
                        action.next_slot = 0;  // 다음 Major Cycle부터 시작

                        Logger::get()->info(
                            "[AdaptiveRTExecutive] Applied period change for '{}': {}ms",
                            req.action_name, req.new_period_ms
                        );
                    }
                }
            }
        }
    }

    SPSCQueue<PeriodChangeRequest, 32> period_change_queue_;
};
```

### 10.8 비교표: 3가지 방식

| 방식 | 유연성 | 오버헤드 | 검증 | 권장 용도 |
|------|--------|----------|------|-----------|
| **빌드 타임 생성** | ⭐⭐ | ⭐⭐⭐⭐⭐ (없음) | ⭐⭐⭐⭐⭐ (컴파일) | 고정 주기 시스템 (권장) |
| **런타임 로드** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ (초기화만) | ⭐⭐⭐ (런타임) | 설정 변경 빈번 |
| **실행 중 변경** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ (매 Major Cycle) | ⭐⭐ (어려움) | 적응형 제어 |

### 10.9 실제 사용 시나리오

#### 시나리오 1: 로봇 타입별 주기 설정

```json
// config/robot_type_a.json
{
  "robot_type": "manipulator",
  "periods_ms": [1, 5, 10, 100],  // 정밀 제어
  "actions": [
    {"name": "joint_encoder", "period_ms": 1, "wcet_us": 30},
    {"name": "inverse_kinematics", "period_ms": 5, "wcet_us": 150},
    {"name": "trajectory_control", "period_ms": 10, "wcet_us": 200}
  ]
}

// config/robot_type_b.json
{
  "robot_type": "mobile_robot",
  "periods_ms": [10, 20, 50, 100],  // 느린 제어
  "actions": [
    {"name": "lidar_scan", "period_ms": 10, "wcet_us": 500},
    {"name": "path_planning", "period_ms": 50, "wcet_us": 2000},
    {"name": "velocity_control", "period_ms": 20, "wcet_us": 100}
  ]
}
```

#### 시나리오 2: 모드별 주기 전환

```cpp
// 초기화 모드: 느린 주기
rtExec.registerAction("init_check", 100, initCallback);

// 운영 모드로 전환 시
void switchToOperationMode() {
    // Non-RT에서 요청
    rtExec.requestPeriodChange("control_loop", 5);  // 100ms → 5ms
    rtExec.requestPeriodChange("sensor_read", 1);   // 비활성 → 1ms
}

// 안전 모드로 전환 시
void switchToSafeMode() {
    rtExec.requestPeriodChange("control_loop", 50);  // 5ms → 50ms (느리게)
}
```

### 10.10 결론

**동적 주기 설정 구현 방안:**

1. **빌드 타임 생성** (권장 ⭐⭐⭐⭐⭐)
   - JSON 설정 → CMake → 자동 헤더 생성
   - 오버헤드 없음, 컴파일 타임 검증
   - 로봇 타입별 설정 파일 유지

2. **런타임 로드**
   - 설정 변경 빈번한 경우
   - 초기화 시점 오버헤드만 발생

3. **실행 중 변경**
   - Major Cycle 경계에서만 안전하게 변경
   - 모드 전환 시 유용

4. **검증 도구 필수**
   - CPU 사용률 검증
   - WCET 검증
   - LCM 크기 검증

**구현 우선순위:**
1. Phase 1: 빌드 타임 생성 구현 (1주)
2. Phase 2: 검증 도구 개발 (3일)
3. Phase 3: 런타임 로드 추가 (1주)
4. Phase 4: 실행 중 변경 (선택사항, 2주)

---

## 참고 문헌

1. **Real-Time Systems Design Principles**
   - Liu, C. L., & Layland, J. W. (1973). "Scheduling Algorithms for Multiprogramming in a Hard-Real-Time Environment"

2. **Cyclic Executive Pattern**
   - Buttazzo, G. C. (2011). "Hard Real-Time Computing Systems: Predictable Scheduling Algorithms and Applications"

3. **PREEMPT_RT Linux**
   - Reghenzani, F., et al. (2019). "The Real-Time Linux Kernel: A Survey on PREEMPT_RT"

4. **Lock-Free Programming**
   - Herlihy, M., & Shavit, N. (2012). "The Art of Multiprocessor Programming"

5. **State Machine Patterns**
   - Samek, M. (2008). "Practical UML Statecharts in C/C++: Event-Driven Programming for Embedded Systems"

---

**문서 끝**
