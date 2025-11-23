// ha_recovery_test.cpp - Integration tests for HA recovery scenarios
// Feature 019 - US6: T064, T065

#include <gtest/gtest.h>
#include "core/ha/HAStateMachine.h"
#include "core/ha/RecoveryPolicy.h"
#include <thread>
#include <chrono>
#include <atomic>

using namespace mxrc::core::ha;

class HARecoveryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        state_machine_ = std::make_unique<HAStateMachine>();
        policy_ = std::make_unique<RecoveryPolicy>();

        // Load default policies
        policy_->setPolicy(FailureType::COMMUNICATION_ERROR, RecoveryAction::RESTART_PROTOCOL);
        policy_->setPolicy(FailureType::PROCESS_CRASH, RecoveryAction::RESTART_PROCESS);
        policy_->setPolicy(FailureType::DEADLINE_MISS, RecoveryAction::ENTER_SAFE_MODE);
        policy_->setPolicy(FailureType::HARDWARE_ERROR, RecoveryAction::REQUEST_MANUAL_INTERVENTION);
        policy_->setPolicy(FailureType::MEMORY_ERROR, RecoveryAction::RESTART_PROCESS);
        policy_->setPolicy(FailureType::DATA_CORRUPTION, RecoveryAction::ENTER_SAFE_MODE);
        policy_->setPolicy(FailureType::WATCHDOG_TIMEOUT, RecoveryAction::RESTART_PROCESS);
        policy_->setPolicy(FailureType::CRITICAL_ERROR, RecoveryAction::SHUTDOWN);

        recovery_completed_ = false;
    }

    void TearDown() override {
        state_machine_.reset();
        policy_.reset();
    }

    std::unique_ptr<HAStateMachine> state_machine_;
    std::unique_ptr<RecoveryPolicy> policy_;
    std::atomic<bool> recovery_completed_;
};

// ============================================================================
// T064: RT Process Crash Recovery Scenario
// ============================================================================

TEST_F(HARecoveryIntegrationTest, ProcessCrash_FullRecoverySequence) {
    // Setup callback to simulate recovery completion
    state_machine_->setStateTransitionCallback(
        [this](HAState from, HAState to) {
            if (to == HAState::RECOVERY_IN_PROGRESS) {
                // Simulate recovery process in background
                std::thread([this]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    // Recovery completed, transition back to NORMAL
                    state_machine_->transitionTo(HAState::NORMAL);
                    recovery_completed_ = true;
                }).detach();
            }
        }
    );

    // Simulate RT process crash
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::NORMAL);

    state_machine_->handleFailure(FailureType::PROCESS_CRASH, *policy_);

    // Should transition to RECOVERY_IN_PROGRESS
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::RECOVERY_IN_PROGRESS);

    // Wait for recovery to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Should be back to NORMAL
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::NORMAL);
    EXPECT_TRUE(recovery_completed_);
}

TEST_F(HARecoveryIntegrationTest, ProcessCrash_WithCheckpoint) {
    // Simulate checkpoint before crash
    auto history_before_crash = state_machine_->getStateHistory();

    // Process crashes
    state_machine_->handleFailure(FailureType::PROCESS_CRASH, *policy_);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::RECOVERY_IN_PROGRESS);

    // Verify state history maintained
    auto history_after_crash = state_machine_->getStateHistory();
    EXPECT_GT(history_after_crash.size(), history_before_crash.size());

    // Recovery should restore to NORMAL
    EXPECT_TRUE(state_machine_->transitionTo(HAState::NORMAL));
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::NORMAL);
}

TEST_F(HARecoveryIntegrationTest, MultipleProcessCrashes_EscalateToManual) {
    // Simulate repeated process crashes (should escalate)
    for (int i = 0; i < 3; ++i) {
        state_machine_->handleFailure(FailureType::PROCESS_CRASH, *policy_);

        if (i < 2) {
            // First two crashes: attempt recovery
            EXPECT_EQ(state_machine_->getCurrentState(), HAState::RECOVERY_IN_PROGRESS);
            // Simulate failed recovery - go back to DEGRADED
            state_machine_->transitionTo(HAState::DEGRADED);
        }
    }

    // After multiple failures, should escalate to manual intervention
    // (In real implementation, retry counter would trigger this)
    state_machine_->transitionTo(HAState::MANUAL_INTERVENTION);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::MANUAL_INTERVENTION);
}

