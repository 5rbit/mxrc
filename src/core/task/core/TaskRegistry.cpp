#include "core/task/core/TaskRegistry.h"

namespace mxrc::core::task {

using namespace mxrc::core::action;

void TaskRegistry::registerDefinition(const TaskDefinition& definition) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (definitions_.find(definition.id) != definitions_.end()) {
        Logger::get()->warn(
            "Task definition '{}' is already registered. Overwriting.",
            definition.id
        );
    }

    definitions_[definition.id] = std::make_shared<TaskDefinition>(definition);
    
    Logger::get()->info(
        "Registered Task definition: {} (name: {}, mode: {})",
        definition.id, definition.name,
        taskExecutionModeToString(definition.executionMode)
    );
}

std::shared_ptr<TaskDefinition> TaskRegistry::getDefinition(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = definitions_.find(id);
    if (it != definitions_.end()) {
        return it->second;
    }
    
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
        definitions_.erase(it);
        Logger::get()->info("Removed Task definition: {}", id);
        return true;
    }
    
    return false;
}

void TaskRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    definitions_.clear();
    Logger::get()->info("Cleared all Task definitions");
}

} // namespace mxrc::core::task
