# 연구 문서: Event-Enhanced Hybrid Architecture

**날짜**: 2025-11-16
**브랜치**: 019-event-enhanced-hybrid
**목적**: Phase 0 - 기술 선택 및 설계 패턴 연구

## 개요

이 문서는 이벤트 강화 하이브리드 아키텍처 구현을 위한 기술적 의사결정을 위해 수행한 연구 결과를 정리합니다. 주요 연구 주제는 Lock-Free Queue 구현 방식과 이벤트 기반 아키텍처의 best practices입니다.

## 연구 주제 1: Lock-Free Queue 구현 방식

### 의사결정 필요

**문제**: 이벤트 큐를 구현하기 위해 자체 구현할 것인지, 외부 라이브러리(Boost.Lockfree)를 사용할 것인지 결정 필요

### 평가한 대안들

#### 대안 1: 자체 구현 (Single-Producer Single-Consumer Queue)

**장점**:

- 의존성 추가 없음 (프로젝트 자체 완결성)
- 프로젝트 요구사항에 맞게 최적화 가능
- 코드 크기 최소화 (~200 라인 예상)
- 디버깅 및 유지보수 용이

**단점**:

- 개발 시간 추가 필요
- 검증에 시간이 걸림
- 일반적인 lock-free 알고리즘보다 기능이 제한적일 수 있음

**구현 전략**:

```cpp
template<typename T>
class SPSCLockFreeQueue {
private:
    std::vector<T> buffer_;
    std::atomic<size_t> writePos_{0};
    std::atomic<size_t> readPos_{0};
    size_t capacity_;

public:
    SPSCLockFreeQueue(size_t capacity)
        : buffer_(capacity), capacity_(capacity) {}

    bool tryPush(const T& item) {
        size_t currentWrite = writePos_.load(std::memory_order_relaxed);
        size_t nextWrite = (currentWrite + 1) % capacity_;

        if (nextWrite == readPos_.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }

        buffer_[currentWrite] = item;
        writePos_.store(nextWrite, std::memory_order_release);
        return true;
    }

    bool tryPop(T& item) {
        size_t currentRead = readPos_.load(std::memory_order_relaxed);

        if (currentRead == writePos_.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }

        item = buffer_[currentRead];
        readPos_.store((currentRead + 1) % capacity_, std::memory_order_release);
        return true;
    }

    size_t size() const {
        size_t write = writePos_.load(std::memory_order_acquire);
        size_t read = readPos_.load(std::memory_order_acquire);
        return (write >= read) ? (write - read) : (capacity_ - read + write);
    }
};
```

**검증 방법**:

- ThreadSanitizer를 사용한 data race 검출
- 스트레스 테스트 (1M+ 메시지 push/pop)
- 성능 벤치마크 (10,000 ops/sec 이상 확인)

#### 대안 2: Boost.Lockfree (boost::lockfree::spsc_queue)

**장점**:

- 검증된 구현 (production-ready)
- 즉시 사용 가능
- 다양한 플랫폼에서 최적화됨
- 풍부한 테스트 커버리지

**단점**:

- 외부 의존성 추가 (Boost 라이브러리)
- Boost 전체를 빌드하거나 header-only 모드 설정 필요
- 프로젝트 의존성 복잡도 증가
- 바이너리 크기 증가 가능

**사용 예시**:

```cpp
#include <boost/lockfree/spsc_queue.hpp>

boost::lockfree::spsc_queue<std::shared_ptr<IEvent>> eventQueue_(10000);

// Push
eventQueue_.push(event);

// Pop
std::shared_ptr<IEvent> event;
if (eventQueue_.pop(event)) {
    // Process event
}
```

#### 대안 3: std::queue + std::mutex (Lock-Based)

**장점**:

- 구현이 매우 간단
- 표준 라이브러리만 사용
- 정확성 보장이 쉬움

**단점**:

- Lock contention으로 인한 성능 저하
- 실시간 성능에 악영향 (우선순위 역전 가능성)
- 본 프로젝트의 lock-free 목표에 부합하지 않음

### 선택된 대안: 자체 구현 (SPSC Lock-Free Queue)

**결정**: Single-Producer Single-Consumer (SPSC) Lock-Free Queue를 자체 구현

**이유**:

1. **의존성 최소화**: Boost 라이브러리 추가 없이 프로젝트 자체 완결성 유지
2. **단순한 사용 패턴**: 이벤트 발행은 여러 스레드에서 가능하지만, 실제 큐 push는 EventBus의 publish() 함수에서만 수행 (Single Producer). 이벤트 처리는 단일 dispatch 스레드에서만 수행 (Single Consumer)
3. **검증 가능성**: SPSC는 MPMC(Multi-Producer Multi-Consumer)보다 구현이 단순하여 검증이 쉬움
4. **성능**: SPSC는 CAS(Compare-And-Swap) 없이 단순한 atomic load/store만으로 구현 가능하여 성능이 우수
5. **프로젝트 철학 부합**: MXRC는 의존성을 최소화하고 핵심 기능을 자체 구현하는 철학을 따름

