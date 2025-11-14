#ifndef MXRC_CORE_TASK_TASK_EXECUTION_H
#define MXRC_CORE_TASK_TASK_EXECUTION_H

#include "TaskStatus.h"
#include <string>
#include <chrono>

namespace mxrc::core::task {

/**
 * @brief Task 실행 결과
 */
struct TaskExecution {
    std::string taskId;                             // Task ID
    std::string executionId;                        // 실행 ID (unique)
    TaskStatus status{TaskStatus::IDLE};            // 실행 상태
    float progress{0.0f};                           // 진행률 (0.0 ~ 1.0)
    std::string errorMessage;                       // 에러 메시지
    std::chrono::milliseconds executionTime{0};     // 실행 시간
    std::chrono::system_clock::time_point startTime;    // 시작 시간
    std::chrono::system_clock::time_point endTime;      // 종료 시간
    int executionCount{0};                          // 실행 횟수 (periodic의 경우)

    bool isSuccessful() const {
        return status == TaskStatus::COMPLETED;
    }

    bool isFailed() const {
        return status == TaskStatus::FAILED ||
               status == TaskStatus::CANCELLED;
    }

    bool isRunning() const {
        return status == TaskStatus::RUNNING;
    }
};

} // namespace mxrc::core::task

#endif // MXRC_CORE_TASK_TASK_EXECUTION_H
