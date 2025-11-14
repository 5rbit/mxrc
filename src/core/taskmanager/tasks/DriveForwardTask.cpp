#include "DriveForwardTask.h"
#include <iostream>
#include <chrono>
#include <thread>

namespace mxrc::core::taskmanager::tasks {

DriveForwardTask::DriveForwardTask(const std::string& id, const std::string& type,
                                   const std::map<std::string, std::string>& params)
    : Task(id, "DriveForward", type, params) {
}

void DriveForwardTask::execute() {
    status_ = TaskStatus::RUNNING;
    progress_ = 0.0f;
    std::cout << "DriveForwardTask [" << id_ << "] is starting. Parameters: ";
    for (const auto& [key, value] : parameters_) {
        std::cout << key << "=" << value << " ";
    }
    std::cout << std::endl;

    for (int i = 0; i <= 100; ++i) {
        if (status_ == TaskStatus::CANCELLED || status_ == TaskStatus::PAUSED) {
            std::cout << "DriveForwardTask [" << id_ << "] was "
                     << (status_ == TaskStatus::CANCELLED ? "cancelled" : "paused")
                     << "." << std::endl;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        progress_ = static_cast<float>(i) / 100.0f;
    }

    progress_ = 1.0f;
    status_ = TaskStatus::COMPLETED;
    std::cout << "DriveForwardTask [" << id_ << "] has completed." << std::endl;
}

void DriveForwardTask::cancel() {
    if (status_ == TaskStatus::RUNNING || status_ == TaskStatus::PAUSED) {
        status_ = TaskStatus::CANCELLED;
    }
}

void DriveForwardTask::pause() {
    if (status_ == TaskStatus::RUNNING) {
        status_ = TaskStatus::PAUSED;
    }
}

} // namespace mxrc::core::taskmanager::tasks
