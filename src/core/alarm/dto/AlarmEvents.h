// AlarmEvents.h - Alarm 시스템 이벤트 정의
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_CORE_ALARM_DTO_ALARMEVENTS_H
#define MXRC_CORE_ALARM_DTO_ALARMEVENTS_H

#include "core/event/dto/EventBase.h"
#include "AlarmDto.h"
#include "AlarmSeverity.h"
#include <string>
#include <chrono>

namespace mxrc::core::alarm {

/**
 * @brief Alarm 발생 이벤트
 *
 * 새로운 Alarm이 발생했을 때 발행됩니다.
 */
class AlarmRaisedEvent : public event::EventBase {
public:
    std::string alarmId;          ///< Alarm ID
    std::string alarmType;        ///< Alarm 유형
    AlarmSeverity severity;       ///< 심각도
    std::string message;          ///< Alarm 메시지
    std::string source;           ///< Alarm 발생원

    AlarmRaisedEvent(const std::string& alarmId, const std::string& alarmType,
                     AlarmSeverity severity, const std::string& message,
                     const std::string& source,
                     std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(event::EventType::ALARM_RAISED, alarmId, timestamp),
          alarmId(alarmId),
          alarmType(alarmType),
          severity(severity),
          message(message),
          source(source) {}

    AlarmRaisedEvent(const AlarmDto& alarm,
                     std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(event::EventType::ALARM_RAISED, alarm.alarm_id, timestamp),
          alarmId(alarm.alarm_id),
          alarmType(alarm.alarm_code),
          severity(alarm.severity),
          message(alarm.alarm_name),
          source(alarm.source) {}
};

/**
 * @brief Alarm 해제 이벤트
 *
 * Alarm이 리셋/해제되었을 때 발행됩니다.
 */
class AlarmClearedEvent : public event::EventBase {
public:
    std::string alarmId;          ///< Alarm ID
    std::string alarmType;        ///< Alarm 유형
    std::string clearedBy;        ///< 해제자 (시스템/운영자)

    AlarmClearedEvent(const std::string& alarmId, const std::string& alarmType,
                      const std::string& clearedBy,
                      std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(event::EventType::ALARM_CLEARED, alarmId, timestamp),
          alarmId(alarmId),
          alarmType(alarmType),
          clearedBy(clearedBy) {}
};

/**
 * @brief Alarm 심각도 상향 이벤트
 *
 * Alarm의 심각도가 상향 조정되었을 때 발행됩니다.
 */
class AlarmEscalatedEvent : public event::EventBase {
public:
    std::string alarmId;          ///< Alarm ID
    std::string alarmType;        ///< Alarm 유형
    AlarmSeverity oldSeverity;    ///< 이전 심각도
    AlarmSeverity newSeverity;    ///< 새로운 심각도
    int occurrenceCount;          ///< 재발 횟수

    AlarmEscalatedEvent(const std::string& alarmId, const std::string& alarmType,
                        AlarmSeverity oldSeverity, AlarmSeverity newSeverity,
                        int occurrenceCount,
                        std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(event::EventType::ALARM_ESCALATED, alarmId, timestamp),
          alarmId(alarmId),
          alarmType(alarmType),
          oldSeverity(oldSeverity),
          newSeverity(newSeverity),
          occurrenceCount(occurrenceCount) {}
};

/**
 * @brief Alarm 확인 이벤트
 *
 * 운영자가 Alarm을 확인했을 때 발행됩니다.
 */
class AlarmAcknowledgedEvent : public event::EventBase {
public:
    std::string alarmId;          ///< Alarm ID
    std::string alarmType;        ///< Alarm 유형
    std::string acknowledgedBy;   ///< 확인자

    AlarmAcknowledgedEvent(const std::string& alarmId, const std::string& alarmType,
                           const std::string& acknowledgedBy,
                           std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(event::EventType::ALARM_ACKNOWLEDGED, alarmId, timestamp),
          alarmId(alarmId),
          alarmType(alarmType),
          acknowledgedBy(acknowledgedBy) {}
};

} // namespace mxrc::core::alarm

#endif // MXRC_CORE_ALARM_DTO_ALARMEVENTS_H