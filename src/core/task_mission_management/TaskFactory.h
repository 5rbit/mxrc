#ifndef TASK_FACTORY_H
#define TASK_FACTORY_H

#include "AbstractTask.h"
#include <functional> // For std::function
#include <map>
#include <string>
#include <memory> // For std::unique_ptr

namespace mxrc {
namespace task_mission {

/**
 * @brief A factory class for creating instances of AbstractTask based on a registered ID.
 *
 * This factory allows for dynamic creation of task objects without knowing their concrete types.
 * Tasks must be registered with the factory using a unique ID and a creation function.
 */
class TaskFactory {
public:
    /**
     * @brief Registers a task type with the factory.
     *
     * @param taskId A unique string identifier for the task.
     * @param creator A function that creates a std::unique_ptr to an instance of the task.
     * @return True if registration is successful, false if the taskId is already registered.
     */
    bool registerTask(const std::string& taskId, std::function<std::unique_ptr<AbstractTask>()> creator) {
        if (creators_.count(taskId)) {
            return false; // Task ID already registered
        }
        creators_[taskId] = std::move(creator);
        return true;
    }

    /**
     * @brief Creates an instance of a registered task.
     *
     * @param taskId The unique string identifier of the task to create.
     * @return A std::unique_ptr to the created AbstractTask instance, or nullptr if the taskId is not registered.
     */
    std::unique_ptr<AbstractTask> createTask(const std::string& taskId) const {
        auto it = creators_.find(taskId);
        if (it != creators_.end()) {
            return it->second(); // Call the creator function
        }
        return nullptr; // Task ID not found
    }

private:
    std::map<std::string, std::function<std::unique_ptr<AbstractTask>()>> creators_;
};

} // namespace task_mission
} // namespace mxrc

#endif // TASK_FACTORY_H
