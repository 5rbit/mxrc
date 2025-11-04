#include "TaskFactory.h"

namespace mxrc {
namespace task {

bool TaskFactory::registerTask(const std::string& taskId, TaskCreator creator) {
    if (creators.count(taskId)) {
        // Task with this ID already registered
        return false;
    }
    creators[taskId] = std::move(creator);
    return true;
}

std::unique_ptr<AbstractTask> TaskFactory::createTask(const std::string& taskId) {
    auto it = creators.find(taskId);
    if (it != creators.end()) {
        return it->second();
    }
    // Task not found
    return nullptr;
}

} // namespace task
} // namespace mxrc