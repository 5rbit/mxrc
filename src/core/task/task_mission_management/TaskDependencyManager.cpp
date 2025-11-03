#include "TaskDependencyManager.h"
#include <iostream>
#include <chrono>

namespace mxrc {
namespace task_mission {

TaskDependencyManager::TaskDependencyManager() {
}

TaskDependencyManager::~TaskDependencyManager() {
}

void TaskDependencyManager::registerTaskDependencies(const std::string& taskId, const std::vector<std::string>& dependencies) {
    std::lock_guard<std::mutex> lock(mutex_);
    task_dependencies_[taskId] = std::set<std::string>(dependencies.begin(), dependencies.end());
    // Initialize task state if not already present
    if (task_states_.find(taskId) == task_states_.end()) {
        task_states_[taskId] = TaskState::PENDING;
    }
    for (const auto& dep_id : dependencies) {
        if (task_states_.find(dep_id) == task_states_.end()) {
            task_states_[dep_id] = TaskState::PENDING;
        }
    }
}

void TaskDependencyManager::updateTaskState(const std::string& taskId, TaskState newState) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (task_states_.find(taskId) != task_states_.end()) {
        task_states_[taskId] = newState;
        condition_.notify_all(); // Notify any waiting tasks
    }
}

bool TaskDependencyManager::areDependenciesMet(const std::string& taskId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = task_dependencies_.find(taskId);
    if (it == task_dependencies_.end()) {
        return true; // No dependencies registered, so they are met
    }

    for (const auto& dep_id : it->second) {
        auto state_it = task_states_.find(dep_id);
        if (state_it == task_states_.end() || (state_it->second != TaskState::COMPLETED && state_it->second != TaskState::CANCELLED)) {
            return false; // Dependency not found or not completed/cancelled
        }
    }
    return true;
}

bool TaskDependencyManager::waitForDependencies(const std::string& taskId, long long timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    return condition_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this, &taskId] {
        return areDependenciesMet(taskId);
    });
}

void TaskDependencyManager::clearAllDependencies() {
    std::lock_guard<std::mutex> lock(mutex_);
    task_states_.clear();
    task_dependencies_.clear();
}

} // namespace task_mission
} // namespace mxrc
