#include <iostream>
#include <memory>
#include "core/task/TaskManager.h"
#include "core/task/OperatorInterface.h"

int main() {
    std::cout << "MXRC Task Management Module Example" << std::endl;

    // Create a TaskManager instance
    auto taskManager = std::make_shared<mxrc::core::task::TaskManager>();

    // Create an OperatorInterface instance, injecting the TaskManager
    mxrc::core::task::OperatorInterface opInterface(taskManager);

    // Define a new Task using the OperatorInterface
    std::map<std::string, std::string> driveParams = {{"speed", "1.0"}, {"distance", "10.0"}};
    std::string driveTaskId = opInterface.defineNewTask("DriveForward", "DriveToPosition", driveParams);
    std::cout << "Defined Task: DriveForward with ID " << driveTaskId << std::endl;

    std::map<std::string, std::string> liftParams = {{"height", "0.5"}, {"payload", "pallet"}};
    std::string liftTaskId = opInterface.defineNewTask("LiftPallet", "LiftPallet", liftParams);
    std::cout << "Defined Task: LiftPallet with ID " << liftTaskId << std::endl;

    // Get all available tasks
    std::cout << "\nAvailable Tasks:" << std::endl;
    for (const auto& taskDto : opInterface.getAvailableTasks()) {
        std::cout << "- " << taskDto.name << " (ID: " << taskDto.id << ", Type: " << taskDto.type << ")" << std::endl;
    }

    // Get details of a specific task
    auto taskDetails = opInterface.getTaskDetails(driveTaskId);
    if (taskDetails) {
        std::cout << "\nDetails for " << taskDetails->name << ":" << std::endl;
        std::cout << "  Status: " << taskDetails->status << std::endl;
        std::cout << "  Parameters: ";
        for (const auto& param : taskDetails->parameters) {
            std::cout << param.first << ": " << param.second << ", ";
        }
        std::cout << std::endl;
    }

    // Start task execution
    std::map<std::string, std::string> runtimeDriveParams = {{"speed", "1.5"}};
    std::string driveExecutionId = opInterface.startTaskExecution(driveTaskId, runtimeDriveParams);
    std::cout << "\nStarted execution for DriveForward. Execution ID: " << driveExecutionId << std::endl;

    // Monitor task status
    auto driveStatus = opInterface.monitorTaskStatus(driveExecutionId);
    if (driveStatus) {
        std::cout << "  DriveForward Status: " << driveStatus->status << ", Progress: " << driveStatus->progress << std::endl;
    }

    // Simulate task completion (manual update for now)
    // In a real system, an external executor would update this.
    taskManager->updateTaskStatus(driveTaskId, mxrc::core::task::TaskStatus::COMPLETED);
    driveStatus = opInterface.monitorTaskStatus(driveExecutionId);
    if (driveStatus) {
        std::cout << "  DriveForward Status (after completion): " << driveStatus->status << std::endl;
    }

    return 0;
}
