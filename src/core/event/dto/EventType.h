// EventType.h - 이벤트 타입 열거형
// Copyright (C) 2025 MXRC Project
// 시스템에서 발생할 수 있는 모든 이벤트 타입 정의

#ifndef MXRC_CORE_EVENT_DTO_EVENTTYPE_H
#define MXRC_CORE_EVENT_DTO_EVENTTYPE_H

#include <string>

namespace mxrc::core::event {

/**
 * @brief 시스템 이벤트 타입 열거형
 *
 * Action, Sequence, Task, DataStore 계층에서 발생하는 모든 이벤트 타입을 정의합니다.
 * 각 계층별로 그룹화되어 있으며, 확장 가능하도록 설계되었습니다.
 */
enum class EventType {
    // ===== Action Layer Events =====
    /** Action 실행 시작 */
    ACTION_STARTED,

    /** Action 성공적으로 완료 */
    ACTION_COMPLETED,

    /** Action 실행 실패 */
    ACTION_FAILED,

    /** Action 취소됨 */
    ACTION_CANCELLED,

    /** Action 타임아웃 발생 */
    ACTION_TIMEOUT,

    // ===== Sequence Layer Events =====
    /** Sequence 실행 시작 */
    SEQUENCE_STARTED,

    /** Sequence 내 개별 Step 시작 */
    SEQUENCE_STEP_STARTED,

    /** Sequence 내 개별 Step 완료 */
    SEQUENCE_STEP_COMPLETED,

    /** Sequence 성공적으로 완료 */
    SEQUENCE_COMPLETED,

    /** Sequence 실행 실패 */
    SEQUENCE_FAILED,

    /** Sequence 취소됨 */
    SEQUENCE_CANCELLED,

    /** Sequence 일시정지 */
    SEQUENCE_PAUSED,

    /** Sequence 재개 */
    SEQUENCE_RESUMED,

    /** Sequence 진행률 업데이트 (5% 단위) */
    SEQUENCE_PROGRESS_UPDATED,

    // ===== Task Layer Events =====
    /** Task 실행 시작 */
    TASK_STARTED,

    /** Task 성공적으로 완료 */
    TASK_COMPLETED,

    /** Task 실행 실패 */
    TASK_FAILED,

    /** Task 취소됨 */
    TASK_CANCELLED,

    /** Task 스케줄링됨 (주기적/트리거 실행) */
    TASK_SCHEDULED,

    /** Task 진행률 업데이트 */
    TASK_PROGRESS_UPDATED,

    // ===== DataStore Events =====
    /** DataStore 값 변경됨 */
    DATASTORE_VALUE_CHANGED,

    /** DataStore 값 삭제됨 */
    DATASTORE_VALUE_REMOVED,

    // ===== System Events =====
    /** 알 수 없는 이벤트 타입 (오류 처리용) */
    UNKNOWN
};

/**
 * @brief EventType을 문자열로 변환
 *
 * @param type 변환할 이벤트 타입
 * @return 이벤트 타입의 문자열 표현
 */
inline std::string eventTypeToString(EventType type) {
    switch (type) {
        // Action Events
        case EventType::ACTION_STARTED: return "ACTION_STARTED";
        case EventType::ACTION_COMPLETED: return "ACTION_COMPLETED";
        case EventType::ACTION_FAILED: return "ACTION_FAILED";
        case EventType::ACTION_CANCELLED: return "ACTION_CANCELLED";
        case EventType::ACTION_TIMEOUT: return "ACTION_TIMEOUT";

        // Sequence Events
        case EventType::SEQUENCE_STARTED: return "SEQUENCE_STARTED";
        case EventType::SEQUENCE_STEP_STARTED: return "SEQUENCE_STEP_STARTED";
        case EventType::SEQUENCE_STEP_COMPLETED: return "SEQUENCE_STEP_COMPLETED";
        case EventType::SEQUENCE_COMPLETED: return "SEQUENCE_COMPLETED";
        case EventType::SEQUENCE_FAILED: return "SEQUENCE_FAILED";
        case EventType::SEQUENCE_CANCELLED: return "SEQUENCE_CANCELLED";
        case EventType::SEQUENCE_PAUSED: return "SEQUENCE_PAUSED";
        case EventType::SEQUENCE_RESUMED: return "SEQUENCE_RESUMED";
        case EventType::SEQUENCE_PROGRESS_UPDATED: return "SEQUENCE_PROGRESS_UPDATED";

        // Task Events
        case EventType::TASK_STARTED: return "TASK_STARTED";
        case EventType::TASK_COMPLETED: return "TASK_COMPLETED";
        case EventType::TASK_FAILED: return "TASK_FAILED";
        case EventType::TASK_CANCELLED: return "TASK_CANCELLED";
        case EventType::TASK_SCHEDULED: return "TASK_SCHEDULED";
        case EventType::TASK_PROGRESS_UPDATED: return "TASK_PROGRESS_UPDATED";

        // DataStore Events
        case EventType::DATASTORE_VALUE_CHANGED: return "DATASTORE_VALUE_CHANGED";
        case EventType::DATASTORE_VALUE_REMOVED: return "DATASTORE_VALUE_REMOVED";

        // System Events
        case EventType::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_DTO_EVENTTYPE_H
