#include "commands/StartTaskCommand.h"
#include <iostream>
#include "../TaskDto.h"
#include "../interfaces/ITask.h"
#include "../TaskDefinitionRegistry.h"
#include "../TaskExecutor.h"

namespace mxrc::core::taskmanager {

StartTaskCommand::StartTaskCommand(TaskManager& taskManager, const std::string& taskId, const std::map<std::string, std::string>& runtimeParameters)
    : taskManager_(taskManager), taskId_(taskId), runtimeParameters_(runtimeParameters) {}

void StartTaskCommand::execute() {
    std::cout << "StartTaskCommand: Executing task " << taskId_ << std::endl;

    auto registry = taskManager_.getRegistry();
    auto executor = taskManager_.getExecutor();

    if (!registry || !executor) {
        std::cerr << "StartTaskCommand: Registry or Executor is null!" << std::endl;
        return;
    }

    auto taskDto = taskManager_.getTaskDefinitionById(taskId_);
    if (!taskDto) {
        std::cerr << "StartTaskCommand: Task definition not found for ID " << taskId_ << std::endl;
        return;
    }

    const auto* definition = registry->getDefinition(taskDto->type);
    if (!definition) {
        std::cerr << "StartTaskCommand: Task type '" << taskDto->type << "' not found in registry." << std::endl;
        return;
    }

    // Merge default and runtime parameters
    std::map<std::string, std::string> finalParameters = definition->defaultParams;
    finalParameters.insert(runtimeParameters_.begin(), runtimeParameters_.end());

    auto task = registry->createTask(taskDto->type, taskId_, taskDto->type, finalParameters);

    if (!task) {
        std::cerr << "StartTaskCommand: Failed to create task of type " << taskDto->type << std::endl;
        return;
    }

    executor->submit(task);
}

} // namespace mxrc::core::taskmanager
