# 기능 사양서: Event-Enhanced Hybrid Architecture for Task Layer

**기능 브랜치**: `019-event-enhanced-hybrid`
**생성일**: 2025-11-16
**최종 수정**: 2025-11-16
**상태**: Phase 1 - 이벤트 시스템 기반 구축
**버전**: 2.0

## 개요

이 사양서는 MXRC의 Action, Sequence, Task 계층에 이벤트 기반 아키텍처를 도입하여 관찰 가능성(Observability)과 확장성을 향상시키는 것을 목표로 합니다.

**핵심 설계 원칙**:

1. **크리티컬 패스 보호**: 이벤트 발행은 절대 핵심 실행 경로(Direct Calls)를 블로킹하거나 지연시키지 않음
2. **하이브리드 아키텍처**: 기존의 직접 호출 방식을 유지하면서 이벤트 시스템을 추가하는 점진적 접근
3. **논블로킹 이벤트**: 모든 이벤트 발행과 처리는 비동기로 수행되어 실시간 성능에 영향 없음
4. **독립적 확장**: 새로운 모니터링/로깅 컴포넌트를 핵심 로직 수정 없이 추가 가능

## 아키텍처 비전

### 크리티컬 패스 vs 이벤트 경로

```
[크리티컬 패스 - 직접 호출]
TaskExecutor::execute()
    ↓ (Direct Call - Synchronous)
SequenceEngine::execute()
    ↓ (Direct Call - Synchronous)
ActionExecutor::execute()
    ↓ (Direct Call - Synchronous)
IAction::execute()
    ↓
결과 반환 (동기)

[이벤트 경로 - 비동기 발행]
각 계층의 상태 변경
    ↓ (Non-blocking Event Publish)
EventBus::publish() → 이벤트 큐
    ↓ (Async Processing)
구독자들 (모니터링, 로깅, UI 업데이트 등)
```

**설계 철학**:
- 제어 흐름(Control Flow)은 직접 호출로 유지 → 신뢰성과 성능 보장
- 관찰과 부가 기능(Observation & Side Effects)은 이벤트로 처리 → 확장성과 분리

## 사용자 시나리오 및 테스트

### 사용자 스토리 1 - 실시간 실행 상태 모니터링 (우선순위: P1)

개발자가 실행 중인 Task, Sequence, Action의 진행 상태를 실시간으로 모니터링하고, 상태 변화 이벤트를 `EventBus`를 통해 구독하여 외부 시스템(UI, 로깅)에 즉시 전달할 수 있습니다.

**이 우선순위인 이유**: 시스템의 현재 동작을 파악하고 디버깅하기 위한 가장 기본적인 관찰 가능성(Observability)을 확보합니다.

**독립 테스트**: Action 실행 시작/완료 이벤트를 구독하고, 이벤트 수신 여부와 타임스탬프를 검증하여 독립적으로 테스트 가능합니다.

**인수 시나리오**:

1. **주어진 상황** Task 실행 이벤트 리스너가 `EventBus`에 등록되어 있고, **언제** TaskExecutor가 Task를 실행하면, **그러면** `TaskStarted`, `TaskCompleted` 이벤트가 순서대로 발행되고 모든 구독자가 이벤트를 수신합니다.
2. **주어진 상황** Action 상태 변화 모니터링 시스템이 있고, **언제** Action이 `RUNNING` → `COMPLETED`로 전환되면, **그러면** `ActionStatusChanged` 이벤트가 이전 상태, 새 상태, 타임스탬프를 포함하여 발행됩니다.

---

### 사용자 스토리 2 - DataStore와 EventBus 연동 (우선순위: P1)

개발자가 `DataStore`의 데이터 변경을 `EventBus`를 통해 구독하거나, `EventBus`의 이벤트를 통해 `DataStore`의 값을 변경할 수 있습니다.

**이 우선순위인 이유**: 기존의 중앙 상태 저장소(`DataStore`)와 새로운 이벤트 시스템(`EventBus`)을 연결하여, 두 시스템이 시너지를 내는 하이브리드 아키텍처의 핵심을 구현합니다.

**독립 테스트**: `DataStore`의 특정 키를 구독하는 리스너를 `EventBus`에 등록 후, 해당 키의 값을 변경했을 때 `EventBus`로 이벤트가 수신되는지 검증합니다.

**인수 시나리오**:

1. **주어진 상황** `DataStore`의 `robot.mode` 키에 대한 변경 이벤트를 `EventBus` 구독자가 구독하고 있고, **언제** `DataStore`의 `robot.mode` 값이 "AUTO"로 변경되면, **그러면** `EventBus`를 통해 `DataStoreValueChanged` 이벤트가 발행됩니다.
2. **주어진 상황** `EventBus`의 `ActionFailed` 이벤트를 구독하여 `DataStore` 값을 변경하는 리스너가 있고, **언제** 특정 Action이 실패하여 `ActionFailed` 이벤트가 발행되면, **그러면** `DataStore`의 `system.last_error` 키 값이 해당 에러 정보로 업데이트됩니다.

