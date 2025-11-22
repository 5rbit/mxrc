#include <gtest/gtest.h>
#include "core/event/core/PrioritizedEvent.h"
#include <chrono>

using namespace mxrc::core::event;

/**
 * @brief Unit tests for PrioritizedEvent structure (Feature 022 P3)
 *
 * Validates:
 * - Size constraint (< 128 bytes)
 * - Priority ordering
 * - Move semantics
 * - Helper functions
 */
class PrioritizedEventTest : public ::testing::Test {
protected:
    uint64_t getCurrentTimeNs() {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
    }
};

// ============================================================================
// Size and Layout Tests
// ============================================================================

TEST_F(PrioritizedEventTest, SizeConstraint_LessThan128Bytes) {
    size_t size = sizeof(PrioritizedEvent);
    std::cout << "sizeof(PrioritizedEvent) = " << size << " bytes" << std::endl;

    EXPECT_LT(size, 128u) << "PrioritizedEvent must be < 128 bytes for cache efficiency";
}

TEST_F(PrioritizedEventTest, MoveSemantics_NoexceptGuarantee) {
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<PrioritizedEvent>)
        << "Move constructor should be noexcept for lock-free queue compatibility";

    EXPECT_TRUE(std::is_nothrow_move_assignable_v<PrioritizedEvent>)
        << "Move assignment should be noexcept for lock-free queue compatibility";
}

// ============================================================================
// Priority Ordering Tests
// ============================================================================

TEST_F(PrioritizedEventTest, PriorityOrdering_CriticalBeforeNormal) {
    PrioritizedEvent critical{
        .type = "critical",
        .priority = EventPriority::CRITICAL,
        .payload = 1,
        .timestamp_ns = 1000,
        .sequence_num = 0
    };

    PrioritizedEvent normal{
        .type = "normal",
        .priority = EventPriority::NORMAL,
        .payload = 2,
        .timestamp_ns = 1000,
        .sequence_num = 0
    };

    // In max-heap (std::priority_queue), smaller value has higher priority
    // So critical > normal means critical comes out first
    EXPECT_TRUE(normal < critical) << "CRITICAL should have higher priority than NORMAL";
}

TEST_F(PrioritizedEventTest, PriorityOrdering_AllLevels) {
    PrioritizedEvent critical{.priority = EventPriority::CRITICAL, .timestamp_ns = 1000};
    PrioritizedEvent high{.priority = EventPriority::HIGH, .timestamp_ns = 1000};
    PrioritizedEvent normal{.priority = EventPriority::NORMAL, .timestamp_ns = 1000};
    PrioritizedEvent low{.priority = EventPriority::LOW, .timestamp_ns = 1000};

    // Verify order: CRITICAL > HIGH > NORMAL > LOW
    EXPECT_TRUE(high < critical);
    EXPECT_TRUE(normal < high);
    EXPECT_TRUE(low < normal);
}

TEST_F(PrioritizedEventTest, TimestampOrdering_WithinSamePriority) {
    PrioritizedEvent older{
        .priority = EventPriority::NORMAL,
        .timestamp_ns = 1000,
        .sequence_num = 0
    };

    PrioritizedEvent newer{
        .priority = EventPriority::NORMAL,
        .timestamp_ns = 2000,
        .sequence_num = 0
    };

    // Older timestamp should come first
    EXPECT_TRUE(newer < older) << "Older events should be processed first";
}

TEST_F(PrioritizedEventTest, SequenceOrdering_WithinSameTimestamp) {
    PrioritizedEvent first{
        .priority = EventPriority::NORMAL,
        .timestamp_ns = 1000,
        .sequence_num = 1
    };

    PrioritizedEvent second{
        .priority = EventPriority::NORMAL,
        .timestamp_ns = 1000,
        .sequence_num = 2
    };

    // Lower sequence number should come first
    EXPECT_TRUE(second < first) << "Lower sequence number should be processed first";
}

// ============================================================================
// Helper Function Tests
// ============================================================================

TEST_F(PrioritizedEventTest, MakePrioritizedEvent_IntPayload) {
    auto event = makePrioritizedEvent("test.event", EventPriority::HIGH, 42, 1);

    EXPECT_EQ(event.type, "test.event");
    EXPECT_EQ(event.priority, EventPriority::HIGH);
    EXPECT_EQ(std::get<int>(event.payload), 42);
    EXPECT_GT(event.timestamp_ns, 0u);
    EXPECT_EQ(event.sequence_num, 1u);
}

