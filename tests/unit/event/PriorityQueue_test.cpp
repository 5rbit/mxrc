#include <gtest/gtest.h>
#include "core/event/core/PriorityQueue.h"
#include <thread>
#include <vector>
#include <atomic>

using namespace mxrc::core::event;

/**
 * @brief Unit tests for Feature 022 P3: PriorityQueue
 *
 * Test Coverage:
 * - Push/pop operations with priority ordering
 * - Backpressure policy (80%/90%/100% thresholds)
 * - CRITICAL events never dropped
 * - Metrics collection
 * - Thread safety (MPSC pattern)
 * - Queue capacity limits
 */
class PriorityQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        queue_ = std::make_unique<PriorityQueue>(100);  // Small capacity for testing
    }

    std::unique_ptr<PriorityQueue> queue_;
};

// ============================================================================
// Basic Push/Pop Tests
// ============================================================================

TEST_F(PriorityQueueTest, PushPop_SingleEvent_Success) {
    auto event = makePrioritizedEvent("test.event", EventPriority::NORMAL, 42, 0);

    EXPECT_TRUE(queue_->push(std::move(event)));
    EXPECT_EQ(queue_->size(), 1u);

    auto popped = queue_->pop();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(popped->type, "test.event");
    EXPECT_EQ(std::get<int>(popped->payload), 42);
    EXPECT_EQ(queue_->size(), 0u);
}

TEST_F(PriorityQueueTest, Pop_EmptyQueue_ReturnsNullopt) {
    auto popped = queue_->pop();
    EXPECT_FALSE(popped.has_value());
}

TEST_F(PriorityQueueTest, Empty_NewQueue_IsEmpty) {
    EXPECT_TRUE(queue_->empty());
    EXPECT_EQ(queue_->size(), 0u);
}

TEST_F(PriorityQueueTest, Empty_AfterPushPop_IsEmpty) {
    auto event = makePrioritizedEvent("test.event", EventPriority::NORMAL, 42, 0);
    queue_->push(std::move(event));
    queue_->pop();

    EXPECT_TRUE(queue_->empty());
}

TEST_F(PriorityQueueTest, Capacity_Returns100) {
    EXPECT_EQ(queue_->capacity(), 100u);
}

// ============================================================================
// Priority Ordering Tests
// ============================================================================

TEST_F(PriorityQueueTest, PriorityOrdering_CriticalBeforeHigh) {
    auto high = makePrioritizedEvent("high", EventPriority::HIGH, 1, 0);
    auto critical = makePrioritizedEvent("critical", EventPriority::CRITICAL, 2, 0);

    queue_->push(std::move(high));
    queue_->push(std::move(critical));

    auto first = queue_->pop();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->type, "critical");
    EXPECT_EQ(first->priority, EventPriority::CRITICAL);
}

TEST_F(PriorityQueueTest, PriorityOrdering_AllFourLevels) {
    auto low = makePrioritizedEvent("low", EventPriority::LOW, 1, 0);
    auto normal = makePrioritizedEvent("normal", EventPriority::NORMAL, 2, 0);
    auto high = makePrioritizedEvent("high", EventPriority::HIGH, 3, 0);
    auto critical = makePrioritizedEvent("critical", EventPriority::CRITICAL, 4, 0);

    // Push in reverse order
    queue_->push(std::move(low));
    queue_->push(std::move(normal));
    queue_->push(std::move(high));
    queue_->push(std::move(critical));

    // Pop should return in priority order
    auto e1 = queue_->pop();
    EXPECT_EQ(e1->priority, EventPriority::CRITICAL);

    auto e2 = queue_->pop();
    EXPECT_EQ(e2->priority, EventPriority::HIGH);

    auto e3 = queue_->pop();
    EXPECT_EQ(e3->priority, EventPriority::NORMAL);

    auto e4 = queue_->pop();
    EXPECT_EQ(e4->priority, EventPriority::LOW);
}