---

### 사용자 스토리 3 - 확장 가능한 모니터링 컴포넌트 추가 (우선순위: P2)

개발자가 핵심 실행 로직을 수정하지 않고, 새로운 모니터링, 로깅, 메트릭 수집 컴포넌트를 `EventBus` 구독만으로 시스템에 추가할 수 있습니다.

**이 우선순위인 이유**: 시스템 확장성과 유지보수성을 대폭 향상시키며, 향후 모니터링 기능 구현의 기반을 제공합니다.

**독립 테스트**: 새로운 이벤트 리스너를 등록하고, 기존 Task를 실행하여 새 리스너가 이벤트를 받는지만 확인하면 됩니다.

**인수 시나리오**:

1. **주어진 상황** 커스텀 메트릭 수집기를 개발했고, **언제** `EventBus`에 실행 시간 이벤트 구독자로 등록하면, **그러면** 기존 코드 수정 없이 모든 Action/Sequence/Task의 실행 시간이 자동으로 수집됩니다.
2. **주어진 상황** 외부 로깅 시스템 통합이 필요하고, **언제** 상태 변화 이벤트 리스너를 구현하여 등록하면, **그러면** 모든 상태 전환이 외부 시스템으로 전송됩니다.

---

### 엣지 케이스

- **이벤트 발행 중 구독자 등록/해제**: 이벤트 발행 도중 새로운 구독자가 추가되거나 기존 구독자가 제거되면 어떻게 됩니까?
  - 현재 발행 중인 이벤트는 발행 시점의 구독자 목록을 사용하고, 다음 이벤트부터 변경된 구독자 목록이 적용됩니다.
- **구독자 처리 중 예외 발생**: 이벤트 구독자의 콜백 함수에서 예외가 발생하면 시스템이 어떻게 됩니까?
  - 예외는 격리되어 해당 구독자만 영향받고, 다른 구독자와 핵심 실행 로직은 계속 동작합니다. 에러는 로깅됩니다.
- **이벤트 큐 오버플로우**: 이벤트 생성 속도가 처리 속도를 초과하면 어떻게 됩니까?
  - 설정 가능한 드롭 정책(가장 오래된 것부터 버리기 등)에 따라 처리됩니다. 크리티컬 패스는 항상 논블로킹으로 유지됩니다.

## 요구사항

### 기능적 요구사항

#### FR-1: 이벤트 발행 (Event Publishing)

##### FR-1.1: 상태 변화 이벤트

- **FR-1.1.1**: Action 계층은 다음 상태 전환 시 이벤트를 발행해야 합니다:
  - `ActionStarted`: PENDING → RUNNING
  - `ActionCompleted`: RUNNING → COMPLETED
  - `ActionFailed`: RUNNING → FAILED
  - `ActionCancelled`: * → CANCELLED
  - `ActionTimeout`: RUNNING → TIMEOUT

- **FR-1.1.2**: Sequence 계층은 다음 상태 전환 시 이벤트를 발행해야 합니다:
  - `SequenceStarted`: PENDING → RUNNING
  - `SequenceCompleted`: RUNNING → COMPLETED
  - `SequenceFailed`: RUNNING → FAILED
  - `SequenceCancelled`: * → CANCELLED
  - `SequencePaused`: RUNNING → PAUSED
  - `SequenceResumed`: PAUSED → RUNNING

- **FR-1.1.3**: Task 계층은 다음 상태 전환 시 이벤트를 발행해야 합니다:
  - `TaskStarted`: IDLE → RUNNING
  - `TaskCompleted`: RUNNING → COMPLETED
  - `TaskFailed`: RUNNING → FAILED
  - `TaskCancelled`: * → CANCELLED
  - `TaskScheduled`: IDLE → SCHEDULED (periodic/triggered 모드)

##### FR-1.2: 진행률 이벤트

- **FR-1.2.1**: Sequence와 Task는 진행률 변경 시 `ProgressUpdated` 이벤트를 발행해야 합니다.
- **FR-1.2.2**: 진행률 이벤트는 과도한 발행을 방지하기 위해 최소 5% 변화 또는 1초 간격으로 제한됩니다.

##### FR-1.3: 논블로킹 발행

- **FR-1.3.1**: 모든 이벤트 발행은 논블로킹 방식으로 수행되어야 합니다.
- **FR-1.3.2**: 이벤트 발행 실패는 크리티컬 패스에 영향을 주지 않아야 합니다.
- **FR-1.3.3**: 이벤트 발행 오버헤드는 기존 실행 시간의 5% 미만이어야 합니다.

#### FR-2: 이벤트 구독 (Event Subscription)

##### FR-2.1: 구독 관리

- **FR-2.1.1**: 개발자는 이벤트 타입별로 콜백 함수를 등록할 수 있어야 합니다.
- **FR-2.1.2**: 구독자는 런타임에 동적으로 등록/해제될 수 있어야 합니다.
- **FR-2.1.3**: 구독 ID를 통해 특정 구독을 식별하고 관리할 수 있어야 합니다.

