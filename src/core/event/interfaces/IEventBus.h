// IEventBus.h - EventBus 인터페이스
// Copyright (C) 2025 MXRC Project
// 중앙 이벤트 허브 인터페이스 정의

#ifndef MXRC_CORE_EVENT_INTERFACES_IEVENTBUS_H
#define MXRC_CORE_EVENT_INTERFACES_IEVENTBUS_H

#include "IEvent.h"
#include <memory>
#include <functional>

namespace mxrc::core::event {

/**
 * @brief 이벤트 필터 함수 타입
 *
 * 이벤트를 필터링하는 데 사용되는 predicate 함수입니다.
 * @param event 평가할 이벤트
 * @return true이면 이벤트를 구독자에게 전달, false이면 무시
 */
using EventFilter = std::function<bool(const std::shared_ptr<IEvent>&)>;

/**
 * @brief 이벤트 콜백 함수 타입
 *
 * 구독자가 이벤트를 받을 때 호출되는 콜백 함수입니다.
 * @param event 전달된 이벤트
 */
using EventCallback = std::function<void(std::shared_ptr<IEvent>)>;

/**
 * @brief 구독 ID 타입
 *
 * 구독을 고유하게 식별하는 ID (구독 해제 시 사용)
 */
using SubscriptionId = std::string;

/**
 * @brief 중앙 이벤트 버스 인터페이스
 *
 * Publisher-Subscriber 패턴의 중앙 허브입니다.
 * 비동기 이벤트 발행/구독 기능을 제공하며, Lock-Free Queue를 통해
 * 크리티컬 패스를 보호합니다.
 */
class IEventBus {
public:
    virtual ~IEventBus() = default;

    /**
     * @brief 이벤트를 비동기로 발행
     *
     * 이벤트를 큐에 넣고 즉시 반환합니다 (논블로킹).
     * 큐가 가득 찬 경우 이벤트를 드롭하고 false를 반환합니다.
     *
     * @param event 발행할 이벤트
     * @return true이면 큐에 성공적으로 추가됨, false이면 큐 오버플로우로 드롭됨
     */
    virtual bool publish(std::shared_ptr<IEvent> event) = 0;

    /**
     * @brief 이벤트 구독 등록
     *
     * 필터 조건을 만족하는 이벤트에 대해 콜백 함수를 호출합니다.
     *
     * @param filter 이벤트 필터 함수 (nullptr이면 모든 이벤트 수신)
     * @param callback 이벤트 수신 시 호출될 콜백 함수
     * @return 구독 ID (구독 해제 시 사용)
     */
    virtual SubscriptionId subscribe(EventFilter filter, EventCallback callback) = 0;

    /**
     * @brief 구독 해제
     *
     * @param subscriptionId 해제할 구독 ID
     * @return true이면 성공, false이면 해당 구독 ID가 존재하지 않음
     */
    virtual bool unsubscribe(const SubscriptionId& subscriptionId) = 0;

    /**
     * @brief EventBus 시작
     *
     * 이벤트 처리 스레드를 시작합니다.
     */
    virtual void start() = 0;

    /**
     * @brief EventBus 정지
     *
     * 이벤트 처리 스레드를 안전하게 종료하고 대기 중인 모든 이벤트를 처리합니다.
     */
    virtual void stop() = 0;

    /**
     * @brief EventBus의 실행 상태 확인
     *
     * @return true이면 실행 중, false이면 정지 상태
     */
    virtual bool isRunning() const = 0;
};

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_INTERFACES_IEVENTBUS_H
