#include "TaskManagerInit.h"
#include "TaskDefinitionRegistry.h" // Use TaskDefinitionRegistry
#include "tasks/DummyTask.h"
#include "tasks/DriveForwardTask.h"
#include "tasks/LiftPalletTask.h"
#include "tasks/InspectAreaTask.h"
#include "tasks/FailureTypeTask.h"
#include <iostream>
#include <memory> // For std::make_shared

namespace mxrc::core::taskmanager {

void initializeTaskManagerModule() {
    // TODO: Implement task registration with TaskDefinitionRegistry
    // This requires refactoring task classes to implement all ITask methods
    std::cout << "TaskManager module initialization (placeholder)" << std::endl;
    /* Old code using getInstance():
    auto& registry = TaskDefinitionRegistry::getInstance();

    registry.registerDefinition("DummyTask", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<mxrc::core::taskmanager::tasks::DummyTask>(id, type, params);
    });
    registry.registerDefinition("DriveToPosition", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<mxrc::core::taskmanager::tasks::DriveForwardTask>(id, type, params);
    });
    registry.registerDefinition("LiftPallet", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<mxrc::core::taskmanager::tasks::LiftPalletTask>(id, type, params);
    });
    registry.registerDefinition("Inspection", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<mxrc::core::taskmanager::tasks::InspectAreaTask>(id, type, params);
    });
    registry.registerDefinition("FailureType", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<mxrc::core::taskmanager::tasks::FailureTypeTask>(id, type, params);
    });
    std::cout << "TaskDefinitionRegistry에 모든 Task 유형 등록 완료 (TaskManagerInit)." << std::endl;
    */
}

} // namespace mxrc::core::taskmanager