##### FR-2.2: 필터링

- **FR-2.2.1**: 구독자는 이벤트 타입으로 필터링할 수 있어야 합니다.
- **FR-2.2.2**: 구독자는 특정 ID(actionId, sequenceId, taskId)로 필터링할 수 있어야 합니다.
- **FR-2.2.3**: 구독자는 사용자 정의 필터 함수를 제공할 수 있어야 합니다.

#### FR-3: 이벤트 처리 (Event Processing)

##### FR-3.1: 비동기 처리

- **FR-3.1.1**: 이벤트는 Lock-Free Queue를 통해 비동기로 처리되어야 합니다.
- **FR-3.1.2**: 전용 이벤트 처리 스레드가 큐에서 이벤트를 소비하고 구독자에게 전달합니다.
- **FR-3.1.3**: 이벤트 처리 지연은 99%ile 기준 10ms 미만이어야 합니다.

##### FR-3.2: 예외 격리

- **FR-3.2.1**: 구독자 콜백의 예외는 해당 구독자만 영향받도록 격리되어야 합니다.
- **FR-3.2.2**: 구독자 예외는 로그에 기록되어야 합니다.
- **FR-3.2.3**: 구독자 예외는 다른 구독자나 이벤트 발행자에게 전파되지 않아야 합니다.

##### FR-3.3: 큐 관리

- **FR-3.3.1**: 이벤트 큐는 설정 가능한 최대 크기를 가져야 합니다.
- **FR-3.3.2**: 큐 오버플로우 시 설정 가능한 드롭 정책(oldest-first, newest-first)을 적용해야 합니다.
- **FR-3.3.3**: 큐 상태(크기, 드롭 카운트)를 모니터링할 수 있어야 합니다.

#### FR-4: DataStore 연동

##### FR-4.1: DataStore → EventBus

- **FR-4.1.1**: `DataStore`의 데이터 변경은 `DataStoreValueChanged` 이벤트로 발행되어야 합니다.
- **FR-4.1.2**: 기존 `DataStore::Notifier`는 내부적으로 `EventBus`를 사용하도록 통합되어야 합니다.
- **FR-4.1.3**: 이벤트는 키(key), 이전 값(oldValue), 새 값(newValue)을 포함해야 합니다.

##### FR-4.2: EventBus → DataStore

- **FR-4.2.1**: 이벤트 기반 `DataStore` 업데이트를 위한 헬퍼 함수를 제공해야 합니다.
- **FR-4.2.2**: 순환 업데이트 방지 메커니즘이 있어야 합니다 (무한 루프 방지).

### 비기능적 요구사항

#### NFR-1: 성능

- **NFR-1.1**: 이벤트 발행 오버헤드는 실행 시간의 5% 미만이어야 합니다.
- **NFR-1.2**: 이벤트 처리 지연은 99%ile 기준 10ms 미만이어야 합니다.
- **NFR-1.3**: Lock-Free Queue는 최소 10,000 ops/sec 처리량을 지원해야 합니다.

#### NFR-2: 신뢰성

- **NFR-2.1**: 이벤트 시스템 장애는 크리티컬 패스 실행에 영향을 주지 않아야 합니다.
- **NFR-2.2**: 구독자 장애는 격리되어 다른 구독자에게 영향을 주지 않아야 합니다.
- **NFR-2.3**: 시스템은 이벤트 손실을 감지하고 로그에 기록해야 합니다.

#### NFR-3: 확장성

- **NFR-3.1**: 새로운 이벤트 타입을 기존 코드 수정 없이 추가할 수 있어야 합니다.
- **NFR-3.2**: 새로운 구독자를 기존 코드 수정 없이 추가할 수 있어야 합니다.
- **NFR-3.3**: 최소 100개의 동시 구독자를 지원해야 합니다.

#### NFR-4: 관찰 가능성

- **NFR-4.1**: 모든 이벤트는 고유 ID와 타임스탬프를 가져야 합니다.
- **NFR-4.2**: 이벤트 통계(발행/처리/드롭 카운트)를 조회할 수 있어야 합니다.
- **NFR-4.3**: 구독자 정보(등록된 구독자 수, 타입별 분포)를 조회할 수 있어야 합니다.

### 주요 엔티티 및 컴포넌트

#### 이벤트 시스템 핵심 컴포넌트

```cpp
// 1. Event 기본 인터페이스
class IEvent {
    virtual EventType getType() const = 0;
    virtual std::string getEventId() const = 0;
    virtual std::chrono::system_clock::time_point getTimestamp() const = 0;
    virtual std::string getTargetId() const = 0;  // actionId, sequenceId, taskId
};

// 2. EventBus - 중앙 이벤트 허브
class EventBus {
    // 논블로킹 이벤트 발행
    void publish(std::shared_ptr<IEvent> event);

    // 구독 관리
    SubscriptionId subscribe(EventType type, EventCallback callback);
    SubscriptionId subscribe(EventFilter filter, EventCallback callback);
    void unsubscribe(SubscriptionId id);

    // 상태 조회
    EventBusStats getStats() const;
};

// 3. Subscription - 구독 정보
struct Subscription {
    SubscriptionId id;
    EventFilter filter;
    EventCallback callback;
};

// 4. Lock-Free Event Queue
template<typename T>
class LockFreeQueue {
    bool tryPush(T item);
    bool tryPop(T& item);
    size_t size() const;
};
```

