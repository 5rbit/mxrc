#pragma once
#include "../interfaces/ICommand.h" // Updated include
#include "../TaskManager.h" // Updated include
#include "../interfaces/ITask.h" // Corrected include
#include "../factory/TaskFactory.h" // Corrected include
#include <string>
#include <map>
#include <memory>

class StartTaskCommand : public ICommand {
public:
    StartTaskCommand(TaskManager& taskManager, const std::string& taskId, const std::map<std::string, std::string>& runtimeParameters);
    void execute() override;

private:
    TaskManager& taskManager_;
    std::string taskId_;
    std::map<std::string, std::string> runtimeParameters_;
};
