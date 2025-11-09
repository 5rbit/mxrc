#include "TaskManager.h"
#include <algorithm>

std::string TaskManager::registerTaskDefinition(const std::string& taskName, const std::string& taskType, const std::map<std::string, std::string>& defaultParameters) {
    // Check for duplicate task name
    auto it = std::find_if(tasks_.begin(), tasks_.end(), [&](const auto& pair) {
        return pair.second->getName() == taskName;
    });

    if (it != tasks_.end()) {
        throw std::runtime_error("Task with this name already exists.");
    }

    auto task = std::make_unique<Task>(taskName, taskType, defaultParameters);
    std::string taskId = task->getId();
    tasks_[taskId] = std::move(task);
    return taskId;
}

std::vector<TaskDto> TaskManager::getAllTaskDefinitions() const {
    std::vector<TaskDto> dtos;
    for (const auto& pair : tasks_) {
        const auto& task = pair.second;
        dtos.push_back({
            task->getId(),
            task->getName(),
            task->getType(),
            task->getParameters(),
            taskStatusToString(task->getStatus()),
            task->getProgress(),
            task->getCreatedAt(),
            task->getUpdatedAt()
        });
    }
    return dtos;
}

std::unique_ptr<TaskDto> TaskManager::getTaskDefinitionById(const std::string& taskId) const {
    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) {
        const auto& task = it->second;
        return std::make_unique<TaskDto>(
            task->getId(),
            task->getName(),
            task->getType(),
            task->getParameters(),
            taskStatusToString(task->getStatus()),
            task->getProgress(),
            task->getCreatedAt(),
            task->getUpdatedAt()
        );
    }
    return nullptr;
}

std::string TaskManager::requestTaskExecution(const std::string& taskId, const std::map<std::string, std::string>& runtimeParameters) {
    auto it = tasks_.find(taskId);
    if (it == tasks_.end()) {
        throw std::runtime_error("Task definition not found.");
    }
    // For simplicity, we'll just update the status of the existing task definition.
    // In a real system, this would create a new TaskExecution instance.
    it->second->setStatus(TaskStatus::RUNNING);
    it->second->setParameters(runtimeParameters); // Update parameters for execution
    return taskId; // Returning taskId as executionId for simplicity
}

std::unique_ptr<TaskDto> TaskManager::getTaskExecutionStatus(const std::string& executionId) const {
    // For simplicity, executionId is assumed to be taskId
    return getTaskDefinitionById(executionId);
}

void TaskManager::updateTaskStatus(const std::string& taskId, TaskStatus status) {
    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) {
        it->second->setStatus(status);
    } else {
        throw std::runtime_error("Task not found for status update.");
    }
}
