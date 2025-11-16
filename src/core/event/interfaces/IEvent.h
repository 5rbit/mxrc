// IEvent.h - 이벤트 기본 인터페이스
// Copyright (C) 2025 MXRC Project
// 모든 이벤트 타입이 구현해야 하는 기본 인터페이스

#ifndef MXRC_CORE_EVENT_INTERFACES_IEVENT_H
#define MXRC_CORE_EVENT_INTERFACES_IEVENT_H

#include <string>
#include <chrono>

namespace mxrc::core::event {

/**
 * @brief 이벤트 타입 열거형 (전방 선언)
 *
 * 실제 정의는 EventType.h에 있습니다.
 */
enum class EventType;

/**
 * @brief 모든 이벤트의 기본 인터페이스
 *
 * 모든 이벤트는 이 인터페이스를 구현하여 EventBus를 통해 전달될 수 있습니다.
 * 타임스탬프, 이벤트 ID, 이벤트 타입, 대상 ID 등 공통 속성을 제공합니다.
 */
class IEvent {
public:
    virtual ~IEvent() = default;

    /**
     * @brief 고유한 이벤트 ID 반환
     * @return 이벤트의 고유 식별자 (UUID 형식)
     */
    virtual std::string getEventId() const = 0;

    /**
     * @brief 이벤트 타입 반환
     * @return 이벤트의 타입 (ACTION_STARTED, SEQUENCE_COMPLETED 등)
     */
    virtual EventType getType() const = 0;

    /**
     * @brief 이벤트 발생 시각 반환
     * @return 이벤트가 생성된 시각 (UTC)
     */
    virtual std::chrono::system_clock::time_point getTimestamp() const = 0;

    /**
     * @brief 이벤트와 연관된 대상 엔티티의 ID 반환
     * @return 대상 ID (예: action ID, sequence ID, task ID)
     */
    virtual std::string getTargetId() const = 0;

    /**
     * @brief 이벤트의 타입 이름 문자열 반환
     * @return 이벤트 타입의 문자열 표현 (로깅 및 디버깅용)
     */
    virtual std::string getTypeName() const = 0;
};

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_INTERFACES_IEVENT_H
