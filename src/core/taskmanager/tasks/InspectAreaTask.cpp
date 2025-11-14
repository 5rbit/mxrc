#include "InspectAreaTask.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace mxrc::core::taskmanager::tasks {

InspectAreaTask::InspectAreaTask(const std::string& id, const std::string& type, const std::map<std::string, std::string>& params)
    : Task(id, "InspectAreaTask", type, params) {
}

void InspectAreaTask::execute() {
    std::cout << "InspectAreaTask::execute() called. Task ID: " << getId() << ", Type: " << getType() << ", Parameters: ";
    for (const auto& [key, value] : getParameters()) {
        std::cout << key << "=" << value << " ";
    }
    std::cout << std::endl;

    status_ = TaskStatus::RUNNING;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    progress_ = 0.3f;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    progress_ = 0.7f;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    progress_ = 1.0f;
    status_ = TaskStatus::COMPLETED;
}

void InspectAreaTask::cancel() {
    std::cout << "InspectAreaTask::cancel() called. Task ID: " << getId() << std::endl;
    if (status_ == TaskStatus::RUNNING || status_ == TaskStatus::PAUSED) {
        status_ = TaskStatus::CANCELLED;
    }
}

void InspectAreaTask::pause() {
    std::cout << "InspectAreaTask::pause() called. Task ID: " << getId() << std::endl;
    if (status_ == TaskStatus::RUNNING) {
        status_ = TaskStatus::PAUSED;
    }
}

} // namespace mxrc::core::taskmanager::tasks
