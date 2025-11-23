#pragma once

#include <string>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>

namespace mxrc {
namespace config {

/**
 * @brief JSON configuration file loader
 *
 * Production readiness: Centralized configuration management.
 * Loads JSON files and provides type-safe access to configuration data.
 */
class ConfigLoader {
public:
    /**
     * @brief Load configuration from JSON file
     *
     * @param file_path Path to JSON configuration file
     * @return true if successfully loaded
     * @return false if file not found or invalid JSON
     */
    bool loadFromFile(const std::filesystem::path& file_path);

    /**
     * @brief Load configuration from JSON string
     *
     * @param json_str JSON string
     * @return true if successfully parsed
     * @return false if invalid JSON
     */
    bool loadFromString(const std::string& json_str);

    /**
     * @brief Get the raw JSON object
     *
     * @return const nlohmann::json& Reference to internal JSON object
     */
    const nlohmann::json& getJson() const { return config_; }

    /**
     * @brief Get value by key path (supports nested keys with '.')
     *
     * Example: getValue<int>("performance.cpu_affinity.priority")
     *
     * @tparam T Value type
     * @param key_path Key path (use '.' for nested keys)
     * @param default_value Default value if key not found
     * @return T Value or default
     */
    template<typename T>
    T getValue(const std::string& key_path, const T& default_value = T{}) const;

    /**
     * @brief Check if key exists
     *
     * @param key_path Key path (use '.' for nested keys)
     * @return true if key exists
     */
    bool hasKey(const std::string& key_path) const;

    /**
     * @brief Get configuration section by key path
     *
     * @param key_path Key path (use '.' for nested keys)
     * @return nlohmann::json Configuration section
     * @throws std::runtime_error if key not found
     */
    nlohmann::json getSection(const std::string& key_path) const;

    /**
     * @brief Check if configuration is loaded
     *
     * @return true if configuration data exists
     */
    bool isLoaded() const { return !config_.empty(); }

    /**
     * @brief Clear configuration
     */
    void clear() { config_.clear(); }

private:
    nlohmann::json config_;

    /**
     * @brief Navigate to nested key in JSON object
     *
     * @param key_path Key path (use '.' for nested keys)
     * @return const nlohmann::json* Pointer to value, or nullptr if not found
     */
    const nlohmann::json* navigateToKey(const std::string& key_path) const;
};

// Template implementation
template<typename T>
T ConfigLoader::getValue(const std::string& key_path, const T& default_value) const {
    const nlohmann::json* value = navigateToKey(key_path);
    if (value == nullptr) {
        return default_value;
    }

    try {
        return value->get<T>();
    } catch (const nlohmann::json::exception&) {
        return default_value;
    }
}

} // namespace config
} // namespace mxrc
