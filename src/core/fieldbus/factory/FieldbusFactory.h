#pragma once

#include "../interfaces/IFieldbus.h"
#include <string>
#include <memory>
#include <map>
#include <functional>

namespace mxrc::core::fieldbus {

/**
 * @brief Factory for creating fieldbus instances
 *
 * Uses Factory Pattern to create protocol-specific fieldbus implementations.
 * Supports registration of custom protocols at runtime.
 *
 * Design Goals (Feature 019 US4):
 * - Extensibility: Easy to add new protocols
 * - Configuration-driven: Create from config file
 * - Type safety: Returns IFieldbus interface
 * - Error handling: Clear error messages
 *
 * Usage Example:
 * @code
 * // Create from config
 * FieldbusConfig config;
 * config.protocol = "EtherCAT";
 * config.config_file = "config/ethercat/robot.yaml";
 * config.cycle_time_us = 1000;
 *
 * auto fieldbus = FieldbusFactory::create(config);
 * if (!fieldbus) {
 *     spdlog::error("Failed to create fieldbus");
 * }
 *
 * // Or create by protocol name
 * auto ethercat = FieldbusFactory::createByName("EtherCAT", config);
 * @endcode
 */
class FieldbusFactory {
public:
    /**
     * @brief Factory function type for creating fieldbus instances
     *
     * @param config Fieldbus configuration
     * @return Shared pointer to fieldbus instance, or nullptr on failure
     */
    using Creator = std::function<IFieldbusPtr(const FieldbusConfig& config)>;

    /**
     * @brief Create fieldbus instance from configuration
     *
     * Reads protocol name from config and creates appropriate implementation.
     *
     * @param config Fieldbus configuration
     * @return Shared pointer to fieldbus, or nullptr if protocol not supported
     */
    static IFieldbusPtr create(const FieldbusConfig& config);

    /**
     * @brief Create fieldbus instance by protocol name
     *
     * Directly specify protocol name instead of reading from config.
     *
     * @param protocol Protocol name (e.g., "EtherCAT", "CANopen", "Mock")
     * @param config Configuration (protocol field will be overwritten)
     * @return Shared pointer to fieldbus, or nullptr if protocol not supported
     */
    static IFieldbusPtr createByName(const std::string& protocol,
                                      const FieldbusConfig& config);

    /**
     * @brief Register custom fieldbus protocol
     *
     * Allows users to add support for custom protocols at runtime.
     *
     * @param protocol Protocol name
     * @param creator Factory function for creating instances
     * @return true if registered successfully, false if protocol already exists
     */
    static bool registerProtocol(const std::string& protocol, Creator creator);

    /**
     * @brief Check if protocol is supported
     *
     * @param protocol Protocol name
     * @return true if protocol is registered
     */
    static bool isProtocolSupported(const std::string& protocol);

    /**
     * @brief Get list of supported protocols
     *
     * @return Vector of protocol names
     */
    static std::vector<std::string> getSupportedProtocols();

    /**
     * @brief Unregister protocol (for testing)
     *
     * @param protocol Protocol name
     * @return true if unregistered, false if not found
     */
    static bool unregisterProtocol(const std::string& protocol);

    /**
     * @brief Clear all registered protocols (for testing)
     */
    static void clearProtocols();

private:
    /**
     * @brief Get protocol registry (singleton)
     *
     * @return Reference to protocol registry
     */
    static std::map<std::string, Creator>& getRegistry();

    /**
     * @brief Initialize built-in protocols
     *
     * @param registry Protocol registry to initialize
     */
    static void initializeBuiltInProtocols(std::map<std::string, Creator>& registry);
};

} // namespace mxrc::core::fieldbus