#### 계층별 이벤트 타입

**Action 계층**:

- `ActionStarted`: actionId, actionType, timestamp
- `ActionCompleted`: actionId, result, duration
- `ActionFailed`: actionId, error, duration
- `ActionCancelled`: actionId, reason
- `ActionTimeout`: actionId, timeoutDuration

**Sequence 계층**:

- `SequenceStarted`: sequenceId, totalSteps
- `SequenceStepStarted`: sequenceId, stepIndex, actionId
- `SequenceStepCompleted`: sequenceId, stepIndex, actionId, result
- `SequenceCompleted`: sequenceId, duration
- `SequenceFailed`: sequenceId, failedStepIndex, error
- `SequenceCancelled`: sequenceId, currentStepIndex
- `SequencePaused`: sequenceId, currentStepIndex
- `SequenceResumed`: sequenceId, fromStepIndex
- `SequenceProgressUpdated`: sequenceId, progress (0.0-1.0)

**Task 계층**:

- `TaskStarted`: taskId, taskType, executionMode
- `TaskCompleted`: taskId, result, duration
- `TaskFailed`: taskId, error, duration
- `TaskCancelled`: taskId, reason
- `TaskScheduled`: taskId, nextExecutionTime (periodic/triggered)
- `TaskProgressUpdated`: taskId, progress (0.0-1.0)

**DataStore 계층**:

- `DataStoreValueChanged`: key, oldValue, newValue, timestamp

## 구현 아키텍처

### 디렉토리 구조

```
src/core/event/
├── interfaces/
│   ├── IEvent.h                    # 이벤트 기본 인터페이스
│   └── IEventBus.h                 # EventBus 인터페이스
├── core/
│   ├── EventBus.{h,cpp}           # 중앙 이벤트 버스 구현
│   ├── EventDispatcher.{h,cpp}    # 이벤트 처리 스레드
│   └── SubscriptionManager.{h,cpp} # 구독 관리
├── dto/
│   ├── EventType.h                 # 이벤트 타입 열거형
│   ├── EventBase.h                 # 기본 이벤트 구조체
│   ├── ActionEvents.h              # Action 이벤트 정의
│   ├── SequenceEvents.h            # Sequence 이벤트 정의
│   ├── TaskEvents.h                # Task 이벤트 정의
│   └── DataStoreEvents.h           # DataStore 이벤트 정의
├── util/
│   ├── LockFreeQueue.{h,cpp}      # Lock-Free 큐 구현
│   ├── EventFilter.h               # 이벤트 필터
│   └── EventStats.h                # 이벤트 통계
└── adapters/
    └── DataStoreEventAdapter.{h,cpp} # DataStore ↔ EventBus 어댑터

tests/unit/event/
├── EventBus_test.cpp
├── EventDispatcher_test.cpp
├── SubscriptionManager_test.cpp
├── LockFreeQueue_test.cpp
└── DataStoreEventAdapter_test.cpp
```

### 핵심 구현 패턴


#### 1. 논블로킹 이벤트 발행 패턴

```cpp
// ActionExecutor.cpp
void ActionExecutor::executeAsync(...) {
    // 크리티컬 패스 - 직접 호출
    state.status = ActionStatus::RUNNING;
    auto startTime = std::chrono::steady_clock::now();

    // 이벤트 발행 - 논블로킹 (실패해도 크리티컬 패스에 영향 없음)
    if (eventBus_) {
        try {
            auto event = std::make_shared<ActionStartedEvent>(
                actionId, action->getType(), startTime);
            eventBus_->publish(event);  // 논블로킹
        } catch (...) {
            // 이벤트 발행 실패는 로그만 남기고 무시
            LOG_WARN("Failed to publish ActionStarted event for {}", actionId);
        }
    }

    // 크리티컬 패스 계속 진행
    action->execute(context);
    // ...
}
```

#### 2. EventBus 내부 구현

