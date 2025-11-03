#ifndef MXRC_LIFT_PALLET_H
#define MXRC_LIFT_PALLET_H

#include "core/task/task_mission_management/AbstractTask.h"

namespace mxrc {
namespace task_mission {

class LiftPallet : public AbstractTask {
public:
    bool initialize(TaskContext& context) override;
    bool execute(TaskContext& context) override;
    void terminate(TaskContext& context) override;
    std::string getTaskId() const override;
};

} // namespace task_mission
} // namespace mxrc

#endif // MXRC_LIFT_PALLET_H
