#include "core/sequence/core/SequenceRegistry.h"

namespace mxrc::core::sequence {

void SequenceRegistry::registerDefinition(const SequenceDefinition& definition) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (definitions_.find(definition.id) != definitions_.end()) {
        mxrc::core::action::Logger::get()->warn(
            "Sequence definition '{}' is already registered. Overwriting.",
            definition.id
        );
    }

    definitions_[definition.id] = std::make_shared<SequenceDefinition>(definition);
    
    mxrc::core::action::Logger::get()->info(
        "Registered Sequence definition: {} (name: {})",
        definition.id, definition.name
    );
}

std::shared_ptr<SequenceDefinition> SequenceRegistry::getDefinition(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = definitions_.find(id);
    if (it != definitions_.end()) {
        return it->second;
    }
    
    return nullptr;
}

bool SequenceRegistry::hasDefinition(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return definitions_.find(id) != definitions_.end();
}

std::vector<std::string> SequenceRegistry::getAllDefinitionIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> ids;
    ids.reserve(definitions_.size());
    
    for (const auto& [id, _] : definitions_) {
        ids.push_back(id);
    }
    
    return ids;
}

bool SequenceRegistry::removeDefinition(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = definitions_.find(id);
    if (it != definitions_.end()) {
        definitions_.erase(it);
        mxrc::core::action::Logger::get()->info(
            "Removed Sequence definition: {}", id
        );
        return true;
    }
    
    return false;
}

void SequenceRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    definitions_.clear();
    mxrc::core::action::Logger::get()->info("Cleared all Sequence definitions");
}

} // namespace mxrc::core::sequence