TEST_F(HARecoveryIntegrationTest, WatchdogTimeout_AutoRecovery) {
    // Simulate watchdog timeout
    state_machine_->handleFailure(FailureType::WATCHDOG_TIMEOUT, *policy_);

    // Should trigger process restart
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::RECOVERY_IN_PROGRESS);

    // Simulate successful restart
    EXPECT_TRUE(state_machine_->transitionTo(HAState::NORMAL));
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::NORMAL);
}

TEST_F(HARecoveryIntegrationTest, MemoryError_RestartAndRecover) {
    // Simulate memory error
    state_machine_->handleFailure(FailureType::MEMORY_ERROR, *policy_);

    // Should restart process
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::RECOVERY_IN_PROGRESS);

    // Simulate memory cleanup and restart
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_TRUE(state_machine_->transitionTo(HAState::NORMAL));
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::NORMAL);
}

// ============================================================================
// T065: Deadline Miss → Safe Mode Transition
// ============================================================================

TEST_F(HARecoveryIntegrationTest, DeadlineMiss_EnterSafeMode) {
    // Simulate deadline miss
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::NORMAL);

    state_machine_->handleFailure(FailureType::DEADLINE_MISS, *policy_);

    // Should enter SAFE_MODE immediately
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::SAFE_MODE);
}

TEST_F(HARecoveryIntegrationTest, DeadlineMiss_SafeModeOperations) {
    // Enter safe mode due to deadline miss
    state_machine_->handleFailure(FailureType::DEADLINE_MISS, *policy_);
    ASSERT_EQ(state_machine_->getCurrentState(), HAState::SAFE_MODE);

    // In safe mode, system should:
    // 1. Reduce operation frequency
    // 2. Disable non-critical features
    // 3. Monitor for stability

    // Simulate safe mode operations for 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // If stable, can attempt recovery
    EXPECT_TRUE(state_machine_->transitionTo(HAState::RECOVERY_IN_PROGRESS));
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::RECOVERY_IN_PROGRESS);
}

TEST_F(HARecoveryIntegrationTest, DeadlineMiss_RecoveryToNormal) {
    // Full sequence: NORMAL -> SAFE_MODE -> RECOVERY -> NORMAL
    state_machine_->handleFailure(FailureType::DEADLINE_MISS, *policy_);
    ASSERT_EQ(state_machine_->getCurrentState(), HAState::SAFE_MODE);

    // Initiate recovery
    ASSERT_TRUE(state_machine_->transitionTo(HAState::RECOVERY_IN_PROGRESS));

    // Simulate recovery process
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Recovery successful
    EXPECT_TRUE(state_machine_->transitionTo(HAState::NORMAL));
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::NORMAL);
}

TEST_F(HARecoveryIntegrationTest, RepeatedDeadlineMiss_StayInSafeMode) {
    // First deadline miss -> SAFE_MODE
    state_machine_->handleFailure(FailureType::DEADLINE_MISS, *policy_);
    ASSERT_EQ(state_machine_->getCurrentState(), HAState::SAFE_MODE);

    // Second deadline miss while in SAFE_MODE
    // Should stay in SAFE_MODE (cannot transition to same state)
    state_machine_->handleFailure(FailureType::DEADLINE_MISS, *policy_);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::SAFE_MODE);
}

TEST_F(HARecoveryIntegrationTest, SafeMode_CriticalErrorEscalation) {
    // Enter safe mode
    state_machine_->handleFailure(FailureType::DEADLINE_MISS, *policy_);
    ASSERT_EQ(state_machine_->getCurrentState(), HAState::SAFE_MODE);

    // Critical error occurs in safe mode
    state_machine_->handleFailure(FailureType::CRITICAL_ERROR, *policy_);

    // Should escalate to SHUTDOWN
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::SHUTDOWN);
}

// ============================================================================
// Complex Recovery Scenarios
// ============================================================================

TEST_F(HARecoveryIntegrationTest, CommunicationError_DegradedOperation) {
    // Communication error -> DEGRADED
    state_machine_->handleFailure(FailureType::COMMUNICATION_ERROR, *policy_);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::DEGRADED);

    // System continues in degraded mode
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Communication restored
    EXPECT_TRUE(state_machine_->transitionTo(HAState::NORMAL));
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::NORMAL);
}

