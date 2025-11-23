// HotKeyConfig.h - Hot Key configuration loader
// Feature 019: Architecture Improvements - US2 Hot Key Optimization
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_CORE_DATASTORE_HOTKEY_HOTKEYCONFIG_H
#define MXRC_CORE_DATASTORE_HOTKEY_HOTKEYCONFIG_H

#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <yaml-cpp/yaml.h>

namespace mxrc::core::datastore {

/**
 * @brief Hot Key metadata from IPC schema
 */
struct HotKeyInfo {
    std::string key_name;        ///< DataStore key name
    std::string type;            ///< C++ type (e.g., "Vector3d", "array<double, 64>")
    std::string description;     ///< Human-readable description
    size_t estimated_size_bytes; ///< Estimated memory size

    HotKeyInfo() = default;
    HotKeyInfo(const std::string& name, const std::string& t, const std::string& desc, size_t size)
        : key_name(name), type(t), description(desc), estimated_size_bytes(size) {}
};

/**
 * @brief Hot Key configuration loader
 *
 * Loads Hot Key configuration from IPC schema YAML file.
 * Validates Hot Key constraints:
 * - Maximum 32 Hot Keys (FR-006)
 * - Maximum 512 bytes per value (64-axis motor data)
 * - Total memory < 10MB (FR-006)
 *
 * Usage:
 * ```cpp
 * HotKeyConfig config;
 * if (config.loadFromSchema("config/ipc/ipc-schema.yaml")) {
 *     for (const auto& info : config.getHotKeys()) {
 *         hotKeyCache.registerHotKey(info.key_name);
 *     }
 * }
 * ```
 */
class HotKeyConfig {
public:
    HotKeyConfig() = default;
    ~HotKeyConfig() = default;

    /**
     * @brief Load Hot Key configuration from IPC schema YAML
     *
     * Parses ipc-schema.yaml and extracts keys with hot_key=true.
     * Validates constraints and estimates memory usage.
     *
     * @param schema_path Path to ipc-schema.yaml
     * @return true if loading succeeded, false on error
     */
    bool loadFromSchema(const std::filesystem::path& schema_path);

    /**
     * @brief Get list of Hot Keys
     *
     * @return Vector of HotKeyInfo
     */
    const std::vector<HotKeyInfo>& getHotKeys() const { return hot_keys_; }

    /**
     * @brief Get number of Hot Keys
     *
     * @return Hot Key count
     */
    size_t getHotKeyCount() const { return hot_keys_.size(); }

    /**
     * @brief Check if key is configured as Hot Key
     *
     * @param key_name DataStore key name
     * @return true if key is Hot Key
     */
    bool isHotKey(const std::string& key_name) const;

    /**
     * @brief Get total estimated memory usage
     *
     * @return Memory usage in bytes
     */
    size_t getTotalMemoryUsage() const { return total_memory_usage_; }

    /**
     * @brief Get validation errors
     *
     * @return Vector of error messages (empty if no errors)
     */
    const std::vector<std::string>& getErrors() const { return errors_; }

    /**
     * @brief Get validation warnings
     *
     * @return Vector of warning messages
     */
    const std::vector<std::string>& getWarnings() const { return warnings_; }

    /**
     * @brief Validate Hot Key constraints
     *
     * Checks:
     * - Hot Key count ≤ 32 (MAX_HOT_KEYS)
     * - Each value size ≤ 512 bytes (MAX_HOT_KEY_SIZE_BYTES)
     * - Total memory < 10MB
     *
     * @return true if all constraints satisfied
     */
    bool validateConstraints();

private:
    /// List of Hot Keys
    std::vector<HotKeyInfo> hot_keys_;

    /// Total memory usage estimate (bytes)
    size_t total_memory_usage_{0};

    /// Validation errors
    std::vector<std::string> errors_;

    /// Validation warnings
    std::vector<std::string> warnings_;

    /// Maximum Hot Key count (from research.md)
    static constexpr size_t MAX_HOT_KEYS = 32;

    /// Maximum value size per Hot Key (64-axis motor data)
    static constexpr size_t MAX_HOT_KEY_SIZE_BYTES = 512;

    /// Maximum total memory usage (10MB target)
    static constexpr size_t MAX_TOTAL_MEMORY_BYTES = 10 * 1024 * 1024;

    /**
     * @brief Estimate type size in bytes
     *
     * @param type_str Type string (e.g., "double", "array<double, 64>")
     * @return Estimated size in bytes
     */
    size_t estimateTypeSize(const std::string& type_str) const;
};

}  // namespace mxrc::core::datastore

#endif  // MXRC_CORE_DATASTORE_HOTKEY_HOTKEYCONFIG_H
