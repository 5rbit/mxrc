#include "OperatorInterface.h"

OperatorInterface::OperatorInterface(std::shared_ptr<ITaskManager> taskManager)
    : taskManager_(std::move(taskManager))
{
    if (!taskManager_) {
        throw std::invalid_argument("ITaskManager cannot be null.");
    }
}

std::string OperatorInterface::defineNewTask(const std::string& taskName, const std::string& taskType, const std::map<std::string, std::string>& defaultParameters) {
    return taskManager_->registerTaskDefinition(taskName, taskType, defaultParameters);
}

std::vector<TaskDto> OperatorInterface::getAvailableTasks() const {
    return taskManager_->getAllTaskDefinitions();
}

std::unique_ptr<TaskDto> OperatorInterface::getTaskDetails(const std::string& taskId) const {
    return taskManager_->getTaskDefinitionById(taskId);
}

std::string OperatorInterface::startTaskExecution(const std::string& taskId, const std::map<std::string, std::string>& runtimeParameters) {
    return taskManager_->requestTaskExecution(taskId, runtimeParameters);
}

std::unique_ptr<TaskDto> OperatorInterface::monitorTaskStatus(const std::string& executionId) const {
    return taskManager_->getTaskExecutionStatus(executionId);
}