TEST_F(PrioritizedEventTest, MakePrioritizedEvent_DoublePayload) {
    auto event = makePrioritizedEvent("sensor.temp", EventPriority::NORMAL, 25.5, 2);

    EXPECT_EQ(event.type, "sensor.temp");
    EXPECT_EQ(event.priority, EventPriority::NORMAL);
    EXPECT_DOUBLE_EQ(std::get<double>(event.payload), 25.5);
    EXPECT_GT(event.timestamp_ns, 0u);
    EXPECT_EQ(event.sequence_num, 2u);
}

TEST_F(PrioritizedEventTest, MakePrioritizedEvent_StringPayload) {
    auto event = makePrioritizedEvent("error.msg", EventPriority::CRITICAL, std::string("timeout"), 3);

    EXPECT_EQ(event.type, "error.msg");
    EXPECT_EQ(event.priority, EventPriority::CRITICAL);
    EXPECT_EQ(std::get<std::string>(event.payload), "timeout");
    EXPECT_GT(event.timestamp_ns, 0u);
    EXPECT_EQ(event.sequence_num, 3u);
}

// ============================================================================
// Priority String Conversion Tests
// ============================================================================

TEST_F(PrioritizedEventTest, PriorityToString_AllLevels) {
    EXPECT_STREQ(priorityToString(EventPriority::CRITICAL), "CRITICAL");
    EXPECT_STREQ(priorityToString(EventPriority::HIGH), "HIGH");
    EXPECT_STREQ(priorityToString(EventPriority::NORMAL), "NORMAL");
    EXPECT_STREQ(priorityToString(EventPriority::LOW), "LOW");
}

// ============================================================================
// Copy and Move Behavior Tests
// ============================================================================

TEST_F(PrioritizedEventTest, CopyConstructor_PreservesValues) {
    PrioritizedEvent original{
        .type = "original",
        .priority = EventPriority::HIGH,
        .payload = 123,
        .timestamp_ns = 5000,
        .sequence_num = 10
    };

    PrioritizedEvent copy = original;

    EXPECT_EQ(copy.type, original.type);
    EXPECT_EQ(copy.priority, original.priority);
    EXPECT_EQ(std::get<int>(copy.payload), std::get<int>(original.payload));
    EXPECT_EQ(copy.timestamp_ns, original.timestamp_ns);
    EXPECT_EQ(copy.sequence_num, original.sequence_num);
}

TEST_F(PrioritizedEventTest, MoveConstructor_TransfersOwnership) {
    PrioritizedEvent original{
        .type = "movable",
        .priority = EventPriority::NORMAL,
        .payload = std::string("large string to avoid SSO"),
        .timestamp_ns = 6000,
        .sequence_num = 11
    };

    std::string expected_str = std::get<std::string>(original.payload);

    PrioritizedEvent moved = std::move(original);

    EXPECT_EQ(moved.type, "movable");
    EXPECT_EQ(moved.priority, EventPriority::NORMAL);
    EXPECT_EQ(std::get<std::string>(moved.payload), expected_str);
    EXPECT_EQ(moved.timestamp_ns, 6000u);
    EXPECT_EQ(moved.sequence_num, 11u);
}

// ============================================================================
// Payload Variant Tests
// ============================================================================

TEST_F(PrioritizedEventTest, PayloadVariant_HoldsInt) {
    PrioritizedEvent event;
    event.payload = 42;

    EXPECT_TRUE(std::holds_alternative<int>(event.payload));
    EXPECT_EQ(std::get<int>(event.payload), 42);
}

TEST_F(PrioritizedEventTest, PayloadVariant_HoldsDouble) {
    PrioritizedEvent event;
    event.payload = 3.14159;

    EXPECT_TRUE(std::holds_alternative<double>(event.payload));
    EXPECT_DOUBLE_EQ(std::get<double>(event.payload), 3.14159);
}

TEST_F(PrioritizedEventTest, PayloadVariant_HoldsString) {
    PrioritizedEvent event;
    event.payload = std::string("test message");

    EXPECT_TRUE(std::holds_alternative<std::string>(event.payload));
    EXPECT_EQ(std::get<std::string>(event.payload), "test message");
}

// Main is provided by the run_tests executable
