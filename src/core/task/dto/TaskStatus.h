#ifndef MXRC_CORE_TASK_TASK_STATUS_H
#define MXRC_CORE_TASK_TASK_STATUS_H

#include <string>

namespace mxrc::core::task {

/**
 * @brief Task 실행 상태
 */
enum class TaskStatus {
    IDLE,           // 유휴 상태 (실행 대기)
    RUNNING,        // 실행 중
    COMPLETED,      // 정상 완료
    FAILED,         // 실패
    CANCELLED,      // 취소됨
    PAUSED,         // 일시정지
    SCHEDULED       // 예약됨 (periodic/triggered)
};

/**
 * @brief TaskStatus를 문자열로 변환
 */
inline std::string taskStatusToString(TaskStatus status) {
    switch (status) {
        case TaskStatus::IDLE:       return "IDLE";
        case TaskStatus::RUNNING:    return "RUNNING";
        case TaskStatus::COMPLETED:  return "COMPLETED";
        case TaskStatus::FAILED:     return "FAILED";
        case TaskStatus::CANCELLED:  return "CANCELLED";
        case TaskStatus::PAUSED:     return "PAUSED";
        case TaskStatus::SCHEDULED:  return "SCHEDULED";
        default:                     return "UNKNOWN";
    }
}

} // namespace mxrc::core::task

#endif // MXRC_CORE_TASK_TASK_STATUS_H
