#include "ActionRegistry.h"
#include "core/action/util/Logger.h"

namespace mxrc::core::action {

void ActionRegistry::registerDefinition(const ActionDefinition& definition) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto logger = Logger::get();

    if (definitions_.find(definition.id) != definitions_.end()) {
        logger->warn("Action definition '{}' is already registered. Overwriting.", definition.id);
    }

    definitions_[definition.id] = definition;
    logger->info("Registered Action definition: {} (type: {})", definition.id, definition.type);
}

std::shared_ptr<const ActionDefinition> ActionRegistry::getDefinition(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = definitions_.find(id);
    if (it != definitions_.end()) {
        return std::make_shared<ActionDefinition>(it->second);
    }

    return nullptr;
}

void ActionRegistry::registerType(const std::string& type, const std::string& description) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto logger = Logger::get();

    if (typeDescriptions_.find(type) != typeDescriptions_.end()) {
        logger->warn("Action type '{}' is already registered. Overwriting.", type);
    }

    typeDescriptions_[type] = description;
    logger->info("Registered Action type: {} - {}", type, description);
}

bool ActionRegistry::hasType(const std::string& type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return typeDescriptions_.find(type) != typeDescriptions_.end();
}

bool ActionRegistry::hasDefinition(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return definitions_.find(id) != definitions_.end();
}

std::vector<std::string> ActionRegistry::getAllDefinitionIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ids;
    ids.reserve(definitions_.size());

    for (const auto& pair : definitions_) {
        ids.push_back(pair.first);
    }

    return ids;
}

std::vector<std::string> ActionRegistry::getAllTypes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> types;
    types.reserve(typeDescriptions_.size());

    for (const auto& pair : typeDescriptions_) {
        types.push_back(pair.first);
    }

    return types;
}

} // namespace mxrc::core::action
