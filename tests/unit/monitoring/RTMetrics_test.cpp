// RTMetrics_test.cpp
// Copyright (C) 2025 MXRC Project

#include "core/rt/RTMetrics.h"
#include "core/rt/RTStateMachine.h"
#include <gtest/gtest.h>

using namespace mxrc::core::rt;
using namespace mxrc::core::monitoring;

class RTMetricsTest : public ::testing::Test {
protected:
    std::shared_ptr<MetricsCollector> collector_;
    std::unique_ptr<RTMetrics> metrics_;

    void SetUp() override {
        collector_ = std::make_shared<MetricsCollector>();
        metrics_ = std::make_unique<RTMetrics>(collector_);
    }
};

// ============================================================================
// Cycle Duration Tests
// ============================================================================

TEST_F(RTMetricsTest, RecordMinorCycleDuration) {
    metrics_->recordMinorCycleDuration(0.001);  // 1ms
    metrics_->recordMinorCycleDuration(0.0015); // 1.5ms
    metrics_->recordMinorCycleDuration(0.0008); // 0.8ms

    std::string output = collector_->exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("rt_cycle_duration_seconds"));
    EXPECT_NE(std::string::npos, output.find("type=\"minor\""));
    EXPECT_NE(std::string::npos, output.find("_count"));
    EXPECT_NE(std::string::npos, output.find("_sum"));
}

TEST_F(RTMetricsTest, RecordMajorCycleDuration) {
    metrics_->recordMajorCycleDuration(0.010);  // 10ms
    metrics_->recordMajorCycleDuration(0.012);  // 12ms

    std::string output = collector_->exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("rt_cycle_duration_seconds"));
    EXPECT_NE(std::string::npos, output.find("type=\"major\""));
}

TEST_F(RTMetricsTest, MinorAndMajorCycleSeparate) {
    metrics_->recordMinorCycleDuration(0.001);
    metrics_->recordMajorCycleDuration(0.010);

    std::string output = collector_->exportPrometheus();

    // Both minor and major should be present
    EXPECT_NE(std::string::npos, output.find("type=\"minor\""));
    EXPECT_NE(std::string::npos, output.find("type=\"major\""));
}

// ============================================================================
// Jitter Tests
// ============================================================================

TEST_F(RTMetricsTest, RecordCycleJitter) {
    metrics_->recordCycleJitter(0.00001);  // 10μs
    metrics_->recordCycleJitter(0.00005);  // 50μs
    metrics_->recordCycleJitter(0.0001);   // 100μs

    std::string output = collector_->exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("rt_cycle_jitter_seconds"));
    EXPECT_NE(std::string::npos, output.find("_count"));
}

// ============================================================================
// Deadline Miss Tests
// ============================================================================

TEST_F(RTMetricsTest, IncrementDeadlineMisses) {
    metrics_->incrementDeadlineMisses();
    metrics_->incrementDeadlineMisses();
    metrics_->incrementDeadlineMisses();

    std::string output = collector_->exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("rt_deadline_misses_total"));
    EXPECT_NE(std::string::npos, output.find(" 3"));
}

TEST_F(RTMetricsTest, DeadlineMissesStartAtZero) {
    std::string output = collector_->exportPrometheus();

    // Counter should be present with initial value 0
    EXPECT_NE(std::string::npos, output.find("rt_deadline_misses_total"));
    EXPECT_NE(std::string::npos, output.find(" 0"));
}

// ============================================================================
// State Machine Tests
// ============================================================================

TEST_F(RTMetricsTest, UpdateState) {
    metrics_->updateState(RTState::INIT);
    std::string output1 = collector_->exportPrometheus();
    EXPECT_NE(std::string::npos, output1.find("rt_state 0.000000"));

    metrics_->updateState(RTState::READY);
    std::string output2 = collector_->exportPrometheus();
    EXPECT_NE(std::string::npos, output2.find("rt_state 1.000000"));

    metrics_->updateState(RTState::RUNNING);
    std::string output3 = collector_->exportPrometheus();
    EXPECT_NE(std::string::npos, output3.find("rt_state 2.000000"));

    metrics_->updateState(RTState::SAFE_MODE);
    std::string output4 = collector_->exportPrometheus();
    EXPECT_NE(std::string::npos, output4.find("rt_state 3.000000"));

    metrics_->updateState(RTState::SHUTDOWN);
    std::string output5 = collector_->exportPrometheus();
    EXPECT_NE(std::string::npos, output5.find("rt_state 4.000000"));
}

TEST_F(RTMetricsTest, IncrementStateTransitions) {
    for (int i = 0; i < 5; ++i) {
        metrics_->incrementStateTransitions();
    }

    std::string output = collector_->exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("rt_state_transitions_total"));
    EXPECT_NE(std::string::npos, output.find(" 5"));
}

