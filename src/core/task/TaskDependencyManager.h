#ifndef TASK_DEPENDENCY_MANAGER_H
#define TASK_DEPENDENCY_MANAGER_H

#include "AbstractTask.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <condition_variable>

namespace mxrc {
namespace task {

class TaskDependencyManager {
public:
    TaskDependencyManager();
    ~TaskDependencyManager();

    /**
     * @brief Registers a task with its dependencies.
     * @param taskId The ID of the task to register.
     * @param dependencies A list of task IDs that this task depends on.
     */
    void registerTaskDependencies(const std::string& taskId, const std::vector<std::string>& dependencies);

    /**
     * @brief Updates the state of a task.
     * @param taskId The ID of the task whose state is being updated.
     * @param newState The new state of the task.
     */
    void updateTaskState(const std::string& taskId, mxrc::task::TaskState newState);

    /**
     * @brief Checks if a task's dependencies are met.
     * @param taskId The ID of the task to check.
     * @return True if all dependencies are met (completed or cancelled), false otherwise.
     */
    bool areDependenciesMet(const std::string& taskId) const;

    /**
     * @brief Waits until a task's dependencies are met or a timeout occurs.
     * @param taskId The ID of the task to wait for.
     * @param timeout_ms The maximum time to wait in milliseconds.
     * @return True if dependencies are met within the timeout, false otherwise.
     */
    bool waitForDependencies(const std::string& taskId, long long timeout_ms);

    /**
     * @brief Clears all registered tasks and their dependencies.
     */
    void clearAllDependencies();

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::map<std::string, mxrc::task::TaskState> task_states_;
    std::map<std::string, std::set<std::string>> task_dependencies_;
};

#endif // TASK_DEPENDENCY_MANAGER_H
