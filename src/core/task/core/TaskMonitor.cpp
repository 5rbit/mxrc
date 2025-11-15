#include "core/task/core/TaskMonitor.h"

namespace mxrc::core::task {

using namespace mxrc::core::action;

TaskMonitor::TaskMonitor() {
    Logger::get()->info("[TaskMonitor] Initialized");
}

void TaskMonitor::startTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(mutex_);

    TaskExecutionInfo info;
    info.taskId = taskId;
    info.status = TaskStatus::RUNNING;
    info.progress = 0.0f;
    info.startTime = std::chrono::steady_clock::now();
    info.retryCount = 0;

    tasks_[taskId] = info;

    Logger::get()->info("[TaskMonitor] START - Task: {} (status: RUNNING)", taskId);
}

void TaskMonitor::updateProgress(const std::string& taskId, float progress) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) {
        it->second.progress = progress;
        Logger::get()->debug("[TaskMonitor] PROGRESS - Task: {} (progress: {:.1f}%)",
                            taskId, progress * 100.0f);
    } else {
        Logger::get()->warn("[TaskMonitor] PROGRESS - Task {} not found", taskId);
    }
}

void TaskMonitor::updateStatus(const std::string& taskId, TaskStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) {
        TaskStatus prevStatus = it->second.status;
        it->second.status = status;
        Logger::get()->info("[TaskMonitor] STATUS - Task: {} ({} -> {})",
                           taskId,
                           taskStatusToString(prevStatus),
                           taskStatusToString(status));
    } else {
        Logger::get()->warn("[TaskMonitor] STATUS - Task {} not found", taskId);
    }
}

void TaskMonitor::endTask(const std::string& taskId, TaskStatus status, const std::string& errorMessage) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) {
        it->second.status = status;
        it->second.endTime = std::chrono::steady_clock::now();
        it->second.errorMessage = errorMessage;
        it->second.progress = (status == TaskStatus::COMPLETED) ? 1.0f : it->second.progress;

        int64_t elapsedMs = it->second.getElapsedMs();

        if (status == TaskStatus::COMPLETED) {
            Logger::get()->info("[TaskMonitor] END - Task: {} completed in {}ms (progress: 100.0%)",
                               taskId, elapsedMs);
        } else {
            Logger::get()->error("[TaskMonitor] END - Task: {} failed in {}ms (status: {}, error: '{}')",
                                taskId, elapsedMs, taskStatusToString(status), errorMessage);
        }
    } else {
        Logger::get()->warn("[TaskMonitor] END - Task {} not found", taskId);
    }
}

void TaskMonitor::incrementRetryCount(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) {
        it->second.retryCount++;
        Logger::get()->info("[TaskMonitor] RETRY - Task: {} (retry count: {})",
                           taskId, it->second.retryCount);
    } else {
        Logger::get()->warn("[TaskMonitor] RETRY - Task {} not found", taskId);
    }
}

std::optional<TaskMonitor::TaskExecutionInfo> TaskMonitor::getTaskInfo(const std::string& taskId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) {
        return it->second;
    }
    return std::nullopt;
}

int TaskMonitor::getRunningTaskCount() const {
    std::lock_guard<std::mutex> lock(mutex_);

    int count = 0;
    for (const auto& [_, info] : tasks_) {
        if (info.status == TaskStatus::RUNNING) {
            count++;
        }
    }
    return count;
}

int TaskMonitor::getCompletedTaskCount() const {
    std::lock_guard<std::mutex> lock(mutex_);

    int count = 0;
    for (const auto& [_, info] : tasks_) {
        if (info.status == TaskStatus::COMPLETED) {
            count++;
        }
    }
    return count;
}

int TaskMonitor::getFailedTaskCount() const {
    std::lock_guard<std::mutex> lock(mutex_);

    int count = 0;
    for (const auto& [_, info] : tasks_) {
        if (info.status == TaskStatus::FAILED) {
            count++;
        }
    }
    return count;
}

void TaskMonitor::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t taskCount = tasks_.size();
    tasks_.clear();

    Logger::get()->info("[TaskMonitor] CLEAR - Removed {} task records", taskCount);
}

void TaskMonitor::removeTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) {
        tasks_.erase(it);
        Logger::get()->debug("[TaskMonitor] REMOVE - Task: {}", taskId);
    } else {
        Logger::get()->warn("[TaskMonitor] REMOVE - Task {} not found", taskId);
    }
}

} // namespace mxrc::core::task
