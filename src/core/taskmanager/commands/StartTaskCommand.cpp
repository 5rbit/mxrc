#include "commands/StartTaskCommand.h" // Updated include
#include <iostream> // For logging
#include "../TaskDto.h" // Explicitly include TaskDto for unique_ptr<TaskDto>
#include "../interfaces/ITask.h" // Corrected include
#include "../factory/TaskFactory.h" // Corrected include

StartTaskCommand::StartTaskCommand(TaskManager& taskManager, const std::string& taskId, const std::map<std::string, std::string>& runtimeParameters)
    : taskManager_(taskManager), taskId_(taskId), runtimeParameters_(runtimeParameters) {}

void StartTaskCommand::execute() {
    // 1. Get the Task definition from TaskManager
    std::unique_ptr<TaskDto> taskDto = taskManager_.getTaskDefinitionById(taskId_);
    if (!taskDto) {
        std::cerr << "Error: StartTaskCommand failed. Task definition not found for ID: " << taskId_ << std::endl;
        throw std::runtime_error("Task definition not found.");
    }

    // 2. Use TaskFactory to create an ITask instance
    std::unique_ptr<ITask> executableTask; // Corrected type
    try {
        // Combine default parameters with runtime parameters, runtime parameters override defaults
        std::map<std::string, std::string> combinedParameters = taskDto->parameters;
        for (const auto& [key, value] : runtimeParameters_) {
            combinedParameters[key] = value;
        }

        executableTask = TaskFactory::getInstance().createTask(taskDto->type, combinedParameters); // Corrected factory and method
        std::cout << "StartTaskCommand: Created executable task of type '" << taskDto->type << "' for task ID: " << taskId_ << std::endl;
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: StartTaskCommand failed to create executable task: " << e.what() << std::endl;
        throw; // Re-throw the exception
    }

    // 3. Store this ITask in TaskManager::activeExecutableTasks_
    // For simplicity, using taskId as executionId. In a real system, a unique executionId would be generated.
    taskManager_.addExecutableTask(taskId_, std::move(executableTask));

    // 4. Update the Task DTO's status in TaskManager::tasks_ to RUNNING
    taskManager_.updateTaskStatus(taskId_, TaskStatus::RUNNING);

    // 5. Call execute() on the ITask, now that it's managed by TaskManager
    ITask* activeTask = taskManager_.getExecutableTask(taskId_); // Corrected type
    if (activeTask) {
        activeTask->execute();
        std::cout << "StartTaskCommand: Executable task for ID " << taskId_ << " started." << std::endl;
    } else {
        std::cerr << "Error: StartTaskCommand could not retrieve active executable task for ID: " << taskId_ << std::endl;
        throw std::runtime_error("Failed to retrieve active executable task.");
    }
}
