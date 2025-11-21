#include <gtest/gtest.h>
#include "core/rt/perf/PerfMonitor.h"
#include <thread>
#include <chrono>
#include <cmath>
#include <fstream>

using namespace mxrc::rt::perf;

class PerfMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        monitor_ = std::make_unique<PerfMonitor>();
    }

    void TearDown() override {
        monitor_.reset();
    }

    std::unique_ptr<PerfMonitor> monitor_;
};

// Test default configuration
TEST_F(PerfMonitorTest, DefaultConfig) {
    PerfMonitorConfig config;
    EXPECT_EQ(config.cycle_time_us, 1000);
    EXPECT_EQ(config.deadline_us, 1000);
    EXPECT_TRUE(config.enable_histogram);
    EXPECT_EQ(config.histogram_buckets, 100);
    EXPECT_EQ(config.sample_buffer_size, 10000);
    EXPECT_FALSE(config.enable_tracing);
}

// Test basic cycle tracking
TEST_F(PerfMonitorTest, BasicCycleTracking) {
    PerfMonitorConfig config;
    config.process_name = "test_process";
    config.cycle_time_us = 1000;
    config.deadline_us = 1000;
    config.enable_tracing = false;

    EXPECT_TRUE(monitor_->configure(config));

    // Execute a few cycles
    for (int i = 0; i < 10; ++i) {
        monitor_->startCycle();
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        monitor_->endCycle();
    }

    auto stats = monitor_->getStats();
    EXPECT_EQ(stats.total_cycles, 10);
    EXPECT_GT(stats.avg_latency, 0.0);
    EXPECT_GT(stats.min_latency, 0.0);
    EXPECT_GT(stats.max_latency, stats.min_latency);
}

// Test deadline tracking
TEST_F(PerfMonitorTest, DeadlineTracking) {
    PerfMonitorConfig config;
    config.deadline_us = 500;  // 500us deadline
    config.enable_tracing = false;

    EXPECT_TRUE(monitor_->configure(config));

    // Execute cycles that meet deadline
    for (int i = 0; i < 5; ++i) {
        monitor_->startCycle();
        std::this_thread::sleep_for(std::chrono::microseconds(100));  // Well under deadline
        monitor_->endCycle();
    }

    // Execute cycles that miss deadline
    for (int i = 0; i < 3; ++i) {
        monitor_->startCycle();
        std::this_thread::sleep_for(std::chrono::microseconds(600));  // Over deadline
        monitor_->endCycle();
    }

    auto stats = monitor_->getStats();
    EXPECT_EQ(stats.total_cycles, 8);
    EXPECT_EQ(stats.deadline_misses, 3);
    EXPECT_NEAR(stats.deadline_miss_rate, 37.5, 0.1);  // 3/8 = 37.5%
}

// Test didMissDeadline
TEST_F(PerfMonitorTest, DidMissDeadline) {
    PerfMonitorConfig config;
    config.deadline_us = 500;
    config.enable_tracing = false;

    EXPECT_TRUE(monitor_->configure(config));

    // Cycle under deadline
    monitor_->startCycle();
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    monitor_->endCycle();
    EXPECT_FALSE(monitor_->didMissDeadline());

    // Cycle over deadline
    monitor_->startCycle();
    std::this_thread::sleep_for(std::chrono::microseconds(600));
    monitor_->endCycle();
    EXPECT_TRUE(monitor_->didMissDeadline());
}

// Test statistics calculation
TEST_F(PerfMonitorTest, Statistics) {
    PerfMonitorConfig config;
    config.deadline_us = 1000;
    config.enable_tracing = false;

    EXPECT_TRUE(monitor_->configure(config));

    // Execute cycles with varying latencies
    std::vector<int> latencies = {100, 150, 200, 250, 300, 350, 400, 450, 500};

    for (int latency : latencies) {
        monitor_->startCycle();
        std::this_thread::sleep_for(std::chrono::microseconds(latency));
        monitor_->endCycle();
    }

    auto stats = monitor_->getStats();
    EXPECT_EQ(stats.total_cycles, 9);
    EXPECT_GT(stats.avg_latency, 200.0);  // Average should be around 300us
    EXPECT_LT(stats.avg_latency, 400.0);
    EXPECT_GT(stats.jitter, 0.0);          // Should have some jitter
}

