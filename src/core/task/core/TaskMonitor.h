#ifndef MXRC_CORE_TASK_MONITOR_H
#define MXRC_CORE_TASK_MONITOR_H

#include "core/task/dto/TaskDto.h"
#include "core/action/util/Logger.h"
#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include <optional>

namespace mxrc::core::task {

/**
 * @brief Task 실행 모니터링 및 추적
 *
 * Task의 실행 상태, 진행률, 실행 시간 등을 추적하고 모니터링합니다.
 * 스레드 안전성을 보장하며, 여러 Task의 동시 실행을 추적할 수 있습니다.
 */
class TaskMonitor {
public:
    /**
     * @brief Task 실행 정보
     */
    struct TaskExecutionInfo {
        std::string taskId;
        TaskStatus status;
        float progress;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point endTime;
        std::string errorMessage;
        int retryCount;

        /**
         * @brief 실행 시간 계산 (밀리초)
         */
        int64_t getElapsedMs() const {
            auto end = (status == TaskStatus::RUNNING || status == TaskStatus::PENDING)
                       ? std::chrono::steady_clock::now()
                       : endTime;
            return std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime).count();
        }
    };

    TaskMonitor();
    ~TaskMonitor() = default;

    // 복사 및 이동 금지
    TaskMonitor(const TaskMonitor&) = delete;
    TaskMonitor& operator=(const TaskMonitor&) = delete;
    TaskMonitor(TaskMonitor&&) = delete;
    TaskMonitor& operator=(TaskMonitor&&) = delete;

    /**
     * @brief Task 실행 시작 기록
     * @param taskId Task 식별자
     */
    void startTask(const std::string& taskId);

    /**
     * @brief Task 진행률 업데이트
     * @param taskId Task 식별자
     * @param progress 진행률 (0.0 ~ 1.0)
     */
    void updateProgress(const std::string& taskId, float progress);

    /**
     * @brief Task 상태 업데이트
     * @param taskId Task 식별자
     * @param status 새로운 상태
     */
    void updateStatus(const std::string& taskId, TaskStatus status);

    /**
     * @brief Task 실행 완료 기록
     * @param taskId Task 식별자
     * @param status 최종 상태
     * @param errorMessage 에러 메시지 (실패 시)
     */
    void endTask(const std::string& taskId, TaskStatus status, const std::string& errorMessage = "");

    /**
     * @brief Task 재시도 횟수 증가
     * @param taskId Task 식별자
     */
    void incrementRetryCount(const std::string& taskId);

    /**
     * @brief Task 실행 정보 조회
     * @param taskId Task 식별자
     * @return Task 실행 정보 (존재하지 않으면 empty optional)
     */
    std::optional<TaskExecutionInfo> getTaskInfo(const std::string& taskId) const;

    /**
     * @brief 실행 중인 Task 수 조회
     */
    int getRunningTaskCount() const;

    /**
     * @brief 완료된 Task 수 조회
     */
    int getCompletedTaskCount() const;

    /**
     * @brief 실패한 Task 수 조회
     */
    int getFailedTaskCount() const;

    /**
     * @brief 모든 Task 정보 초기화
     */
    void clear();

    /**
     * @brief Task 정보 제거
     * @param taskId Task 식별자
     */
    void removeTask(const std::string& taskId);

private:
    mutable std::mutex mutex_;
    std::map<std::string, TaskExecutionInfo> tasks_;
};

} // namespace mxrc::core::task

#endif // MXRC_CORE_TASK_MONITOR_H
