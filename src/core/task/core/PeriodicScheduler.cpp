#include "core/task/core/PeriodicScheduler.h"

namespace mxrc::core::task {

using namespace mxrc::core::action;

PeriodicScheduler::PeriodicScheduler() {
    Logger::get()->info("[PeriodicScheduler] Initialized");
}

PeriodicScheduler::~PeriodicScheduler() {
    stopAll();
    Logger::get()->info("[PeriodicScheduler] Destroyed");
}

void PeriodicScheduler::start(
    const std::string& taskId,
    std::chrono::milliseconds interval,
    ExecutionCallback callback
) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 이미 실행 중이면 중지
    if (schedules_.find(taskId) != schedules_.end()) {
        Logger::get()->warn("[PeriodicScheduler] Task {} already scheduled, restarting", taskId);
        stop(taskId);
    }

    auto info = std::make_unique<ScheduleInfo>();
    info->taskId = taskId;
    info->interval = interval;
    info->callback = callback;
    info->running = true;

    Logger::get()->info("[PeriodicScheduler] START - Task: {} (interval: {}ms)",
                       taskId, interval.count());

    // 스케줄 실행 스레드 시작
    info->thread = std::make_unique<std::thread>(
        &PeriodicScheduler::runSchedule, this, info.get()
    );

    schedules_[taskId] = std::move(info);
}

void PeriodicScheduler::stop(const std::string& taskId) {
    std::unique_ptr<ScheduleInfo> info;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = schedules_.find(taskId);
        if (it == schedules_.end()) {
            Logger::get()->warn("[PeriodicScheduler] STOP - Task {} not found", taskId);
            return;
        }

        info = std::move(it->second);
        schedules_.erase(it);
    }

    // 스레드 중지
    if (info && info->running) {
        info->running = false;
        if (info->thread && info->thread->joinable()) {
            info->thread->join();
        }
        Logger::get()->info("[PeriodicScheduler] STOP - Task: {} (total executions: {})",
                           taskId, info->executionCount.load());
    }
}

bool PeriodicScheduler::isRunning(const std::string& taskId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = schedules_.find(taskId);
    return (it != schedules_.end()) && it->second->running;
}

int PeriodicScheduler::getExecutionCount(const std::string& taskId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = schedules_.find(taskId);
    if (it != schedules_.end()) {
        return it->second->executionCount;
    }
    return 0;
}

void PeriodicScheduler::stopAll() {
    std::vector<std::string> taskIds;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [taskId, _] : schedules_) {
            taskIds.push_back(taskId);
        }
    }

    for (const auto& taskId : taskIds) {
        stop(taskId);
    }

    Logger::get()->info("[PeriodicScheduler] All schedules stopped");
}

void PeriodicScheduler::runSchedule(ScheduleInfo* info) {
    Logger::get()->debug("[PeriodicScheduler] Schedule thread started for task: {}",
                        info->taskId);

    ExecutionContext context;

    while (info->running) {
        auto startTime = std::chrono::steady_clock::now();

        try {
            // 콜백 실행
            info->callback(context);
            info->executionCount++;

            Logger::get()->debug("[PeriodicScheduler] Task {} executed (count: {})",
                                info->taskId, info->executionCount.load());

        } catch (const std::exception& e) {
            Logger::get()->error("[PeriodicScheduler] Task {} execution failed: {}",
                                info->taskId, e.what());
        }

        // 다음 실행까지 대기
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime
        );

        auto sleepTime = info->interval - elapsed;
        if (sleepTime.count() > 0 && info->running) {
            std::this_thread::sleep_for(sleepTime);
        }
    }

    Logger::get()->debug("[PeriodicScheduler] Schedule thread ended for task: {}",
                        info->taskId);
}

} // namespace mxrc::core::task
