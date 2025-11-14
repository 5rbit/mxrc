#pragma once

#include "core/taskmanager/interfaces/ICommand.h"
#include <string>

namespace mxrc::core::taskmanager {

class ITaskManager; // Forward declaration

class PauseTaskCommand : public ICommand {
public:
    PauseTaskCommand(ITaskManager& taskManager, std::string taskId);
    ~PauseTaskCommand() override = default;

    void execute() override;

    const std::string& getTaskId() const { return taskId_; } // Add getter for taskId

private:
    ITaskManager& taskManager_;
    std::string taskId_;
};

} // namespace mxrc::core::taskmanager
