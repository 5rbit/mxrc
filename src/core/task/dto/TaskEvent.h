#pragma once

#include <string>
#include <chrono>
#include "TaskStatus.h"

namespace mxrc::core::task {

/**
 * @brief Task 이벤트 타입
 *
 * Feature 016: Pallet Shuttle Control System
 */
enum class TaskEventType {
    TASK_STARTED,      ///< Task 시작
    TASK_PAUSED,       ///< Task 일시 중지
    TASK_RESUMED,      ///< Task 재개
    TASK_COMPLETED,    ///< Task 완료
    TASK_FAILED,       ///< Task 실패
    TASK_CANCELLED     ///< Task 취소
};

/**
 * @brief Task 이벤트 DTO
 *
 * Task 생명주기 이벤트를 EventBus를 통해 전파합니다.
 */
struct TaskEvent {
    /**
     * @brief Task ID
     */
    std::string task_id;

    /**
     * @brief 이벤트 타입
     */
    TaskEventType event_type;

    /**
     * @brief Task 상태
     */
    TaskStatus status;

    /**
     * @brief 진행률 (0.0 ~ 1.0)
     */
    float progress{0.0f};

    /**
     * @brief 이벤트 발생 시각
     */
    std::chrono::system_clock::time_point timestamp;

    /**
     * @brief 추가 메시지 (선택적)
     */
    std::string message;

    /**
     * @brief 기본 생성자
     */
    TaskEvent() = default;

    /**
     * @brief 편의 생성자
     */
    TaskEvent(
        std::string id,
        TaskEventType type,
        TaskStatus stat,
        float prog = 0.0f)
        : task_id(std::move(id))
        , event_type(type)
        , status(stat)
        , progress(prog)
        , timestamp(std::chrono::system_clock::now())
    {
    }
};

/**
 * @brief TaskEventType을 문자열로 변환
 */
inline std::string toString(TaskEventType type) {
    switch (type) {
        case TaskEventType::TASK_STARTED:   return "TASK_STARTED";
        case TaskEventType::TASK_PAUSED:    return "TASK_PAUSED";
        case TaskEventType::TASK_RESUMED:   return "TASK_RESUMED";
        case TaskEventType::TASK_COMPLETED: return "TASK_COMPLETED";
        case TaskEventType::TASK_FAILED:    return "TASK_FAILED";
        case TaskEventType::TASK_CANCELLED: return "TASK_CANCELLED";
        default:                            return "UNKNOWN";
    }
}

} // namespace mxrc::core::task
