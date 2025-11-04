#include "DriveToPosition.h"
#include "core/task/TaskFactory.h"
#include <iostream>

namespace mxrc {
namespace task {

bool DriveToPosition::initialize(TaskContext& context) {
    std::cout << "DriveToPosition initialized." << std::endl;
    return true;
}

bool DriveToPosition::execute(TaskContext& context) {
    std::cout << "DriveToPosition executing." << std::endl;
    return true;
}

void DriveToPosition::terminate(TaskContext& context) {
    std::cout << "DriveToPosition terminated." << std::endl;
}

std::string DriveToPosition::getTaskId() const {
    return "DriveToPosition";
}

namespace {
    bool registered = TaskFactory::getInstance().registerTask("DriveToPosition", []() {
        return std::make_unique<DriveToPosition>();
    });
}

} // namespace task
} // namespace mxrc
