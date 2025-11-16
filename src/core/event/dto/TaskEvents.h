// TaskEvents.h - Task 계층 이벤트 정의
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_CORE_EVENT_DTO_TASKEVENTS_H
#define MXRC_CORE_EVENT_DTO_TASKEVENTS_H

#include "EventBase.h"
#include <string>
#include <chrono>

namespace mxrc::core::event {

/**
 * @brief Task 실행 시작 이벤트
 */
class TaskStartedEvent : public EventBase {
public:
    std::string taskId;        ///< Task ID
    std::string taskName;      ///< Task 이름
    std::string executionMode; ///< 실행 모드 (ONCE, PERIODIC, TRIGGERED)
    std::string workType;      ///< Work 타입 (ACTION, SEQUENCE)

    TaskStartedEvent(const std::string& taskId, const std::string& taskName,
                     const std::string& executionMode, const std::string& workType,
                     std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::TASK_STARTED, taskId, timestamp),
          taskId(taskId),
          taskName(taskName),
          executionMode(executionMode),
          workType(workType) {}
};

/**
 * @brief Task 성공 완료 이벤트
 */
class TaskCompletedEvent : public EventBase {
public:
    std::string taskId;        ///< Task ID
    std::string taskName;      ///< Task 이름
    long durationMs;           ///< 실행 시간 (밀리초)
    double progressPercent;    ///< 최종 진행률 (100.0)

    TaskCompletedEvent(const std::string& taskId, const std::string& taskName,
                       long durationMs, double progressPercent = 100.0,
                       std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::TASK_COMPLETED, taskId, timestamp),
          taskId(taskId),
          taskName(taskName),
          durationMs(durationMs),
          progressPercent(progressPercent) {}
};

/**
 * @brief Task 실패 이벤트
 */
class TaskFailedEvent : public EventBase {
public:
    std::string taskId;        ///< Task ID
    std::string taskName;      ///< Task 이름
    std::string errorMessage;  ///< 오류 메시지
    long durationMs;           ///< 실패까지 걸린 시간 (밀리초)
    double progressPercent;    ///< 실패 시점 진행률

    TaskFailedEvent(const std::string& taskId, const std::string& taskName,
                    const std::string& errorMessage, long durationMs, double progressPercent,
                    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::TASK_FAILED, taskId, timestamp),
          taskId(taskId),
          taskName(taskName),
          errorMessage(errorMessage),
          durationMs(durationMs),
          progressPercent(progressPercent) {}
};

/**
 * @brief Task 취소 이벤트
 */
class TaskCancelledEvent : public EventBase {
public:
    std::string taskId;        ///< Task ID
    std::string taskName;      ///< Task 이름
    long durationMs;           ///< 취소까지 걸린 시간 (밀리초)
    double progressPercent;    ///< 취소 시점 진행률

    TaskCancelledEvent(const std::string& taskId, const std::string& taskName,
                       long durationMs, double progressPercent,
                       std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::TASK_CANCELLED, taskId, timestamp),
          taskId(taskId),
          taskName(taskName),
          durationMs(durationMs),
          progressPercent(progressPercent) {}
};

/**
 * @brief Task 스케줄링됨 이벤트 (주기적/트리거 실행)
 */
class TaskScheduledEvent : public EventBase {
public:
    std::string taskId;        ///< Task ID
    std::string taskName;      ///< Task 이름
    std::string executionMode; ///< 실행 모드 (PERIODIC, TRIGGERED)
    std::string trigger;       ///< 트리거 정보 (주기 또는 트리거 조건)

    TaskScheduledEvent(const std::string& taskId, const std::string& taskName,
                       const std::string& executionMode, const std::string& trigger,
                       std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::TASK_SCHEDULED, taskId, timestamp),
          taskId(taskId),
          taskName(taskName),
          executionMode(executionMode),
          trigger(trigger) {}
};

/**
 * @brief Task 진행률 업데이트 이벤트
 */
class TaskProgressUpdatedEvent : public EventBase {
public:
    std::string taskId;        ///< Task ID
    std::string taskName;      ///< Task 이름
    double progressPercent;    ///< 진행률 (0.0 ~ 100.0)
    std::string currentPhase;  ///< 현재 진행 단계 (선택적)

    TaskProgressUpdatedEvent(const std::string& taskId, const std::string& taskName,
                             double progressPercent, const std::string& currentPhase = "",
                             std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::TASK_PROGRESS_UPDATED, taskId, timestamp),
          taskId(taskId),
          taskName(taskName),
          progressPercent(progressPercent),
          currentPhase(currentPhase) {}
};

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_DTO_TASKEVENTS_H
