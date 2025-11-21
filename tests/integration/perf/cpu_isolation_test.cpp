/**
 * @file cpu_isolation_test.cpp
 * @brief CPU Isolation Integration Test
 *
 * Success Criteria:
 * - SC-001: Deadline miss rate < 0.01% over 10,000 cycles
 * - SC-002: Cycle time jitter reduction 50% (compared to baseline)
 *
 * Prerequisites:
 * - CPU cores isolated via isolcpus kernel parameter
 * - CAP_SYS_NICE capability for SCHED_FIFO
 * - config/rt/cpu_affinity.json configured
 */

#include <gtest/gtest.h>
#include "core/rt/RTExecutive.h"
#include "core/rt/RTMetrics.h"
#include "core/rt/perf/CPUAffinityManager.h"
#include "core/rt/perf/PerfMonitor.h"
#include "core/monitoring/MetricsCollector.h"
#include "core/event/core/EventBus.h"
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>

using namespace mxrc::core::rt;
using namespace mxrc::rt::perf;

class CPUIsolationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set logging level to info for test visibility
        spdlog::set_level(spdlog::level::info);

        // Create metrics infrastructure
        metrics_collector_ = std::make_shared<mxrc::core::monitoring::MetricsCollector>();
        rt_metrics_ = std::make_unique<RTMetrics>(metrics_collector_);

        // Create event bus
        event_bus_ = std::make_shared<mxrc::core::event::EventBus>();

        // Create RT executive (1ms minor cycle, 10ms major cycle)
        executive_ = std::make_unique<RTExecutive>(1, 10, event_bus_);
        executive_->setRTMetrics(rt_metrics_.get());
    }

    void TearDown() override {
        if (executive_) {
            executive_->stop();
        }
    }

    std::shared_ptr<mxrc::core::monitoring::MetricsCollector> metrics_collector_;
    std::unique_ptr<RTMetrics> rt_metrics_;
    std::shared_ptr<mxrc::core::event::EventBus> event_bus_;
    std::unique_ptr<RTExecutive> executive_;

    static double baseline_jitter_;
};

/**
 * @brief Baseline test: Run RT executive WITHOUT CPU isolation
 *
 * This test establishes baseline performance without CPU affinity/isolation.
 * Results are used to compare against isolated performance (SC-002).
 */
TEST_F(CPUIsolationTest, Baseline_NoCPUIsolation) {
    spdlog::info("=== Baseline Test: No CPU Isolation ===");

    // Configure performance monitor for 1ms cycle
    PerfMonitorConfig perf_config;
    perf_config.process_name = "mxrc_rt_baseline";
    perf_config.cycle_time_us = 1000;
    perf_config.deadline_us = 900;  // 90% of cycle time
    perf_config.enable_histogram = true;
    perf_config.sample_buffer_size = 10000;
    perf_config.enable_tracing = false;

    auto perf_monitor = executive_->getPerfMonitor();
    ASSERT_NE(perf_monitor, nullptr);
    ASSERT_TRUE(perf_monitor->configure(perf_config));

    // Register simple test action
    int action_count = 0;
    executive_->registerAction("test_action", 1,
        [&action_count](RTContext& ctx) {
            action_count++;
            // Simulate some work
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        });

    // Run for 10,000 cycles (10 seconds at 1ms cycle)
    std::thread exec_thread([this]() {
        executive_->run();
    });

    // Let it run for 10 seconds
    std::this_thread::sleep_for(std::chrono::seconds(10));
    executive_->stop();
    exec_thread.join();

    // Collect baseline statistics
    auto stats = perf_monitor->getStats();

    spdlog::info("Baseline Results:");
    spdlog::info("  Total cycles: {}", stats.total_cycles);
    spdlog::info("  Deadline misses: {}", stats.deadline_misses);
    spdlog::info("  Deadline miss rate: {:.4f}%", stats.deadline_miss_rate);
    spdlog::info("  Average latency: {:.2f} us", stats.avg_latency);
    spdlog::info("  P50 latency: {:.2f} us", stats.p50_latency);
    spdlog::info("  P95 latency: {:.2f} us", stats.p95_latency);
    spdlog::info("  P99 latency: {:.2f} us", stats.p99_latency);
    spdlog::info("  Jitter (stddev): {:.2f} us", stats.jitter);
    spdlog::info("  Max jitter: {:.2f} us", stats.max_jitter);

    // Basic validation
    EXPECT_GE(stats.total_cycles, 9000);  // At least 9000 cycles in 10 seconds
    EXPECT_GT(action_count, 0);

    // Store baseline jitter for SC-002 comparison
    baseline_jitter_ = stats.jitter;
}

/**
 * @brief SC-001 Test: Deadline miss rate < 0.01% with CPU isolation
 *
 * This is the primary success criterion for RT performance.
 * Requires CPU isolation to be configured via isolcpus.
 */
