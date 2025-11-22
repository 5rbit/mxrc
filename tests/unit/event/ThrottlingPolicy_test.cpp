#include <gtest/gtest.h>
#include "core/event/adapters/ThrottlingPolicy.h"
#include <thread>
#include <chrono>

using namespace mxrc::core::event;

/**
 * @brief Unit tests for Feature 022 P3: ThrottlingPolicy
 *
 * Test Coverage:
 * - Basic throttling behavior (allow first, throttle subsequent)
 * - Multiple event types (independent throttling)
 * - Time-based throttling (allow after interval)
 * - Reset functionality
 * - Thread safety
 */
class ThrottlingPolicyTest : public ::testing::Test {
protected:
    void SetUp() override {
        policy_ = std::make_unique<ThrottlingPolicy>(100);  // 100ms throttle interval
    }

    std::unique_ptr<ThrottlingPolicy> policy_;
};

// ============================================================================
// Basic Throttling Tests
// ============================================================================

TEST_F(ThrottlingPolicyTest, FirstEvent_AlwaysAllowed) {
    EXPECT_TRUE(policy_->shouldSend("temperature"));
}

TEST_F(ThrottlingPolicyTest, ImmediateRepeat_Throttled) {
    EXPECT_TRUE(policy_->shouldSend("temperature"));   // First: allowed
    EXPECT_FALSE(policy_->shouldSend("temperature"));  // Immediate repeat: throttled
}

TEST_F(ThrottlingPolicyTest, MultipleImmediateRepeats_AllThrottled) {
    EXPECT_TRUE(policy_->shouldSend("temperature"));   // First: allowed

    for (int i = 0; i < 10; ++i) {
        EXPECT_FALSE(policy_->shouldSend("temperature"));  // All repeats: throttled
    }
}

// ============================================================================
// Time-Based Throttling Tests
// ============================================================================

TEST_F(ThrottlingPolicyTest, AfterInterval_Allowed) {
    EXPECT_TRUE(policy_->shouldSend("temperature"));   // First: allowed
    EXPECT_FALSE(policy_->shouldSend("temperature"));  // Immediate: throttled

    // Wait for throttle interval to pass
    std::this_thread::sleep_for(std::chrono::milliseconds(105));

    EXPECT_TRUE(policy_->shouldSend("temperature"));  // After interval: allowed
}

TEST_F(ThrottlingPolicyTest, MultipleIntervals_AllAllowed) {
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(policy_->shouldSend("temperature"));  // First of interval: allowed
        std::this_thread::sleep_for(std::chrono::milliseconds(105));
    }
}

TEST_F(ThrottlingPolicyTest, JustBeforeInterval_StillThrottled) {
    EXPECT_TRUE(policy_->shouldSend("temperature"));  // First: allowed

    // Wait 95ms (just before 100ms interval)
    std::this_thread::sleep_for(std::chrono::milliseconds(95));

    EXPECT_FALSE(policy_->shouldSend("temperature"));  // Still throttled
}

// ============================================================================
// Multiple Event Types Tests
// ============================================================================

TEST_F(ThrottlingPolicyTest, DifferentEventTypes_IndependentThrottling) {
    EXPECT_TRUE(policy_->shouldSend("temperature"));  // First: allowed
    EXPECT_TRUE(policy_->shouldSend("pressure"));     // Different type: allowed
    EXPECT_TRUE(policy_->shouldSend("humidity"));     // Different type: allowed

    EXPECT_FALSE(policy_->shouldSend("temperature"));  // Repeat: throttled
    EXPECT_FALSE(policy_->shouldSend("pressure"));     // Repeat: throttled
    EXPECT_FALSE(policy_->shouldSend("humidity"));     // Repeat: throttled
}

TEST_F(ThrottlingPolicyTest, MultipleTypes_IndependentTimers) {
    EXPECT_TRUE(policy_->shouldSend("temperature"));  // T=0: allowed

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(policy_->shouldSend("pressure"));  // T=50: allowed (different type)

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    // T=110: temperature throttle expired (110ms since first)
    EXPECT_TRUE(policy_->shouldSend("temperature"));  // Allowed

    // T=110: pressure throttle NOT expired (only 60ms since)
    EXPECT_FALSE(policy_->shouldSend("pressure"));  // Still throttled
}

// ============================================================================
// Reset Tests
// ============================================================================

