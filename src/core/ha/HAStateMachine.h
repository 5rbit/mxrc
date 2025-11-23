#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <memory>

namespace mxrc::core::ha {

/**
 * @brief HA state enumeration
 *
 * Feature 019 - US6: High Availability State Machine
 *
 * States represent the current operational status of the system:
 * - NORMAL: System operating correctly
 * - DEGRADED: Minor issues, reduced capability
 * - SAFE_MODE: Critical failure, minimal operations only
 * - RECOVERY_IN_PROGRESS: Attempting to recover from failure
 * - MANUAL_INTERVENTION: Requires operator action
 * - SHUTDOWN: System shutting down
 */
enum class HAState {
    NORMAL,                  ///< Normal operation
    DEGRADED,                ///< Degraded performance
    SAFE_MODE,               ///< Safe mode (motors stopped, monitoring only)
    RECOVERY_IN_PROGRESS,    ///< Recovery action executing
    MANUAL_INTERVENTION,     ///< Requires manual intervention
    SHUTDOWN                 ///< System shutting down
};

/**
 * @brief Failure type enumeration
 *
 * Categorizes different types of system failures for appropriate recovery:
 * - RT_PROCESS_CRASH: RT process terminated unexpectedly
 * - DEADLINE_MISS_CONSECUTIVE: Multiple consecutive deadline misses
 * - ETHERCAT_COMM_FAILURE: EtherCAT communication error
 * - SENSOR_FAILURE: Sensor data invalid or missing
 * - MEMORY_EXHAUSTION: Out of memory condition
 * - UNKNOWN: Unclassified failure
 */
enum class FailureType {
    RT_PROCESS_CRASH,
    DEADLINE_MISS_CONSECUTIVE,
    ETHERCAT_COMM_FAILURE,
    SENSOR_FAILURE,
    MOTOR_OVERCURRENT,
    DATASTORE_CORRUPTION,
    MEMORY_EXHAUSTION,
    UNKNOWN
};

/**
 * @brief Recovery action enumeration
 *
 * Actions to take when a failure occurs:
 * - RESTART_RT_PROCESS: Restart the RT process
 * - ENTER_SAFE_MODE: Stop motors, enter safe mode
 * - NOTIFY_AND_WAIT: Alert operator and wait for manual intervention
 * - SHUTDOWN_SYSTEM: Shut down the entire system
 * - RELOAD_CONFIGURATION: Reload configuration and reinitialize
 */
enum class RecoveryAction {
    RESTART_RT_PROCESS,
    ENTER_SAFE_MODE,
    NOTIFY_AND_WAIT,
    SHUTDOWN_SYSTEM,
    RELOAD_CONFIGURATION,
    NONE
};

/**
 * @brief Convert HAState to string
 */
inline const char* toString(HAState state) {
    switch (state) {
        case HAState::NORMAL: return "NORMAL";
        case HAState::DEGRADED: return "DEGRADED";
        case HAState::SAFE_MODE: return "SAFE_MODE";
        case HAState::RECOVERY_IN_PROGRESS: return "RECOVERY_IN_PROGRESS";
        case HAState::MANUAL_INTERVENTION: return "MANUAL_INTERVENTION";
        case HAState::SHUTDOWN: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert FailureType to string
 */
inline const char* toString(FailureType type) {
    switch (type) {
        case FailureType::RT_PROCESS_CRASH: return "RT_PROCESS_CRASH";
        case FailureType::DEADLINE_MISS_CONSECUTIVE: return "DEADLINE_MISS_CONSECUTIVE";
        case FailureType::ETHERCAT_COMM_FAILURE: return "ETHERCAT_COMM_FAILURE";
        case FailureType::SENSOR_FAILURE: return "SENSOR_FAILURE";
        case FailureType::MOTOR_OVERCURRENT: return "MOTOR_OVERCURRENT";
        case FailureType::DATASTORE_CORRUPTION: return "DATASTORE_CORRUPTION";
        case FailureType::MEMORY_EXHAUSTION: return "MEMORY_EXHAUSTION";
        case FailureType::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert RecoveryAction to string
 */
inline const char* toString(RecoveryAction action) {
    switch (action) {
        case RecoveryAction::RESTART_RT_PROCESS: return "RESTART_RT_PROCESS";
        case RecoveryAction::ENTER_SAFE_MODE: return "ENTER_SAFE_MODE";
        case RecoveryAction::NOTIFY_AND_WAIT: return "NOTIFY_AND_WAIT";
        case RecoveryAction::SHUTDOWN_SYSTEM: return "SHUTDOWN_SYSTEM";
        case RecoveryAction::RELOAD_CONFIGURATION: return "RELOAD_CONFIGURATION";
        case RecoveryAction::NONE: return "NONE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief HA State Machine for system failure management
 *
 * Feature 019 - US6: High Availability Policy
 *
 * Manages system state transitions in response to failures:
 * - Tracks current and previous state
 * - Executes recovery actions based on failure type
 * - Enforces state transition rules
 * - Counts recovery attempts
 * - Triggers manual intervention after max retries
 *
 * State Transition Rules:
 * - NORMAL → DEGRADED (minor error)
 * - NORMAL → RECOVERY_IN_PROGRESS (recoverable failure)
 * - NORMAL → SAFE_MODE (critical failure)
 * - DEGRADED → NORMAL (error resolved)
 * - DEGRADED → RECOVERY_IN_PROGRESS (failure escalation)
 * - RECOVERY_IN_PROGRESS → NORMAL (recovery success)
 * - RECOVERY_IN_PROGRESS → MANUAL_INTERVENTION (recovery failed 3x)
 * - RECOVERY_IN_PROGRESS → SAFE_MODE (critical failure during recovery)
 * - SAFE_MODE → RECOVERY_IN_PROGRESS (attempt recovery)
 * - MANUAL_INTERVENTION → NORMAL (operator resolved)
 * - MANUAL_INTERVENTION → SHUTDOWN (operator decision)
 *
 * Usage Example:
 * @code
 * HAStateMachine sm(recovery_policies);
 * sm.handleFailure(FailureType::DEADLINE_MISS_CONSECUTIVE);
 * // State: NORMAL → SAFE_MODE
 * // Action: ENTER_SAFE_MODE executed
 * @endcode
 */
class HAStateMachine {
public:
    /**
     * @brief State transition callback
     *
     * @param from Previous state
     * @param to New state
     * @param failure_type Failure that triggered transition
     */
    using StateTransitionCallback = std::function<void(HAState from, HAState to, FailureType failure_type)>;

    /**
     * @brief Recovery action callback
     *
     * @param action Recovery action to execute
     * @return true if action succeeded, false otherwise
     */
    using RecoveryActionCallback = std::function<bool(RecoveryAction action)>;

    /**
     * @brief Construct HA State Machine
     *
     * @param max_recovery_attempts Maximum recovery attempts before manual intervention (default: 3)
     */
    explicit HAStateMachine(uint32_t max_recovery_attempts = 3);

    /**
     * @brief Set recovery policy mapping
     *
     * Maps failure types to recovery actions. Loaded from ha-policy.yaml.
     *
     * @param policies Map of failure type to recovery action
     */
    void setRecoveryPolicies(const std::map<FailureType, RecoveryAction>& policies);

    /**
     * @brief Register state transition callback
     *
     * @param callback Callback to invoke on state transitions
     */
    void registerStateTransitionCallback(StateTransitionCallback callback);

    /**
     * @brief Register recovery action callback
     *
     * @param callback Callback to execute recovery actions
     */
    void registerRecoveryActionCallback(RecoveryActionCallback callback);

    /**
     * @brief Handle a system failure
     *
     * Determines appropriate recovery action and executes state transition:
     * 1. Look up recovery action for failure type
     * 2. Validate state transition
     * 3. Execute recovery action
     * 4. Transition to new state
     * 5. Update metrics
     *
     * @param failure_type Type of failure that occurred
     * @return true if handled successfully, false otherwise
     */
    bool handleFailure(FailureType failure_type);

    /**
     * @brief Transition to a specific state
     *
     * Validates transition and updates state. Used by recovery actions.
     *
     * @param new_state Target state
     * @return true if transition is valid and completed, false otherwise
     */
    bool transitionTo(HAState new_state);

    /**
     * @brief Report recovery success
     *
     * Resets recovery attempt counter and transitions to NORMAL state.
     *
     * @return true if transition succeeded
     */
    bool reportRecoverySuccess();

    /**
     * @brief Report recovery failure
     *
     * Increments recovery attempt counter. If max attempts exceeded,
     * transitions to MANUAL_INTERVENTION.
     *
     * @return true if transition succeeded
     */
    bool reportRecoveryFailure();

    /**
     * @brief Get current state
     *
     * @return Current HA state
     */
    HAState getCurrentState() const {
        return current_state_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get previous state
     *
     * @return Previous HA state
     */
    HAState getPreviousState() const {
        return previous_state_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get recovery attempt count
     *
     * @return Number of recovery attempts for current failure
     */
    uint32_t getRecoveryAttemptCount() const {
        return recovery_attempt_count_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get time in current state
     *
     * @return Duration in current state (milliseconds)
     */
    uint64_t getTimeInCurrentState() const;

    /**
     * @brief Check if state transition is valid
     *
     * @param from Source state
     * @param to Target state
     * @return true if transition is allowed
     */
    static bool isValidTransition(HAState from, HAState to);

private:
    // Current and previous states
    std::atomic<HAState> current_state_{HAState::NORMAL};
    std::atomic<HAState> previous_state_{HAState::NORMAL};

    // Recovery management
    std::map<FailureType, RecoveryAction> recovery_policies_;
    std::atomic<uint32_t> recovery_attempt_count_{0};
    const uint32_t max_recovery_attempts_;

    // Timing
    std::chrono::steady_clock::time_point state_entered_at_;

    // Callbacks
    StateTransitionCallback state_transition_callback_;
    RecoveryActionCallback recovery_action_callback_;

    /**
     * @brief Execute recovery action
     *
     * @param action Recovery action to execute
     * @return true if succeeded, false otherwise
     */
    bool executeRecoveryAction(RecoveryAction action);

    /**
     * @brief Determine target state for failure
     *
     * @param failure_type Type of failure
     * @param action Recovery action to execute
     * @return Target state after recovery action
     */
    HAState determineTargetState(FailureType failure_type, RecoveryAction action);
};

} // namespace mxrc::core::ha
