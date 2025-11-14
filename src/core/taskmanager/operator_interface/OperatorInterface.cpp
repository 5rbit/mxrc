#include "operator_interface/OperatorInterface.h"
#include "../interfaces/ITaskManager.h"
#include "../TaskDto.h"
#include "../commands/CancelTaskCommand.h" // Include CancelTaskCommand
#include "../commands/PauseTaskCommand.h"  // Include PauseTaskCommand
#include <memory>
#include <utility>
#include <stdexcept>

namespace mxrc::core::taskmanager {

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
    // The StartTaskCommand will be created and executed by the TaskManager
    // This method will return an execution ID, which for now is the taskId itself.
    // In a real system, a unique execution ID would be generated.
    return taskManager_->requestTaskExecution(taskId, runtimeParameters);
}

std::unique_ptr<TaskDto> OperatorInterface::monitorTaskStatus(const std::string& executionId) const {
    return taskManager_->getTaskExecutionStatus(executionId);
}

void OperatorInterface::cancelTask(const std::string& taskId) {
    // Create and execute a CancelTaskCommand
    auto command = std::make_shared<CancelTaskCommand>(*taskManager_.get(), taskId);
    taskManager_->executeCommand(command);
}

void OperatorInterface::pauseTask(const std::string& taskId) {
    // Create and execute a PauseTaskCommand
    auto command = std::make_shared<PauseTaskCommand>(*taskManager_.get(), taskId);
    taskManager_->executeCommand(command);
}

} // namespace mxrc::core::taskmanager
