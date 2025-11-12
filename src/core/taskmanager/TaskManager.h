#pragma once

#include "../interfaces/ITaskManager.h" // Updated include
#include "Task.h" // Already in core/
#include "../interfaces/ITask.h" // Corrected include
#include "../interfaces/ICommand.h"        // Updated include
#include "../factory/TaskFactory.h" // Corrected include
#include <map>
#include <stdexcept>
#include <string> // Ensure string is included

class TaskManager : public ITaskManager {
public:
    TaskManager() = default;
    virtual ~TaskManager() = default;

    virtual std::string registerTaskDefinition(const std::string& taskName, const std::string& taskType, const std::map<std::string, std::string>& defaultParameters) override;
    virtual std::vector<TaskDto> getAllTaskDefinitions() const override;
    virtual std::unique_ptr<TaskDto> getTaskDefinitionById(const std::string& taskId) const override;
    virtual std::string requestTaskExecution(const std::string& taskId, const std::map<std::string, std::string>& runtimeParameters) override;
    virtual std::unique_ptr<TaskDto> getTaskExecutionStatus(const std::string& executionId) const override;
    virtual void updateTaskStatus(const std::string& taskId, TaskStatus status) override;
    virtual void updateTaskProgress(const std::string& taskId, int progress) override;

    // New method to execute commands
    void executeCommand(std::unique_ptr<ICommand> command);

    // New method to add an executable task to the active list
    void addExecutableTask(const std::string& executionId, std::unique_ptr<ITask> executableTask); // Corrected type

    // New method to get an executable task
    ITask* getExecutableTask(const std::string& executionId); // Corrected type

private:
    std::map<std::string, std::unique_ptr<Task>> tasks_;
    std::map<std::string, std::unique_ptr<ITask>> activeExecutableTasks_; // Corrected type
    // In a real system, this would also manage active task executions, not just definitions.
    // For now, we'll use the same map for simplicity, assuming taskId is also executionId for simplicity.
};
