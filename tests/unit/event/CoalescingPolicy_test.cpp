#include <gtest/gtest.h>
#include "core/event/adapters/CoalescingPolicy.h"
#include <thread>
#include <chrono>

using namespace mxrc::core::event;

/**
 * @brief Unit tests for Feature 022 P3: CoalescingPolicy
 *
 * Test Coverage:
 * - Basic coalescing (merge events within window)
 * - Window expiration (flush after window)
 * - Multiple event types (independent coalescing)
 * - Flush functionality
 * - Thread safety
 */
class CoalescingPolicyTest : public ::testing::Test {
protected:
    void SetUp() override {
        policy_ = std::make_unique<CoalescingPolicy>(100);  // 100ms coalescing window
    }

    std::unique_ptr<CoalescingPolicy> policy_;
};

// Helper function to create PrioritizedEvent
PrioritizedEvent makeEvent(const std::string& type, double value, EventPriority priority = EventPriority::NORMAL) {
    PrioritizedEvent event;
    event.type = type;
    event.priority = priority;
    event.payload = value;
    auto now = std::chrono::system_clock::now();
    event.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    event.sequence_num = 0;
    return event;
}

// ============================================================================
// Basic Coalescing Tests
// ============================================================================

TEST_F(CoalescingPolicyTest, FirstEvent_Stored) {
    auto result = policy_->coalesce(makeEvent("temperature", 25.0));
    EXPECT_FALSE(result.has_value());  // First event is stored, not returned
    EXPECT_EQ(policy_->getPendingCount(), 1u);
}

TEST_F(CoalescingPolicyTest, ImmediateRepeat_Merged) {
    policy_->coalesce(makeEvent("temperature", 25.0));
    auto result = policy_->coalesce(makeEvent("temperature", 25.5));

    EXPECT_FALSE(result.has_value());  // Merged, not returned
    EXPECT_EQ(policy_->getPendingCount(), 1u);  // Still only 1 pending
}

TEST_F(CoalescingPolicyTest, MultipleImmediateRepeats_AllMerged) {
    policy_->coalesce(makeEvent("temperature", 25.0));

    for (int i = 1; i <= 10; ++i) {
        auto result = policy_->coalesce(makeEvent("temperature", 25.0 + i * 0.1));
        EXPECT_FALSE(result.has_value());  // All merged
    }

    EXPECT_EQ(policy_->getPendingCount(), 1u);  // Still only 1 pending
}

// ============================================================================
// Window Expiration Tests
// ============================================================================

TEST_F(CoalescingPolicyTest, AfterWindow_PreviousEventFlushed) {
    policy_->coalesce(makeEvent("temperature", 25.0));

    // Wait for window to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(105));

    auto result = policy_->coalesce(makeEvent("temperature", 26.0));

    EXPECT_TRUE(result.has_value());  // Previous event returned
    EXPECT_EQ(result->type, "temperature");
    EXPECT_EQ(policy_->getPendingCount(), 1u);  // New event is now pending
}

TEST_F(CoalescingPolicyTest, MultipleWindows_EachFlushes) {
    for (int i = 0; i < 5; ++i) {
        auto result = policy_->coalesce(makeEvent("temperature", 25.0 + i));

        if (i == 0) {
            EXPECT_FALSE(result.has_value());  // First event stored
        } else {
            EXPECT_TRUE(result.has_value());  // Previous event flushed
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(105));
    }
}

TEST_F(CoalescingPolicyTest, JustBeforeWindow_StillMerged) {
    policy_->coalesce(makeEvent("temperature", 25.0));

    // Wait 95ms (just before 100ms window)
    std::this_thread::sleep_for(std::chrono::milliseconds(95));

    auto result = policy_->coalesce(makeEvent("temperature", 25.5));

    EXPECT_FALSE(result.has_value());  // Still merged, not flushed
    EXPECT_EQ(policy_->getPendingCount(), 1u);
}

// ============================================================================
// Multiple Event Types Tests
// ============================================================================

TEST_F(CoalescingPolicyTest, DifferentEventTypes_IndependentCoalescing) {
    policy_->coalesce(makeEvent("temperature", 25.0));
    policy_->coalesce(makeEvent("pressure", 1013.0));
    policy_->coalesce(makeEvent("humidity", 60.0));

    EXPECT_EQ(policy_->getPendingCount(), 3u);  // All stored independently
}

TEST_F(CoalescingPolicyTest, MultipleTypes_IndependentWindows) {
    policy_->coalesce(makeEvent("temperature", 25.0));  // T=0

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    policy_->coalesce(makeEvent("pressure", 1013.0));  // T=50

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    // T=110: temperature window expired (110ms since first)
    auto temp_result = policy_->coalesce(makeEvent("temperature", 26.0));
    EXPECT_TRUE(temp_result.has_value());  // Flushed

    // T=110: pressure window NOT expired (only 60ms since)
    auto pressure_result = policy_->coalesce(makeEvent("pressure", 1014.0));
    EXPECT_FALSE(pressure_result.has_value());  // Still merged
}

