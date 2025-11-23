#include "ConfigLoader.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

namespace mxrc {
namespace config {

bool ConfigLoader::loadFromFile(const std::filesystem::path& file_path) {
    if (!std::filesystem::exists(file_path)) {
        spdlog::error("Configuration file not found: {}", file_path.string());
        return false;
    }

    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            spdlog::error("Failed to open configuration file: {}", file_path.string());
            return false;
        }

        file >> config_;
        spdlog::info("Configuration loaded from: {}", file_path.string());
        return true;
    } catch (const nlohmann::json::exception& e) {
        spdlog::error("Failed to parse JSON configuration from {}: {}",
                      file_path.string(), e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Failed to load configuration from {}: {}",
                      file_path.string(), e.what());
        return false;
    }
}

bool ConfigLoader::loadFromString(const std::string& json_str) {
    try {
        config_ = nlohmann::json::parse(json_str);
        spdlog::info("Configuration loaded from string");
        return true;
    } catch (const nlohmann::json::exception& e) {
        spdlog::error("Failed to parse JSON string: {}", e.what());
        return false;
    }
}

bool ConfigLoader::hasKey(const std::string& key_path) const {
    return navigateToKey(key_path) != nullptr;
}

nlohmann::json ConfigLoader::getSection(const std::string& key_path) const {
    const nlohmann::json* value = navigateToKey(key_path);
    if (value == nullptr) {
        throw std::runtime_error("Configuration key not found: " + key_path);
    }
    return *value;
}

const nlohmann::json* ConfigLoader::navigateToKey(const std::string& key_path) const {
    if (config_.empty()) {
        return nullptr;
    }

    // Split key_path by '.'
    std::vector<std::string> keys;
    std::stringstream ss(key_path);
    std::string key;
    while (std::getline(ss, key, '.')) {
        keys.push_back(key);
    }

    // Navigate through nested JSON
    const nlohmann::json* current = &config_;
    for (const auto& k : keys) {
        if (!current->is_object() || !current->contains(k)) {
            return nullptr;
        }
        current = &(*current)[k];
    }

    return current;
}

} // namespace config
} // namespace mxrc
