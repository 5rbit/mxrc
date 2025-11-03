#ifndef ABSTRACT_TASK_H
#define ABSTRACT_TASK_H

#include <string>
#include <memory> // For std::unique_ptr

namespace mxrc {
namespace task_mission {

// Forward declaration
class TaskContext;

/**
 * @brief The base interface for all executable tasks in the system.
 *
 * All concrete tasks must inherit from this interface and implement its pure virtual methods.
 * This ensures a common contract for task execution and management.
 */
class AbstractTask {
public:
    /**
     * @brief Virtual destructor to ensure proper cleanup of derived task classes.
     */
    virtual ~AbstractTask() = default;

    /**
     * @brief Initializes the task.
     *
     * This method is called once before the task's execution phase. It can be used
     * to set up resources, validate initial context, or perform any pre-execution setup.
     *
     * @param context The TaskContext containing input parameters for initialization.
     * @return True if initialization is successful, false otherwise.
     */
    virtual bool initialize(TaskContext& context) = 0;

    /**
     * @brief Executes the main logic of the task.
     *
     * This method contains the core functionality of the task. It should perform
     * its designated operation and update the TaskContext with any outputs or results.
     *
     * @param context The TaskContext containing input parameters and to store output results.
     * @return True if the task execution is successful, false otherwise.
     */
    virtual bool execute(TaskContext& context) = 0;

    /**
     * @brief Terminates the task and cleans up resources.
     *
     * This method is called after the task's execution phase, regardless of success or failure.
     * It should be used to release any resources acquired during initialization or execution.
     *
     * @param context The TaskContext containing any final state or results.
     */
    virtual void terminate(TaskContext& context) = 0;

    /**
     * @brief Returns a unique identifier for the task type.
     *
     * @return A string representing the task's ID.
     */
    virtual std::string getTaskId() const = 0;
};

} // namespace task_mission
} // namespace mxrc

#endif // ABSTRACT_TASK_H
