// EventBus.h - 중앙 이벤트 버스
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_CORE_EVENT_CORE_EVENTBUS_H
#define MXRC_CORE_EVENT_CORE_EVENTBUS_H

#include "interfaces/IEventBus.h"
#include "core/SubscriptionManager.h"
#include "util/LockFreeQueue.h"
#include "util/EventStats.h"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <vector>

namespace mxrc::core::event {

/**
 * @brief Event observer interface for tracing
 *
 * Production readiness: Observer pattern for distributed tracing.
 * Observers can monitor event lifecycle (publish, dispatch) without
 * modifying EventBus core logic.
 */
class IEventObserver {
public:
    virtual ~IEventObserver() = default;

    /**
     * @brief Called before event is published
     *
     * @param event Event being published
     */
    virtual void onBeforePublish(const std::shared_ptr<IEvent>& event) = 0;

    /**
     * @brief Called after event is published
     *
     * @param event Event that was published
     * @param success Whether publish succeeded
     */
    virtual void onAfterPublish(const std::shared_ptr<IEvent>& event, bool success) = 0;

    /**
     * @brief Called before event is dispatched to subscribers
     *
     * @param event Event being dispatched
     */
    virtual void onBeforeDispatch(const std::shared_ptr<IEvent>& event) = 0;

    /**
     * @brief Called after event is dispatched to subscribers
     *
     * @param event Event that was dispatched
     * @param subscriber_count Number of subscribers that received the event
     */
    virtual void onAfterDispatch(const std::shared_ptr<IEvent>& event, size_t subscriber_count) = 0;
};

/**
 * @brief 중앙 이벤트 버스 구현
 *
 * SPSC Lock-Free Queue + Mutex 기반의 비동기 이벤트 처리 시스템입니다.
 * 여러 스레드에서 동시에 publish 가능하며 (mutex로 보호됨),
 * 큐 자체는 lock-free로 동작하여 높은 성능을 제공합니다.
 */
class EventBus : public IEventBus {

public:
    /**
     * @brief EventBus 생성자
     *
     * @param queueCapacity 이벤트 큐의 최대 용량 (기본값: 10,000)
     */
    explicit EventBus(size_t queueCapacity = 10000);

    /**
     * @brief EventBus 소멸자
     *
     * 실행 중인 경우 자동으로 stop()을 호출합니다.
     */
    ~EventBus() override;

    // 복사/이동 방지
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    // IEventBus 인터페이스 구현
    bool publish(std::shared_ptr<IEvent> event) override;
    SubscriptionId subscribe(EventFilter filter, EventCallback callback) override;
    bool unsubscribe(const SubscriptionId& subscriptionId) override;
    void start() override;
    void stop() override;
    bool isRunning() const override;

    /**
     * @brief 통계 정보 조회
     *
     * @return 이벤트 통계 구조체 참조
     */
    const EventStats& getStats() const { return stats_; }

    /**
     * @brief 통계 정보 초기화
     */
    void resetStats() { stats_.reset(); }

    /**
     * @brief Register event observer for tracing
     *
     * Production readiness: Add observer to monitor event lifecycle.
     *
     * @param observer Observer to register
     */
    void registerObserver(std::shared_ptr<IEventObserver> observer);

    /**
     * @brief Unregister event observer
     *
     * @param observer Observer to unregister
     */
    void unregisterObserver(std::shared_ptr<IEventObserver> observer);

private:
    // Core EventBus members
    SPSCLockFreeQueue<std::shared_ptr<IEvent>> eventQueue_;
    std::mutex publishMutex_;
    SubscriptionManager subscriptionManager_;
    EventStats stats_;
    std::thread dispatchThread_;
    std::atomic<bool> running_{false};

    /**
     * @brief 이벤트 디스패치 루프 (별도 스레드에서 실행)
     */
    void dispatchLoop();

    /**
     * @brief 이벤트를 구독자들에게 전달
     */
    void dispatchToSubscribers(std::shared_ptr<IEvent> event);

    // Production readiness: Event observers for tracing
    std::vector<std::shared_ptr<IEventObserver>> observers_;
    std::mutex observerMutex_;  // Protects observers_ vector

    /**
     * @brief Notify all observers before publish
     */
    void notifyBeforePublish(const std::shared_ptr<IEvent>& event);

    /**
     * @brief Notify all observers after publish
     */
    void notifyAfterPublish(const std::shared_ptr<IEvent>& event, bool success);

    /**
     * @brief Notify all observers before dispatch
     */
    void notifyBeforeDispatch(const std::shared_ptr<IEvent>& event);

    /**
     * @brief Notify all observers after dispatch
     */
    void notifyAfterDispatch(const std::shared_ptr<IEvent>& event, size_t subscriber_count);
};

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_CORE_EVENTBUS_H
