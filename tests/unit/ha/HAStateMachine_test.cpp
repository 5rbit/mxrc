// HAStateMachine_test.cpp - Unit tests for HAStateMachine
// Feature 019 - US6: T063

#include <gtest/gtest.h>
#include "core/ha/HAStateMachine.h"
#include "core/ha/RecoveryPolicy.h"
#include <atomic>

using namespace mxrc::core::ha;

class HAStateMachineTest : public ::testing::Test {
protected:
    void SetUp() override {
        state_machine_ = std::make_unique<HAStateMachine>();

        // Setup callbacks for testing
        transition_count_ = 0;
        last_from_state_ = HAState::NORMAL;
        last_to_state_ = HAState::NORMAL;

        state_machine_->setStateTransitionCallback(
            [this](HAState from, HAState to) {
                transition_count_++;
                last_from_state_ = from;
                last_to_state_ = to;
            }
        );
    }

    void TearDown() override {
        state_machine_.reset();
    }

    std::unique_ptr<HAStateMachine> state_machine_;
    int transition_count_;
    HAState last_from_state_;
    HAState last_to_state_;
};

// ============================================================================
// T063: HAStateMachine State Transition Tests
// ============================================================================

TEST_F(HAStateMachineTest, InitialState_IsNormal) {
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::NORMAL);
}

TEST_F(HAStateMachineTest, TransitionToSameState_Ignored) {
    // Try to transition to current state
    bool result = state_machine_->transitionTo(HAState::NORMAL);

    // Should fail (no transition needed)
    EXPECT_FALSE(result);
    EXPECT_EQ(transition_count_, 0);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::NORMAL);
}

TEST_F(HAStateMachineTest, ValidTransition_NormalToDegraded) {
    bool result = state_machine_->transitionTo(HAState::DEGRADED);

    EXPECT_TRUE(result);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::DEGRADED);
    EXPECT_EQ(transition_count_, 1);
    EXPECT_EQ(last_from_state_, HAState::NORMAL);
    EXPECT_EQ(last_to_state_, HAState::DEGRADED);
}

TEST_F(HAStateMachineTest, ValidTransition_DegradedToSafeMode) {
    // First transition to DEGRADED
    ASSERT_TRUE(state_machine_->transitionTo(HAState::DEGRADED));

    // Then transition to SAFE_MODE
    bool result = state_machine_->transitionTo(HAState::SAFE_MODE);

    EXPECT_TRUE(result);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::SAFE_MODE);
    EXPECT_EQ(transition_count_, 2);
}

TEST_F(HAStateMachineTest, ValidTransition_SafeModeToRecovery) {
    // Transition sequence: NORMAL -> DEGRADED -> SAFE_MODE -> RECOVERY
    ASSERT_TRUE(state_machine_->transitionTo(HAState::DEGRADED));
    ASSERT_TRUE(state_machine_->transitionTo(HAState::SAFE_MODE));

    bool result = state_machine_->transitionTo(HAState::RECOVERY_IN_PROGRESS);

    EXPECT_TRUE(result);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::RECOVERY_IN_PROGRESS);
    EXPECT_EQ(transition_count_, 3);
}

TEST_F(HAStateMachineTest, ValidTransition_RecoveryToNormal) {
    // Go through full cycle: NORMAL -> DEGRADED -> SAFE_MODE -> RECOVERY -> NORMAL
    ASSERT_TRUE(state_machine_->transitionTo(HAState::DEGRADED));
    ASSERT_TRUE(state_machine_->transitionTo(HAState::SAFE_MODE));
    ASSERT_TRUE(state_machine_->transitionTo(HAState::RECOVERY_IN_PROGRESS));

    bool result = state_machine_->transitionTo(HAState::NORMAL);

    EXPECT_TRUE(result);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::NORMAL);
    EXPECT_EQ(transition_count_, 4);
}

