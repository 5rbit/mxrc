#pragma once
#include "core/taskmanager/Task.h" // Updated include
#include <iostream>
#include <map>
#include <string>

namespace mxrc::core::taskmanager::tasks {

class DriveForwardTask : public Task {
public:
    DriveForwardTask(const std::string& id, const std::string& type,
                     const std::map<std::string, std::string>& params);

    void execute() override;
    void cancel() override;
    void pause() override;
};

} // namespace mxrc::core::taskmanager::tasks