// Test percentile calculation
TEST_F(PerfMonitorTest, Percentiles) {
    PerfMonitorConfig config;
    config.deadline_us = 10000;
    config.enable_tracing = false;

    EXPECT_TRUE(monitor_->configure(config));

    // Execute 100 cycles with known latencies
    for (int i = 1; i <= 100; ++i) {
        monitor_->startCycle();
        std::this_thread::sleep_for(std::chrono::microseconds(i * 10));
        monitor_->endCycle();
    }

    auto stats = monitor_->getStats();
    EXPECT_EQ(stats.total_cycles, 100);

    // Verify percentiles are in expected range
    // P50 should be around 500us (50th element * 10)
    // P95 should be around 950us (95th element * 10)
    // P99 should be around 990us (99th element * 10)
    // Note: Use relaxed bounds due to timing variations in sleep
    EXPECT_GT(stats.p50_latency, 400.0);
    EXPECT_LT(stats.p50_latency, 700.0);
    EXPECT_GT(stats.p95_latency, 850.0);
    EXPECT_LT(stats.p95_latency, 1200.0);
    EXPECT_GT(stats.p99_latency, 900.0);
    EXPECT_LT(stats.p99_latency, 1300.0);
}

// Test reset functionality
TEST_F(PerfMonitorTest, Reset) {
    PerfMonitorConfig config;
    EXPECT_TRUE(monitor_->configure(config));

    // Execute some cycles
    for (int i = 0; i < 5; ++i) {
        monitor_->startCycle();
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        monitor_->endCycle();
    }

    auto stats_before = monitor_->getStats();
    EXPECT_EQ(stats_before.total_cycles, 5);

    // Reset
    monitor_->reset();

    auto stats_after = monitor_->getStats();
    EXPECT_EQ(stats_after.total_cycles, 0);
    EXPECT_EQ(stats_after.deadline_misses, 0);
    EXPECT_EQ(stats_after.avg_latency, 0.0);
}

// Test histogram collection
TEST_F(PerfMonitorTest, Histogram) {
    PerfMonitorConfig config;
    config.deadline_us = 1000;
    config.enable_histogram = true;
    config.histogram_buckets = 10;
    config.enable_tracing = false;

    EXPECT_TRUE(monitor_->configure(config));

    // Execute cycles with varying latencies
    for (int i = 0; i < 20; ++i) {
        monitor_->startCycle();
        std::this_thread::sleep_for(std::chrono::microseconds(i * 50));
        monitor_->endCycle();
    }

    auto histogram = monitor_->getHistogram();
    EXPECT_EQ(histogram.size(), 10);

    // Sum of all buckets should equal total cycles
    uint64_t total_samples = 0;
    for (uint64_t count : histogram) {
        total_samples += count;
    }
    EXPECT_EQ(total_samples, 20);
}

// Test CycleGuard RAII
TEST_F(PerfMonitorTest, CycleGuard) {
    PerfMonitorConfig config;
    config.enable_tracing = false;
    EXPECT_TRUE(monitor_->configure(config));

    {
        CycleGuard guard(*monitor_);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        // endCycle() called automatically when guard goes out of scope
    }

    auto stats = monitor_->getStats();
    EXPECT_EQ(stats.total_cycles, 1);
    EXPECT_GT(stats.avg_latency, 0.0);
}

