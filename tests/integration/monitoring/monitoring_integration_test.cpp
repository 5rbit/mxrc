// monitoring_integration_test.cpp
// Copyright (C) 2025 MXRC Project

#include "core/monitoring/MetricsCollector.h"
#include "core/monitoring/MetricsServer.h"
#include "core/rt/RTMetrics.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

using namespace mxrc::core::monitoring;
using namespace mxrc::core::rt;

class MonitoringIntegrationTest : public ::testing::Test {
protected:
    std::shared_ptr<MetricsCollector> collector_;
    uint16_t test_port_ = 19200;

    void SetUp() override {
        collector_ = std::make_shared<MetricsCollector>();
    }

    std::string fetchMetrics(uint16_t port) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            return "";
        }

        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(sock);
            return "";
        }

        std::string request = "GET /metrics HTTP/1.1\r\nHost: localhost\r\n\r\n";
        send(sock, request.c_str(), request.size(), 0);

        char buffer[8192];
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        close(sock);

        if (bytes_received <= 0) {
            return "";
        }

        buffer[bytes_received] = '\0';
        return std::string(buffer);
    }
};

// ============================================================================
// End-to-End Workflow Tests
// ============================================================================

TEST_F(MonitoringIntegrationTest, CompleteRTMonitoringWorkflow) {
    // Setup: Create RTMetrics with server
    auto rt_metrics = std::make_unique<RTMetrics>(collector_);
    MetricsServer server(collector_, test_port_);

    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Simulate RT process operation
    rt_metrics->updateState(RTState::INIT);
    rt_metrics->incrementStateTransitions();

    rt_metrics->updateState(RTState::READY);
    rt_metrics->incrementStateTransitions();

    rt_metrics->updateState(RTState::RUNNING);
    rt_metrics->incrementStateTransitions();
    rt_metrics->updateNonRTHeartbeatAlive(true);
    rt_metrics->updateNonRTHeartbeatTimeout(5.0);

    // Record some cycles
    for (int i = 0; i < 10; ++i) {
        rt_metrics->recordMinorCycleDuration(0.0009 + i * 0.0001);
        rt_metrics->incrementDataStoreWrites("ROBOT_STATE");
        rt_metrics->incrementDataStoreReads("SENSOR_DATA");
    }

    rt_metrics->recordCycleJitter(0.00003);

    // Fetch metrics via HTTP
    std::string response = fetchMetrics(test_port_);

    // Verify all RT metrics are present
    EXPECT_NE(std::string::npos, response.find("rt_state 2.000000"));  // RUNNING
    EXPECT_NE(std::string::npos, response.find("rt_state_transitions_total 3"));
    EXPECT_NE(std::string::npos, response.find("rt_nonrt_heartbeat_alive 1"));
    EXPECT_NE(std::string::npos, response.find("rt_nonrt_heartbeat_timeout_seconds 5"));
    EXPECT_NE(std::string::npos, response.find("rt_cycle_duration_seconds"));
    EXPECT_NE(std::string::npos, response.find("type=\"minor\""));
    EXPECT_NE(std::string::npos, response.find("rt_cycle_jitter_seconds"));
    EXPECT_NE(std::string::npos, response.find("rt_datastore_writes_total{key=\"ROBOT_STATE\"} 10"));
    EXPECT_NE(std::string::npos, response.find("rt_datastore_reads_total{key=\"SENSOR_DATA\"} 10"));

    server.stop();
}

TEST_F(MonitoringIntegrationTest, RTSafeModeScenario) {
    auto rt_metrics = std::make_unique<RTMetrics>(collector_);
    MetricsServer server(collector_, test_port_);

    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Normal operation
    rt_metrics->updateState(RTState::RUNNING);
    rt_metrics->updateNonRTHeartbeatAlive(true);

    // Heartbeat lost -> SAFE_MODE
    rt_metrics->updateNonRTHeartbeatAlive(false);
    rt_metrics->updateState(RTState::SAFE_MODE);
    rt_metrics->incrementSafeModeEntries();
    rt_metrics->incrementStateTransitions();

    // Record some operations in SAFE_MODE
    for (int i = 0; i < 5; ++i) {
        rt_metrics->recordMinorCycleDuration(0.001);
        rt_metrics->incrementDeadlineMisses();
    }

    // Heartbeat recovered -> back to RUNNING
    rt_metrics->updateNonRTHeartbeatAlive(true);
    rt_metrics->updateState(RTState::RUNNING);
    rt_metrics->incrementStateTransitions();

    // Verify metrics
    std::string response = fetchMetrics(test_port_);

    EXPECT_NE(std::string::npos, response.find("rt_state 2"));  // Back to RUNNING
    EXPECT_NE(std::string::npos, response.find("rt_safe_mode_entries_total 1"));
    EXPECT_NE(std::string::npos, response.find("rt_deadline_misses_total 5"));
    EXPECT_NE(std::string::npos, response.find("rt_nonrt_heartbeat_alive 1"));

    server.stop();
}

