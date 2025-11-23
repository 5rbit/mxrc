// failover_manager_test.cpp
// Copyright (C) 2025 MXRC Project
//
// T064: FailoverManager unit test

#include "core/ha/FailoverManager.h"
#include "core/ha/StateCheckpoint.h"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>

using namespace mxrc::ha;

class FailoverManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        policy_.process_name = "test_process";
        policy_.health_check_interval_ms = 1000;
        policy_.health_check_timeout_ms = 500;
        policy_.failure_threshold = 3;
        policy_.restart_delay_ms = 100;
        policy_.max_restart_count = 5;
        policy_.restart_window_sec = 60;
        policy_.enable_state_recovery = true;
        policy_.checkpoint_interval_sec = 60;
        policy_.enable_leader_election = false;

        test_config_path_ = "/tmp/test_failover_policy.json";
    }

    void TearDown() override {
        if (std::filesystem::exists(test_config_path_)) {
            std::filesystem::remove(test_config_path_);
        }
    }

    FailoverPolicy policy_;
    std::string test_config_path_;
};

// T064.1: FailoverPolicy validation
TEST_F(FailoverManagerTest, PolicyValidation) {
    // Valid policy
    EXPECT_TRUE(policy_.isValid());

    // Invalid: timeout >= interval
    policy_.health_check_timeout_ms = 1000;
    EXPECT_FALSE(policy_.isValid());
    policy_.health_check_timeout_ms = 500;

    // Invalid: failure_threshold < 1
    policy_.failure_threshold = 0;
    EXPECT_FALSE(policy_.isValid());
    policy_.failure_threshold = 3;

    // Invalid: max_restart_count == 0
    policy_.max_restart_count = 0;
    EXPECT_FALSE(policy_.isValid());
    policy_.max_restart_count = 5;

    // Invalid: recovery enabled but checkpoint_interval == 0
    policy_.checkpoint_interval_sec = 0;
    EXPECT_FALSE(policy_.isValid());
    policy_.checkpoint_interval_sec = 60;
}

// T064.2: Factory creation with valid policy
TEST_F(FailoverManagerTest, FactoryCreation) {
    auto manager = createFailoverManager(policy_);
    EXPECT_NE(manager, nullptr);

    auto& retrieved_policy = manager->getPolicy();
    EXPECT_EQ(retrieved_policy.process_name, "test_process");
    EXPECT_EQ(retrieved_policy.failure_threshold, 3);
}

// T064.3: Factory creation with invalid policy throws
TEST_F(FailoverManagerTest, FactoryCreationInvalidPolicy) {
    policy_.failure_threshold = 0;
    EXPECT_THROW(createFailoverManager(policy_), std::invalid_argument);
}

// T064.4: Start and stop failover manager
TEST_F(FailoverManagerTest, StartStop) {
    auto manager = createFailoverManager(policy_);

    EXPECT_TRUE(manager->start());

    // Should not start again
    EXPECT_FALSE(manager->start());

    manager->stop();

    // Can start again after stop
    EXPECT_TRUE(manager->start());
    manager->stop();
}

// T064.5: Restart count tracking
TEST_F(FailoverManagerTest, RestartCountTracking) {
    auto manager = createFailoverManager(policy_);
    manager->start();

    // Initially 0
    EXPECT_EQ(manager->getRestartCount("test_process"), 0);

    // After restart
    manager->triggerRestart("test_process", false);
    EXPECT_EQ(manager->getRestartCount("test_process"), 1);

    manager->triggerRestart("test_process", false);
    EXPECT_EQ(manager->getRestartCount("test_process"), 2);

    manager->stop();
}

// T064.6: Restart count reset
TEST_F(FailoverManagerTest, RestartCountReset) {
    auto manager = createFailoverManager(policy_);
    manager->start();

    manager->triggerRestart("test_process", false);
    manager->triggerRestart("test_process", false);
    EXPECT_EQ(manager->getRestartCount("test_process"), 2);

    manager->resetRestartCount("test_process");
    EXPECT_EQ(manager->getRestartCount("test_process"), 0);

    manager->stop();
}

// T064.7: Can restart check (within limit)
TEST_F(FailoverManagerTest, CanRestartWithinLimit) {
    auto manager = createFailoverManager(policy_);
    manager->start();

    // Initially can restart
    EXPECT_TRUE(manager->canRestart("test_process"));

    // After reaching max (5), cannot restart
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(manager->canRestart("test_process"));
        manager->triggerRestart("test_process", false);
    }

    EXPECT_FALSE(manager->canRestart("test_process"));

    manager->stop();
}

