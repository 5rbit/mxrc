#pragma once
#include "../interfaces/ICommand.h"
#include "../TaskManager.h"
#include "../interfaces/ITask.h"
#include "../TaskDefinitionRegistry.h" // Use TaskDefinitionRegistry
#include <string>
#include <map>
#include <memory>

namespace mxrc::core::taskmanager {

class StartTaskCommand : public ICommand {
public:
    StartTaskCommand(TaskManager& taskManager, const std::string& taskId, const std::map<std::string, std::string>& runtimeParameters);
    void execute() override;

private:
    TaskManager& taskManager_;
    std::string taskId_;
    std::map<std::string, std::string> runtimeParameters_;
};

} // namespace mxrc::core::taskmanager
