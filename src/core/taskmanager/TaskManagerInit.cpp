#include "TaskManagerInit.h" // Updated include
#include "factory/TaskFactory.h" // Updated include
#include "tasks/DummyTask.h" // Updated include
#include "tasks/DriveForwardTask.h" // Updated include
#include "tasks/LiftPalletTask.h" // Updated include
#include "tasks/InspectAreaTask.h" // Updated include
#include "tasks/FailureTypeTask.h" // Updated include
#include <iostream>

void initializeTaskManagerModule() { // Renamed function
    TaskFactory::getInstance().registerTaskType("DummyTask", [](const std::map<std::string, std::string>& params) {
        return std::make_unique<DummyTask>(params);
    });
    TaskFactory::getInstance().registerTaskType("DriveToPosition", [](const std::map<std::string, std::string>& params) {
        return std::make_unique<DriveForwardTask>(params);
    });
    TaskFactory::getInstance().registerTaskType("LiftPallet", [](const std::map<std::string, std::string>& params) {
        return std::make_unique<LiftPalletTask>(params);
    });
    TaskFactory::getInstance().registerTaskType("Inspection", [](const std::map<std::string, std::string>& params) {
        return std::make_unique<InspectAreaTask>(params);
    });
    TaskFactory::getInstance().registerTaskType("FailureType", [](const std::map<std::string, std::string>& params) {
        return std::make_unique<FailureTypeTask>(params);
    });
    std::cout << "TaskFactory에 모든 Task 유형 등록 완료 (TaskManagerInit)." << std::endl; // Updated message
}