TEST_F(RTMetricsTest, IncrementSafeModeEntries) {
    metrics_->incrementSafeModeEntries();
    metrics_->incrementSafeModeEntries();

    std::string output = collector_->exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("rt_safe_mode_entries_total"));
    EXPECT_NE(std::string::npos, output.find(" 2"));
}

TEST_F(RTMetricsTest, StateTransitionScenario) {
    // Simulate a typical state transition scenario
    metrics_->updateState(RTState::INIT);
    metrics_->incrementStateTransitions();

    metrics_->updateState(RTState::READY);
    metrics_->incrementStateTransitions();

    metrics_->updateState(RTState::RUNNING);
    metrics_->incrementStateTransitions();

    metrics_->updateState(RTState::SAFE_MODE);
    metrics_->incrementStateTransitions();
    metrics_->incrementSafeModeEntries();

    metrics_->updateState(RTState::RUNNING);
    metrics_->incrementStateTransitions();

    std::string output = collector_->exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("rt_state 2.000000"));  // RUNNING
    EXPECT_NE(std::string::npos, output.find("rt_state_transitions_total 5"));
    EXPECT_NE(std::string::npos, output.find("rt_safe_mode_entries_total 1"));
}

// ============================================================================
// Heartbeat Tests
// ============================================================================

TEST_F(RTMetricsTest, UpdateNonRTHeartbeatAlive) {
    metrics_->updateNonRTHeartbeatAlive(true);
    std::string output1 = collector_->exportPrometheus();
    EXPECT_NE(std::string::npos, output1.find("rt_nonrt_heartbeat_alive 1"));

    metrics_->updateNonRTHeartbeatAlive(false);
    std::string output2 = collector_->exportPrometheus();
    EXPECT_NE(std::string::npos, output2.find("rt_nonrt_heartbeat_alive 0"));
}

TEST_F(RTMetricsTest, UpdateNonRTHeartbeatTimeout) {
    metrics_->updateNonRTHeartbeatTimeout(5.0);

    std::string output = collector_->exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("rt_nonrt_heartbeat_timeout_seconds"));
    EXPECT_NE(std::string::npos, output.find("5.0"));
}

TEST_F(RTMetricsTest, HeartbeatScenario) {
    // Simulate heartbeat loss and recovery
    metrics_->updateNonRTHeartbeatAlive(true);
    metrics_->updateNonRTHeartbeatTimeout(5.0);

    std::string output1 = collector_->exportPrometheus();
    EXPECT_NE(std::string::npos, output1.find("rt_nonrt_heartbeat_alive 1"));

    // Heartbeat lost
    metrics_->updateNonRTHeartbeatAlive(false);
    metrics_->incrementSafeModeEntries();
    metrics_->updateState(RTState::SAFE_MODE);

    std::string output2 = collector_->exportPrometheus();
    EXPECT_NE(std::string::npos, output2.find("rt_nonrt_heartbeat_alive 0"));
    EXPECT_NE(std::string::npos, output2.find("rt_state 3.000000"));

    // Heartbeat recovered
    metrics_->updateNonRTHeartbeatAlive(true);
    metrics_->updateState(RTState::RUNNING);

    std::string output3 = collector_->exportPrometheus();
    EXPECT_NE(std::string::npos, output3.find("rt_nonrt_heartbeat_alive 1"));
    EXPECT_NE(std::string::npos, output3.find("rt_state 2.000000"));
}

// ============================================================================
// DataStore Tests
// ============================================================================

TEST_F(RTMetricsTest, IncrementDataStoreWrites) {
    metrics_->incrementDataStoreWrites("ROBOT_X");
    metrics_->incrementDataStoreWrites("ROBOT_X");
    metrics_->incrementDataStoreWrites("ROBOT_Y");

    std::string output = collector_->exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("rt_datastore_writes_total"));
    EXPECT_NE(std::string::npos, output.find("key=\"ROBOT_X\""));
    EXPECT_NE(std::string::npos, output.find("key=\"ROBOT_Y\""));
}

TEST_F(RTMetricsTest, IncrementDataStoreReads) {
    metrics_->incrementDataStoreReads("SENSOR_1");
    metrics_->incrementDataStoreReads("SENSOR_2");
    metrics_->incrementDataStoreReads("SENSOR_1");

    std::string output = collector_->exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("rt_datastore_reads_total"));
    EXPECT_NE(std::string::npos, output.find("key=\"SENSOR_1\""));
    EXPECT_NE(std::string::npos, output.find("key=\"SENSOR_2\""));
}

