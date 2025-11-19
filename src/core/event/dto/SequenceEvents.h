// SequenceEvents.h - Sequence 계층 이벤트 정의
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_CORE_EVENT_DTO_SEQUENCEEVENTS_H
#define MXRC_CORE_EVENT_DTO_SEQUENCEEVENTS_H

#include "EventBase.h"
#include <string>
#include <chrono>

namespace mxrc::core::event {

/**
 * @brief Sequence 실행 시작 이벤트
 */
class SequenceStartedEvent : public EventBase {
public:
    std::string sequenceId;    ///< Sequence ID
    std::string sequenceName;  ///< Sequence 이름
    int totalSteps;            ///< 전체 Step 수

    SequenceStartedEvent(const std::string& sequenceId, const std::string& sequenceName,
                         int totalSteps,
                         std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::SEQUENCE_STARTED, sequenceId, timestamp),
          sequenceId(sequenceId),
          sequenceName(sequenceName),
          totalSteps(totalSteps) {}
};

/**
 * @brief Sequence Step 시작 이벤트
 */
class SequenceStepStartedEvent : public EventBase {
public:
    std::string sequenceId;    ///< Sequence ID
    std::string stepId;        ///< Step (Action) ID
    std::string stepType;      ///< Step 타입
    int stepIndex;             ///< Step 인덱스 (0-based)
    int totalSteps;            ///< 전체 Step 수

    SequenceStepStartedEvent(const std::string& sequenceId, const std::string& stepId,
                             const std::string& stepType, int stepIndex, int totalSteps,
                             std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::SEQUENCE_STEP_STARTED, sequenceId, timestamp),
          sequenceId(sequenceId),
          stepId(stepId),
          stepType(stepType),
          stepIndex(stepIndex),
          totalSteps(totalSteps) {}
};

/**
 * @brief Sequence Step 완료 이벤트
 */
class SequenceStepCompletedEvent : public EventBase {
public:
    std::string sequenceId;    ///< Sequence ID
    std::string stepId;        ///< Step (Action) ID
    std::string stepType;      ///< Step 타입
    int stepIndex;             ///< Step 인덱스 (0-based)
    int totalSteps;            ///< 전체 Step 수

    SequenceStepCompletedEvent(const std::string& sequenceId, const std::string& stepId,
                               const std::string& stepType, int stepIndex, int totalSteps,
                               std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::SEQUENCE_STEP_COMPLETED, sequenceId, timestamp),
          sequenceId(sequenceId),
          stepId(stepId),
          stepType(stepType),
          stepIndex(stepIndex),
          totalSteps(totalSteps) {}
};

/**
 * @brief Sequence 성공 완료 이벤트
 */
class SequenceCompletedEvent : public EventBase {
public:
    std::string sequenceId;    ///< Sequence ID
    std::string sequenceName;  ///< Sequence 이름
    int completedSteps;        ///< 완료된 Step 수
    int totalSteps;            ///< 전체 Step 수
    long durationMs;           ///< 실행 시간 (밀리초)

    SequenceCompletedEvent(const std::string& sequenceId, const std::string& sequenceName,
                           int completedSteps, int totalSteps, long durationMs,
                           std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::SEQUENCE_COMPLETED, sequenceId, timestamp),
          sequenceId(sequenceId),
          sequenceName(sequenceName),
          completedSteps(completedSteps),
          totalSteps(totalSteps),
          durationMs(durationMs) {}
};

/**
 * @brief Sequence 실패 이벤트
 */
class SequenceFailedEvent : public EventBase {
public:
    std::string sequenceId;    ///< Sequence ID
    std::string sequenceName;  ///< Sequence 이름
    std::string errorMessage;  ///< 오류 메시지
    int completedSteps;        ///< 완료된 Step 수
    int totalSteps;            ///< 전체 Step 수
    int failedStepIndex;       ///< 실패한 Step 인덱스
    long durationMs;           ///< 실패까지 걸린 시간 (밀리초)

    SequenceFailedEvent(const std::string& sequenceId, const std::string& sequenceName,
                        const std::string& errorMessage, int completedSteps, int totalSteps,
                        int failedStepIndex, long durationMs,
                        std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::SEQUENCE_FAILED, sequenceId, timestamp),
          sequenceId(sequenceId),
          sequenceName(sequenceName),
          errorMessage(errorMessage),
          completedSteps(completedSteps),
          totalSteps(totalSteps),
          failedStepIndex(failedStepIndex),
          durationMs(durationMs) {}
};

/**
 * @brief Sequence 취소 이벤트
 */
class SequenceCancelledEvent : public EventBase {
public:
    std::string sequenceId;    ///< Sequence ID
    std::string sequenceName;  ///< Sequence 이름
    int completedSteps;        ///< 완료된 Step 수
    int totalSteps;            ///< 전체 Step 수
    long durationMs;           ///< 취소까지 걸린 시간 (밀리초)

    SequenceCancelledEvent(const std::string& sequenceId, const std::string& sequenceName,
                           int completedSteps, int totalSteps, long durationMs,
                           std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::SEQUENCE_CANCELLED, sequenceId, timestamp),
          sequenceId(sequenceId),
          sequenceName(sequenceName),
          completedSteps(completedSteps),
          totalSteps(totalSteps),
          durationMs(durationMs) {}
};

/**
 * @brief Sequence 일시정지 이벤트
 */
class SequencePausedEvent : public EventBase {
public:
    std::string sequenceId;    ///< Sequence ID
    std::string sequenceName;  ///< Sequence 이름
    int currentStepIndex;      ///< 현재 Step 인덱스
    int totalSteps;            ///< 전체 Step 수

    SequencePausedEvent(const std::string& sequenceId, const std::string& sequenceName,
                        int currentStepIndex, int totalSteps,
                        std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::SEQUENCE_PAUSED, sequenceId, timestamp),
          sequenceId(sequenceId),
          sequenceName(sequenceName),
          currentStepIndex(currentStepIndex),
          totalSteps(totalSteps) {}
};

/**
 * @brief Sequence 재개 이벤트
 */
class SequenceResumedEvent : public EventBase {
public:
    std::string sequenceId;    ///< Sequence ID
    std::string sequenceName;  ///< Sequence 이름
    int currentStepIndex;      ///< 재개할 Step 인덱스
    int totalSteps;            ///< 전체 Step 수

    SequenceResumedEvent(const std::string& sequenceId, const std::string& sequenceName,
                         int currentStepIndex, int totalSteps,
                         std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::SEQUENCE_RESUMED, sequenceId, timestamp),
          sequenceId(sequenceId),
          sequenceName(sequenceName),
          currentStepIndex(currentStepIndex),
          totalSteps(totalSteps) {}
};

/**
 * @brief Sequence 진행률 업데이트 이벤트
 */
class SequenceProgressUpdatedEvent : public EventBase {
public:
    std::string sequenceId;    ///< Sequence ID
    std::string sequenceName;  ///< Sequence 이름
    int completedSteps;        ///< 완료된 Step 수
    int totalSteps;            ///< 전체 Step 수
    double progressPercent;    ///< 진행률 (0.0 ~ 100.0)

    SequenceProgressUpdatedEvent(const std::string& sequenceId, const std::string& sequenceName,
                                 int completedSteps, int totalSteps, double progressPercent,
                                 std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::SEQUENCE_PROGRESS_UPDATED, sequenceId, timestamp),
          sequenceId(sequenceId),
          sequenceName(sequenceName),
          completedSteps(completedSteps),
          totalSteps(totalSteps),
          progressPercent(progressPercent) {}
};

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_DTO_SEQUENCEEVENTS_H
