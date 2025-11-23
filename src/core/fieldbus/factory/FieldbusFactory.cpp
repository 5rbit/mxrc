#include "FieldbusFactory.h"
#include "../drivers/MockDriver.h"
#include "../drivers/EtherCATDriver.h"
#include <spdlog/spdlog.h>

namespace mxrc::core::fieldbus {

std::map<std::string, FieldbusFactory::Creator>& FieldbusFactory::getRegistry() {
    static std::map<std::string, Creator> registry;
    static bool initialized = false;

    if (!initialized) {
        initializeBuiltInProtocols();
        initialized = true;
    }

    return registry;
}

void FieldbusFactory::initializeBuiltInProtocols() {
    // Register Mock driver (always available for testing)
    registerProtocol("Mock", [](const FieldbusConfig& config) -> IFieldbusPtr {
        return std::make_shared<MockDriver>(config);
    });

    // Register EtherCAT driver (Feature 019 US4 - T041)
    registerProtocol("EtherCAT", [](const FieldbusConfig& config) -> IFieldbusPtr {
        return std::make_shared<EtherCATDriver>(config);
    });

    spdlog::info("[FieldbusFactory] Initialized built-in protocols: Mock, EtherCAT");
}

IFieldbusPtr FieldbusFactory::create(const FieldbusConfig& config) {
    if (config.protocol.empty()) {
        spdlog::error("[FieldbusFactory] Protocol name is empty");
        return nullptr;
    }

    return createByName(config.protocol, config);
}

IFieldbusPtr FieldbusFactory::createByName(const std::string& protocol,
                                            const FieldbusConfig& config) {
    auto& registry = getRegistry();
    auto it = registry.find(protocol);

    if (it == registry.end()) {
        spdlog::error("[FieldbusFactory] Unsupported protocol: {}", protocol);
        spdlog::info("[FieldbusFactory] Supported protocols:");
        for (const auto& [name, _] : registry) {
            spdlog::info("  - {}", name);
        }
        return nullptr;
    }

    try {
        auto instance = it->second(config);
        if (instance) {
            spdlog::info("[FieldbusFactory] Created {} fieldbus instance", protocol);
        } else {
            spdlog::error("[FieldbusFactory] Failed to create {} instance", protocol);
        }
        return instance;
    } catch (const std::exception& e) {
        spdlog::error("[FieldbusFactory] Exception creating {}: {}", protocol, e.what());
        return nullptr;
    }
}

bool FieldbusFactory::registerProtocol(const std::string& protocol, Creator creator) {
    if (protocol.empty()) {
        spdlog::warn("[FieldbusFactory] Cannot register protocol with empty name");
        return false;
    }

    if (!creator) {
        spdlog::warn("[FieldbusFactory] Cannot register null creator for protocol: {}", protocol);
        return false;
    }

    auto& registry = getRegistry();
    if (registry.find(protocol) != registry.end()) {
        spdlog::warn("[FieldbusFactory] Protocol already registered: {}", protocol);
        return false;
    }

    registry[protocol] = creator;
    spdlog::debug("[FieldbusFactory] Registered protocol: {}", protocol);
    return true;
}

bool FieldbusFactory::isProtocolSupported(const std::string& protocol) {
    auto& registry = getRegistry();
    return registry.find(protocol) != registry.end();
}

std::vector<std::string> FieldbusFactory::getSupportedProtocols() {
    auto& registry = getRegistry();
    std::vector<std::string> protocols;
    protocols.reserve(registry.size());

    for (const auto& [name, _] : registry) {
        protocols.push_back(name);
    }

    return protocols;
}

bool FieldbusFactory::unregisterProtocol(const std::string& protocol) {
    auto& registry = getRegistry();
    auto it = registry.find(protocol);

    if (it == registry.end()) {
        return false;
    }

    registry.erase(it);
    spdlog::debug("[FieldbusFactory] Unregistered protocol: {}", protocol);
    return true;
}

void FieldbusFactory::clearProtocols() {
    auto& registry = getRegistry();
    registry.clear();
    spdlog::debug("[FieldbusFactory] Cleared all registered protocols");
}

} // namespace mxrc::core::fieldbus
