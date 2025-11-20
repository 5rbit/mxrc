// MetricsServer_test.cpp
// Copyright (C) 2025 MXRC Project

#include "core/monitoring/MetricsServer.h"
#include "core/monitoring/MetricsCollector.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

using namespace mxrc::core::monitoring;

class MetricsServerTest : public ::testing::Test {
protected:
    std::shared_ptr<MetricsCollector> collector_;
    uint16_t test_port_ = 19100;  // Use non-standard port for testing

    void SetUp() override {
        collector_ = std::make_shared<MetricsCollector>();
    }

    std::string sendHttpRequest(uint16_t port, const std::string& path) {
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

        std::string request = "GET " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
        send(sock, request.c_str(), request.size(), 0);

        char buffer[4096];
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
// Server Lifecycle Tests
// ============================================================================

TEST_F(MetricsServerTest, ServerStartStop) {
    MetricsServer server(collector_, test_port_);

    EXPECT_FALSE(server.isRunning());

    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

    server.stop();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(MetricsServerTest, ServerStartTwice) {
    MetricsServer server(collector_, test_port_);

    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

    // Starting again should return false (already running)
    EXPECT_FALSE(server.start());
    EXPECT_TRUE(server.isRunning());

    server.stop();
}

TEST_F(MetricsServerTest, ServerStopBeforeStart) {
    MetricsServer server(collector_, test_port_);

    // Stopping before starting should be safe
    server.stop();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(MetricsServerTest, ServerStopTwice) {
    MetricsServer server(collector_, test_port_);

    EXPECT_TRUE(server.start());
    server.stop();
    EXPECT_FALSE(server.isRunning());

    // Stopping again should be safe
    server.stop();
    EXPECT_FALSE(server.isRunning());
}

// ============================================================================
// HTTP Request Tests
// ============================================================================

TEST_F(MetricsServerTest, GetMetricsEndpoint) {
    collector_->incrementCounter("test_counter", {}, 42);

    MetricsServer server(collector_, test_port_);
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string response = sendHttpRequest(test_port_, "/metrics");

    EXPECT_NE(std::string::npos, response.find("HTTP/1.1 200 OK"));
    EXPECT_NE(std::string::npos, response.find("Content-Type: text/plain"));
    EXPECT_NE(std::string::npos, response.find("test_counter 42"));

    server.stop();
}

TEST_F(MetricsServerTest, GetRootEndpoint) {
    MetricsServer server(collector_, test_port_);
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string response = sendHttpRequest(test_port_, "/");

    EXPECT_NE(std::string::npos, response.find("HTTP/1.1 200 OK"));
    EXPECT_NE(std::string::npos, response.find("Content-Type: text/html"));
    EXPECT_NE(std::string::npos, response.find("MXRC Metrics Server"));

    server.stop();
}

TEST_F(MetricsServerTest, Get404NotFound) {
    MetricsServer server(collector_, test_port_);
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string response = sendHttpRequest(test_port_, "/unknown");

    EXPECT_NE(std::string::npos, response.find("HTTP/1.1 404 Not Found"));

    server.stop();
}

// ============================================================================
// Metrics Content Tests
// ============================================================================

TEST_F(MetricsServerTest, MetricsEndpointReturnsPrometheusFormat) {
    collector_->incrementCounter("http_requests_total", {{"method", "GET"}}, 100);
    collector_->setGauge("cpu_usage_percent", 75.5, {});

    auto histogram = collector_->getOrCreateHistogram(
        "request_duration_seconds",
        {},
        {0.1, 0.5, 1.0}
    );
    histogram->observe(0.3);
    histogram->observe(0.7);

    MetricsServer server(collector_, test_port_);
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string response = sendHttpRequest(test_port_, "/metrics");

    // Verify Prometheus format
    EXPECT_NE(std::string::npos, response.find("# TYPE http_requests_total counter"));
    EXPECT_NE(std::string::npos, response.find("http_requests_total{method=\"GET\"} 100"));
    EXPECT_NE(std::string::npos, response.find("# TYPE cpu_usage_percent gauge"));
    EXPECT_NE(std::string::npos, response.find("cpu_usage_percent 75.5"));
    EXPECT_NE(std::string::npos, response.find("# TYPE request_duration_seconds histogram"));
    EXPECT_NE(std::string::npos, response.find("_bucket"));
    EXPECT_NE(std::string::npos, response.find("_sum"));
    EXPECT_NE(std::string::npos, response.find("_count"));

    server.stop();
}

TEST_F(MetricsServerTest, MetricsUpdateDynamically) {
    MetricsServer server(collector_, test_port_);
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // First request
    collector_->incrementCounter("requests", {}, 10);
    std::string response1 = sendHttpRequest(test_port_, "/metrics");
    EXPECT_NE(std::string::npos, response1.find("requests 10"));

    // Update metrics
    collector_->incrementCounter("requests", {}, 5);

    // Second request should show updated value
    std::string response2 = sendHttpRequest(test_port_, "/metrics");
    EXPECT_NE(std::string::npos, response2.find("requests 15"));

    server.stop();
}

// ============================================================================
// Concurrent Request Tests
// ============================================================================

TEST_F(MetricsServerTest, ConcurrentRequests) {
    MetricsServer server(collector_, test_port_);
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, &success_count]() {
            std::string response = sendHttpRequest(test_port_, "/metrics");
            if (response.find("HTTP/1.1 200 OK") != std::string::npos) {
                success_count++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(num_threads, success_count.load());

    server.stop();
}

TEST_F(MetricsServerTest, RapidSequentialRequests) {
    MetricsServer server(collector_, test_port_);
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int success_count = 0;
    for (int i = 0; i < 50; ++i) {
        std::string response = sendHttpRequest(test_port_, "/metrics");
        if (response.find("HTTP/1.1 200 OK") != std::string::npos) {
            success_count++;
        }
    }

    EXPECT_EQ(50, success_count);

    server.stop();
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(MetricsServerTest, EmptyMetricsCollector) {
    // No metrics added
    MetricsServer server(collector_, test_port_);
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string response = sendHttpRequest(test_port_, "/metrics");

    EXPECT_NE(std::string::npos, response.find("HTTP/1.1 200 OK"));
    // Should return valid response even with no metrics

    server.stop();
}

TEST_F(MetricsServerTest, VeryLargeMetricsOutput) {
    // Create many metrics
    for (int i = 0; i < 100; ++i) {
        collector_->incrementCounter("metric_" + std::to_string(i), {}, i);
    }

    MetricsServer server(collector_, test_port_);
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string response = sendHttpRequest(test_port_, "/metrics");

    EXPECT_NE(std::string::npos, response.find("HTTP/1.1 200 OK"));
    EXPECT_NE(std::string::npos, response.find("metric_0"));
    EXPECT_NE(std::string::npos, response.find("metric_99"));

    server.stop();
}

TEST_F(MetricsServerTest, ServerRestartability) {
    MetricsServer server(collector_, test_port_);

    // Start, stop, start again
    EXPECT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string response1 = sendHttpRequest(test_port_, "/metrics");
    EXPECT_NE(std::string::npos, response1.find("HTTP/1.1 200 OK"));

    server.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string response2 = sendHttpRequest(test_port_, "/metrics");
    EXPECT_NE(std::string::npos, response2.find("HTTP/1.1 200 OK"));

    server.stop();
}

TEST_F(MetricsServerTest, InvalidPort) {
    // Port 0 should be invalid or auto-assigned
    MetricsServer server(collector_, 0);
    // Implementation may vary - just ensure it doesn't crash
    server.start();
    server.stop();
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(MetricsServerTest, ResponseTime) {
    collector_->incrementCounter("test", {}, 1);

    MetricsServer server(collector_, test_port_);
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto start = std::chrono::steady_clock::now();
    std::string response = sendHttpRequest(test_port_, "/metrics");
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_NE(std::string::npos, response.find("HTTP/1.1 200 OK"));
    EXPECT_LT(duration, 100);  // Should respond within 100ms

    server.stop();
}

TEST_F(MetricsServerTest, MemoryStability) {
    MetricsServer server(collector_, test_port_);
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Make many requests to check for memory leaks
    for (int i = 0; i < 1000; ++i) {
        sendHttpRequest(test_port_, "/metrics");
    }

    // If we get here without crashes, memory is stable
    EXPECT_TRUE(server.isRunning());

    server.stop();
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST_F(MetricsServerTest, DestructorStopsServer) {
    {
        MetricsServer server(collector_, test_port_);
        EXPECT_TRUE(server.start());
        EXPECT_TRUE(server.isRunning());
        // Destructor should stop the server
    }

    // Should be able to start a new server on the same port
    MetricsServer server2(collector_, test_port_);
    EXPECT_TRUE(server2.start());
    server2.stop();
}
