// event_storm_test.cpp - Event Storm Performance Benchmark
// Copyright (C) 2025 MXRC Project
//
// Tests Feature 022 Phase 4 T029: Large-scale event processing stability
// Validates system behavior under high event load (100,000 events)

#include "gtest/gtest.h"
#include "core/event/core/EventBus.h"
#include "core/event/util/EventFilter.h"
#include "dto/ActionEvents.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cmath>

using namespace mxrc::core::event;

namespace mxrc::core::event {

/**
 * @brief Performance benchmark for event storm scenarios
 *
 * T029 Requirements:
 * - Process 100,000 events with mixed priorities
 * - Measure drop rates by priority
 * - Measure RT cycle jitter
 *
 * NOTE: Current implementation treats all events as NORMAL priority.
 * Priority-based drop policy will be implemented in future phases.
 */
class EventStormTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Large queue to handle high throughput
        eventBus_ = std::make_shared<EventBus>(50000);  // 50k capacity
        eventBus_->start();
    }

    void TearDown() override {
        eventBus_->stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::shared_ptr<EventBus> eventBus_;
};

// ============================================================================
// T029: Event Storm Test
// ============================================================================

TEST_F(EventStormTest, HighThroughputStability) {
    const int TOTAL_EVENTS = 100000;  // 100k events

    // Event distribution (currently all treated as NORMAL priority)
    // Future: 50% CRITICAL, 30% NORMAL, 20% LOW
    const int CRITICAL_COUNT = TOTAL_EVENTS / 2;      // 50k
    const int NORMAL_COUNT = TOTAL_EVENTS * 3 / 10;   // 30k
    const int LOW_COUNT = TOTAL_EVENTS - CRITICAL_COUNT - NORMAL_COUNT;  // 20k

    std::atomic<int> processedCount{0};
    std::atomic<int> publishSuccessCount{0};
    std::atomic<int> publishFailCount{0};

    // Subscribe to process events
    eventBus_->subscribe(
        Filters::all(),
        [&processedCount](std::shared_ptr<IEvent> event) {
            processedCount.fetch_add(1);
        });

    // Publish events rapidly
    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < TOTAL_EVENTS; ++i) {
        std::string eventType;

        // Simulate priority distribution (currently no effect)
        if (i < CRITICAL_COUNT) {
            eventType = "critical_" + std::to_string(i);
        } else if (i < CRITICAL_COUNT + NORMAL_COUNT) {
            eventType = "normal_" + std::to_string(i);
        } else {
            eventType = "low_" + std::to_string(i);
        }

        auto event = std::make_shared<ActionStartedEvent>(eventType, "EventStorm");
        bool success = eventBus_->publish(event);

        if (success) {
            publishSuccessCount.fetch_add(1);
        } else {
            publishFailCount.fetch_add(1);
        }
    }

    auto publishDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime);

    // Wait for all events to be processed
    std::this_thread::sleep_for(std::chrono::seconds(5));

    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime);

    // Get stats
    const auto& stats = eventBus_->getStats();

    // Report metrics
    std::cout << "\n=== Event Storm Test Results ===" << std::endl;
    std::cout << "Total events: " << TOTAL_EVENTS << std::endl;
    std::cout << "Publish duration: " << publishDuration.count() << " ms" << std::endl;
    std::cout << "Total duration: " << totalDuration.count() << " ms" << std::endl;
    std::cout << "Throughput: " << (TOTAL_EVENTS * 1000 / publishDuration.count()) << " events/sec" << std::endl;
    std::cout << "\nPublish results:" << std::endl;
    std::cout << "  Success: " << publishSuccessCount.load() << std::endl;
    std::cout << "  Failed: " << publishFailCount.load() << std::endl;
    std::cout << "\nEventBus stats:" << std::endl;
    std::cout << "  Published: " << stats.publishedEvents.load() << std::endl;
    std::cout << "  Processed: " << stats.processedEvents.load() << std::endl;
    std::cout << "  Dropped: " << stats.droppedEvents.load() << std::endl;
    std::cout << "  Failed callbacks: " << stats.failedCallbacks.load() << std::endl;
    std::cout << "\nDrop rates:" << std::endl;

    double dropRate = 0.0;
    if (publishSuccessCount.load() > 0) {
        dropRate = (100.0 * stats.droppedEvents.load()) / publishSuccessCount.load();
    }
    std::cout << "  Overall: " << dropRate << "%" << std::endl;

    // Validate stability
    EXPECT_EQ(publishSuccessCount.load() + publishFailCount.load(), TOTAL_EVENTS)
        << "All publish attempts should be counted";

    EXPECT_EQ(stats.publishedEvents.load(), publishSuccessCount.load())
        << "Published count should match successful publishes";

    EXPECT_EQ(stats.droppedEvents.load(), publishFailCount.load())
        << "Dropped count should match failed publishes";

    // Process all successfully published events
    EXPECT_EQ(processedCount.load(), publishSuccessCount.load())
        << "All successfully published events should be processed";

    // System should handle high load without crashes
    SUCCEED() << "Event storm test completed successfully";
}