// Test multiple CycleGuards
TEST_F(PerfMonitorTest, MultipleCycleGuards) {
    PerfMonitorConfig config;
    config.enable_tracing = false;
    EXPECT_TRUE(monitor_->configure(config));

    for (int i = 0; i < 5; ++i) {
        CycleGuard guard(*monitor_);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    auto stats = monitor_->getStats();
    EXPECT_EQ(stats.total_cycles, 5);
}

// Test jitter calculation
TEST_F(PerfMonitorTest, JitterCalculation) {
    PerfMonitorConfig config;
    config.deadline_us = 10000;
    config.enable_tracing = false;

    EXPECT_TRUE(monitor_->configure(config));

    // Execute cycles with consistent latency (low jitter)
    for (int i = 0; i < 10; ++i) {
        monitor_->startCycle();
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        monitor_->endCycle();
    }

    auto stats_low_jitter = monitor_->getStats();
    double low_jitter = stats_low_jitter.jitter;

    monitor_->reset();

    // Execute cycles with varying latency (high jitter)
    std::vector<int> varying_latencies = {100, 900, 200, 800, 300, 700, 400, 600, 500, 550};
    for (int latency : varying_latencies) {
        monitor_->startCycle();
        std::this_thread::sleep_for(std::chrono::microseconds(latency));
        monitor_->endCycle();
    }

    auto stats_high_jitter = monitor_->getStats();
    double high_jitter = stats_high_jitter.jitter;

    // High jitter should be significantly larger than low jitter
    EXPECT_GT(high_jitter, low_jitter * 2);
}

// Test JSON config loading
TEST_F(PerfMonitorTest, LoadConfigFromJSON) {
    // Create test config file
    std::ofstream config_file("/tmp/test_perf_monitor.json");
    config_file << R"({
        "process_name": "test_rt_process",
        "cycle_time_us": 2000,
        "deadline_us": 1500,
        "enable_histogram": true,
        "histogram_buckets": 50,
        "sample_buffer_size": 5000,
        "enable_tracing": true
    })";
    config_file.close();

    EXPECT_TRUE(monitor_->loadConfig("/tmp/test_perf_monitor.json"));

    // Execute a cycle to verify configuration was applied
    monitor_->startCycle();
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    monitor_->endCycle();

    auto stats = monitor_->getStats();
    EXPECT_EQ(stats.total_cycles, 1);

    auto histogram = monitor_->getHistogram();
    EXPECT_EQ(histogram.size(), 50);  // Verify histogram_buckets was applied
}

// Test edge case: zero cycles
TEST_F(PerfMonitorTest, ZeroCycles) {
    PerfMonitorConfig config;
    EXPECT_TRUE(monitor_->configure(config));

    auto stats = monitor_->getStats();
    EXPECT_EQ(stats.total_cycles, 0);
    EXPECT_EQ(stats.deadline_misses, 0);
    EXPECT_EQ(stats.avg_latency, 0.0);
    EXPECT_EQ(stats.jitter, 0.0);
}

// Test total execution time
TEST_F(PerfMonitorTest, TotalExecutionTime) {
    PerfMonitorConfig config;
    config.enable_tracing = false;
    EXPECT_TRUE(monitor_->configure(config));

    // Execute 10 cycles of ~100us each
    for (int i = 0; i < 10; ++i) {
        monitor_->startCycle();
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        monitor_->endCycle();
    }

    auto stats = monitor_->getStats();
    EXPECT_GT(stats.total_execution_time_us, 900);   // At least 900us (10 * 90us)
    EXPECT_LT(stats.total_execution_time_us, 3000);  // Less than 3000us (relaxed bound)
    EXPECT_NEAR(stats.avg_execution_time_us, stats.total_execution_time_us / 10.0, 20.0);
}

// Test max jitter
TEST_F(PerfMonitorTest, MaxJitter) {
    PerfMonitorConfig config;
    config.enable_tracing = false;
    EXPECT_TRUE(monitor_->configure(config));

    // Execute cycles with one outlier
    for (int i = 0; i < 9; ++i) {
        monitor_->startCycle();
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        monitor_->endCycle();
    }

    // One cycle with much higher latency
    monitor_->startCycle();
    std::this_thread::sleep_for(std::chrono::microseconds(1000));
    monitor_->endCycle();

    auto stats = monitor_->getStats();
    // Max jitter should reflect the outlier
    EXPECT_GT(stats.max_jitter, 500.0);
}
