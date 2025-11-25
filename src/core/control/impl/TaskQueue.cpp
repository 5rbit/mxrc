#include "TaskQueue.h"
#include <spdlog/spdlog.h>

namespace mxrc::core::control {

bool TaskQueue::enqueue(std::shared_ptr<task::ITask> task, Priority priority) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string task_id = task->getId();

    // 이미 큐에 있는지 확인
    if (task_priority_map_.find(task_id) != task_priority_map_.end()) {
        spdlog::warn("[TaskQueue] Task already in queue: {}", task_id);
        return false;
    }

    // Priority에 해당하는 큐에 추가
    getQueue(priority).push(task);

    // 매핑 저장
    task_priority_map_[task_id] = priority;

    spdlog::debug("[TaskQueue] Enqueued task: {} (priority: {})",
        task_id, static_cast<int>(priority));

    return true;
}

std::optional<std::shared_ptr<task::ITask>> TaskQueue::dequeue() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 우선순위 순서대로 검색
    std::optional<std::shared_ptr<task::ITask>> task;

    if ((task = emergency_stop_.pop())) {
        task_priority_map_.erase((*task)->getId());
        return task;
    }

    if ((task = safety_issue_.pop())) {
        task_priority_map_.erase((*task)->getId());
        return task;
    }

    if ((task = urgent_task_.pop())) {
        task_priority_map_.erase((*task)->getId());
        return task;
    }

    if ((task = normal_task_.pop())) {
        task_priority_map_.erase((*task)->getId());
        return task;
    }

    if ((task = maintenance_.pop())) {
        task_priority_map_.erase((*task)->getId());
        return task;
    }

    return std::nullopt;
}

bool TaskQueue::isEmpty() const {
    std::lock_guard<std::mutex> lock(mutex_);

    return emergency_stop_.empty() &&
           safety_issue_.empty() &&
           urgent_task_.empty() &&
           normal_task_.empty() &&
           maintenance_.empty();
}

size_t TaskQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);

    return emergency_stop_.size() +
           safety_issue_.size() +
           urgent_task_.size() +
           normal_task_.size() +
           maintenance_.size();
}

void TaskQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    emergency_stop_.clear();
    safety_issue_.clear();
    urgent_task_.clear();
    normal_task_.clear();
    maintenance_.clear();

    task_priority_map_.clear();

    spdlog::info("[TaskQueue] Cleared all tasks");
}

bool TaskQueue::remove(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Task의 Priority 확인
    auto it = task_priority_map_.find(task_id);
    if (it == task_priority_map_.end()) {
        spdlog::warn("[TaskQueue] Task not found: {}", task_id);
        return false;
    }

    Priority priority = it->second;

    // 해당 Priority 큐에서 제거
    bool removed = getQueue(priority).remove(task_id);

    if (removed) {
        task_priority_map_.erase(it);
        spdlog::info("[TaskQueue] Removed task: {}", task_id);
    }

    return removed;
}

TaskQueue::PriorityQueue& TaskQueue::getQueue(Priority priority) {
    switch (priority) {
        case Priority::EMERGENCY_STOP:
            return emergency_stop_;
        case Priority::SAFETY_ISSUE:
            return safety_issue_;
        case Priority::URGENT_TASK:
            return urgent_task_;
        case Priority::NORMAL_TASK:
            return normal_task_;
        case Priority::MAINTENANCE:
            return maintenance_;
        default:
            spdlog::error("[TaskQueue] Unknown priority: {}",
                static_cast<int>(priority));
            return normal_task_;  // Fallback
    }
}

} // namespace mxrc::core::control
