#include "factory/TaskFactory.h" // Updated include
#include "../interfaces/ITask.h" // Explicitly include ITask

TaskFactory& TaskFactory::getInstance() {
    static TaskFactory instance;
    return instance;
}

void TaskFactory::registerTaskType(const std::string& type, TaskCreator creator) {
    creators_[type] = std::move(creator);
}

std::unique_ptr<ITask> TaskFactory::createTask(const std::string& type, const std::map<std::string, std::string>& parameters) {
    auto it = creators_.find(type);
    if (it != creators_.end()) {
        return it->second(parameters);
    }
    throw std::runtime_error("Unknown task type: " + type);
}
