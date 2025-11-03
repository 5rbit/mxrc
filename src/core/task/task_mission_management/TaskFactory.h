#ifndef MXRC_TASK_FACTORY_H
#define MXRC_TASK_FACTORY_H

#include "AbstractTask.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace mxrc {
namespace task_mission {

class TaskFactory {
public:
    using TaskCreator = std::function<std::unique_ptr<AbstractTask>()>;

    static TaskFactory& getInstance() {
        static TaskFactory instance;
        return instance;
    }

    bool registerTask(const std::string& taskId, TaskCreator creator);
    std::unique_ptr<AbstractTask> createTask(const std::string& taskId);

private:
    TaskFactory() = default;
    std::unordered_map<std::string, TaskCreator> creators;
};

} // namespace task_mission
} // namespace mxrc

#endif // MXRC_TASK_FACTORY_H
