#include "DummyTask.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace mxrc::core::taskmanager::tasks {

DummyTask::DummyTask(const std::string& id, const std::string& type, const std::map<std::string, std::string>& params)
    : Task(id, "DummyTask", type, params) {
}

void DummyTask::execute() {
    std::cout << "DummyTask::execute() called. Task ID: " << getId() << ", Type: " << getType() << ", Parameters: ";
    for (const auto& [key, value] : getParameters()) {
        std::cout << key << "=" << value << " ";
    }
    std::cout << std::endl;

    status_ = TaskStatus::RUNNING;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    progress_ = 0.5f;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    progress_ = 1.0f;
    status_ = TaskStatus::COMPLETED;
}

void DummyTask::cancel() {
    std::cout << "DummyTask::cancel() called. Task ID: " << getId() << std::endl;
    if (status_ == TaskStatus::RUNNING || status_ == TaskStatus::PAUSED) {
        status_ = TaskStatus::CANCELLED;
    }
}

void DummyTask::pause() {
    std::cout << "DummyTask::pause() called. Task ID: " << getId() << std::endl;
    if (status_ == TaskStatus::RUNNING) {
        status_ = TaskStatus::PAUSED;
    }
}

} // namespace mxrc::core::taskmanager::tasks
