#ifndef MXRC_CORE_TASK_PERIODIC_SCHEDULER_H
#define MXRC_CORE_TASK_PERIODIC_SCHEDULER_H

#include "core/task/dto/TaskDefinition.h"
#include "core/action/util/ExecutionContext.h"
#include "core/action/util/Logger.h"
#include <functional>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>

namespace mxrc::core::task {

/**
 * @brief 주기적 Task 실행을 위한 스케줄러
 *
 * Phase 3B-2에서 구현
 * Task를 지정된 간격으로 반복 실행합니다.
 */
class PeriodicScheduler {
public:
    using ExecutionCallback = std::function<void(mxrc::core::action::ExecutionContext&)>;

    PeriodicScheduler();
    ~PeriodicScheduler();

    // 복사 및 이동 금지
    PeriodicScheduler(const PeriodicScheduler&) = delete;
    PeriodicScheduler& operator=(const PeriodicScheduler&) = delete;
    PeriodicScheduler(PeriodicScheduler&&) = delete;
    PeriodicScheduler& operator=(PeriodicScheduler&&) = delete;

    /**
     * @brief 주기적 실행 시작
     * @param taskId Task 식별자
     * @param interval 실행 간격
     * @param callback 실행할 콜백 함수
     */
    void start(
        const std::string& taskId,
        std::chrono::milliseconds interval,
        ExecutionCallback callback
    );

    /**
     * @brief 주기적 실행 중지
     * @param taskId Task 식별자
     */
    void stop(const std::string& taskId);

    /**
     * @brief 실행 중인지 확인
     * @param taskId Task 식별자
     */
    bool isRunning(const std::string& taskId) const;

    /**
     * @brief 실행 횟수 조회
     * @param taskId Task 식별자
     */
    int getExecutionCount(const std::string& taskId) const;

    /**
     * @brief 모든 스케줄 중지
     */
    void stopAll();

private:
    struct ScheduleInfo {
        std::string taskId;
        std::chrono::milliseconds interval;
        ExecutionCallback callback;
        std::unique_ptr<std::thread> thread;
        std::atomic<bool> running{false};
        std::atomic<int> executionCount{0};
    };

    mutable std::mutex mutex_;
    std::map<std::string, std::unique_ptr<ScheduleInfo>> schedules_;

    /**
     * @brief 스케줄 실행 루프
     */
    void runSchedule(ScheduleInfo* info);

    /**
     * @brief 주기적 실행 중지 (내부용, 잠금 없음)
     * @param taskId Task 식별자
     * @return 중지된 스케줄 정보. 해당 taskId가 없으면 nullptr.
     */
    std::unique_ptr<ScheduleInfo> stopInternal(const std::string& taskId);
};

} // namespace mxrc::core::task

#endif // MXRC_CORE_TASK_PERIODIC_SCHEDULER_H