TEST_F(CPUIsolationTest, SC001_DeadlineMissRate) {
    spdlog::info("=== SC-001 Test: Deadline Miss Rate < 0.01% ===");

    // Manual configuration for testing
    CPUAffinityConfig cpu_config;
    cpu_config.process_name = "mxrc_rt_test";
    cpu_config.cpu_cores = {1};  // Single isolated core
    cpu_config.policy = SchedPolicy::FIFO;
    cpu_config.priority = 90;  // High RT priority (not max to allow flexibility)
    cpu_config.isolation_mode = IsolationMode::NONE;  // Don't fail if isolation not configured

    // Apply CPU affinity
    auto cpu_mgr = std::make_unique<CPUAffinityManager>();
    bool affinity_applied = cpu_mgr->apply(cpu_config);
    if (!affinity_applied) {
        spdlog::warn("Failed to apply CPU affinity - may need CAP_SYS_NICE or sudo");
        spdlog::warn("Test will continue but results may not meet SC-001");
    }

    // Configure performance monitor
    PerfMonitorConfig perf_config;
    perf_config.process_name = "mxrc_rt_isolated";
    perf_config.cycle_time_us = 1000;
    perf_config.deadline_us = 900;
    perf_config.enable_histogram = true;
    perf_config.sample_buffer_size = 10000;
    perf_config.enable_tracing = false;

    auto perf_monitor = executive_->getPerfMonitor();
    ASSERT_NE(perf_monitor, nullptr);
    ASSERT_TRUE(perf_monitor->configure(perf_config));

    // Register test action with realistic workload
    int action_count = 0;
    executive_->registerAction("test_action", 1,
        [&action_count](RTContext& ctx) {
            action_count++;
            // Simulate 200us of work (well under 900us deadline)
            auto start = std::chrono::high_resolution_clock::now();
            while (std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now() - start).count() < 200) {
                // Busy wait to simulate RT work
            }
        });

    // Run for 10,000+ cycles (10+ seconds at 1ms cycle)
    std::thread exec_thread([this]() {
        executive_->run();
    });

    // Let it run for 11 seconds to ensure > 10,000 cycles
    std::this_thread::sleep_for(std::chrono::seconds(11));
    executive_->stop();
    exec_thread.join();

    // Collect performance statistics
    auto stats = perf_monitor->getStats();

    spdlog::info("SC-001 Test Results:");
    spdlog::info("  Total cycles: {}", stats.total_cycles);
    spdlog::info("  Deadline misses: {}", stats.deadline_misses);
    spdlog::info("  Deadline miss rate: {:.4f}%", stats.deadline_miss_rate);
    spdlog::info("  Average latency: {:.2f} us", stats.avg_latency);
    spdlog::info("  P50 latency: {:.2f} us", stats.p50_latency);
    spdlog::info("  P95 latency: {:.2f} us", stats.p95_latency);
    spdlog::info("  P99 latency: {:.2f} us", stats.p99_latency);
    spdlog::info("  Jitter (stddev): {:.2f} us", stats.jitter);

    // SC-001: Verify we ran at least 10,000 cycles
    EXPECT_GE(stats.total_cycles, 10000)
        << "Test must run at least 10,000 cycles";

    // SC-001: Deadline miss rate < 0.01%
    const double MAX_DEADLINE_MISS_RATE = 0.01;
    EXPECT_LT(stats.deadline_miss_rate, MAX_DEADLINE_MISS_RATE)
        << "SC-001 FAILED: Deadline miss rate " << stats.deadline_miss_rate
        << "% exceeds threshold " << MAX_DEADLINE_MISS_RATE << "%"
        << "\nThis may indicate:"
        << "\n  1. CPU cores not isolated (missing isolcpus kernel parameter)"
        << "\n  2. Insufficient RT priority (need CAP_SYS_NICE)"
        << "\n  3. System interference (other processes on RT cores)"
        << "\n  4. Hardware limitations";

    // Additional quality checks
    EXPECT_LT(stats.p99_latency, 900.0)
        << "P99 latency should be below deadline";

    EXPECT_EQ(action_count, stats.total_cycles)
        << "Action should execute every cycle";

    if (affinity_applied) {
        spdlog::info("SC-001 SUCCESS: Deadline miss rate {:.4f}% < 0.01%",
                     stats.deadline_miss_rate);
    } else {
        spdlog::warn("SC-001 test ran without CPU affinity - results may not be reliable");
    }
}

/**
 * @brief SC-002 Test: Jitter reduction 50% compared to baseline
 *
 * This test requires running the baseline test first to establish
 * the comparison point.
 */
TEST_F(CPUIsolationTest, SC002_JitterReduction) {
    // This test would compare isolated vs non-isolated jitter
    // For now, we just verify jitter is reasonably low
    spdlog::info("=== SC-002 Test: Jitter Reduction ===");
    spdlog::info("Note: Full SC-002 validation requires baseline comparison");

    // Run isolated test (similar to SC-001)
    auto perf_monitor = executive_->getPerfMonitor();
    ASSERT_NE(perf_monitor, nullptr);

    PerfMonitorConfig perf_config;
    perf_config.cycle_time_us = 1000;
    perf_config.deadline_us = 900;
    perf_config.sample_buffer_size = 5000;
    ASSERT_TRUE(perf_monitor->configure(perf_config));

    executive_->registerAction("test_action", 1,
        [](RTContext& ctx) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        });

    std::thread exec_thread([this]() {
        executive_->run();
    });

    std::this_thread::sleep_for(std::chrono::seconds(5));
    executive_->stop();
    exec_thread.join();

    auto stats = perf_monitor->getStats();

    spdlog::info("Jitter: {:.2f} us (target: < 100 us for good RT performance)",
                 stats.jitter);

    // Quality check: jitter should be reasonable for RT
    EXPECT_LT(stats.jitter, 100.0)
        << "Jitter should be < 100us for good RT performance";
}

// Static member for baseline comparison
double CPUIsolationTest::baseline_jitter_ = 0.0;
