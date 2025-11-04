#include "LiftPallet.h"
#include "core/task/TaskFactory.h"
#include <iostream>

namespace mxrc {
namespace task {

bool LiftPallet::initialize(TaskContext& context) {
    std::cout << "LiftPallet initialized." << std::endl;
    return true;
}

bool LiftPallet::execute(TaskContext& context) {
    std::cout << "LiftPallet executing." << std::endl;
    return true;
}

void LiftPallet::terminate(TaskContext& context) {
    std::cout << "LiftPallet terminated." << std::endl;
}

std::string LiftPallet::getTaskId() const {
    return "LiftPallet";
}

namespace {
    bool registered = TaskFactory::getInstance().registerTask("LiftPallet", []() {
        return std::make_unique<LiftPallet>();
    });
}

} // namespace task_mission
} // namespace mxrc
