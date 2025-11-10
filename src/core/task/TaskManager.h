#pragma once

#include "ITaskManager.h"
#include "Task.h"
#include <map>
#include <stdexcept>

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
    virtual void updateTaskProgress(const std::string& taskId, int progress);

private:
    std::map<std::string, std::unique_ptr<Task>> tasks_;
    // In a real system, this would also manage active task executions, not just definitions.
    // For now, we'll use the same map for simplicity, assuming taskId is also executionId for simplicity.
};