TEST_F(RTMetricsTest, IncrementDataStoreSeqlockRetries) {
    metrics_->incrementDataStoreSeqlockRetries("CONFIG_VALUE");

    std::string output = collector_->exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("rt_datastore_seqlock_retries_total"));
    EXPECT_NE(std::string::npos, output.find("key=\"CONFIG_VALUE\""));
}

TEST_F(RTMetricsTest, DataStoreMultipleKeys) {
    // Simulate realistic DataStore operations
    for (int i = 0; i < 100; ++i) {
        metrics_->incrementDataStoreWrites("ROBOT_X");
        metrics_->incrementDataStoreReads("ROBOT_X");
    }

    for (int i = 0; i < 50; ++i) {
        metrics_->incrementDataStoreWrites("ROBOT_Y");
        metrics_->incrementDataStoreReads("ROBOT_Y");
    }

    metrics_->incrementDataStoreSeqlockRetries("ROBOT_X");
    metrics_->incrementDataStoreSeqlockRetries("ROBOT_X");

    std::string output = collector_->exportPrometheus();

    // Verify all keys are present with correct labels
    EXPECT_NE(std::string::npos, output.find("rt_datastore_writes_total{key=\"ROBOT_X\"} 100"));
    EXPECT_NE(std::string::npos, output.find("rt_datastore_reads_total{key=\"ROBOT_X\"} 100"));
    EXPECT_NE(std::string::npos, output.find("rt_datastore_writes_total{key=\"ROBOT_Y\"} 50"));
    EXPECT_NE(std::string::npos, output.find("rt_datastore_seqlock_retries_total{key=\"ROBOT_X\"} 2"));
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(RTMetricsTest, FullCycleMetrics) {
    // Simulate a complete RT cycle with all metrics
    metrics_->updateState(RTState::RUNNING);
    metrics_->recordMinorCycleDuration(0.0009);
    metrics_->recordCycleJitter(0.00002);
    metrics_->updateNonRTHeartbeatAlive(true);
    metrics_->incrementDataStoreWrites("ROBOT_X");
    metrics_->incrementDataStoreReads("SENSOR_1");

    std::string output = collector_->exportPrometheus();

    // Verify all metric types are present
    EXPECT_NE(std::string::npos, output.find("rt_state"));
    EXPECT_NE(std::string::npos, output.find("rt_cycle_duration_seconds"));
    EXPECT_NE(std::string::npos, output.find("rt_cycle_jitter_seconds"));
    EXPECT_NE(std::string::npos, output.find("rt_nonrt_heartbeat_alive"));
    EXPECT_NE(std::string::npos, output.find("rt_datastore_writes_total"));
    EXPECT_NE(std::string::npos, output.find("rt_datastore_reads_total"));
}

TEST_F(RTMetricsTest, PerformanceUnderLoad) {
    // Simulate high-frequency metric updates
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 10000; ++i) {
        metrics_->recordMinorCycleDuration(0.001);
        metrics_->incrementDataStoreWrites("KEY_" + std::to_string(i % 10));
        if (i % 100 == 0) {
            metrics_->incrementStateTransitions();
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should complete in reasonable time (< 1 second)
    EXPECT_LT(duration, 1000);

    std::string output = collector_->exportPrometheus();
    EXPECT_FALSE(output.empty());
}

TEST_F(RTMetricsTest, GetCollector) {
    auto retrieved_collector = metrics_->getCollector();

    EXPECT_EQ(collector_, retrieved_collector);
    EXPECT_NE(nullptr, retrieved_collector);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(RTMetricsTest, ZeroValues) {
    metrics_->recordMinorCycleDuration(0.0);
    metrics_->recordCycleJitter(0.0);
    metrics_->updateNonRTHeartbeatTimeout(0.0);

    std::string output = collector_->exportPrometheus();

    // Should handle zero values correctly
    EXPECT_NE(std::string::npos, output.find("rt_cycle_duration_seconds"));
}

TEST_F(RTMetricsTest, VerySmallDurations) {
    metrics_->recordMinorCycleDuration(0.000001);  // 1μs
    metrics_->recordCycleJitter(0.00000001);       // 10ns

    std::string output = collector_->exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("rt_cycle_duration_seconds"));
    EXPECT_NE(std::string::npos, output.find("rt_cycle_jitter_seconds"));
}

TEST_F(RTMetricsTest, VeryLargeDurations) {
    metrics_->recordMajorCycleDuration(1.0);  // 1 second
    metrics_->updateNonRTHeartbeatTimeout(3600.0);  // 1 hour

    std::string output = collector_->exportPrometheus();

    EXPECT_NE(std::string::npos, output.find("rt_cycle_duration_seconds"));
    EXPECT_NE(std::string::npos, output.find("rt_nonrt_heartbeat_timeout_seconds"));
}