```cpp
class EventBus : public IEventBus {
public:
    EventBus(size_t queueSize = 10000)
        : eventQueue_(queueSize)
        , running_(true)
        , dispatchThread_(&EventBus::dispatchLoop, this) {}

    void publish(std::shared_ptr<IEvent> event) override {
        // 논블로킹 큐 푸시 (실패 시 즉시 반환)
        if (!eventQueue_.tryPush(event)) {
            stats_.droppedEvents.fetch_add(1);
            LOG_WARN("Event queue full, dropped event: {}", event->getEventId());
        } else {
            stats_.publishedEvents.fetch_add(1);
        }
    }

private:
    void dispatchLoop() {
        while (running_) {
            std::shared_ptr<IEvent> event;
            if (eventQueue_.tryPop(event)) {
                dispatchToSubscribers(event);
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }

    void dispatchToSubscribers(std::shared_ptr<IEvent> event) {
        auto subscribers = subscriptionManager_.getSubscribers(event->getType());
        for (const auto& sub : subscribers) {
            try {
                if (sub.filter(event)) {
                    sub.callback(event);
                }
                stats_.processedEvents.fetch_add(1);
            } catch (const std::exception& e) {
                LOG_ERROR("Subscriber exception: {}", e.what());
                stats_.failedCallbacks.fetch_add(1);
            }
        }
    }

    LockFreeQueue<std::shared_ptr<IEvent>> eventQueue_;
    SubscriptionManager subscriptionManager_;
    std::atomic<bool> running_;
    std::thread dispatchThread_;
    EventBusStats stats_;
};
```

#### 3. DataStore 통합 패턴

```cpp
// DataStoreEventAdapter - DataStore와 EventBus 연결
class DataStoreEventAdapter {
public:
    DataStoreEventAdapter(
        std::shared_ptr<DataStore> dataStore,
        std::shared_ptr<EventBus> eventBus)
        : dataStore_(dataStore)
        , eventBus_(eventBus) {

        // DataStore의 Notifier를 EventBus로 연결
        dataStore_->setNotifier([this](const std::string& key,
                                       const std::any& oldVal,
                                       const std::any& newVal) {
            auto event = std::make_shared<DataStoreValueChanged>(
                key, oldVal, newVal);
            eventBus_->publish(event);
        });
    }

    // EventBus 이벤트 → DataStore 업데이트 헬퍼
    void subscribeToActionResults() {
        eventBus_->subscribe(EventType::ACTION_COMPLETED,
            [this](std::shared_ptr<IEvent> event) {
                auto actionEvent = std::static_pointer_cast<ActionCompletedEvent>(event);
                // 순환 업데이트 방지를 위해 notifier 일시 비활성화
                dataStore_->setWithoutNotify(
                    "action." + actionEvent->actionId + ".result",
                    actionEvent->result);
            });
    }
};
```

### 계층별 이벤트 통합

#### Action 계층 수정 사항

```cpp
// ActionExecutor.h
class ActionExecutor {
public:
    // 생성자에 EventBus 추가 (optional)
    ActionExecutor(std::shared_ptr<EventBus> eventBus = nullptr)
        : eventBus_(eventBus) {}

    std::string executeAsync(...) {
        // RUNNING 상태 전환
        publishEvent<ActionStartedEvent>(actionId, ...);

        // 실행...

        // COMPLETED/FAILED 상태 전환
        if (success) {
            publishEvent<ActionCompletedEvent>(actionId, result, duration);
        } else {
            publishEvent<ActionFailedEvent>(actionId, error, duration);
        }
    }

private:
    std::shared_ptr<EventBus> eventBus_;  // optional

    template<typename EventT, typename... Args>
    void publishEvent(Args&&... args) {
        if (eventBus_) {
            try {
                eventBus_->publish(std::make_shared<EventT>(std::forward<Args>(args)...));
            } catch (...) { /* 무시 */ }
        }
    }
};
```

#### Sequence 계층 수정 사항

```cpp
// SequenceEngine.cpp
SequenceResult SequenceEngine::execute(...) {
    publishEvent<SequenceStartedEvent>(definition.id, definition.steps.size());

    for (size_t i = 0; i < steps.size(); ++i) {
        publishEvent<SequenceStepStartedEvent>(definition.id, i, step.actionId);

        // Action 실행 (크리티컬 패스)
        auto result = executor_->execute(...);

        publishEvent<SequenceStepCompletedEvent>(definition.id, i, step.actionId, result);

        // 진행률 업데이트 (5% 이상 변화 시만)
        float newProgress = (i + 1) / static_cast<float>(steps.size());
        if (newProgress - lastPublishedProgress >= 0.05f) {
            publishEvent<SequenceProgressUpdatedEvent>(definition.id, newProgress);
            lastPublishedProgress = newProgress;
        }
    }

    publishEvent<SequenceCompletedEvent>(definition.id, totalDuration);
}
```

#### Task 계층 수정 사항

```cpp
// TaskExecutor.cpp
TaskExecution TaskExecutor::execute(...) {
    publishEvent<TaskStartedEvent>(taskId, definition.type, definition.mode);

    if (definition.type == TaskType::ACTION) {
        result = executeAction(...);
    } else {
        result = executeSequence(...);
    }

    if (result.success) {
        publishEvent<TaskCompletedEvent>(taskId, result, duration);
    } else {
        publishEvent<TaskFailedEvent>(taskId, result.error, duration);
    }
}
```

## 성공 기준

### 측정 가능한 결과

#### 성능 기준

- **SC-P-001**: 이벤트 발행 오버헤드가 기존 실행 시간 대비 5% 미만으로 유지됩니다.
  - 측정 방법: 벤치마크 테스트로 이벤트 시스템 활성화/비활성화 시 성능 비교
