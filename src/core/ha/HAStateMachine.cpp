#include "HAStateMachine.h"
#include <spdlog/spdlog.h>

namespace mxrc::core::ha {

HAStateMachine::HAStateMachine(uint32_t max_recovery_attempts)
    : max_recovery_attempts_(max_recovery_attempts),
      state_entered_at_(std::chrono::steady_clock::now())
{
    spdlog::info("HAStateMachine initialized with max_recovery_attempts={}", max_recovery_attempts);
}

void HAStateMachine::setRecoveryPolicies(const std::map<FailureType, RecoveryAction>& policies) {
    recovery_policies_ = policies;
    spdlog::info("Recovery policies configured: {} entries", policies.size());
}

void HAStateMachine::registerStateTransitionCallback(StateTransitionCallback callback) {
    state_transition_callback_ = std::move(callback);
}

void HAStateMachine::registerRecoveryActionCallback(RecoveryActionCallback callback) {
    recovery_action_callback_ = std::move(callback);
}

bool HAStateMachine::handleFailure(FailureType failure_type) {
    HAState current = current_state_.load(std::memory_order_acquire);

    spdlog::warn("Handling failure: {} in state {}", toString(failure_type), toString(current));

    // Look up recovery action for this failure type
    auto it = recovery_policies_.find(failure_type);
    if (it == recovery_policies_.end()) {
        spdlog::error("No recovery policy defined for failure type: {}", toString(failure_type));
        return false;
    }

    RecoveryAction action = it->second;
    spdlog::info("Recovery action: {}", toString(action));

    // Execute recovery action
    if (!executeRecoveryAction(action)) {
        spdlog::error("Recovery action {} failed", toString(action));
        return reportRecoveryFailure();
    }

    // Determine target state
    HAState target_state = determineTargetState(failure_type, action);

    // Transition to target state
    if (!transitionTo(target_state)) {
        spdlog::error("State transition to {} failed", toString(target_state));
        return false;
    }

    return true;
}

bool HAStateMachine::transitionTo(HAState new_state) {
    HAState current = current_state_.load(std::memory_order_acquire);

    // Validate transition
    if (!isValidTransition(current, new_state)) {
        spdlog::error("Invalid state transition: {} → {}", toString(current), toString(new_state));
        return false;
    }

    // Update previous state
    previous_state_.store(current, std::memory_order_release);

    // Update current state
    current_state_.store(new_state, std::memory_order_release);

    // Update timing
    state_entered_at_ = std::chrono::steady_clock::now();

    spdlog::info("State transition: {} → {}", toString(current), toString(new_state));

    // Invoke callback
    if (state_transition_callback_) {
        state_transition_callback_(current, new_state, FailureType::UNKNOWN);
    }

    return true;
}

bool HAStateMachine::reportRecoverySuccess() {
    spdlog::info("Recovery succeeded");

    // Reset recovery attempt counter
    recovery_attempt_count_.store(0, std::memory_order_release);

    // Transition to NORMAL
    return transitionTo(HAState::NORMAL);
}

bool HAStateMachine::reportRecoveryFailure() {
    uint32_t attempts = recovery_attempt_count_.fetch_add(1, std::memory_order_acq_rel) + 1;

    spdlog::warn("Recovery failed (attempt {}/{})", attempts, max_recovery_attempts_);

    // Check if max attempts exceeded
    if (attempts >= max_recovery_attempts_) {
        spdlog::error("Max recovery attempts exceeded, transitioning to MANUAL_INTERVENTION");
        recovery_attempt_count_.store(0, std::memory_order_release);
        return transitionTo(HAState::MANUAL_INTERVENTION);
    }

    return true;
}

uint64_t HAStateMachine::getTimeInCurrentState() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - state_entered_at_);
    return duration.count();
}

bool HAStateMachine::isValidTransition(HAState from, HAState to) {
    // Same state is always valid (no-op)
    if (from == to) {
        return true;
    }

    // Define valid transitions
    switch (from) {
        case HAState::NORMAL:
            return to == HAState::DEGRADED ||
                   to == HAState::RECOVERY_IN_PROGRESS ||
                   to == HAState::SAFE_MODE ||
                   to == HAState::SHUTDOWN;

        case HAState::DEGRADED:
            return to == HAState::NORMAL ||
                   to == HAState::RECOVERY_IN_PROGRESS ||
                   to == HAState::SHUTDOWN;

        case HAState::SAFE_MODE:
            return to == HAState::RECOVERY_IN_PROGRESS ||
                   to == HAState::MANUAL_INTERVENTION ||
                   to == HAState::SHUTDOWN;

        case HAState::RECOVERY_IN_PROGRESS:
            return to == HAState::NORMAL ||
                   to == HAState::SAFE_MODE ||
                   to == HAState::MANUAL_INTERVENTION ||
                   to == HAState::SHUTDOWN;

        case HAState::MANUAL_INTERVENTION:
            return to == HAState::NORMAL ||
                   to == HAState::SHUTDOWN;

        case HAState::SHUTDOWN:
            return false;  // SHUTDOWN is terminal

        default:
            return false;
    }
}

bool HAStateMachine::executeRecoveryAction(RecoveryAction action) {
    if (!recovery_action_callback_) {
        spdlog::error("No recovery action callback registered");
        return false;
    }

    spdlog::info("Executing recovery action: {}", toString(action));

    return recovery_action_callback_(action);
}

HAState HAStateMachine::determineTargetState(FailureType failure_type, RecoveryAction action) {
    // Critical failures always go to SAFE_MODE
    if (failure_type == FailureType::DEADLINE_MISS_CONSECUTIVE ||
        failure_type == FailureType::ETHERCAT_COMM_FAILURE ||
        failure_type == FailureType::MOTOR_OVERCURRENT) {
        return HAState::SAFE_MODE;
    }

    // Determine target state based on recovery action
    switch (action) {
        case RecoveryAction::ENTER_SAFE_MODE:
            return HAState::SAFE_MODE;

        case RecoveryAction::RESTART_RT_PROCESS:
        case RecoveryAction::RELOAD_CONFIGURATION:
            return HAState::RECOVERY_IN_PROGRESS;

        case RecoveryAction::NOTIFY_AND_WAIT:
            return HAState::MANUAL_INTERVENTION;

        case RecoveryAction::SHUTDOWN_SYSTEM:
            return HAState::SHUTDOWN;

        case RecoveryAction::NONE:
            return HAState::DEGRADED;

        default:
            return HAState::DEGRADED;
    }
}

} // namespace mxrc::core::ha