// T064.8: Restart window expiry
TEST_F(FailoverManagerTest, RestartWindowExpiry) {
    policy_.restart_window_sec = 1;  // 1 second window
    auto manager = createFailoverManager(policy_);
    manager->start();

    manager->triggerRestart("test_process", false);
    EXPECT_EQ(manager->getRestartCount("test_process"), 1);

    // Wait for window to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Count should be 0 after window expiry
    EXPECT_EQ(manager->getRestartCount("test_process"), 0);
    EXPECT_TRUE(manager->canRestart("test_process"));

    manager->stop();
}

// T064.9: Handle process failure
TEST_F(FailoverManagerTest, HandleProcessFailure) {
    auto manager = createFailoverManager(policy_);
    manager->start();

    manager->handleProcessFailure("test_process");

    // Should have triggered restart
    EXPECT_EQ(manager->getRestartCount("test_process"), 1);

    manager->stop();
}

// T064.10: Handle process failure with restart limit
TEST_F(FailoverManagerTest, HandleProcessFailureRestartLimit) {
    auto manager = createFailoverManager(policy_);
    manager->start();

    // Trigger max restarts
    for (int i = 0; i < 5; i++) {
        manager->handleProcessFailure("test_process");
    }

    EXPECT_EQ(manager->getRestartCount("test_process"), 5);

    // Next failure should not restart
    manager->handleProcessFailure("test_process");
    EXPECT_EQ(manager->getRestartCount("test_process"), 5);

    manager->stop();
}

// T064.11: Load policy from JSON file
TEST_F(FailoverManagerTest, LoadPolicyFromJSON) {
    // Create test config file
    nlohmann::json j;
    j["process_name"] = "loaded_process";
    j["health_check_interval_ms"] = 2000;
    j["health_check_timeout_ms"] = 1000;
    j["failure_threshold"] = 5;
    j["restart_delay_ms"] = 200;
    j["max_restart_count"] = 10;
    j["restart_window_sec"] = 120;
    j["enable_state_recovery"] = false;
    j["checkpoint_interval_sec"] = 0;
    j["enable_leader_election"] = true;

    std::ofstream file(test_config_path_);
    file << j.dump(2);
    file.close();

    auto manager = createFailoverManager(policy_);
    EXPECT_TRUE(manager->loadPolicy(test_config_path_));

    auto& loaded_policy = manager->getPolicy();
    EXPECT_EQ(loaded_policy.process_name, "loaded_process");
    EXPECT_EQ(loaded_policy.health_check_interval_ms, 2000);
    EXPECT_EQ(loaded_policy.failure_threshold, 5);
    EXPECT_EQ(loaded_policy.max_restart_count, 10);
    EXPECT_FALSE(loaded_policy.enable_state_recovery);
    EXPECT_TRUE(loaded_policy.enable_leader_election);
}

// T064.12: Load policy from non-existent file
TEST_F(FailoverManagerTest, LoadPolicyNonExistentFile) {
    auto manager = createFailoverManager(policy_);
    EXPECT_FALSE(manager->loadPolicy("/nonexistent/path/config.json"));
}

// T064.13: Integration with StateCheckpoint
TEST_F(FailoverManagerTest, IntegrationWithCheckpoint) {
    std::shared_ptr<IStateCheckpoint> checkpoint_mgr = createStateCheckpointManager("test_process");
    auto manager = createFailoverManager(policy_, checkpoint_mgr);

    manager->start();

    // Create a checkpoint
    auto checkpoint = checkpoint_mgr->createCheckpoint();
    checkpoint.rt_state = {{"state", "running"}};
    checkpoint.is_complete = true;
    checkpoint_mgr->saveCheckpoint(checkpoint);

    // Trigger restart with recovery
    EXPECT_TRUE(manager->triggerRestart("test_process", true));

    manager->stop();
}

// T064.14: Trigger restart with wrong process name
TEST_F(FailoverManagerTest, TriggerRestartWrongProcessName) {
    auto manager = createFailoverManager(policy_);
    manager->start();

    EXPECT_FALSE(manager->triggerRestart("wrong_process", false));

    manager->stop();
}

// T064.15: Process name mismatch in handleProcessFailure
TEST_F(FailoverManagerTest, HandleProcessFailureWrongName) {
    auto manager = createFailoverManager(policy_);
    manager->start();

    // Should not trigger restart for wrong process
    manager->handleProcessFailure("wrong_process");
    EXPECT_EQ(manager->getRestartCount("test_process"), 0);

    manager->stop();
}
