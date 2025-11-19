// EventBase.h - 기본 이벤트 구조체
// Copyright (C) 2025 MXRC Project
// IEvent 인터페이스의 기본 구현을 제공하는 추상 클래스

#ifndef MXRC_CORE_EVENT_DTO_EVENTBASE_H
#define MXRC_CORE_EVENT_DTO_EVENTBASE_H

#include "EventType.h"
#include "interfaces/IEvent.h"
#include <string>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace mxrc::core::event {

/**
 * @brief IEvent의 기본 구현을 제공하는 추상 클래스
 *
 * 모든 구체적인 이벤트 타입은 이 클래스를 상속하여
 * 공통 속성(eventId, timestamp, targetId)을 자동으로 얻습니다.
 */
class EventBase : public IEvent {
protected:
    std::string eventId_;                         ///< 고유 이벤트 ID
    EventType type_;                              ///< 이벤트 타입
    std::chrono::system_clock::time_point timestamp_; ///< 이벤트 발생 시각
    std::string targetId_;                        ///< 대상 엔티티 ID

    /**
     * @brief 고유한 이벤트 ID 생성
     *
     * 간단한 UUID 스타일 문자열을 생성합니다 (타임스탬프 + 랜덤).
     *
     * @return 생성된 고유 ID
     */
    static std::string generateEventId() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);

        std::stringstream ss;
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        ss << "evt_" << ms << "_";

        // 랜덤 16진수 추가
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << dis(gen);
        }

        return ss.str();
    }

public:
    /**
     * @brief EventBase 생성자
     *
     * @param type 이벤트 타입
     * @param targetId 대상 엔티티 ID (action ID, sequence ID, task ID 등)
     * @param timestamp 이벤트 발생 시각 (기본값: 현재 시각)
     */
    EventBase(EventType type, std::string targetId,
              std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : eventId_(generateEventId()),
          type_(type),
          timestamp_(timestamp),
          targetId_(std::move(targetId)) {}

    virtual ~EventBase() = default;

    // IEvent 인터페이스 구현
    std::string getEventId() const override {
        return eventId_;
    }

    EventType getType() const override {
        return type_;
    }

    std::chrono::system_clock::time_point getTimestamp() const override {
        return timestamp_;
    }

    std::string getTargetId() const override {
        return targetId_;
    }

    std::string getTypeName() const override {
        return eventTypeToString(type_);
    }
};

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_DTO_EVENTBASE_H
