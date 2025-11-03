#ifndef MXRC_DRIVE_TO_POSITION_TASK_H
#define MXRC_DRIVE_TO_POSITION_TASK_H

#include "AbstractTask.h"
#include "TaskContext.h"
#include <string>
#include <iostream>

namespace mxrc {
namespace task_mission {

class DriveToPositionTask : public AbstractTask {
public:
    DriveToPositionTask(const std::string& id) : taskId_(id) {}

    bool initialize(TaskContext& context) override {
        std::cout << "DriveToPositionTask " << taskId_ << ": Initializing..." << std::endl;
        // Example: Get target position from context
        try {
            targetX_ = context.get<double>("target_x");
            targetY_ = context.get<double>("target_y");
            speed_ = context.get<double>("speed");
            std::cout << "DriveToPositionTask " << taskId_ << ": Target: (" << targetX_ << ", " << targetY_ << "), Speed: " << speed_ << std::endl;
            currentState = TaskState::PENDING;
            return true;
        } catch (const std::runtime_error& e) {
            std::cerr << "DriveToPositionTask " << taskId_ << ": Initialization failed - " << e.what() << std::endl;
            currentState = TaskState::FAILED;
            return false;
        }
    }

    bool execute(TaskContext& context) override {
        if (currentState == TaskState::PENDING || currentState == TaskState::PAUSED) {
            currentState = TaskState::RUNNING;
            std::cout << "DriveToPositionTask " << taskId_ << ": Executing drive to (" << targetX_ << ", " << targetY_ << ") at speed " << speed_ << std::endl;
            // Simulate driving
            // In a real scenario, this would involve robot control commands
            // For now, we'll just complete it successfully
            context.set<bool>("drive_successful", true);
            currentState = TaskState::COMPLETED;
            return true;
        } else if (currentState == TaskState::RUNNING) {
            std::cout << "DriveToPositionTask " << taskId_ << ": Still driving..." << std::endl;
            return true; // Still running
        } else {
            std::cout << "DriveToPositionTask " << taskId_ << ": Cannot execute from current state " << static_cast<int>(currentState) << std::endl;
            return false;
        }
    }

    void terminate(TaskContext& context) override {
        std::cout << "DriveToPositionTask " << taskId_ << ": Terminating..." << std::endl;
        // Clean up resources if any
        if (currentState == TaskState::RUNNING) {
            currentState = TaskState::CANCELLED; // Or FAILED if termination was due to an error
        }
    }

    std::string getTaskId() const override {
        return taskId_;
    }

    FailureStrategy getFailureStrategy() const override { return FailureStrategy::RETRY_TRANSIENT; } // Example strategy

private:
    std::string taskId_;
    double targetX_ = 0.0;
    double targetY_ = 0.0;
    double speed_ = 0.0;
};

} // namespace task_mission
} // namespace mxrc

#endif // MXRC_DRIVE_TO_POSITION_TASK_H