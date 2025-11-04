#ifndef MXRC_DRIVE_TO_POSITION_H
#define MXRC_DRIVE_TO_POSITION_H

#include "core/task/AbstractTask.h"

namespace mxrc {
namespace task {

class DriveToPosition : public AbstractTask {
public:
    bool initialize(TaskContext& context) override;
    bool execute(TaskContext& context) override;
    void terminate(TaskContext& context) override;
    std::string getTaskId() const override;
};

} // namespace task_mission
} // namespace mxrc

#endif // MXRC_DRIVE_TO_POSITION_H
