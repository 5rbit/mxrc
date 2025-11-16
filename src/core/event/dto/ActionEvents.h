// ActionEvents.h - Action 계층 이벤트 정의
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_CORE_EVENT_DTO_ACTIONEVENTS_H
#define MXRC_CORE_EVENT_DTO_ACTIONEVENTS_H

#include "EventBase.h"
#include <string>
#include <chrono>

namespace mxrc::core::event {

/**
 * @brief Action 실행 시작 이벤트
 */
class ActionStartedEvent : public EventBase {
public:
    std::string actionId;      ///< Action ID
    std::string actionType;    ///< Action 타입 (Delay, Move 등)

    ActionStartedEvent(const std::string& actionId, const std::string& actionType,
                       std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::ACTION_STARTED, actionId, timestamp),
          actionId(actionId),
          actionType(actionType) {}
};

/**
 * @brief Action 성공 완료 이벤트
 */
class ActionCompletedEvent : public EventBase {
public:
    std::string actionId;      ///< Action ID
    std::string actionType;    ///< Action 타입
    long durationMs;           ///< 실행 시간 (밀리초)
    std::string result;        ///< 실행 결과 (선택적)

    ActionCompletedEvent(const std::string& actionId, const std::string& actionType,
                         long durationMs, const std::string& result = "",
                         std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::ACTION_COMPLETED, actionId, timestamp),
          actionId(actionId),
          actionType(actionType),
          durationMs(durationMs),
          result(result) {}
};

/**
 * @brief Action 실패 이벤트
 */
class ActionFailedEvent : public EventBase {
public:
    std::string actionId;      ///< Action ID
    std::string actionType;    ///< Action 타입
    std::string errorMessage;  ///< 오류 메시지
    long durationMs;           ///< 실패까지 걸린 시간 (밀리초)

    ActionFailedEvent(const std::string& actionId, const std::string& actionType,
                      const std::string& errorMessage, long durationMs = 0,
                      std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::ACTION_FAILED, actionId, timestamp),
          actionId(actionId),
          actionType(actionType),
          errorMessage(errorMessage),
          durationMs(durationMs) {}
};

/**
 * @brief Action 취소 이벤트
 */
class ActionCancelledEvent : public EventBase {
public:
    std::string actionId;      ///< Action ID
    std::string actionType;    ///< Action 타입
    long durationMs;           ///< 취소까지 걸린 시간 (밀리초)

    ActionCancelledEvent(const std::string& actionId, const std::string& actionType,
                         long durationMs = 0,
                         std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::ACTION_CANCELLED, actionId, timestamp),
          actionId(actionId),
          actionType(actionType),
          durationMs(durationMs) {}
};

/**
 * @brief Action 타임아웃 이벤트
 */
class ActionTimeoutEvent : public EventBase {
public:
    std::string actionId;      ///< Action ID
    std::string actionType;    ///< Action 타입
    long timeoutMs;            ///< 설정된 타임아웃 (밀리초)
    long elapsedMs;            ///< 실제 경과 시간 (밀리초)

    ActionTimeoutEvent(const std::string& actionId, const std::string& actionType,
                       long timeoutMs, long elapsedMs,
                       std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::ACTION_TIMEOUT, actionId, timestamp),
          actionId(actionId),
          actionType(actionType),
          timeoutMs(timeoutMs),
          elapsedMs(elapsedMs) {}
};

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_DTO_ACTIONEVENTS_H
