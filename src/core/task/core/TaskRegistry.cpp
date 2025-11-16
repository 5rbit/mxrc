#include "core/task/core/TaskRegistry.h"

namespace mxrc::core::task {

using namespace mxrc::core::action;

void TaskRegistry::registerDefinition(const TaskDefinition& definition) {
    std::lock_guard<std::mutex> lock(mutex_);

    bool isOverwrite = definitions_.find(definition.id) != definitions_.end();

    if (isOverwrite) {
        Logger::get()->warn(
            "[TaskRegistry] OVERWRITE - Task definition '{}' already exists, replacing previous definition",
            definition.id
        );
    }

    definitions_[definition.id] = std::make_shared<TaskDefinition>(definition);

    Logger::get()->info(
        "[TaskRegistry] {} - Task: {} (name: '{}', mode: {}, workType: {}, work: '{}')",
        isOverwrite ? "UPDATE" : "REGISTER",
        definition.id, definition.name,
        taskExecutionModeToString(definition.executionMode),
        definition.workType == TaskWorkType::ACTION ? "ACTION" : "SEQUENCE",
        definition.workId
    );
    Logger::get()->debug("[TaskRegistry] Total registered tasks: {}", definitions_.size());
}

std::shared_ptr<TaskDefinition> TaskRegistry::getDefinition(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = definitions_.find(id);
    if (it != definitions_.end()) {
        Logger::get()->debug("[TaskRegistry] RETRIEVE - Found Task definition: {}", id);
        return it->second;
    }

    Logger::get()->warn("[TaskRegistry] RETRIEVE - Task definition not found: {}", id);
    return nullptr;
}

bool TaskRegistry::hasDefinition(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return definitions_.find(id) != definitions_.end();
}

std::vector<std::string> TaskRegistry::getAllDefinitionIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> ids;
    ids.reserve(definitions_.size());
    
    for (const auto& [id, _] : definitions_) {
        ids.push_back(id);
    }
    
    return ids;
}

bool TaskRegistry::removeDefinition(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = definitions_.find(id);
    if (it != definitions_.end()) {
        const auto& def = it->second;
        Logger::get()->info("[TaskRegistry] REMOVE - Task: {} (name: '{}', mode: {})",
                           id, def->name, taskExecutionModeToString(def->executionMode));
        definitions_.erase(it);
        Logger::get()->debug("[TaskRegistry] Remaining registered tasks: {}", definitions_.size());
        return true;
    }

    Logger::get()->warn("[TaskRegistry] REMOVE - Task definition not found: {}", id);
    return false;
}

void TaskRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = definitions_.size();
    definitions_.clear();
    Logger::get()->info("[TaskRegistry] CLEAR - Removed all {} Task definitions", count);
}

} // namespace mxrc::core::task