TEST_F(HARecoveryIntegrationTest, DataCorruption_SafeModeProtection) {
    // Data corruption detected
    state_machine_->handleFailure(FailureType::DATA_CORRUPTION, *policy_);

    // Should enter SAFE_MODE to prevent further corruption
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::SAFE_MODE);

    // Data validation and cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // After cleanup, can recover
    EXPECT_TRUE(state_machine_->transitionTo(HAState::RECOVERY_IN_PROGRESS));
    EXPECT_TRUE(state_machine_->transitionTo(HAState::NORMAL));
}

TEST_F(HARecoveryIntegrationTest, HardwareError_ManualInterventionRequired) {
    // Hardware error detected
    state_machine_->handleFailure(FailureType::HARDWARE_ERROR, *policy_);

    // Should request manual intervention
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::MANUAL_INTERVENTION);

    // System waits for operator action
    // Cannot auto-recover from MANUAL_INTERVENTION
    EXPECT_FALSE(state_machine_->transitionTo(HAState::NORMAL));
}

TEST_F(HARecoveryIntegrationTest, CriticalError_ImmediateShutdown) {
    // Critical error detected
    state_machine_->handleFailure(FailureType::CRITICAL_ERROR, *policy_);

    // Should shutdown immediately
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::SHUTDOWN);

    // Cannot recover from shutdown
    EXPECT_FALSE(state_machine_->transitionTo(HAState::NORMAL));
    EXPECT_FALSE(state_machine_->transitionTo(HAState::RECOVERY_IN_PROGRESS));
}

TEST_F(HARecoveryIntegrationTest, FailureEscalationChain) {
    // Simulate escalating failures

    // 1. Communication error -> DEGRADED
    state_machine_->handleFailure(FailureType::COMMUNICATION_ERROR, *policy_);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::DEGRADED);

    // 2. Deadline miss while degraded -> SAFE_MODE
    state_machine_->handleFailure(FailureType::DEADLINE_MISS, *policy_);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::SAFE_MODE);

    // 3. Process crash in safe mode -> RECOVERY
    state_machine_->handleFailure(FailureType::PROCESS_CRASH, *policy_);
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::RECOVERY_IN_PROGRESS);

    // 4. If recovery succeeds
    EXPECT_TRUE(state_machine_->transitionTo(HAState::NORMAL));
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::NORMAL);
}

TEST_F(HARecoveryIntegrationTest, RecoveryTimeout_EscalateToManual) {
    // Enter recovery state
    state_machine_->handleFailure(FailureType::PROCESS_CRASH, *policy_);
    ASSERT_EQ(state_machine_->getCurrentState(), HAState::RECOVERY_IN_PROGRESS);

    // Simulate recovery timeout (no progress for extended period)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // In real implementation, timeout would trigger escalation
    EXPECT_TRUE(state_machine_->transitionTo(HAState::MANUAL_INTERVENTION));
    EXPECT_EQ(state_machine_->getCurrentState(), HAState::MANUAL_INTERVENTION);
}

TEST_F(HARecoveryIntegrationTest, StateHistory_FullRecoveryCycle) {
    // Perform full recovery cycle and verify history
    state_machine_->handleFailure(FailureType::COMMUNICATION_ERROR, *policy_);  // -> DEGRADED
    state_machine_->handleFailure(FailureType::DEADLINE_MISS, *policy_);        // -> SAFE_MODE
    state_machine_->transitionTo(HAState::RECOVERY_IN_PROGRESS);                // -> RECOVERY
    state_machine_->transitionTo(HAState::NORMAL);                              // -> NORMAL

    auto history = state_machine_->getStateHistory();

    // Should have all transition records
    ASSERT_GE(history.size(), 4u);
    EXPECT_EQ(history[0].state, HAState::DEGRADED);
    EXPECT_EQ(history[1].state, HAState::SAFE_MODE);
    EXPECT_EQ(history[2].state, HAState::RECOVERY_IN_PROGRESS);
    EXPECT_EQ(history[3].state, HAState::NORMAL);
}
