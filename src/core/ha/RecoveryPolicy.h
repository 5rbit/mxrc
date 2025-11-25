#pragma once

#include "HAStateMachine.h"
#include <string>
#include <map>
#include <memory>

namespace mxrc::core::ha {

/**
 * @brief Recovery policy configuration loader
 *
 * Feature 019 - US6: HA Policy Configuration
 *
 * Loads recovery policies from YAML configuration file:
 * - Maps failure types to recovery actions
 * - Configures max recovery attempts
 * - Validates policy completeness
 *
 * YAML Format (ha-policy.yaml):
 * @code
 * max_recovery_attempts: 3
 * policies:
 *   RT_PROCESS_CRASH: RESTART_RT_PROCESS
 *   DEADLINE_MISS_CONSECUTIVE: ENTER_SAFE_MODE
 *   ETHERCAT_COMM_FAILURE: ENTER_SAFE_MODE
 *   SENSOR_FAILURE: NOTIFY_AND_WAIT
 *   MOTOR_OVERCURRENT: ENTER_SAFE_MODE
 *   DATASTORE_CORRUPTION: NOTIFY_AND_WAIT
 *   MEMORY_EXHAUSTION: SHUTDOWN_SYSTEM
 *   UNKNOWN: NOTIFY_AND_WAIT
 * @endcode
 *
 * Usage Example:
 * @code
 * RecoveryPolicy policy;
 * policy.loadFromFile("config/ha-policy.yaml");
 * auto policies = policy.getPolicies();
 * HAStateMachine sm(policy.getMaxRecoveryAttempts());
 * sm.setRecoveryPolicies(policies);
 * @endcode
 */
class RecoveryPolicy {
public:
    /**
     * @brief Default constructor
     */
    RecoveryPolicy() = default;

    /**
     * @brief Load recovery policies from YAML file
     *
     * @param yaml_file_path Path to ha-policy.yaml
     * @return true on success, false on error
     */
    bool loadFromFile(const std::string& yaml_file_path);

    /**
     * @brief Get recovery policy mapping
     *
     * @return Map of failure type to recovery action
     */
    const std::map<FailureType, RecoveryAction>& getPolicies() const {
        return policies_;
    }

    /**
     * @brief Get max recovery attempts
     *
     * @return Maximum number of recovery attempts
     */
    uint32_t getMaxRecoveryAttempts() const {
        return max_recovery_attempts_;
    }

    /**
     * @brief Validate policy completeness
     *
     * Checks that all failure types have defined recovery actions.
     *
     * @return true if all failure types are covered, false otherwise
     */
    bool isComplete() const;

    /**
     * @brief Get default policies
     *
     * Returns a sensible default policy configuration.
     *
     * @return Default policy mapping
     */
    static std::map<FailureType, RecoveryAction> getDefaultPolicies();

private:
    std::map<FailureType, RecoveryAction> policies_;
    uint32_t max_recovery_attempts_{3};

    /**
     * @brief Parse failure type from string
     *
     * @param type_str String representation
     * @return Failure type enum value
     */
    static FailureType parseFailureType(const std::string& type_str);

    /**
     * @brief Parse recovery action from string
     *
     * @param action_str String representation
     * @return Recovery action enum value
     */
    static RecoveryAction parseRecoveryAction(const std::string& action_str);
};

} // namespace mxrc::core::ha
