#include "AlarmConfiguration.h"
#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>
#include <fstream>

namespace mxrc::core::alarm {

bool AlarmConfiguration::loadFromFile(const std::string& config_file) {
    try {
        spdlog::info("[AlarmConfiguration] Loading config from: {}", config_file);

        YAML::Node config = YAML::LoadFile(config_file);

        // 버전 확인
        if (!config["version"]) {
            spdlog::error("[AlarmConfiguration] Missing 'version' field");
            return false;
        }

        std::string version = config["version"].as<std::string>();
        spdlog::info("[AlarmConfiguration] Config version: {}", version);

        // alarms 배열 파싱
        if (!config["alarms"] || !config["alarms"].IsSequence()) {
            spdlog::error("[AlarmConfiguration] Missing or invalid 'alarms' array");
            return false;
        }

        configs_.clear();

        for (const auto& alarm_node : config["alarms"]) {
            AlarmConfig alarm_config = parseAlarmConfig(&alarm_node);

            // 중복 체크
            if (configs_.find(alarm_config.code) != configs_.end()) {
                spdlog::error("[AlarmConfiguration] Duplicate alarm code: {}", alarm_config.code);
                return false;
            }

            configs_[alarm_config.code] = alarm_config;
            spdlog::debug("[AlarmConfiguration] Loaded alarm: {} - {}",
                alarm_config.code, alarm_config.name);
        }

        config_file_path_ = config_file;

        spdlog::info("[AlarmConfiguration] Loaded {} alarms successfully", configs_.size());
        return true;

    } catch (const YAML::Exception& e) {
        spdlog::error("[AlarmConfiguration] YAML parse error: {}", e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("[AlarmConfiguration] Error loading config: {}", e.what());
        return false;
    }
}

AlarmConfig AlarmConfiguration::parseAlarmConfig(const void* node_ptr) const {
    const YAML::Node& node = *static_cast<const YAML::Node*>(node_ptr);

    AlarmConfig config;

    // 필수 필드
    config.code = node["code"].as<std::string>();
    config.name = node["name"].as<std::string>();
    config.severity = parseSeverity(node["severity"].as<std::string>());

    // 선택적 필드
    if (node["description"]) {
        config.description = node["description"].as<std::string>();
    }

    if (node["recommended_action"]) {
        config.recommended_action = node["recommended_action"].as<std::string>();
    }

    if (node["recurrence_window"]) {
        config.recurrence_window = std::chrono::seconds(
            node["recurrence_window"].as<int>());
    }

    if (node["recurrence_threshold"]) {
        config.recurrence_threshold = node["recurrence_threshold"].as<uint32_t>();
    }

    if (node["auto_reset"]) {
        config.auto_reset = node["auto_reset"].as<bool>();
    }

    return config;
}

AlarmSeverity AlarmConfiguration::parseSeverity(const std::string& severity_str) const {
    if (severity_str == "CRITICAL") return AlarmSeverity::CRITICAL;
    if (severity_str == "WARNING")  return AlarmSeverity::WARNING;
    if (severity_str == "INFO")     return AlarmSeverity::INFO;

    spdlog::warn("[AlarmConfiguration] Unknown severity '{}', defaulting to INFO", severity_str);
    return AlarmSeverity::INFO;
}

void AlarmConfiguration::addConfig(const AlarmConfig& config) {
    configs_[config.code] = config;
}

std::optional<AlarmConfig> AlarmConfiguration::getAlarmConfig(
    const std::string& alarm_code) const {

    auto it = configs_.find(alarm_code);
    if (it != configs_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<AlarmConfig> AlarmConfiguration::getAllConfigs() const {
    std::vector<AlarmConfig> result;
    result.reserve(configs_.size());

    for (const auto& [code, config] : configs_) {
        result.push_back(config);
    }

    return result;
}

bool AlarmConfiguration::hasAlarmConfig(const std::string& alarm_code) const {
    return configs_.find(alarm_code) != configs_.end();
}

AlarmSeverity AlarmConfiguration::shouldEscalateSeverity(
    const std::string& alarm_code,
    uint32_t recurrence_count) const {

    auto config = getAlarmConfig(alarm_code);
    if (!config) {
        return AlarmSeverity::INFO;  // 기본값
    }

    // 재발 임계값 초과 시 심각도 상향
    if (recurrence_count >= config->recurrence_threshold) {
        if (config->severity == AlarmSeverity::INFO) {
            return AlarmSeverity::WARNING;
        } else if (config->severity == AlarmSeverity::WARNING) {
            return AlarmSeverity::CRITICAL;
        }
    }

    return config->severity;  // 변경 없음
}

bool AlarmConfiguration::validate() const {
    if (configs_.empty()) {
        spdlog::error("[AlarmConfiguration] No alarms configured");
        return false;
    }

    // 각 alarm 검증
    for (const auto& [code, config] : configs_) {
        // 코드 형식 검증 (E###, W###, I###)
        if (code.length() != 4 ||
            (code[0] != 'E' && code[0] != 'W' && code[0] != 'I') ||
            !std::isdigit(code[1]) || !std::isdigit(code[2]) || !std::isdigit(code[3])) {

            spdlog::error("[AlarmConfiguration] Invalid alarm code format: {}", code);
            return false;
        }

        // 이름 검증
        if (config.name.empty()) {
            spdlog::error("[AlarmConfiguration] Empty name for alarm: {}", code);
            return false;
        }
    }

    spdlog::info("[AlarmConfiguration] Validation passed for {} alarms", configs_.size());
    return true;
}

} // namespace mxrc::core::alarm
