#ifndef MXRC_CORE_TASK_TASK_EXECUTION_MODE_H
#define MXRC_CORE_TASK_TASK_EXECUTION_MODE_H

#include <string>

namespace mxrc::core::task {

/**
 * @brief Task 실행 모드
 */
enum class TaskExecutionMode {
    ONCE,       // 단일 실행
    PERIODIC,   // 주기적 실행
    TRIGGERED   // 트리거 기반 실행
};

/**
 * @brief TaskExecutionMode를 문자열로 변환
 */
inline std::string taskExecutionModeToString(TaskExecutionMode mode) {
    switch (mode) {
        case TaskExecutionMode::ONCE:      return "ONCE";
        case TaskExecutionMode::PERIODIC:  return "PERIODIC";
        case TaskExecutionMode::TRIGGERED: return "TRIGGERED";
        default:                           return "UNKNOWN";
    }
}

} // namespace mxrc::core::task

#endif // MXRC_CORE_TASK_TASK_EXECUTION_MODE_H
