#include "DriveToPositionTask.h"
#include "TaskFactory.h"

namespace mxrc {
namespace task_mission {

// Static registration of DriveToPositionTask with the TaskFactory
// This ensures the task is registered when the program starts
struct DriveToPositionTaskRegistrar {
    DriveToPositionTaskRegistrar() {
        TaskFactory::getInstance().registerTask("DriveToPosition", []() {
            return std::make_unique<DriveToPositionTask>("DriveToPosition");
        });
    }
};

static DriveToPositionTaskRegistrar registrar;

} // namespace task_mission
} // namespace mxrc
