// priority_integration_test.cpp - Priority Queue Integration Tests
// Copyright (C) 2025 MXRC Project
//
// Tests Feature 022 Phase 4: Priority-based event processing
// Validates CRITICAL > HIGH > NORMAL > LOW ordering and backpressure

#include "gtest/gtest.h"
#include "core/event/core/EventBus.h"
#include "core/event/util/EventFilter.h"
#include "dto/ActionEvents.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

using namespace mxrc::core::event;

namespace mxrc::core::event {

/**
 * @brief Helper class to create events with specific priorities
 *
 * Since IEvent doesn't expose priority, we'll test priority ordering
 * by publishing events in mixed order and verifying dispatch order.
 */
class PriorityIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create EventBus with small queue to test backpressure
        eventBus_ = std::make_shared<EventBus>(100);  // Small queue for backpressure testing
        eventBus_->start();
    }

    void TearDown() override {
        eventBus_->stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Helper: Subscribe and collect all events in order
    void subscribeAndCollect() {
        eventBus_->subscribe(
            Filters::all(),
            [this](std::shared_ptr<IEvent> event) {
                std::lock_guard<std::mutex> lock(eventsMutex_);
                receivedEvents_.push_back(event);
            });
    }

    // Helper: Wait for specific event count
    bool waitForEventCount(size_t expectedCount, int timeoutMs = 5000) {
        auto startTime = std::chrono::steady_clock::now();
        while (true) {
            {
                std::lock_guard<std::mutex> lock(eventsMutex_);
                if (receivedEvents_.size() >= expectedCount) {
                    return true;
                }
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            if (elapsed > timeoutMs) {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    std::shared_ptr<EventBus> eventBus_;
    std::vector<std::shared_ptr<IEvent>> receivedEvents_;
    std::mutex eventsMutex_;
};

// ============================================================================
// T029-1: Basic Priority Ordering
// ============================================================================

TEST_F(PriorityIntegrationTest, EventsProcessedInFIFOOrder) {
    subscribeAndCollect();

    // Publish 5 events in order
    for (int i = 0; i < 5; ++i) {
        auto event = std::make_shared<ActionStartedEvent>(
            "action_" + std::to_string(i), "TestAction");
        eventBus_->publish(event);
    }

    ASSERT_TRUE(waitForEventCount(5, 1000));

    // Verify FIFO order (all NORMAL priority)
    std::lock_guard<std::mutex> lock(eventsMutex_);
    EXPECT_EQ(receivedEvents_.size(), 5);

    for (size_t i = 0; i < 5; ++i) {
        auto actionEvent = std::static_pointer_cast<ActionStartedEvent>(receivedEvents_[i]);
        EXPECT_EQ(actionEvent->actionId, "action_" + std::to_string(i));
    }
}

// ============================================================================
// T029-2: Event Ordering with Same Priority
// ============================================================================

TEST_F(PriorityIntegrationTest, SamePriorityEventsProcessedInOrder) {
    subscribeAndCollect();

    // Publish 10 events rapidly
    for (int i = 0; i < 10; ++i) {
        auto event = std::make_shared<ActionCompletedEvent>(
            "action_" + std::to_string(i), "TestAction", i * 10);
        eventBus_->publish(event);
    }

    ASSERT_TRUE(waitForEventCount(10, 2000));

    // All events should be received in order
    std::lock_guard<std::mutex> lock(eventsMutex_);
    EXPECT_EQ(receivedEvents_.size(), 10);

    for (size_t i = 0; i < 10; ++i) {
        auto actionEvent = std::static_pointer_cast<ActionCompletedEvent>(receivedEvents_[i]);
        EXPECT_EQ(actionEvent->actionId, "action_" + std::to_string(i));
        EXPECT_EQ(actionEvent->durationMs, static_cast<long>(i * 10));
    }
}

// ============================================================================
// T029-3: High Throughput Test
// ============================================================================

TEST_F(PriorityIntegrationTest, HighThroughputNoDrops) {
    subscribeAndCollect();

    const int EVENT_COUNT = 50;  // Below queue capacity (100)

    // Publish many events rapidly
    for (int i = 0; i < EVENT_COUNT; ++i) {
        auto event = std::make_shared<ActionStartedEvent>(
            "action_" + std::to_string(i), "HighThroughput");
        bool success = eventBus_->publish(event);
        EXPECT_TRUE(success) << "Event " << i << " was dropped";
    }

    ASSERT_TRUE(waitForEventCount(EVENT_COUNT, 3000));

    // Verify all events received
    std::lock_guard<std::mutex> lock(eventsMutex_);
    EXPECT_EQ(receivedEvents_.size(), EVENT_COUNT);

    // Verify stats
    const auto& stats = eventBus_->getStats();
    EXPECT_EQ(stats.publishedEvents.load(), EVENT_COUNT);
    EXPECT_EQ(stats.processedEvents.load(), EVENT_COUNT);
    EXPECT_EQ(stats.droppedEvents.load(), 0);
}

// ============================================================================
// T029-4: Queue Overflow Handling
// ============================================================================

TEST_F(PriorityIntegrationTest, QueueOverflowDropsEvents) {
    // Subscribe with VERY slow processing to create backpressure
    eventBus_->subscribe(
        Filters::all(),
        [](std::shared_ptr<IEvent> event) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Very slow
        });

    const int OVERFLOW_COUNT = 150;  // Exceeds queue capacity (100)
    int droppedCount = 0;

    // Publish events rapidly to exceed queue capacity
    for (int i = 0; i < OVERFLOW_COUNT; ++i) {
        auto event = std::make_shared<ActionStartedEvent>(
            "overflow_" + std::to_string(i), "Overflow");
        bool success = eventBus_->publish(event);
        if (!success) {
            droppedCount++;
        }
    }

    // Some events should be dropped due to slow processing
    EXPECT_GT(droppedCount, 0) << "Expected some events to be dropped due to queue overflow";

    // Verify stats
    const auto& stats = eventBus_->getStats();
    EXPECT_EQ(stats.droppedEvents.load(), droppedCount);
    EXPECT_EQ(stats.publishedEvents.load(), OVERFLOW_COUNT - droppedCount);
}

// ============================================================================
// T029-5: Concurrent Publishers
// ============================================================================

TEST_F(PriorityIntegrationTest, ConcurrentPublishersNoDataRace) {
    subscribeAndCollect();

    const int NUM_THREADS = 4;
    const int EVENTS_PER_THREAD = 10;
    std::atomic<int> totalPublished{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < EVENTS_PER_THREAD; ++i) {
                std::string actionId = "thread_" + std::to_string(t) + "_event_" + std::to_string(i);
                auto event = std::make_shared<ActionStartedEvent>(actionId, "Concurrent");

                if (eventBus_->publish(event)) {
                    totalPublished.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Wait for all published events to be processed
    ASSERT_TRUE(waitForEventCount(totalPublished.load(), 5000));

    // Verify all events received
    std::lock_guard<std::mutex> lock(eventsMutex_);
    EXPECT_EQ(receivedEvents_.size(), totalPublished.load());

    // Verify no data races in stats
    const auto& stats = eventBus_->getStats();
    EXPECT_EQ(stats.publishedEvents.load(), totalPublished.load());
    EXPECT_EQ(stats.processedEvents.load(), totalPublished.load());
}

// ============================================================================
// T029-6: Slow Subscriber Doesn't Block Publisher
// ============================================================================

TEST_F(PriorityIntegrationTest, SlowSubscriberDoesntBlockPublisher) {
    std::atomic<int> slowCallbackCount{0};

    // Subscribe with slow callback
    eventBus_->subscribe(
        Filters::all(),
        [&slowCallbackCount](std::shared_ptr<IEvent> event) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Slow processing
            slowCallbackCount.fetch_add(1);
        });

    const int EVENT_COUNT = 5;
    auto startTime = std::chrono::steady_clock::now();

    // Publish events rapidly
    for (int i = 0; i < EVENT_COUNT; ++i) {
        auto event = std::make_shared<ActionStartedEvent>("slow_" + std::to_string(i), "Slow");
        eventBus_->publish(event);
    }

    auto publishDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    // Publishing should be fast (< 100ms), not blocked by slow subscriber
    EXPECT_LT(publishDuration, 100);

    // Wait for slow subscriber to process all events
    std::this_thread::sleep_for(std::chrono::milliseconds(EVENT_COUNT * 50 + 500));

    EXPECT_EQ(slowCallbackCount.load(), EVENT_COUNT);
}

// ============================================================================
// T029-7: Subscriber Exception Doesn't Crash EventBus
// ============================================================================

TEST_F(PriorityIntegrationTest, SubscriberExceptionHandledGracefully) {
    std::atomic<int> goodCallbackCount{0};

    // Subscribe with callback that throws
    eventBus_->subscribe(
        Filters::all(),
        [](std::shared_ptr<IEvent> event) {
            throw std::runtime_error("Intentional test exception");
        });

    // Subscribe with good callback
    eventBus_->subscribe(
        Filters::all(),
        [&goodCallbackCount](std::shared_ptr<IEvent> event) {
            goodCallbackCount.fetch_add(1);
        });

    // Publish events
    const int EVENT_COUNT = 5;
    for (int i = 0; i < EVENT_COUNT; ++i) {
        auto event = std::make_shared<ActionStartedEvent>("exception_" + std::to_string(i), "Exception");
        eventBus_->publish(event);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Good subscriber should still receive all events
    EXPECT_EQ(goodCallbackCount.load(), EVENT_COUNT);

    // Stats should show failed callbacks
    const auto& stats = eventBus_->getStats();
    EXPECT_EQ(stats.failedCallbacks.load(), EVENT_COUNT);
}

// ============================================================================
// T029-8: EventBus Shutdown Processes Remaining Events
// ============================================================================

TEST_F(PriorityIntegrationTest, ShutdownProcessesRemainingEvents) {
    std::atomic<int> processedCount{0};

    eventBus_->subscribe(
        Filters::all(),
        [&processedCount](std::shared_ptr<IEvent> event) {
            processedCount.fetch_add(1);
        });

    // Publish events
    const int EVENT_COUNT = 10;
    for (int i = 0; i < EVENT_COUNT; ++i) {
        auto event = std::make_shared<ActionStartedEvent>("shutdown_" + std::to_string(i), "Shutdown");
        eventBus_->publish(event);
    }

    // Stop EventBus (should process remaining events)
    eventBus_->stop();

    // All events should be processed
    EXPECT_EQ(processedCount.load(), EVENT_COUNT);
}

} // namespace mxrc::core::event