// ============================================================================
// Throughput Benchmark
// ============================================================================

TEST_F(EventStormTest, PublishThroughputBenchmark) {
    const int EVENT_COUNT = 10000;  // 10k events for quick benchmark
    std::atomic<int> processedCount{0};

    eventBus_->subscribe(
        Filters::all(),
        [&processedCount](std::shared_ptr<IEvent> event) {
            processedCount.fetch_add(1);
        });

    auto startTime = std::chrono::high_resolution_clock::now();

    // Publish events as fast as possible
    for (int i = 0; i < EVENT_COUNT; ++i) {
        auto event = std::make_shared<ActionStartedEvent>(
            "benchmark_" + std::to_string(i), "Throughput");
        eventBus_->publish(event);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    double avgLatencyUs = static_cast<double>(duration.count()) / EVENT_COUNT;
    double throughputPerSec = (EVENT_COUNT * 1000000.0) / duration.count();

    std::cout << "\n=== Throughput Benchmark ===" << std::endl;
    std::cout << "Events: " << EVENT_COUNT << std::endl;
    std::cout << "Duration: " << duration.count() << " μs" << std::endl;
    std::cout << "Avg latency: " << avgLatencyUs << " μs/event" << std::endl;
    std::cout << "Throughput: " << static_cast<int>(throughputPerSec) << " events/sec" << std::endl;

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::seconds(2));

    EXPECT_GT(throughputPerSec, 10000) << "Should handle at least 10k events/sec";
}

// ============================================================================
// Latency Distribution Test
// ============================================================================

TEST_F(EventStormTest, LatencyDistribution) {
    const int EVENT_COUNT = 1000;
    std::vector<int64_t> latencies;
    latencies.reserve(EVENT_COUNT);
    std::mutex latenciesMutex;

    eventBus_->subscribe(
        Filters::all(),
        [&latencies, &latenciesMutex](std::shared_ptr<IEvent> event) {
            auto now = std::chrono::system_clock::now();
            auto eventTime = event->getTimestamp();

            // Calculate latency (current time - event creation time)
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                now - eventTime).count();

            std::lock_guard<std::mutex> lock(latenciesMutex);
            latencies.push_back(latency);
        });

    // Publish events
    for (int i = 0; i < EVENT_COUNT; ++i) {
        auto event = std::make_shared<ActionStartedEvent>(
            "latency_" + std::to_string(i), "Latency");
        eventBus_->publish(event);

        // Add small delay to avoid overwhelming the queue
        if (i % 100 == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    // Wait for all events to be processed
    std::this_thread::sleep_for(std::chrono::seconds(2));

    ASSERT_GT(latencies.size(), static_cast<size_t>(EVENT_COUNT * 0.9))
        << "At least 90% of events should be processed";

    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());

    int64_t sum = 0;
    for (auto latency : latencies) {
        sum += latency;
    }

    double avgNs = static_cast<double>(sum) / latencies.size();
    int64_t p50 = latencies[latencies.size() / 2];
    int64_t p95 = latencies[latencies.size() * 95 / 100];
    int64_t p99 = latencies[latencies.size() * 99 / 100];
    int64_t maxNs = latencies.back();

    std::cout << "\n=== Latency Distribution ===" << std::endl;
    std::cout << "Samples: " << latencies.size() << std::endl;
    std::cout << "Average: " << (avgNs / 1000.0) << " μs" << std::endl;
    std::cout << "P50: " << (p50 / 1000.0) << " μs" << std::endl;
    std::cout << "P95: " << (p95 / 1000.0) << " μs" << std::endl;
    std::cout << "P99: " << (p99 / 1000.0) << " μs" << std::endl;
    std::cout << "Max: " << (maxNs / 1000.0) << " μs" << std::endl;

    // Reasonable latency expectations (relaxed for CI environments)
    EXPECT_LT(avgNs / 1000.0, 2000.0) << "Average latency should be < 2ms";
    EXPECT_LT(p99 / 1000.0, 10000.0) << "P99 latency should be < 10ms";
}

} // namespace mxrc::core::event
