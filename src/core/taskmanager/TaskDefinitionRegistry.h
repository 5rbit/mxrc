#pragma once

#include <functional>
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include "TaskDto.h"

namespace mxrc::core::taskmanager {

class ITask;

class TaskDefinitionRegistry {
public:
    using TaskFactoryFunc = std::function<std::shared_ptr<ITask>(
        const std::string& id,
        const std::string& type,
        const std::map<std::string, std::string>& params
    )>;

    struct TaskDefinition {
        std::string typeName;
        TaskFactoryFunc factory;
        std::map<std::string, std::string> defaultParams;
    };

    void registerDefinition(const std::string& taskTypeName,
                           TaskFactoryFunc factory,
                           const std::map<std::string, std::string>& defaultParams = {});

    std::shared_ptr<ITask> createTask(
        const std::string& taskTypeName,
        const std::string& id,
        const std::string& type,
        const std::map<std::string, std::string>& params);

    std::vector<TaskDto> getAllDefinitions() const;
    const TaskDefinition* getDefinition(const std::string& typeName) const;

private:
    std::unordered_map<std::string, TaskDefinition> definitions_;
};

} // namespace mxrc::core::taskmanager
