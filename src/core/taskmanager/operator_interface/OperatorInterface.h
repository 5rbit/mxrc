#pragma once

#include "../interfaces/IOperatorInterface.h" // Updated include
#include "../interfaces/ITaskManager.h" // Updated include
#include <memory>

namespace mxrc {
namespace core {
namespace taskmanager {

class OperatorInterface : public IOperatorInterface {
public:
    // Constructor takes a shared_ptr to ITaskManager to allow dependency injection
    OperatorInterface(std::shared_ptr<ITaskManager> taskManager);
    virtual ~OperatorInterface() = default;

    virtual std::string defineNewTask(const std::string& taskName, const std::string& taskType, const std::map<std::string, std::string>& defaultParameters) override;
    virtual std::vector<TaskDto> getAvailableTasks() const override;
    virtual std::unique_ptr<TaskDto> getTaskDetails(const std::string& taskId) const override;
    virtual std::string startTaskExecution(const std::string& taskId, const std::map<std::string, std::string>& runtimeParameters) override;
    virtual std::unique_ptr<TaskDto> monitorTaskStatus(const std::string& executionId) const override;
    virtual void cancelTask(const std::string& taskId) override;
    virtual void pauseTask(const std::string& taskId) override;

private:
    std::shared_ptr<ITaskManager> taskManager_;
};

} // namespace taskmanager
} // namespace core
} // namespace mxrc