**마이그레이션 경로**: 성능이 부족할 경우 Boost.Lockfree로 교체 가능하도록 인터페이스를 일반화

### 구현 세부사항

#### Memory Ordering 전략

- **tryPush()**: release semantics (writePos 업데이트 시)
- **tryPop()**: acquire semantics (writePos 읽기 시)
- 이유: 메모리 가시성 보장, 다른 스레드가 쓴 데이터를 읽는 스레드가 올바르게 볼 수 있도록

#### 용량 관리

- 고정 크기 버퍼 (기본 10,000 항목)
- Ring buffer 방식 (circular buffer)
- Full 상태 감지: `(writePos + 1) % capacity == readPos`

#### 성능 목표

- **처리량**: >10,000 ops/sec
- **지연**: Push/Pop 각각 <1μs
- **메모리**: 고정 크기 (10,000 * sizeof(std::shared_ptr) = 약 80KB)

## 연구 주제 2: 이벤트 기반 아키텍처 Best Practices

### 설계 패턴 연구

#### 1. Observer 패턴 vs Event Bus 패턴

**Observer 패턴**:

- 직접 연결: Subject → Observer
- 장점: 단순, 타입 안전
- 단점: 강한 결합, 확장성 낮음

**Event Bus 패턴** (선택됨):

- 중앙 허브: Publisher → EventBus → Subscriber
- 장점: 느슨한 결합, 확장성 높음, 동적 구독/해제
- 단점: 런타임 오버헤드, 타입 안전성 약화 (해결: std::shared_ptr<IEvent> 사용)

**결정**: Event Bus 패턴 선택 (확장성과 느슨한 결합이 중요)

#### 2. Synchronous vs Asynchronous Event Dispatch

**Synchronous** (직접 호출):

- 구독자 콜백을 이벤트 발행 스레드에서 즉시 호출
- 장점: 단순, 이벤트 순서 보장
- 단점: 크리티컬 패스 블로킹 가능, 느린 구독자가 전체 시스템 지연

**Asynchronous** (큐 기반) - 선택됨:

- 이벤트를 큐에 넣고 별도 스레드에서 처리
- 장점: 크리티컬 패스 보호, 구독자 독립성
- 단점: 이벤트 순서 보장 필요, 처리 지연 발생

**결정**: Asynchronous 방식 선택 (크리티컬 패스 보호가 최우선)

#### 3. Event Filtering 전략

**평가한 방법들**:

1. **Type-based filtering**: 이벤트 타입으로만 필터링
2. **Predicate-based filtering**: 사용자 정의 함수로 필터링 (선택됨)
3. **Topic-based filtering**: 주제(topic) 기반 pub/sub

**선택**: Predicate-based filtering

- 타입 + ID + 사용자 정의 조건 모두 지원
- 유연성 최대화

```cpp
using EventFilter = std::function<bool(const std::shared_ptr<IEvent>&)>;

// 타입 필터
auto typeFilter = [](auto event) {
    return event->getType() == EventType::ACTION_STARTED;
};

// ID 필터
auto idFilter = [](auto event) {
    return event->getTargetId() == "action123";
};

// 복합 필터
auto complexFilter = [](auto event) {
    return event->getType() == EventType::SEQUENCE_FAILED &&
           event->getTargetId().starts_with("critical_");
};
```

#### 4. Error Handling in Event System

**원칙**:

1. **발행자 보호**: 이벤트 발행 실패는 발행자에게 영향 없음
2. **구독자 격리**: 한 구독자의 예외가 다른 구독자에게 전파되지 않음
3. **로깅**: 모든 예외는 로그에 기록
4. **통계**: 실패 카운트를 통계로 추적

**구현**:

```cpp
void EventBus::dispatchToSubscribers(std::shared_ptr<IEvent> event) {
    auto subscribers = subscriptionManager_.getSubscribers(event->getType());

    for (const auto& sub : subscribers) {
        try {
            if (sub.filter(event)) {
                sub.callback(event);
                stats_.processedEvents.fetch_add(1);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Subscriber exception for event {}: {}",
                     event->getEventId(), e.what());
            stats_.failedCallbacks.fetch_add(1);
        } catch (...) {
            LOG_ERROR("Unknown subscriber exception for event {}",
                     event->getEventId());
            stats_.failedCallbacks.fetch_add(1);
        }
    }
}
```

## 연구 주제 3: DataStore 통합 패턴

### 문제

기존 DataStore는 Notifier 콜백 방식을 사용. 이를 EventBus와 통합하여 두 시스템이 상호 운용되도록 해야 함.

### 평가한 패턴들

#### 1. Adapter 패턴 (선택됨)

DataStore와 EventBus 사이에 어댑터 계층을 두어 변환

