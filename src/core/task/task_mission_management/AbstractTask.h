#ifndef MXRC_ABSTRACT_TASK_H
#define MXRC_ABSTRACT_TASK_H

#include "TaskContext.h"
#include <string>
#include <functional> // For std::function

namespace mxrc {
namespace task_mission {

enum class TaskState {
    PENDING,
    RUNNING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED
};

enum class FailureStrategy {
    ABORT_MISSION,
    RETRY_TRANSIENT,
    SKIP_TASK,
    CUSTOM_HANDLER
};

class AbstractTask {
public:
    virtual ~AbstractTask() = default;

    virtual bool initialize(TaskContext& context) = 0;
    virtual bool execute(TaskContext& context) = 0;
    virtual void terminate(TaskContext& context) = 0;
    virtual std::string getTaskId() const = 0;
    virtual FailureStrategy getFailureStrategy() const { return FailureStrategy::ABORT_MISSION; } // Default strategy

    TaskState getState() const { return currentState; }
    void setState(TaskState state) { currentState = state; }

protected:
    TaskState currentState = TaskState::PENDING;
};

} // namespace task_mission
} // namespace mxrc

#endif // MXRC_ABSTRACT_TASK_H
