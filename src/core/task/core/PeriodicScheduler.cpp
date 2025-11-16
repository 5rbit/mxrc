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
    std::unique_ptr<ScheduleInfo> oldInfo;

    // 1. 이미 실행 중인 스케줄 추출
    // 뮤텍스 안전성: 짧은 임계 영역, 블로킹 작업 없음
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (schedules_.count(taskId)) {
            Logger::get()->warn("[PeriodicScheduler] Task {} already scheduled, restarting", taskId);
            oldInfo = stopInternal(taskId);
        }
    }

    // 2. 이전 스레드 정리 (mutex lock 없이)
    // 데드락 방지: 스레드 join을 뮤텍스 외부에서 수행하여 다른 작업 블로킹 방지
    if (oldInfo) {
        Logger::get()->info("[PeriodicScheduler] Stopping previous instance of task {}", taskId);
        oldInfo->running = false;
        if (oldInfo->thread && oldInfo->thread->joinable()) {
            oldInfo->thread->join();
        }
        Logger::get()->info("[PeriodicScheduler] Previous instance of task {} stopped (total executions: {})",
                           taskId, oldInfo->executionCount.load());
    }

    // 3. 새 스케줄 생성 및 시작
    // 뮤텍스 안전성: 스레드가 즉시 뮤텍스를 획득하지 않으므로 뮤텍스 보유 중 스레드 시작 안전
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto newInfo = std::make_unique<ScheduleInfo>();
        newInfo->taskId = taskId;
        newInfo->interval = interval;
        newInfo->callback = callback;
        newInfo->running = true;

        Logger::get()->info("[PeriodicScheduler] START - Task: {} (interval: {}ms)",
                           taskId, interval.count());

        // 스케줄 실행 스레드 시작
        newInfo->thread = std::make_unique<std::thread>(
            &PeriodicScheduler::runSchedule, this, newInfo.get()
        );

        schedules_[taskId] = std::move(newInfo);
    }
}

void PeriodicScheduler::stop(const std::string& taskId) {
    std::unique_ptr<ScheduleInfo> info;

    // 뮤텍스 안전성: 락 보유 중 스케줄 정보 추출
    {
        std::lock_guard<std::mutex> lock(mutex_);
        info = stopInternal(taskId);
    }

    // 데드락 방지: 스레드 join을 뮤텍스 외부에서 수행
    if (info && info->running) {
        info->running = false;
        if (info->thread && info->thread->joinable()) {
            info->thread->join();
        }
        Logger::get()->info("[PeriodicScheduler] STOP - Task: {} (total executions: {})",
                           taskId, info->executionCount.load());
    }
}

std::unique_ptr<PeriodicScheduler::ScheduleInfo> PeriodicScheduler::stopInternal(const std::string& taskId) {
    auto it = schedules_.find(taskId);
    if (it == schedules_.end()) {
        Logger::get()->warn("[PeriodicScheduler] STOP - Task {} not found", taskId);
        return nullptr;
    }

    std::unique_ptr<ScheduleInfo> info = std::move(it->second);
    schedules_.erase(it);
    return info;
}

bool PeriodicScheduler::isRunning(const std::string& taskId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = schedules_.find(taskId);
    return it != schedules_.end() && it->second->running;
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
    std::vector<std::unique_ptr<ScheduleInfo>> allSchedules;

    // 뮤텍스 안전성: 락 보유 중 모든 스케줄 추출
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [taskId, info] : schedules_) {
            allSchedules.push_back(std::move(info));
        }
        schedules_.clear();
    }

    // 데드락 방지: 모든 스레드 join을 뮤텍스 외부에서 수행
    for (const auto& info : allSchedules) {
        if (info && info->running) {
            info->running = false;
            if (info->thread && info->thread->joinable()) {
                info->thread->join();
            }
            Logger::get()->info("[PeriodicScheduler] STOP - Task: {} (total executions: {})",
                               info->taskId, info->executionCount.load());
        }
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