TEST_F(HAStateMachineTest, ValidTransition_AnyToManualIntervention) {
    // From NORMAL to MANUAL_INTERVENTION (always valid)
    bool result = state_machine_->transitionTo(HAState::MANUAL_INTERVENTION);

    EXPECT_TRUE(result);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::MANUAL_INTERVENTION);
}

TEST_F(HAStateMachineTest, ValidTransition_AnyToShutdown) {
    // From NORMAL to SHUTDOWN (always valid)
    bool result = state_machine_->transitionTo(HAState::SHUTDOWN);

    EXPECT_TRUE(result);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::SHUTDOWN);
}

TEST_F(HAStateMachineTest, InvalidTransition_NormalToRecovery) {
    // Cannot go directly from NORMAL to RECOVERY_IN_PROGRESS
    bool result = state_machine_->transitionTo(HAState::RECOVERY_IN_PROGRESS);

    EXPECT_FALSE(result);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::NORMAL);
    EXPECT_EQ(transition_count_, 0);
}

TEST_F(HAStateMachineTest, InvalidTransition_DegradedToRecovery) {
    // Cannot go directly from DEGRADED to RECOVERY_IN_PROGRESS (must go through SAFE_MODE)
    ASSERT_TRUE(state_machine_->transitionTo(HAState::DEGRADED));

    bool result = state_machine_->transitionTo(HAState::RECOVERY_IN_PROGRESS);

    EXPECT_FALSE(result);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::DEGRADED);
}

TEST_F(HAStateMachineTest, TransitionFromShutdown_AlwaysFails) {
    // Transition to SHUTDOWN
    ASSERT_TRUE(state_machine_->transitionTo(HAState::SHUTDOWN));

    // Cannot transition out of SHUTDOWN
    EXPECT_FALSE(state_machine_->transitionTo(HAState::NORMAL));
    EXPECT_FALSE(state_machine_->transitionTo(HAState::DEGRADED));
    EXPECT_FALSE(state_machine_->transitionTo(HAState::MANUAL_INTERVENTION));

    EXPECT_EQ(state_machine_->getCurrentState(), HAState::SHUTDOWN);
}

TEST_F(HAStateMachineTest, HandleFailure_CommunicationError_RestartProtocol) {
    RecoveryPolicy policy;
    policy.setPolicy(FailureType::COMMUNICATION_ERROR, RecoveryAction::RESTART_PROTOCOL);

    state_machine_->handleFailure(FailureType::COMMUNICATION_ERROR, policy);

    // Communication error should transition to DEGRADED
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::DEGRADED);
    EXPECT_EQ(transition_count_, 1);
}

TEST_F(HAStateMachineTest, HandleFailure_ProcessCrash_RestartProcess) {
    RecoveryPolicy policy;
    policy.setPolicy(FailureType::PROCESS_CRASH, RecoveryAction::RESTART_PROCESS);

    state_machine_->handleFailure(FailureType::PROCESS_CRASH, policy);

    // Process crash should transition to RECOVERY_IN_PROGRESS
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::RECOVERY_IN_PROGRESS);
}

TEST_F(HAStateMachineTest, HandleFailure_DeadlineMiss_EnterSafeMode) {
    RecoveryPolicy policy;
    policy.setPolicy(FailureType::DEADLINE_MISS, RecoveryAction::ENTER_SAFE_MODE);

    state_machine_->handleFailure(FailureType::DEADLINE_MISS, policy);

    // Deadline miss should transition to SAFE_MODE
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::SAFE_MODE);
}

TEST_F(HAStateMachineTest, HandleFailure_HardwareError_RequestManualIntervention) {
    RecoveryPolicy policy;
    policy.setPolicy(FailureType::HARDWARE_ERROR, RecoveryAction::REQUEST_MANUAL_INTERVENTION);

    state_machine_->handleFailure(FailureType::HARDWARE_ERROR, policy);

    // Hardware error should transition to MANUAL_INTERVENTION
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::MANUAL_INTERVENTION);
}

