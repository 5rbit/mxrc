#include "FailureTypeTask.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace mxrc::core::taskmanager::tasks {

FailureTypeTask::FailureTypeTask(const std::string& id, const std::string& type, const std::map<std::string, std::string>& params)
    : Task(id, "FailureTypeTask", type, params) {
}

void FailureTypeTask::execute() {
    std::cout << "FailureTypeTask::execute() called. Task ID: " << getId() << ", Type: " << getType() << ", Parameters: ";
    for (const auto& [key, value] : getParameters()) {
        std::cout << key << "=" << value << " ";
    }
    std::cout << std::endl;

    status_ = TaskStatus::RUNNING;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    progress_ = 0.2f;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Simulate a task failure
    std::cerr << "Simulating a task failure within FailureTypeTask. Task ID: " << getId() << std::endl;
    status_ = TaskStatus::FAILED;
    progress_ = 1.0f;
}

void FailureTypeTask::cancel() {
    std::cout << "FailureTypeTask::cancel() called. Task ID: " << getId() << std::endl;
    if (status_ == TaskStatus::RUNNING || status_ == TaskStatus::PAUSED) {
        status_ = TaskStatus::CANCELLED;
    }
}

void FailureTypeTask::pause() {
    std::cout << "FailureTypeTask::pause() called. Task ID: " << getId() << std::endl;
    if (status_ == TaskStatus::RUNNING) {
        status_ = TaskStatus::PAUSED;
    }
}

} // namespace mxrc::core::taskmanager::tasks