- **SC-P-002**: 이벤트 처리 지연(발행부터 구독자 콜백까지)이 99%ile 기준 10ms 미만입니다.
  - 측정 방법: 이벤트 타임스탬프와 콜백 실행 시간의 차이 측정
- **SC-P-003**: Lock-Free Queue는 최소 10,000 ops/sec 처리량을 지원합니다.
  - 측정 방법: 단위 테스트에서 초당 처리 작업 수 측정

#### 기능 기준

- **SC-F-001**: `DataStore`의 값이 변경된 후, 20ms 이내에 `EventBus`를 통해 관련 이벤트가 수신됩니다.
  - 측정 방법: 통합 테스트에서 DataStore 변경부터 이벤트 수신까지 시간 측정
- **SC-F-002**: 새로운 모니터링 컴포넌트 추가 시, 기존 코드를 수정하지 않고 이벤트 구독만으로 통합이 가능합니다.
  - 검증 방법: 예제 모니터링 컴포넌트를 구독 API만으로 구현
- **SC-F-003**: 모든 주요 상태 전환(Action/Sequence/Task)에서 이벤트가 발행됩니다.
  - 검증 방법: 통합 테스트에서 각 상태 전환마다 이벤트 수신 확인

#### 신뢰성 기준

- **SC-R-001**: 이벤트 시스템 장애(예: 큐 가득 찬 경우)가 크리티컬 패스에 영향을 주지 않습니다.
  - 검증 방법: 큐를 의도적으로 가득 채운 상태에서 Action 실행 성공 확인
- **SC-R-002**: 구독자 콜백의 예외가 다른 구독자에게 전파되지 않습니다.
  - 검증 방법: 예외를 던지는 구독자와 정상 구독자를 함께 등록하여 테스트

## 테스트 전략

### 단위 테스트 (약 45개 예상)

#### EventBus 테스트 (12 tests)


- 이벤트 발행 및 구독 기본 기능
- 구독 등록/해제
- 필터링 기능 (타입별, ID별, 사용자 정의)
- 구독자 예외 격리
- 큐 오버플로우 처리
- 통계 수집

#### LockFreeQueue 테스트 (8 tests)


- 단일 스레드 push/pop
- 다중 스레드 동시성
- 큐 용량 제한
- 성능 벤치마크 (10,000 ops/sec 이상)

#### SubscriptionManager 테스트 (6 tests)

- 구독 추가/제거
- 이벤트 타입별 구독자 조회
- 스레드 안전성

#### DataStoreEventAdapter 테스트 (8 tests)

- DataStore → EventBus 통합
- EventBus → DataStore 통합
- 순환 업데이트 방지
- 타입 변환 처리

#### 계층별 이벤트 발행 테스트 (11 tests)

- ActionExecutor 이벤트 발행 (4 tests)
- SequenceEngine 이벤트 발행 (4 tests)
- TaskExecutor 이벤트 발행 (3 tests)

### 통합 테스트 (약 15개 예상)

#### 종단 간 이벤트 플로우 (8 tests)

- Task 실행 → 모든 계층 이벤트 발행 확인
- 이벤트 순서 검증
- 진행률 이벤트 업데이트
- 에러 전파 및 이벤트 발행

#### 성능 및 안정성 테스트 (7 tests)

- 크리티컬 패스 오버헤드 측정 (<5%)
- 이벤트 처리 지연 측정 (<10ms)
- 큐 오버플로우 시 크리티컬 패스 영향 없음 검증
- 다중 구독자 동시 처리
- 메모리 누수 방지

### 성능 벤치마크

```cpp
// 예시: 이벤트 시스템 오버헤드 측정
TEST(EventPerformance, PublishOverhead) {
    // 이벤트 시스템 없이 실행
    auto start1 = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        executor->execute(action, context);
    }
    auto duration1 = std::chrono::steady_clock::now() - start1;

    // 이벤트 시스템 포함 실행
    executor->setEventBus(eventBus);
    auto start2 = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        executor->execute(action, context);
    }
    auto duration2 = std::chrono::steady_clock::now() - start2;

    // 오버헤드 계산
    double overhead = (duration2 - duration1) / duration1 * 100.0;
    EXPECT_LT(overhead, 5.0);  // <5% 오버헤드
}
```

## 구현 단계 (Phase Plan)

### Phase 1A: 이벤트 시스템 기반 구축 (Week 1-2)

**목표**: 이벤트 인프라 구현

- [ ] `IEvent` 인터페이스 정의
- [ ] `EventType` 열거형 정의 (모든 이벤트 타입)
- [ ] Lock-Free Queue 구현 및 테스트
- [ ] `EventBus` 핵심 구현 (publish, subscribe, unsubscribe)
- [ ] `SubscriptionManager` 구현
- [ ] 필터링 기능 구현
- [ ] 단위 테스트 (25 tests)

