#include "RecoveryPolicy.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace mxrc::core::ha {

bool RecoveryPolicy::loadFromFile(const std::string& yaml_file_path) {
    try {
        spdlog::info("Loading HA recovery policy from: {}", yaml_file_path);

        YAML::Node config = YAML::LoadFile(yaml_file_path);

        // Load max recovery attempts
        if (config["max_recovery_attempts"]) {
            max_recovery_attempts_ = config["max_recovery_attempts"].as<uint32_t>();
            spdlog::info("Max recovery attempts: {}", max_recovery_attempts_);
        }

        // Load policies
        if (config["policies"]) {
            YAML::Node policies_node = config["policies"];

            for (YAML::const_iterator it = policies_node.begin(); it != policies_node.end(); ++it) {
                std::string failure_type_str = it->first.as<std::string>();
                std::string recovery_action_str = it->second.as<std::string>();

                FailureType failure_type = parseFailureType(failure_type_str);
                RecoveryAction recovery_action = parseRecoveryAction(recovery_action_str);

                policies_[failure_type] = recovery_action;

                spdlog::debug("Policy: {} → {}", failure_type_str, recovery_action_str);
            }

            spdlog::info("Loaded {} recovery policies", policies_.size());
        }

        // Validate completeness
        if (!isComplete()) {
            spdlog::warn("Recovery policy is incomplete (some failure types not covered)");
        }

        return true;

    } catch (const YAML::Exception& e) {
        spdlog::error("Failed to load HA policy from {}: {}", yaml_file_path, e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Error loading HA policy: {}", e.what());
        return false;
    }
}

bool RecoveryPolicy::isComplete() const {
    // Check that all failure types have a defined policy
    std::vector<FailureType> all_types = {
        FailureType::RT_PROCESS_CRASH,
        FailureType::DEADLINE_MISS_CONSECUTIVE,
        FailureType::ETHERCAT_COMM_FAILURE,
        FailureType::SENSOR_FAILURE,
        FailureType::MOTOR_OVERCURRENT,
        FailureType::DATASTORE_CORRUPTION,
        FailureType::MEMORY_EXHAUSTION,
        FailureType::UNKNOWN
    };

    for (FailureType type : all_types) {
        if (policies_.find(type) == policies_.end()) {
            spdlog::warn("Missing recovery policy for: {}", toString(type));
            return false;
        }
    }

    return true;
}

std::map<FailureType, RecoveryAction> RecoveryPolicy::getDefaultPolicies() {
    return {
        {FailureType::RT_PROCESS_CRASH, RecoveryAction::RESTART_RT_PROCESS},
        {FailureType::DEADLINE_MISS_CONSECUTIVE, RecoveryAction::ENTER_SAFE_MODE},
        {FailureType::ETHERCAT_COMM_FAILURE, RecoveryAction::ENTER_SAFE_MODE},
        {FailureType::SENSOR_FAILURE, RecoveryAction::NOTIFY_AND_WAIT},
        {FailureType::MOTOR_OVERCURRENT, RecoveryAction::ENTER_SAFE_MODE},
        {FailureType::DATASTORE_CORRUPTION, RecoveryAction::NOTIFY_AND_WAIT},
        {FailureType::MEMORY_EXHAUSTION, RecoveryAction::SHUTDOWN_SYSTEM},
        {FailureType::UNKNOWN, RecoveryAction::NOTIFY_AND_WAIT}
    };
}

FailureType RecoveryPolicy::parseFailureType(const std::string& type_str) {
    if (type_str == "RT_PROCESS_CRASH") return FailureType::RT_PROCESS_CRASH;
    if (type_str == "DEADLINE_MISS_CONSECUTIVE") return FailureType::DEADLINE_MISS_CONSECUTIVE;
    if (type_str == "ETHERCAT_COMM_FAILURE") return FailureType::ETHERCAT_COMM_FAILURE;
    if (type_str == "SENSOR_FAILURE") return FailureType::SENSOR_FAILURE;
    if (type_str == "MOTOR_OVERCURRENT") return FailureType::MOTOR_OVERCURRENT;
    if (type_str == "DATASTORE_CORRUPTION") return FailureType::DATASTORE_CORRUPTION;
    if (type_str == "MEMORY_EXHAUSTION") return FailureType::MEMORY_EXHAUSTION;
    if (type_str == "UNKNOWN") return FailureType::UNKNOWN;

    spdlog::warn("Unknown failure type: {}, defaulting to UNKNOWN", type_str);
    return FailureType::UNKNOWN;
}

RecoveryAction RecoveryPolicy::parseRecoveryAction(const std::string& action_str) {
    if (action_str == "RESTART_RT_PROCESS") return RecoveryAction::RESTART_RT_PROCESS;
    if (action_str == "ENTER_SAFE_MODE") return RecoveryAction::ENTER_SAFE_MODE;
    if (action_str == "NOTIFY_AND_WAIT") return RecoveryAction::NOTIFY_AND_WAIT;
    if (action_str == "SHUTDOWN_SYSTEM") return RecoveryAction::SHUTDOWN_SYSTEM;
    if (action_str == "RELOAD_CONFIGURATION") return RecoveryAction::RELOAD_CONFIGURATION;
    if (action_str == "NONE") return RecoveryAction::NONE;

    spdlog::warn("Unknown recovery action: {}, defaulting to NOTIFY_AND_WAIT", action_str);
    return RecoveryAction::NOTIFY_AND_WAIT;
}

} // namespace mxrc::core::ha