TEST_F(PriorityQueueTest, PriorityOrdering_SamePriority_FIFOOrder) {
    auto event1 = makePrioritizedEvent("event1", EventPriority::NORMAL, 1, 100);
    auto event2 = makePrioritizedEvent("event2", EventPriority::NORMAL, 2, 101);
    auto event3 = makePrioritizedEvent("event3", EventPriority::NORMAL, 3, 102);

    queue_->push(std::move(event1));
    queue_->push(std::move(event2));
    queue_->push(std::move(event3));

    // Within same priority, should be FIFO (older timestamp/sequence first)
    auto e1 = queue_->pop();
    EXPECT_EQ(e1->sequence_num, 100u);

    auto e2 = queue_->pop();
    EXPECT_EQ(e2->sequence_num, 101u);

    auto e3 = queue_->pop();
    EXPECT_EQ(e3->sequence_num, 102u);
}

// ============================================================================
// Backpressure Policy Tests
// ============================================================================

TEST_F(PriorityQueueTest, Backpressure_Below80Percent_AcceptAll) {
    // 80% of 100 = 80 events
    // Push 79 LOW priority events (should all be accepted)
    for (int i = 0; i < 79; ++i) {
        auto event = makePrioritizedEvent("low", EventPriority::LOW, i, i);
        EXPECT_TRUE(queue_->push(std::move(event)));
    }

    EXPECT_EQ(queue_->size(), 79u);
    EXPECT_EQ(queue_->metrics().low_events_pushed.load(), 79u);
    EXPECT_EQ(queue_->metrics().low_events_dropped.load(), 0u);
}

TEST_F(PriorityQueueTest, Backpressure_80to90Percent_DropLow) {
    // 80% of 100 = 80, 90% = 90
    // Fill to 80 events with NORMAL priority
    for (int i = 0; i < 80; ++i) {
        auto event = makePrioritizedEvent("normal", EventPriority::NORMAL, i, i);
        queue_->push(std::move(event));
    }

    // Try to push LOW priority events (should be dropped)
    auto low_event = makePrioritizedEvent("low", EventPriority::LOW, 100, 100);
    EXPECT_FALSE(queue_->push(std::move(low_event)));
    EXPECT_EQ(queue_->metrics().low_events_dropped.load(), 1u);

    // NORMAL priority should still be accepted
    auto normal_event = makePrioritizedEvent("normal", EventPriority::NORMAL, 101, 101);
    EXPECT_TRUE(queue_->push(std::move(normal_event)));
    EXPECT_EQ(queue_->size(), 81u);
}

TEST_F(PriorityQueueTest, Backpressure_90to100Percent_DropLowAndNormal) {
    // 90% of 100 = 90
    // Fill to 90 events with HIGH priority
    for (int i = 0; i < 90; ++i) {
        auto event = makePrioritizedEvent("high", EventPriority::HIGH, i, i);
        queue_->push(std::move(event));
    }

    // LOW should be dropped
    auto low_event = makePrioritizedEvent("low", EventPriority::LOW, 100, 100);
    EXPECT_FALSE(queue_->push(std::move(low_event)));

    // NORMAL should be dropped
    auto normal_event = makePrioritizedEvent("normal", EventPriority::NORMAL, 101, 101);
    EXPECT_FALSE(queue_->push(std::move(normal_event)));

    // HIGH should still be accepted
    auto high_event = makePrioritizedEvent("high", EventPriority::HIGH, 102, 102);
    EXPECT_TRUE(queue_->push(std::move(high_event)));
    EXPECT_EQ(queue_->size(), 91u);
}

TEST_F(PriorityQueueTest, Backpressure_100Percent_DropAllExceptCritical) {
    // Fill to 100 events with CRITICAL priority
    for (int i = 0; i < 100; ++i) {
        auto event = makePrioritizedEvent("critical", EventPriority::CRITICAL, i, i);
        queue_->push(std::move(event));
    }

    // LOW should be dropped
    auto low_event = makePrioritizedEvent("low", EventPriority::LOW, 100, 100);
    EXPECT_FALSE(queue_->push(std::move(low_event)));

    // NORMAL should be dropped
    auto normal_event = makePrioritizedEvent("normal", EventPriority::NORMAL, 101, 101);
    EXPECT_FALSE(queue_->push(std::move(normal_event)));

    // HIGH should be dropped
    auto high_event = makePrioritizedEvent("high", EventPriority::HIGH, 102, 102);
    EXPECT_FALSE(queue_->push(std::move(high_event)));

    // CRITICAL should ALWAYS be accepted (may exceed capacity)
    auto critical_event = makePrioritizedEvent("critical", EventPriority::CRITICAL, 103, 103);
    EXPECT_TRUE(queue_->push(std::move(critical_event)));
    EXPECT_EQ(queue_->size(), 101u);  // Exceeds capacity
}

