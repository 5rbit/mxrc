#include "LiftPalletTask.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace mxrc::core::taskmanager::tasks {

LiftPalletTask::LiftPalletTask(const std::string& id, const std::string& type, const std::map<std::string, std::string>& params)
    : Task(id, "LiftPalletTask", type, params) {
}

void LiftPalletTask::execute() {
    std::cout << "LiftPalletTask::execute() called. Task ID: " << getId() << ", Type: " << getType() << ", Parameters: ";
    for (const auto& [key, value] : getParameters()) {
        std::cout << key << "=" << value << " ";
    }
    std::cout << std::endl;

    status_ = TaskStatus::RUNNING;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    progress_ = 0.25f;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    progress_ = 0.5f;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    progress_ = 0.75f;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    progress_ = 1.0f;
    status_ = TaskStatus::COMPLETED;
}

void LiftPalletTask::cancel() {
    std::cout << "LiftPalletTask::cancel() called. Task ID: " << getId() << std::endl;
    if (status_ == TaskStatus::RUNNING || status_ == TaskStatus::PAUSED) {
        status_ = TaskStatus::CANCELLED;
    }
}

void LiftPalletTask::pause() {
    std::cout << "LiftPalletTask::pause() called. Task ID: " << getId() << std::endl;
    if (status_ == TaskStatus::RUNNING) {
        status_ = TaskStatus::PAUSED;
    }
}

} // namespace mxrc::core::taskmanager::tasks