**장점**:

- 관심사 분리 명확
- DataStore는 EventBus를 알 필요 없음
- 양방향 통합 가능

**단점**:

- 추가 계층으로 인한 복잡도

**구현**:

```cpp
class DataStoreEventAdapter {
public:
    DataStoreEventAdapter(
        std::shared_ptr<DataStore> dataStore,
        std::shared_ptr<EventBus> eventBus)
        : dataStore_(dataStore), eventBus_(eventBus) {

        // DataStore → EventBus
        dataStore_->setNotifier([this](const std::string& key,
                                       const std::any& oldVal,
                                       const std::any& newVal) {
            auto event = std::make_shared<DataStoreValueChanged>(
                key, oldVal, newVal, std::chrono::system_clock::now());
            eventBus_->publish(event);
        });
    }

    // EventBus → DataStore 헬퍼
    void subscribeToActionResults() {
        eventBus_->subscribe(
            [](auto e) { return e->getType() == EventType::ACTION_COMPLETED; },
            [this](std::shared_ptr<IEvent> event) {
                auto actionEvent = std::static_pointer_cast<ActionCompletedEvent>(event);
                dataStore_->setWithoutNotify(  // 순환 방지
                    "action." + actionEvent->actionId + ".result",
                    actionEvent->result);
            });
    }
};
```

#### 2. Direct Integration 패턴

DataStore가 직접 EventBus를 사용

**거부 이유**: DataStore에 EventBus 의존성 추가, 단일 책임 원칙 위반

### 순환 업데이트 방지

**문제**: EventBus 이벤트 → DataStore 업데이트 → EventBus 이벤트 → 무한 루프

**해결책**:

1. `setWithoutNotify()` API 제공: Notifier를 일시적으로 비활성화
2. 업데이트 플래그: 업데이트가 이벤트에서 유래했는지 추적

## 연구 주제 4: Real-Time 성능 고려사항

### PREEMPT_RT 환경에서의 Thread Priority

**연구 결과**:

- 크리티컬 패스 스레드: `SCHED_FIFO` with high priority (90-99)
- 이벤트 dispatch 스레드: `SCHED_OTHER` or low priority `SCHED_FIFO` (10-20)
- 이유: 이벤트 처리가 제어 루프에 영향을 주지 않도록

**구현**:

```cpp
// EventBus 생성자에서
void EventBus::setDispatchThreadPriority() {
    struct sched_param param;
    param.sched_priority = 15;  // 낮은 우선순위

    pthread_setschedparam(dispatchThread_.native_handle(),
                         SCHED_FIFO, &param);
}
```

### CPU Affinity

**권장사항**:

- 이벤트 dispatch 스레드를 비-실시간 CPU 코어에 할당
- 크리티컬 패스는 전용 코어 사용

```cpp
cpu_set_t cpuset;
CPU_ZERO(&cpuset);
CPU_SET(3, &cpuset);  // CPU 3번에 할당 (예시)
pthread_setaffinity_np(dispatchThread_.native_handle(),
                       sizeof(cpu_set_t), &cpuset);
```

## 참고 자료

### Lock-Free Programming

1. **"C++ Concurrency in Action"** by Anthony Williams (2nd Edition, 2019)
   - Chapter 7: Designing lock-free concurrent data structures
   - SPSC queue 구현 예제

2. **"The Art of Multiprocessor Programming"** by Maurice Herlihy & Nir Shavit
   - Atomic operations and memory consistency models

3. **cppreference.com**: std::memory_order documentation
   - https://en.cppreference.com/w/cpp/atomic/memory_order

### Event-Driven Architecture

1. **Martin Fowler's "Event-Driven Architecture"**
   - https://martinfowler.com/articles/201701-event-driven.html

2. **"Enterprise Integration Patterns"** by Gregor Hohpe
   - Chapter on Event Message patterns

### Real-Time Linux

1. **Linux Foundation PREEMPT_RT Documentation**
   - Thread priority and scheduling policies

2. **"Real-Time Linux Programming"** by Roderick W. Smith
   - Best practices for real-time application development

## 결론

이 연구를 통해 다음과 같은 기술적 의사결정을 내렸습니다:

1. **Lock-Free Queue**: 자체 구현 SPSC queue (의존성 최소화, 충분한 성능)
2. **Event Bus 패턴**: 중앙 허브 방식 (확장성과 느슨한 결합)
3. **Asynchronous Dispatch**: 큐 기반 비동기 처리 (크리티컬 패스 보호)
4. **Predicate-based Filtering**: 유연한 이벤트 필터링
5. **Adapter 패턴**: DataStore ↔ EventBus 통합
6. **Real-Time Consideration**: 낮은 우선순위 dispatch 스레드, CPU affinity 설정

이제 이 결정들을 바탕으로 Phase 1 (데이터 모델 및 계약 정의)로 진행합니다.