TEST_F(PriorityQueueTest, Backpressure_CriticalNeverDropped) {
    // Fill queue to 100% with HIGH events (to avoid drops at 90%)
    for (int i = 0; i < 100; ++i) {
        auto event = makePrioritizedEvent("high", EventPriority::HIGH, i, i);
        EXPECT_TRUE(queue_->push(std::move(event)));
    }
    EXPECT_EQ(queue_->size(), 100u);

    // Push 10 more CRITICAL events (all should be accepted, exceeding capacity)
    for (int i = 0; i < 10; ++i) {
        auto event = makePrioritizedEvent("critical", EventPriority::CRITICAL, i, i);
        EXPECT_TRUE(queue_->push(std::move(event)));
    }

    EXPECT_EQ(queue_->size(), 110u);  // Exceeds capacity
    EXPECT_EQ(queue_->metrics().critical_events_dropped.load(), 0u);
}

// ============================================================================
// Metrics Tests
// ============================================================================

TEST_F(PriorityQueueTest, Metrics_PushCounters_AccurateCounts) {
    queue_->resetMetrics();

    auto critical = makePrioritizedEvent("c", EventPriority::CRITICAL, 1, 0);
    auto high = makePrioritizedEvent("h", EventPriority::HIGH, 2, 0);
    auto normal = makePrioritizedEvent("n", EventPriority::NORMAL, 3, 0);
    auto low = makePrioritizedEvent("l", EventPriority::LOW, 4, 0);

    queue_->push(std::move(critical));
    queue_->push(std::move(high));
    queue_->push(std::move(normal));
    queue_->push(std::move(low));

    const auto& metrics = queue_->metrics();
    EXPECT_EQ(metrics.critical_events_pushed.load(), 1u);
    EXPECT_EQ(metrics.high_events_pushed.load(), 1u);
    EXPECT_EQ(metrics.normal_events_pushed.load(), 1u);
    EXPECT_EQ(metrics.low_events_pushed.load(), 1u);
}

TEST_F(PriorityQueueTest, Metrics_DropCounters_AccurateCounts) {
    queue_->resetMetrics();

    // Fill to 100% to trigger drops
    for (int i = 0; i < 100; ++i) {
        auto event = makePrioritizedEvent("critical", EventPriority::CRITICAL, i, i);
        queue_->push(std::move(event));
    }

    // Try to push dropped events
    auto low = makePrioritizedEvent("l", EventPriority::LOW, 1, 0);
    auto normal = makePrioritizedEvent("n", EventPriority::NORMAL, 2, 0);
    auto high = makePrioritizedEvent("h", EventPriority::HIGH, 3, 0);

    queue_->push(std::move(low));
    queue_->push(std::move(normal));
    queue_->push(std::move(high));

    const auto& metrics = queue_->metrics();
    EXPECT_EQ(metrics.low_events_dropped.load(), 1u);
    EXPECT_EQ(metrics.normal_events_dropped.load(), 1u);
    EXPECT_EQ(metrics.high_events_dropped.load(), 1u);
    EXPECT_EQ(metrics.critical_events_dropped.load(), 0u);  // Never dropped
}

TEST_F(PriorityQueueTest, Metrics_PopCounter_AccurateCount) {
    queue_->resetMetrics();

    for (int i = 0; i < 10; ++i) {
        auto event = makePrioritizedEvent("test", EventPriority::NORMAL, i, i);
        queue_->push(std::move(event));
    }

    for (int i = 0; i < 10; ++i) {
        queue_->pop();
    }

    EXPECT_EQ(queue_->metrics().events_popped.load(), 10u);
}

TEST_F(PriorityQueueTest, Metrics_CurrentSize_TracksQueueSize) {
    queue_->resetMetrics();

    auto event1 = makePrioritizedEvent("test1", EventPriority::NORMAL, 1, 0);
    queue_->push(std::move(event1));
    EXPECT_EQ(queue_->metrics().current_size.load(), 1u);

    auto event2 = makePrioritizedEvent("test2", EventPriority::NORMAL, 2, 0);
    queue_->push(std::move(event2));
    EXPECT_EQ(queue_->metrics().current_size.load(), 2u);

    queue_->pop();
    EXPECT_EQ(queue_->metrics().current_size.load(), 1u);
}

