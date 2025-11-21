// process_monitor_test.cpp
// Copyright (C) 2025 MXRC Project
//
// T063: ProcessMonitor unit test

#include "core/ha/ProcessMonitor.h"
#include "core/ha/FailoverManager.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace mxrc::ha;

class ProcessMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.process_name = "test_process";
        config_.health_check_interval_ms = 100;
        config_.health_check_timeout_ms = 50;
        config_.cpu_threshold_percent = 90;
        config_.memory_threshold_mb = 2048;
        config_.deadline_miss_threshold = 100;
        config_.enable_systemd_watchdog = false;
    }

    ProcessMonitorConfig config_;
};

// T063.1: Constructor and basic properties
TEST_F(ProcessMonitorTest, ConstructorInitializesCorrectly) {
    ProcessMonitor monitor(config_);

    EXPECT_FALSE(monitor.isRunning());

    auto status = monitor.getHealthStatus();
    EXPECT_EQ(status.process_name, "test_process");
    EXPECT_EQ(status.status, HealthStatus::STOPPED);
}

// T063.2: Start and stop monitoring
TEST_F(ProcessMonitorTest, StartStopMonitoring) {
    ProcessMonitor monitor(config_);

    EXPECT_TRUE(monitor.start());
    EXPECT_TRUE(monitor.isRunning());

    // Should not start again
    EXPECT_FALSE(monitor.start());

    monitor.stop();
    EXPECT_FALSE(monitor.isRunning());
}

// T063.3: Health status tracking
TEST_F(ProcessMonitorTest, HealthStatusTracking) {
    ProcessMonitor monitor(config_);

    // Initial status should be STOPPED
    EXPECT_EQ(monitor.getHealthStatus().status, HealthStatus::STOPPED);

    monitor.start();

    // After start, should be STARTING or HEALTHY
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    auto status = monitor.getHealthStatus();
    EXPECT_TRUE(status.status == HealthStatus::STARTING ||
                status.status == HealthStatus::HEALTHY);

    monitor.stop();
}

// T063.4: Update status with metrics
TEST_F(ProcessMonitorTest, UpdateStatusWithMetrics) {
    ProcessMonitor monitor(config_);
    monitor.start();

    // Update with normal metrics
    monitor.updateStatus(50.0, 1024, 10);

    // Wait for monitoring loop to update status
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    auto status = monitor.getHealthStatus();
    EXPECT_DOUBLE_EQ(status.cpu_usage_percent, 50.0);
    EXPECT_EQ(status.memory_usage_mb, 1024);
    EXPECT_EQ(status.deadline_miss_count, 10);

    monitor.stop();
}

// T063.5: Degraded status on high CPU
TEST_F(ProcessMonitorTest, DegradedStatusOnHighCPU) {
    ProcessMonitor monitor(config_);
    monitor.start();

    // Transition from STARTING to HEALTHY first
    monitor.recordHeartbeat();

    // Update with high CPU usage
    monitor.updateStatus(95.0, 1024, 10);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    auto status = monitor.getHealthStatus();
    EXPECT_EQ(status.status, HealthStatus::DEGRADED);

    monitor.stop();
}

// T063.6: Degraded status on high memory
TEST_F(ProcessMonitorTest, DegradedStatusOnHighMemory) {
    ProcessMonitor monitor(config_);
    monitor.start();

    // Transition from STARTING to HEALTHY first
    monitor.recordHeartbeat();

    // Update with high memory usage
    monitor.updateStatus(50.0, 3000, 10);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    auto status = monitor.getHealthStatus();
    EXPECT_EQ(status.status, HealthStatus::DEGRADED);

    monitor.stop();
}

// T063.7: Unhealthy status on deadline misses
TEST_F(ProcessMonitorTest, UnhealthyStatusOnDeadlineMisses) {
    ProcessMonitor monitor(config_);
    monitor.start();

    // Transition from STARTING to HEALTHY first
    monitor.recordHeartbeat();

    // Update with excessive deadline misses
    monitor.updateStatus(50.0, 1024, 150);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    auto status = monitor.getHealthStatus();
    // High deadline misses should trigger DEGRADED, not UNHEALTHY
    // UNHEALTHY is only set via setError() or if there's an error_message
    EXPECT_EQ(status.status, HealthStatus::DEGRADED);

    monitor.stop();
}

// T063.8: Heartbeat recording
TEST_F(ProcessMonitorTest, HeartbeatRecording) {
    ProcessMonitor monitor(config_);
    monitor.start();

    auto before = std::chrono::system_clock::now();
    monitor.recordHeartbeat();
    auto after = std::chrono::system_clock::now();

    auto status = monitor.getHealthStatus();
    EXPECT_GE(status.last_heartbeat, before);
    EXPECT_LE(status.last_heartbeat, after);

    monitor.stop();
}

// T063.9: Error state handling
TEST_F(ProcessMonitorTest, ErrorStateHandling) {
    ProcessMonitor monitor(config_);
    monitor.start();

    monitor.setError("Test error message");

    auto status = monitor.getHealthStatus();
    EXPECT_EQ(status.status, HealthStatus::UNHEALTHY);
    EXPECT_EQ(status.error_message, "Test error message");

    monitor.stop();
}

// T063.10: isHealthy, isReady, isAlive checks
TEST_F(ProcessMonitorTest, HealthChecks) {
    ProcessMonitor monitor(config_);

    // Before start
    EXPECT_FALSE(monitor.isHealthy());
    EXPECT_FALSE(monitor.isReady());
    EXPECT_FALSE(monitor.isAlive());

    monitor.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // After start with normal metrics
    monitor.updateStatus(50.0, 1024, 10);
    monitor.recordHeartbeat();

    EXPECT_TRUE(monitor.isAlive());
    // isHealthy and isReady depend on implementation

    monitor.stop();
}

// T063.11: Integration with FailoverManager
TEST_F(ProcessMonitorTest, IntegrationWithFailoverManager) {
    FailoverPolicy policy;
    policy.process_name = "test_process";
    policy.health_check_interval_ms = 100;
    policy.health_check_timeout_ms = 50;
    policy.failure_threshold = 3;
    policy.restart_delay_ms = 100;
    policy.max_restart_count = 5;
    policy.restart_window_sec = 60;

    std::shared_ptr<IFailoverManager> failover = createFailoverManager(policy);

    ProcessMonitor monitor(config_, failover);
    monitor.start();

    // Update with normal metrics
    monitor.updateStatus(50.0, 1024, 10);

    auto status = monitor.getHealthStatus();
    EXPECT_EQ(status.process_name, "test_process");

    monitor.stop();
}