**완료 기준**: EventBus가 독립적으로 동작하고 모든 단위 테스트 통과

### Phase 1B: 계층별 이벤트 정의 및 통합 (Week 3-4)

**목표**: Action/Sequence/Task 계층에 이벤트 발행 추가

- [ ] 계층별 이벤트 DTO 정의 (ActionEvents.h, SequenceEvents.h, TaskEvents.h)
- [ ] ActionExecutor에 EventBus 통합 및 이벤트 발행
- [ ] SequenceEngine에 EventBus 통합 및 이벤트 발행
- [ ] TaskExecutor에 EventBus 통합 및 이벤트 발행
- [ ] 단위 테스트 (11 tests)
- [ ] 통합 테스트 (8 tests)

**완료 기준**: 모든 계층에서 주요 상태 전환 시 이벤트 발행, 통합 테스트 통과

### Phase 1C: DataStore 통합 (Week 5)

**목표**: DataStore와 EventBus 연동

- [ ] `DataStoreEventAdapter` 구현
- [ ] DataStore → EventBus 이벤트 발행
- [ ] EventBus → DataStore 업데이트 헬퍼
- [ ] 순환 업데이트 방지 메커니즘
- [ ] 단위 테스트 (8 tests)
- [ ] 통합 테스트 (4 tests)

**완료 기준**: DataStore와 EventBus가 양방향으로 통합, 순환 업데이트 없음

### Phase 1D: 성능 최적화 및 검증 (Week 6)

**목표**: 성능 기준 달성 및 벤치마킹

- [ ] 성능 벤치마크 구현
- [ ] 오버헤드 측정 및 최적화 (<5%)
- [ ] 이벤트 처리 지연 측정 및 최적화 (<10ms)
- [ ] 큐 오버플로우 처리 검증
- [ ] 메모리 프로파일링 및 누수 방지
- [ ] 성능 테스트 (3 tests)

**완료 기준**: 모든 성능 기준(SC-P-001~003) 달성

## 가정 사항

- **Lock-Free Queue**: 이벤트 큐는 Lock-Free Queue로 구현하여 동시성 성능을 최적화합니다.
  - 초기에는 간단한 구현으로 시작하고, 성능이 부족하면 외부 라이브러리(예: Boost.Lockfree) 고려
- **PREEMPT_RT 환경**: 이벤트 처리 스레드는 우선순위가 낮은 비실시간 스레드로 동작합니다.
  - 크리티컬 패스는 높은 우선순위, 이벤트 처리는 낮은 우선순위
- **이벤트 손실 허용**: 큐 오버플로우 시 이벤트를 드롭하는 것은 허용됩니다.
  - 크리티컬 패스 보호가 최우선 목표
  - 손실된 이벤트는 통계로 추적
- **EventBus는 Optional**: 기존 코드는 EventBus 없이도 동작해야 합니다.
  - 생성자에서 `EventBus` 파라미터는 `nullptr` 기본값
  - 하위 호환성 보장

## 범위 제외 사항 (향후 Phase)

### Phase 2: DataStore 고도화 (별도 사양서)

- `DataStore` 내부 락 전략 개선 (`std::mutex` → `std::shared_mutex`)
- `std::any`를 타입 안전한 구조체로 변경
- 특수 목적 데이터 저장소 (예: `PoseStore`, `SensorStore`) 구현
- DataStore 성능 최적화 및 벤치마킹

### Phase 3: 고급 이벤트 기능 (별도 사양서)

- 이벤트 영속화 (이벤트를 파일/DB에 저장)
- 이벤트 재생 (디버깅 및 분석용)
- 분산 시스템 간 이벤트 전파 (ROS2, DDS 등)
- 이벤트 집계 및 분석 (메트릭 수집, 대시보드)

### Phase 4: 모니터링 컴포넌트 구현 (별도 사양서)

- 실시간 실행 상태 모니터 UI
- 성능 메트릭 수집 및 시각화
- 에러 추적 및 알림 시스템
- 디버깅 도구 (이벤트 타임라인 뷰어)

## 의존성

### 기존 컴포넌트 (수정 필요)

- **ActionExecutor**: EventBus 통합 (생성자 파라미터 추가, 이벤트 발행 코드 추가)
- **SequenceEngine**: EventBus 통합 (생성자 파라미터 추가, 이벤트 발행 코드 추가)
- **TaskExecutor**: EventBus 통합 (생성자 파라미터 추가, 이벤트 발행 코드 추가)
- **DataStore**: 어댑터 연동 (기존 Notifier API 유지, 내부적으로 EventBus 사용)

### 새로운 의존성

- **Lock-Free Queue 라이브러리**: 초기에는 자체 구현, 필요 시 Boost.Lockfree 고려
- **C++20 기능**: `std::chrono`, `std::atomic`, `std::thread`
- **spdlog**: 이벤트 시스템 로깅 (기존 Logger 활용)

## 위험 요소 및 완화 전략

### 위험 1: 성능 회귀

**설명**: 이벤트 발행 코드가 크리티컬 패스에 추가되어 실시간 성능이 저하될 수 있습니다.

