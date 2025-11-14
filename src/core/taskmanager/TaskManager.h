#pragma once

#include "interfaces/ITaskManager.h"
#include "interfaces/ICommand.h"
#include "TaskDefinitionRegistry.h"
#include "TaskExecutor.h"
#include <memory>
#include <string>
#include <vector>

namespace mxrc {
namespace core {
namespace taskmanager {

class TaskManager : public ITaskManager {
public:
    TaskManager(std::shared_ptr<TaskDefinitionRegistry> registry, std::shared_ptr<TaskExecutor> executor);
    virtual ~TaskManager() = default;

    // ITaskManager 인터페이스 구현
    std::string registerTaskDefinition(const std::string& taskName, const std::string& taskType, const std::map<std::string, std::string>& defaultParameters) override;
    std::vector<TaskDto> getAllTaskDefinitions() const override;
    std::unique_ptr<TaskDto> getTaskDefinitionById(const std::string& taskId) const override;
    std::string requestTaskExecution(const std::string& taskId, const std::map<std::string, std::string>& runtimeParameters) override;
    std::unique_ptr<TaskDto> getTaskExecutionStatus(const std::string& executionId) const override;
    void executeCommand(std::shared_ptr<ICommand> command) override;

    // Registry와 Executor에 대한 접근을 위한 getter
    std::shared_ptr<TaskDefinitionRegistry> getRegistry() const { return registry_; }
    std::shared_ptr<TaskExecutor> getExecutor() const { return executor_; }

private:
    std::shared_ptr<TaskDefinitionRegistry> registry_;
    std::shared_ptr<TaskExecutor> executor_;
    std::vector<TaskDto> tasks_; // 등록된 Task 정의들을 저장
};

} // namespace taskmanager
} // namespace core
} // namespace mxrc