TEST_F(PriorityQueueTest, Metrics_PeakSize_TracksPeakSize) {
    queue_->resetMetrics();

    for (int i = 0; i < 50; ++i) {
        auto event = makePrioritizedEvent("test", EventPriority::NORMAL, i, i);
        queue_->push(std::move(event));
    }

    EXPECT_EQ(queue_->metrics().peak_size.load(), 50u);

    // Pop some events
    for (int i = 0; i < 30; ++i) {
        queue_->pop();
    }

    // Peak should still be 50
    EXPECT_EQ(queue_->metrics().peak_size.load(), 50u);
}

TEST_F(PriorityQueueTest, Metrics_ResetMetrics_ClearsAllCounters) {
    // Push some events
    for (int i = 0; i < 10; ++i) {
        auto event = makePrioritizedEvent("test", EventPriority::NORMAL, i, i);
        queue_->push(std::move(event));
    }

    queue_->resetMetrics();

    const auto& metrics = queue_->metrics();
    EXPECT_EQ(metrics.critical_events_pushed.load(), 0u);
    EXPECT_EQ(metrics.high_events_pushed.load(), 0u);
    EXPECT_EQ(metrics.normal_events_pushed.load(), 0u);
    EXPECT_EQ(metrics.low_events_pushed.load(), 0u);
    EXPECT_EQ(metrics.critical_events_dropped.load(), 0u);
    EXPECT_EQ(metrics.high_events_dropped.load(), 0u);
    EXPECT_EQ(metrics.normal_events_dropped.load(), 0u);
    EXPECT_EQ(metrics.low_events_dropped.load(), 0u);
    EXPECT_EQ(metrics.events_popped.load(), 0u);
    EXPECT_EQ(metrics.current_size.load(), 0u);
    EXPECT_EQ(metrics.peak_size.load(), 0u);
}

// ============================================================================
// Thread Safety Tests (MPSC Pattern)
// ============================================================================

TEST_F(PriorityQueueTest, ThreadSafety_MultipleProducers_SingleConsumer) {
    const int NUM_PRODUCERS = 4;
    const int EVENTS_PER_PRODUCER = 1000;

    std::vector<std::thread> producers;
    std::atomic<int> total_pushed{0};

    // Start consumer thread
    std::atomic<bool> stop_consumer{false};
    std::atomic<int> total_popped{0};
    std::thread consumer([&]() {
        while (!stop_consumer.load()) {
            if (auto event = queue_->pop()) {
                total_popped.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
        // Drain remaining events
        while (auto event = queue_->pop()) {
            total_popped.fetch_add(1);
        }
    });

    // Start producer threads
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < EVENTS_PER_PRODUCER; ++i) {
                auto event = makePrioritizedEvent(
                    "producer" + std::to_string(p),
                    EventPriority::NORMAL,
                    i,
                    p * EVENTS_PER_PRODUCER + i);
                if (queue_->push(std::move(event))) {
                    total_pushed.fetch_add(1);
                }
            }
        });
    }

    // Wait for producers to finish
    for (auto& t : producers) {
        t.join();
    }

    // Stop consumer
    stop_consumer.store(true);
    consumer.join();

    // All pushed events should be popped
    EXPECT_EQ(total_pushed.load(), total_popped.load());
    EXPECT_EQ(queue_->size(), 0u);
}

TEST_F(PriorityQueueTest, ThreadSafety_ConcurrentPushPop_NoDataLoss) {
    const int NUM_EVENTS = 10000;
    std::atomic<int> total_pushed{0};
    std::atomic<int> total_popped{0};
    std::atomic<bool> producer_done{false};

    std::thread producer([&]() {
        for (int i = 0; i < NUM_EVENTS; ++i) {
            auto event = makePrioritizedEvent("test", EventPriority::NORMAL, i, i);
            if (queue_->push(std::move(event))) {
                total_pushed.fetch_add(1);
            }
        }
        producer_done.store(true);
    });

    std::thread consumer([&]() {
        while (!producer_done.load() || !queue_->empty()) {
            if (auto event = queue_->pop()) {
                total_popped.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_GT(total_pushed.load(), 0);  // At least some events should be pushed
    EXPECT_EQ(total_pushed.load(), total_popped.load());  // All pushed events should be popped
}

// Main is provided by the run_tests executable