**영향**: 높음 (실시간 성능은 프로젝트의 핵심 요구사항)

**완화 전략**:

- 철저한 벤치마킹 및 성능 테스트 (SC-P-001~003)
- 논블로킹 이벤트 발행 (tryPush, 실패 시 즉시 반환)
- 이벤트 시스템 비활성화 옵션 제공 (EventBus = nullptr)
- Lock-Free Queue 사용으로 락 경합 최소화
- 성능 기준 미달 시 이벤트 발행 최적화 또는 제거

### 위험 2: 복잡도 증가

**설명**: 비동기 이벤트 처리로 인한 디버깅 및 인과관계 추적의 어려움.

**영향**: 중간 (개발 생산성 저하 가능)

**완화 전략**:

- 이벤트에 고유 ID 및 타임스탬프 부여 (추적성 강화)
- 상세 로깅 (이벤트 발행/처리 모두 기록)
- 이벤트 통계 및 모니터링 API 제공
- 테스트 코드에서 이벤트 순서 검증
- 문서화 강화 (이벤트 플로우 다이어그램)

### 위험 3: DataStore 통합 복잡성

**설명**: 기존 `DataStore::Notifier`와 새로운 `EventBus`를 연동하는 과정에서 예기치 못한 동작 발생 가능.

**영향**: 중간 (데이터 일관성 문제 가능)

**완화 전략**:

- 명확한 어댑터 패턴 적용 (관심사 분리)
- 순환 업데이트 방지 메커니즘 (setWithoutNotify API)
- 통합 테스트로 양방향 통합 검증
- 단계적 통합 (Phase 1C에서 집중적으로 처리)
- 기존 Notifier API 유지로 하위 호환성 보장

### 위험 4: 메모리 누수

**설명**: 이벤트 객체 및 구독자 관리 시 메모리 누수 발생 가능.

**영향**: 중간 (장시간 실행 시 문제 발생)

**완화 전략**:

- `std::shared_ptr`로 이벤트 및 구독자 관리 (RAII)
- 구독 해제 API 제공 및 사용 권장
- 메모리 프로파일링 도구 사용 (Valgrind, AddressSanitizer)
- 장시간 실행 테스트로 메모리 누수 검증

### 위험 5: 큐 오버플로우

**설명**: 이벤트 생성 속도가 처리 속도를 초과하여 큐가 가득 찰 수 있습니다.

**영향**: 낮음 (이벤트 손실은 허용됨)

**완화 전략**:

- 설정 가능한 큐 크기 (기본값: 10,000)
- 드롭 정책 구현 (oldest-first 기본)
- 큐 상태 모니터링 API (크기, 드롭 카운트)
- 로그 경고 (큐 임계치 도달 시)
- 크리티컬 패스는 큐 상태와 무관하게 동작

## 부록

### A. 용어 정리

- **크리티컬 패스 (Critical Path)**: 시스템의 핵심 제어 흐름으로, 실시간 성능에 직접적인 영향을 주는 실행 경로
- **논블로킹 (Non-blocking)**: 호출이 즉시 반환되어 호출자를 대기시키지 않는 방식
- **Lock-Free Queue**: 뮤텍스 없이 원자적 연산만으로 구현된 동시성 큐
- **이벤트 발행 (Event Publishing)**: 시스템에서 발생한 상태 변화를 이벤트로 만들어 EventBus에 전달하는 행위
- **이벤트 구독 (Event Subscription)**: 특정 이벤트 타입을 수신하기 위해 콜백 함수를 등록하는 행위
- **하이브리드 아키텍처 (Hybrid Architecture)**: 직접 호출과 이벤트 기반 방식을 혼합하여 사용하는 아키텍처

### B. 참고 자료

- **기존 사양서**: `specs/017-action-sequence-orchestration/`,`018-action-executor-stateful-refactoring/`
- **CLAUDE.md**: 프로젝트 전체 가이드라인
- **관련 구현**: `src/core/action/`, `src/core/sequence/`, `src/core/task/`
- **EventBus 패턴**: Martin Fowler의 Event-Driven Architecture
- **Lock-Free Queue**: "C++ Concurrency in Action" by Anthony Williams

### C. 검토 체크리스트

- [ ] 크리티컬 패스가 이벤트 시스템에 의해 블로킹되지 않는가?
- [ ] 모든 주요 상태 전환에서 이벤트가 발행되는가?
- [ ] 이벤트 시스템 없이도 기존 기능이 동작하는가? (하위 호환성)
- [ ] 성능 기준(5% 오버헤드, 10ms 지연)을 만족하는가?
- [ ] 구독자 예외가 격리되는가?
- [ ] 큐 오버플로우 시 크리티컬 패스가 영향받지 않는가?
- [ ] DataStore 통합 시 순환 업데이트가 발생하지 않는가?
- [ ] 메모리 누수가 없는가?
- [ ] 모든 테스트(단위 60개, 통합 15개)가 통과하는가?
