// HotKeyConfig.cpp - Hot Key configuration loader implementation
// Feature 019: Architecture Improvements - US2 Hot Key Optimization
// Copyright (C) 2025 MXRC Project

#include "HotKeyConfig.h"
#include <spdlog/spdlog.h>
#include <regex>

namespace mxrc::core::datastore {

bool HotKeyConfig::loadFromSchema(const std::filesystem::path& schema_path) {
    errors_.clear();
    warnings_.clear();
    hot_keys_.clear();
    total_memory_usage_ = 0;

    if (!std::filesystem::exists(schema_path)) {
        errors_.push_back("Schema file not found: " + schema_path.string());
        spdlog::error("[HotKeyConfig] {}", errors_.back());
        return false;
    }

    try {
        YAML::Node schema = YAML::LoadFile(schema_path.string());

        if (!schema["datastore_keys"]) {
            errors_.push_back("Missing 'datastore_keys' section in schema");
            spdlog::error("[HotKeyConfig] {}", errors_.back());
            return false;
        }

        // Parse datastore_keys section
        YAML::Node keys = schema["datastore_keys"];

        for (const auto& kv : keys) {
            std::string key_name = kv.first.as<std::string>();
            YAML::Node key_spec = kv.second;

            // Check if this is a Hot Key
            if (key_spec["hot_key"] && key_spec["hot_key"].as<bool>()) {
                std::string type = key_spec["type"].as<std::string>();
                std::string description = key_spec["description"]
                    ? key_spec["description"].as<std::string>()
                    : "";

                size_t size = estimateTypeSize(type);

                HotKeyInfo info(key_name, type, description, size);
                hot_keys_.push_back(info);
                total_memory_usage_ += size;

                spdlog::debug("[HotKeyConfig] Hot Key: '{}' (type: {}, size: {} bytes)",
                             key_name, type, size);
            }
        }

        spdlog::info("[HotKeyConfig] Loaded {} Hot Keys from '{}'",
                    hot_keys_.size(), schema_path.filename().string());

        return validateConstraints();

    } catch (const YAML::Exception& e) {
        errors_.push_back("YAML parsing error: " + std::string(e.what()));
        spdlog::error("[HotKeyConfig] {}", errors_.back());
        return false;
    } catch (const std::exception& e) {
        errors_.push_back("Error loading schema: " + std::string(e.what()));
        spdlog::error("[HotKeyConfig] {}", errors_.back());
        return false;
    }
}

bool HotKeyConfig::isHotKey(const std::string& key_name) const {
    for (const auto& info : hot_keys_) {
        if (info.key_name == key_name) {
            return true;
        }
    }
    return false;
}

bool HotKeyConfig::validateConstraints() {
    bool valid = true;

    // Check Hot Key count
    if (hot_keys_.size() > MAX_HOT_KEYS) {
        errors_.push_back("Hot Key count exceeds limit: " +
                         std::to_string(hot_keys_.size()) + " > " +
                         std::to_string(MAX_HOT_KEYS));
        spdlog::error("[HotKeyConfig] {}", errors_.back());
        valid = false;
    }

    // Check individual Hot Key sizes
    for (const auto& info : hot_keys_) {
        if (info.estimated_size_bytes > MAX_HOT_KEY_SIZE_BYTES) {
            errors_.push_back("Hot Key '" + info.key_name + "' size exceeds limit: " +
                             std::to_string(info.estimated_size_bytes) + " > " +
                             std::to_string(MAX_HOT_KEY_SIZE_BYTES) + " bytes");
            spdlog::error("[HotKeyConfig] {}", errors_.back());
            valid = false;
        }
    }

    // Check total memory usage
    if (total_memory_usage_ > MAX_TOTAL_MEMORY_BYTES) {
        warnings_.push_back("Total Hot Key memory usage: " +
                           std::to_string(total_memory_usage_) + " bytes (target: < " +
                           std::to_string(MAX_TOTAL_MEMORY_BYTES) + " bytes)");
        spdlog::warn("[HotKeyConfig] {}", warnings_.back());
    } else {
        spdlog::info("[HotKeyConfig] Total memory usage: {} bytes ({:.2f}% of 10MB target)",
                    total_memory_usage_,
                    (total_memory_usage_ * 100.0) / MAX_TOTAL_MEMORY_BYTES);
    }

    return valid;
}

size_t HotKeyConfig::estimateTypeSize(const std::string& type_str) const {
    // Basic type sizes
    static const std::map<std::string, size_t> type_sizes = {
        {"double", 8}, {"float", 4},
        {"int32_t", 4}, {"uint32_t", 4},
        {"int64_t", 8}, {"uint64_t", 8},
        {"bool", 1}, {"string", 256},
        {"Vector3d", 24}  // 3 * double
    };

    // Check basic types
    auto it = type_sizes.find(type_str);
    if (it != type_sizes.end()) {
        return it->second;
    }

    // Array type: array<T, N>
    std::regex array_pattern(R"(array<(\w+),\s*(\d+)>)");
    std::smatch match;
    if (std::regex_match(type_str, match, array_pattern)) {
        std::string element_type = match[1].str();
        size_t count = std::stoull(match[2].str());

        auto elem_it = type_sizes.find(element_type);
        size_t element_size = (elem_it != type_sizes.end()) ? elem_it->second : 8;

        return element_size * count;
    }

    // Default estimate
    spdlog::warn("[HotKeyConfig] Unknown type '{}', estimating 8 bytes", type_str);
    return 8;
}

}  // namespace mxrc::core::datastore
