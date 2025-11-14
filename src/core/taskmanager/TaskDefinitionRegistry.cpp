#include "TaskDefinitionRegistry.h"
#include "core/taskmanager/interfaces/ITask.h"
#include "TaskDto.h"
#include <vector>

namespace mxrc::core::taskmanager {

void TaskDefinitionRegistry::registerDefinition(
    const std::string& taskTypeName,
    TaskFactoryFunc factory,
    const std::map<std::string, std::string>& defaultParams) {

    TaskDefinition def;
    def.typeName = taskTypeName;
    def.factory = factory;
    def.defaultParams = defaultParams;

    definitions_[taskTypeName] = def;
}

std::shared_ptr<ITask> TaskDefinitionRegistry::createTask(
    const std::string& taskTypeName,
    const std::string& id,
    const std::string& type,
    const std::map<std::string, std::string>& params) {

    auto it = definitions_.find(taskTypeName);
    if (it != definitions_.end()) {
        return it->second.factory(id, type, params);
    }
    return nullptr;
}

std::vector<TaskDto> TaskDefinitionRegistry::getAllDefinitions() const {
    std::vector<TaskDto> result;
    for (const auto& [typeName, def] : definitions_) {
        TaskDto dto;
        dto.type = typeName;
        dto.parameters = def.defaultParams;
        result.push_back(dto);
    }
    return result;
}

const TaskDefinitionRegistry::TaskDefinition* TaskDefinitionRegistry::getDefinition(
    const std::string& typeName) const {

    auto it = definitions_.find(typeName);
    if (it != definitions_.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace mxrc::core::taskmanager