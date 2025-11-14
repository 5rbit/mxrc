#include "ActionFactory.h"
#include "core/action/util/Logger.h"
#include <stdexcept>

namespace mxrc::core::action {

void ActionFactory::registerFactory(const std::string& type, FactoryFunction factoryFunc) {
    auto logger = Logger::get();

    if (factories_.find(type) != factories_.end()) {
        logger->warn("Action type '{}' is already registered. Overwriting.", type);
    }

    factories_[type] = factoryFunc;
    logger->info("Registered Action type: {}", type);
}

std::shared_ptr<IAction> ActionFactory::createAction(
    const std::string& type,
    const std::map<std::string, std::string>& parameters) {

    auto logger = Logger::get();

    auto it = factories_.find(type);
    if (it == factories_.end()) {
        logger->error("Unknown Action type: {}", type);
        throw std::runtime_error("Unknown Action type: " + type);
    }

    // ID 추출 (파라미터에 포함되어 있어야 함)
    auto idIt = parameters.find("id");
    if (idIt == parameters.end()) {
        logger->error("Action parameters must include 'id'");
        throw std::runtime_error("Action parameters must include 'id'");
    }

    std::string id = idIt->second;

    logger->debug("Creating Action: {} (type: {})", id, type);
    return it->second(id, parameters);
}

bool ActionFactory::hasType(const std::string& type) const {
    return factories_.find(type) != factories_.end();
}

std::vector<std::string> ActionFactory::getRegisteredTypes() const {
    std::vector<std::string> types;
    types.reserve(factories_.size());

    for (const auto& pair : factories_) {
        types.push_back(pair.first);
    }

    return types;
}

} // namespace mxrc::core::action
