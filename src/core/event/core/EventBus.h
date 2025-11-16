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

namespace mxrc::core::event {

/**
 * @brief 중앙 이벤트 버스 구현
 *
 * SPSC Lock-Free Queue + Mutex 기반의 비동기 이벤트 처리 시스템입니다.
 * 여러 스레드에서 동시에 publish 가능하며 (mutex로 보호됨),
 * 큐 자체는 lock-free로 동작하여 높은 성능을 제공합니다.
 */
class EventBus : public IEventBus {
private:
    SPSCLockFreeQueue<std::shared_ptr<IEvent>> eventQueue_;  ///< 이벤트 큐 (SPSC)
    std::mutex publishMutex_;                                 ///< publish 동시성 보호
    SubscriptionManager subscriptionManager_;                 ///< 구독 관리자
    EventStats stats_;                                         ///< 통계 정보

    std::thread dispatchThread_;       ///< 이벤트 처리 스레드
    std::atomic<bool> running_{false}; ///< 실행 상태 플래그

    /**
     * @brief 이벤트 처리 루프 (dispatch 스레드에서 실행)
     */
    void dispatchLoop();

    /**
     * @brief 구독자에게 이벤트 전달
     *
     * @param event 전달할 이벤트
     */
    void dispatchToSubscribers(std::shared_ptr<IEvent> event);

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
};

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_CORE_EVENTBUS_H
