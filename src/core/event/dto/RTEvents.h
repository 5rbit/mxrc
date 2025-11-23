// RTEvents.h - RT (Real-Time) 이벤트 정의
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_CORE_EVENT_DTO_RTEVENTS_H
#define MXRC_CORE_EVENT_DTO_RTEVENTS_H

#include "EventBase.h"
#include "EventType.h"
#include <string>
#include <cstdint>

namespace mxrc::core::event {

/**
 * @brief RT 상태 변경 이벤트
 */
class RTStateChangedEvent : public EventBase {
public:
    RTStateChangedEvent(const std::string& from_state,
                       const std::string& to_state,
                       const std::string& trigger_event)
        : EventBase(EventType::RT_STATE_CHANGED, "rt_executive")
        , from_state_(from_state)
        , to_state_(to_state)
        , trigger_event_(trigger_event) {}

    std::string getFromState() const { return from_state_; }
    std::string getToState() const { return to_state_; }
    std::string getTriggerEvent() const { return trigger_event_; }

private:
    std::string from_state_;
    std::string to_state_;
    std::string trigger_event_;
};

/**
 * @brief RT SAFE_MODE 진입 이벤트
 */
class RTSafeModeEnteredEvent : public EventBase {
public:
    RTSafeModeEnteredEvent(uint64_t timeout_ms, const std::string& reason)
        : EventBase(EventType::RT_SAFE_MODE_ENTERED, "rt_executive")
        , timeout_ms_(timeout_ms)
        , reason_(reason) {}

    uint64_t getTimeoutMs() const { return timeout_ms_; }
    std::string getReason() const { return reason_; }

private:
    uint64_t timeout_ms_;
    std::string reason_;
};

/**
 * @brief RT SAFE_MODE 복구 이벤트
 */
class RTSafeModeExitedEvent : public EventBase {
public:
    RTSafeModeExitedEvent(uint64_t downtime_ms)
        : EventBase(EventType::RT_SAFE_MODE_EXITED, "rt_executive")
        , downtime_ms_(downtime_ms) {}

    uint64_t getDowntimeMs() const { return downtime_ms_; }

private:
    uint64_t downtime_ms_;
};

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_DTO_RTEVENTS_H