TEST_F(ThrottlingPolicyTest, Reset_ClearsAllState) {
    EXPECT_TRUE(policy_->shouldSend("temperature"));
    EXPECT_FALSE(policy_->shouldSend("temperature"));  // Throttled

    policy_->reset();

    EXPECT_TRUE(policy_->shouldSend("temperature"));  // Allowed after reset
}

TEST_F(ThrottlingPolicyTest, Reset_AffectsAllEventTypes) {
    EXPECT_TRUE(policy_->shouldSend("temperature"));
    EXPECT_TRUE(policy_->shouldSend("pressure"));
    EXPECT_FALSE(policy_->shouldSend("temperature"));  // Throttled
    EXPECT_FALSE(policy_->shouldSend("pressure"));     // Throttled

    policy_->reset();

    EXPECT_TRUE(policy_->shouldSend("temperature"));  // Allowed after reset
    EXPECT_TRUE(policy_->shouldSend("pressure"));     // Allowed after reset
}

TEST_F(ThrottlingPolicyTest, ResetEventType_OnlyAffectsSpecificType) {
    EXPECT_TRUE(policy_->shouldSend("temperature"));
    EXPECT_TRUE(policy_->shouldSend("pressure"));
    EXPECT_FALSE(policy_->shouldSend("temperature"));  // Throttled
    EXPECT_FALSE(policy_->shouldSend("pressure"));     // Throttled

    policy_->resetEventType("temperature");

    EXPECT_TRUE(policy_->shouldSend("temperature"));   // Allowed after reset
    EXPECT_FALSE(policy_->shouldSend("pressure"));     // Still throttled
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(ThrottlingPolicyTest, GetThrottleInterval_ReturnsCorrectValue) {
    EXPECT_EQ(policy_->getThrottleInterval(), 100u);
}

TEST_F(ThrottlingPolicyTest, CustomInterval_Works) {
    auto custom_policy = std::make_unique<ThrottlingPolicy>(50);  // 50ms interval

    EXPECT_TRUE(custom_policy->shouldSend("temperature"));
    EXPECT_FALSE(custom_policy->shouldSend("temperature"));

    std::this_thread::sleep_for(std::chrono::milliseconds(55));

    EXPECT_TRUE(custom_policy->shouldSend("temperature"));  // Allowed after 50ms
}

TEST_F(ThrottlingPolicyTest, ZeroInterval_AllowsAllEvents) {
    auto zero_policy = std::make_unique<ThrottlingPolicy>(0);  // No throttling

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(zero_policy->shouldSend("temperature"));  // All allowed
    }
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(ThrottlingPolicyTest, ThreadSafety_ConcurrentAccess) {
    std::atomic<int> allowed_count{0};
    std::atomic<int> throttled_count{0};

    const int NUM_THREADS = 4;
    const int EVENTS_PER_THREAD = 1000;

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < EVENTS_PER_THREAD; ++i) {
                std::string event_type = "event_" + std::to_string(t % 4);
                if (policy_->shouldSend(event_type)) {
                    allowed_count.fetch_add(1);
                } else {
                    throttled_count.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // At least some events should be throttled
    EXPECT_GT(throttled_count.load(), 0);
    // At least some events should be allowed
    EXPECT_GT(allowed_count.load(), 0);
    // Total should match
    EXPECT_EQ(allowed_count.load() + throttled_count.load(), NUM_THREADS * EVENTS_PER_THREAD);
}

TEST_F(ThrottlingPolicyTest, ThreadSafety_ConcurrentReset) {
    std::atomic<bool> stop{false};

    // Thread that continuously sends events
    std::thread sender([&]() {
        while (!stop.load()) {
            policy_->shouldSend("temperature");
        }
    });

    // Thread that continuously resets
    std::thread resetter([&]() {
        for (int i = 0; i < 100; ++i) {
            policy_->reset();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        stop.store(true);
    });

    sender.join();
    resetter.join();

    // No crash or deadlock = success
    SUCCEED();
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(ThrottlingPolicyTest, EmptyEventType_Works) {
    EXPECT_TRUE(policy_->shouldSend(""));   // First: allowed
    EXPECT_FALSE(policy_->shouldSend(""));  // Repeat: throttled
}

TEST_F(ThrottlingPolicyTest, VeryLongEventType_Works) {
    std::string long_type(1000, 'a');

    EXPECT_TRUE(policy_->shouldSend(long_type));   // First: allowed
    EXPECT_FALSE(policy_->shouldSend(long_type));  // Repeat: throttled
}

// Main is provided by the run_tests executable