TEST_F(CoalescingPolicyTest, MergePreservesLatestData) {
    policy_->coalesce(makeEvent("temperature", 25.0, EventPriority::LOW));
    policy_->coalesce(makeEvent("temperature", 30.0, EventPriority::HIGH));

    std::this_thread::sleep_for(std::chrono::milliseconds(105));

    auto result = policy_->coalesce(makeEvent("temperature", 35.0));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, "temperature");
    EXPECT_EQ(result->priority, EventPriority::HIGH);  // Latest priority preserved
}

// ============================================================================
// Flush Tests
// ============================================================================

TEST_F(CoalescingPolicyTest, Flush_ReturnsAllPending) {
    policy_->coalesce(makeEvent("temperature", 25.0));
    policy_->coalesce(makeEvent("pressure", 1013.0));
    policy_->coalesce(makeEvent("humidity", 60.0));

    auto flushed = policy_->flush();

    EXPECT_EQ(flushed.size(), 3u);
    EXPECT_EQ(policy_->getPendingCount(), 0u);
}

TEST_F(CoalescingPolicyTest, Flush_ClearsAllPending) {
    policy_->coalesce(makeEvent("temperature", 25.0));
    policy_->coalesce(makeEvent("pressure", 1013.0));

    policy_->flush();

    EXPECT_EQ(policy_->getPendingCount(), 0u);
}

TEST_F(CoalescingPolicyTest, FlushEventType_OnlyAffectsSpecificType) {
    policy_->coalesce(makeEvent("temperature", 25.0));
    policy_->coalesce(makeEvent("pressure", 1013.0));

    auto result = policy_->flushEventType("temperature");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, "temperature");
    EXPECT_EQ(policy_->getPendingCount(), 1u);  // Pressure still pending
}

TEST_F(CoalescingPolicyTest, FlushEventType_NonexistentType_ReturnsNullopt) {
    auto result = policy_->flushEventType("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(CoalescingPolicyTest, AfterFlush_NewEventsStoredNormally) {
    policy_->coalesce(makeEvent("temperature", 25.0));
    policy_->flush();

    auto result = policy_->coalesce(makeEvent("temperature", 26.0));

    EXPECT_FALSE(result.has_value());  // New event stored
    EXPECT_EQ(policy_->getPendingCount(), 1u);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(CoalescingPolicyTest, GetCoalesceWindow_ReturnsCorrectValue) {
    EXPECT_EQ(policy_->getCoalesceWindow(), 100u);
}

TEST_F(CoalescingPolicyTest, CustomWindow_Works) {
    auto custom_policy = std::make_unique<CoalescingPolicy>(50);  // 50ms window

    custom_policy->coalesce(makeEvent("temperature", 25.0));
    std::this_thread::sleep_for(std::chrono::milliseconds(55));

    auto result = custom_policy->coalesce(makeEvent("temperature", 26.0));
    EXPECT_TRUE(result.has_value());  // Flushed after 50ms
}

TEST_F(CoalescingPolicyTest, ZeroWindow_FlushesImmediately) {
    auto zero_policy = std::make_unique<CoalescingPolicy>(0);  // No coalescing

    zero_policy->coalesce(makeEvent("temperature", 25.0));
    auto result = zero_policy->coalesce(makeEvent("temperature", 26.0));

    EXPECT_TRUE(result.has_value());  // Immediately flushed
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(CoalescingPolicyTest, ThreadSafety_ConcurrentCoalesce) {
    std::atomic<int> flushed_count{0};

    const int NUM_THREADS = 4;
    const int EVENTS_PER_THREAD = 1000;

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < EVENTS_PER_THREAD; ++i) {
                std::string event_type = "event_" + std::to_string(t % 4);
                auto result = policy_->coalesce(makeEvent(event_type, static_cast<double>(i)));
                if (result.has_value()) {
                    flushed_count.fetch_add(1);
                }
                // Add small delay to allow window expiration
                if (i % 100 == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Either some events were flushed OR some are still pending
    // (Both scenarios are valid depending on timing)
    EXPECT_TRUE(flushed_count.load() > 0 || policy_->getPendingCount() > 0);
}

TEST_F(CoalescingPolicyTest, ThreadSafety_ConcurrentFlush) {
    std::atomic<bool> stop{false};

    // Thread that continuously coalesces events
    std::thread coalescer([&]() {
        int counter = 0;
        while (!stop.load()) {
            policy_->coalesce(makeEvent("temperature", static_cast<double>(counter++)));
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Thread that continuously flushes
    std::thread flusher([&]() {
        for (int i = 0; i < 100; ++i) {
            policy_->flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        stop.store(true);
    });

    coalescer.join();
    flusher.join();

    // No crash or deadlock = success
    SUCCEED();
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(CoalescingPolicyTest, EmptyEventType_Works) {
    auto result = policy_->coalesce(makeEvent("", 0.0));
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(policy_->getPendingCount(), 1u);
}

TEST_F(CoalescingPolicyTest, VeryLongEventType_Works) {
    std::string long_type(1000, 'a');

    auto result = policy_->coalesce(makeEvent(long_type, 0.0));
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(policy_->getPendingCount(), 1u);
}

// Main is provided by the run_tests executable