TEST_F(MonitoringIntegrationTest, DataStoreContentionSimulation) {
    auto rt_metrics = std::make_unique<RTMetrics>(collector_);
    MetricsServer server(collector_, test_port_);

    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Simulate heavy DataStore usage with contention
    const std::vector<std::string> keys = {"ROBOT_X", "ROBOT_Y", "ROBOT_Z", "SENSOR_1", "SENSOR_2"};

    for (int cycle = 0; cycle < 100; ++cycle) {
        for (const auto& key : keys) {
            rt_metrics->incrementDataStoreWrites(key);
            rt_metrics->incrementDataStoreReads(key);

            // Simulate occasional seqlock retries
            if (cycle % 10 == 0) {
                rt_metrics->incrementDataStoreSeqlockRetries(key);
            }
        }
    }

    // Verify metrics
    std::string response = fetchMetrics(test_port_);

    for (const auto& key : keys) {
        EXPECT_NE(std::string::npos, response.find("rt_datastore_writes_total{key=\"" + key + "\"} 100"));
        EXPECT_NE(std::string::npos, response.find("rt_datastore_reads_total{key=\"" + key + "\"} 100"));
        EXPECT_NE(std::string::npos, response.find("rt_datastore_seqlock_retries_total{key=\"" + key + "\"} 10"));
    }

    server.stop();
}

// ============================================================================
// Multi-Server Tests (RT + Non-RT)
// ============================================================================