TEST_F(HAStateMachineTest, HandleFailure_CriticalError_Shutdown) {
    RecoveryPolicy policy;
    policy.setPolicy(FailureType::CRITICAL_ERROR, RecoveryAction::SHUTDOWN);

    state_machine_->handleFailure(FailureType::CRITICAL_ERROR, policy);

    // Critical error should transition to SHUTDOWN
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::SHUTDOWN);
}

TEST_F(HAStateMachineTest, MultipleFailures_StateProgression) {
    RecoveryPolicy policy;
    policy.setPolicy(FailureType::COMMUNICATION_ERROR, RecoveryAction::RESTART_PROTOCOL);
    policy.setPolicy(FailureType::DEADLINE_MISS, RecoveryAction::ENTER_SAFE_MODE);
    policy.setPolicy(FailureType::PROCESS_CRASH, RecoveryAction::RESTART_PROCESS);

    // First failure: Communication error -> DEGRADED
    state_machine_->handleFailure(FailureType::COMMUNICATION_ERROR, policy);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::DEGRADED);

    // Second failure: Deadline miss -> SAFE_MODE
    state_machine_->handleFailure(FailureType::DEADLINE_MISS, policy);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::SAFE_MODE);

    // Third failure: Process crash -> RECOVERY_IN_PROGRESS
    state_machine_->handleFailure(FailureType::PROCESS_CRASH, policy);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::RECOVERY_IN_PROGRESS);
}

TEST_F(HAStateMachineTest, StateHistory_Tracking) {
    // Perform several transitions
    state_machine_->transitionTo(HAState::DEGRADED);
    state_machine_->transitionTo(HAState::SAFE_MODE);
    state_machine_->transitionTo(HAState::RECOVERY_IN_PROGRESS);
    state_machine_->transitionTo(HAState::NORMAL);

    auto history = state_machine_->getStateHistory();

    // Verify history contains all states
    ASSERT_GE(history.size(), 4u);
    EXPECT_EQ(history[0].state, HAState::DEGRADED);
    EXPECT_EQ(history[1].state, HAState::SAFE_MODE);
    EXPECT_EQ(history[2].state, HAState::RECOVERY_IN_PROGRESS);
    EXPECT_EQ(history[3].state, HAState::NORMAL);
}

TEST_F(HAStateMachineTest, StateCallback_CalledOnEveryTransition) {
    int callback_count = 0;
    HAState callback_from = HAState::NORMAL;
    HAState callback_to = HAState::NORMAL;

    state_machine_->setStateTransitionCallback(
        [&](HAState from, HAState to) {
            callback_count++;
            callback_from = from;
            callback_to = to;
        }
    );

    // Transition twice
    state_machine_->transitionTo(HAState::DEGRADED);
    EXPECT_EQ(callback_count, 1);
    EXPECT_EQ(callback_from, HAState::NORMAL);
    EXPECT_EQ(callback_to, HAState::DEGRADED);

    state_machine_->transitionTo(HAState::SAFE_MODE);
    EXPECT_EQ(callback_count, 2);
    EXPECT_EQ(callback_from, HAState::DEGRADED);
    EXPECT_EQ(callback_to, HAState::SAFE_MODE);
}

TEST_F(HAStateMachineTest, NoCallback_NoErrors) {
    // Clear callback
    state_machine_->setStateTransitionCallback(nullptr);

    // Transitions should still work
    EXPECT_TRUE(state_machine_->transitionTo(HAState::DEGRADED));
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::DEGRADED);
}

TEST_F(HAStateMachineTest, RecoveryComplete_TransitionToNormal) {
    // Simulate recovery scenario
    ASSERT_TRUE(state_machine_->transitionTo(HAState::DEGRADED));
    ASSERT_TRUE(state_machine_->transitionTo(HAState::SAFE_MODE));
    ASSERT_TRUE(state_machine_->transitionTo(HAState::RECOVERY_IN_PROGRESS));

    // Recovery complete -> back to NORMAL
    bool result = state_machine_->transitionTo(HAState::NORMAL);

    EXPECT_TRUE(result);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::NORMAL);
}
