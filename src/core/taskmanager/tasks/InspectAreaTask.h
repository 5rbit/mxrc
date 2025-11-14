#pragma once
#include "../Task.h" // Inherit from the new Task base class
#include <iostream>
#include <map>
#include <string>

namespace mxrc::core::taskmanager::tasks {

class InspectAreaTask : public Task {
public:
    InspectAreaTask(const std::string& id, const std::string& type, const std::map<std::string, std::string>& params);

    void execute() override;
    void cancel() override;
    void pause() override;
};

} // namespace mxrc::core::taskmanager::tasks