TEST_F(MonitoringIntegrationTest, DualProcessMetricsServers) {
    // RT process metrics
    auto rt_collector = std::make_shared<MetricsCollector>();
    auto rt_metrics = std::make_unique<RTMetrics>(rt_collector);
    MetricsServer rt_server(rt_collector, 19201);

    // Non-RT process metrics
    auto nonrt_collector = std::make_shared<MetricsCollector>();
    nonrt_collector->incrementCounter("nonrt_tasks_total", {{"status", "completed"}}, 100);
    nonrt_collector->observeHistogram("nonrt_task_duration_seconds", 0.5, {});
    MetricsServer nonrt_server(nonrt_collector, 19202);

    ASSERT_TRUE(rt_server.start());
    ASSERT_TRUE(nonrt_server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // RT metrics
    rt_metrics->updateState(RTState::RUNNING);
    rt_metrics->recordMinorCycleDuration(0.001);

    // Fetch from both servers
    std::string rt_response = fetchMetrics(19201);
    std::string nonrt_response = fetchMetrics(19202);

    // Verify RT metrics
    EXPECT_NE(std::string::npos, rt_response.find("rt_state"));
    EXPECT_NE(std::string::npos, rt_response.find("rt_cycle_duration_seconds"));

    // Verify Non-RT metrics
    EXPECT_NE(std::string::npos, nonrt_response.find("nonrt_tasks_total"));
    EXPECT_NE(std::string::npos, nonrt_response.find("nonrt_task_duration_seconds"));

    // Verify separation (RT metrics not in Non-RT, vice versa)
    EXPECT_EQ(std::string::npos, rt_response.find("nonrt_tasks_total"));
    EXPECT_EQ(std::string::npos, nonrt_response.find("rt_state"));

    rt_server.stop();
    nonrt_server.stop();
}

// ============================================================================
// Performance and Stress Tests
// ============================================================================

TEST_F(MonitoringIntegrationTest, HighFrequencyMetricsUpdates) {
    auto rt_metrics = std::make_unique<RTMetrics>(collector_);
    MetricsServer server(collector_, test_port_);

    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto start = std::chrono::steady_clock::now();

    // Simulate 10,000 RT cycles
    for (int i = 0; i < 10000; ++i) {
        rt_metrics->recordMinorCycleDuration(0.001);
        rt_metrics->incrementDataStoreWrites("KEY_" + std::to_string(i % 10));

        if (i % 100 == 0) {
            rt_metrics->incrementStateTransitions();
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should complete quickly (< 1 second)
    EXPECT_LT(duration, 1000);

    // Metrics should still be accessible
    std::string response = fetchMetrics(test_port_);
    EXPECT_NE(std::string::npos, response.find("rt_cycle_duration_seconds"));

    server.stop();
}

TEST_F(MonitoringIntegrationTest, ConcurrentMetricsUpdateAndFetch) {
    auto rt_metrics = std::make_unique<RTMetrics>(collector_);
    MetricsServer server(collector_, test_port_);

    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::atomic<bool> running{true};
    std::atomic<int> fetch_success{0};

    // Thread 1: Continuously update metrics
    std::thread updater([&]() {
        int count = 0;
        while (running && count < 1000) {
            rt_metrics->recordMinorCycleDuration(0.001);
            rt_metrics->incrementDataStoreWrites("DATA");
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            count++;
        }
    });

    // Thread 2-5: Continuously fetch metrics
    std::vector<std::thread> fetchers;
    for (int i = 0; i < 4; ++i) {
        fetchers.emplace_back([&]() {
            for (int j = 0; j < 50; ++j) {
                std::string response = fetchMetrics(test_port_);
                if (response.find("HTTP/1.1 200 OK") != std::string::npos) {
                    fetch_success++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    updater.join();
    for (auto& fetcher : fetchers) {
        fetcher.join();
    }
    running = false;

    // All fetches should succeed
    EXPECT_EQ(200, fetch_success.load());

    server.stop();
}

// ============================================================================
// Real-world Scenario Tests
// ============================================================================

TEST_F(MonitoringIntegrationTest, FullLifecycleSimulation) {
    auto rt_metrics = std::make_unique<RTMetrics>(collector_);
    MetricsServer server(collector_, test_port_);

    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 1. INIT phase
    rt_metrics->updateState(RTState::INIT);
    rt_metrics->incrementStateTransitions();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 2. READY phase
    rt_metrics->updateState(RTState::READY);
    rt_metrics->incrementStateTransitions();
    rt_metrics->updateNonRTHeartbeatAlive(true);
    rt_metrics->updateNonRTHeartbeatTimeout(5.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 3. RUNNING phase - normal operation
    rt_metrics->updateState(RTState::RUNNING);
    rt_metrics->incrementStateTransitions();

    for (int i = 0; i < 100; ++i) {
        rt_metrics->recordMinorCycleDuration(0.0009 + (i % 10) * 0.00001);
        rt_metrics->recordCycleJitter(0.00001 + (i % 5) * 0.000005);
        rt_metrics->incrementDataStoreWrites("ROBOT_STATE");
        rt_metrics->incrementDataStoreReads("SENSOR_DATA");

        if (i % 10 == 0) {
            rt_metrics->recordMajorCycleDuration(0.010);
        }
    }

    // 4. Brief SAFE_MODE
    rt_metrics->updateNonRTHeartbeatAlive(false);
    rt_metrics->updateState(RTState::SAFE_MODE);
    rt_metrics->incrementSafeModeEntries();
    rt_metrics->incrementStateTransitions();

    for (int i = 0; i < 10; ++i) {
        rt_metrics->recordMinorCycleDuration(0.001);
        rt_metrics->incrementDeadlineMisses();
    }

    // 5. Recovery to RUNNING
    rt_metrics->updateNonRTHeartbeatAlive(true);
    rt_metrics->updateState(RTState::RUNNING);
    rt_metrics->incrementStateTransitions();

    for (int i = 0; i < 50; ++i) {
        rt_metrics->recordMinorCycleDuration(0.0009);
    }

    // 6. SHUTDOWN
    rt_metrics->updateState(RTState::SHUTDOWN);
    rt_metrics->incrementStateTransitions();

    // Verify final state
    std::string response = fetchMetrics(test_port_);

    EXPECT_NE(std::string::npos, response.find("rt_state 4.000000"));  // SHUTDOWN
    EXPECT_NE(std::string::npos, response.find("rt_state_transitions_total 6"));
    EXPECT_NE(std::string::npos, response.find("rt_safe_mode_entries_total 1"));
    EXPECT_NE(std::string::npos, response.find("rt_deadline_misses_total 10"));
    EXPECT_NE(std::string::npos, response.find("rt_datastore_writes_total{key=\"ROBOT_STATE\"} 100"));

    server.stop();
}

TEST_F(MonitoringIntegrationTest, PrometheusScrapingCompatibility) {
    auto rt_metrics = std::make_unique<RTMetrics>(collector_);
    MetricsServer server(collector_, test_port_);

    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Generate various metrics
    rt_metrics->updateState(RTState::RUNNING);
    rt_metrics->recordMinorCycleDuration(0.001);
    rt_metrics->recordMajorCycleDuration(0.010);
    rt_metrics->recordCycleJitter(0.00002);
    rt_metrics->incrementDeadlineMisses();
    rt_metrics->updateNonRTHeartbeatAlive(true);
    rt_metrics->incrementDataStoreWrites("TEST_KEY");

    // Fetch metrics
    std::string response = fetchMetrics(test_port_);

    // Verify Prometheus text format compliance
    EXPECT_NE(std::string::npos, response.find("HTTP/1.1 200 OK"));
    EXPECT_NE(std::string::npos, response.find("Content-Type: text/plain"));

    // HELP and TYPE comments
    EXPECT_NE(std::string::npos, response.find("# HELP"));
    EXPECT_NE(std::string::npos, response.find("# TYPE"));

    // Metric types
    EXPECT_NE(std::string::npos, response.find("counter"));
    EXPECT_NE(std::string::npos, response.find("gauge"));
    EXPECT_NE(std::string::npos, response.find("histogram"));

    // Histogram suffixes
    EXPECT_NE(std::string::npos, response.find("_bucket"));
    EXPECT_NE(std::string::npos, response.find("_sum"));
    EXPECT_NE(std::string::npos, response.find("_count"));
    EXPECT_NE(std::string::npos, response.find("le="));

    server.stop();
}
