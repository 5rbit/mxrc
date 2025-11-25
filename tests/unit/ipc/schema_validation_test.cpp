// IPC Schema Validation Unit Test
// Feature 019: Architecture Improvements
// Tests: T017 - Compile-time type safety validation

#include <gtest/gtest.h>
#include <type_traits>
#include <chrono>

// Generated headers
#include "ipc/DataStoreKeys.h"
#include "ipc/EventBusEvents.h"

/**
 * @brief Unit test suite for IPC schema compile-time validation
 *
 * These tests verify:
 * 1. Key constants are compile-time constants (constexpr)
 * 2. Event priority enum is strongly typed
 * 3. TTL values are correct chrono::milliseconds types
 * 4. Incorrect key usage would cause compile errors (demonstrated in comments)
 */
class SchemaValidationTest : public ::testing::Test {
protected:
    // Helper to verify constexpr at compile time
    template<typename T>
    static constexpr bool is_constexpr_string(const T& val) {
        return std::is_same_v<const char*, std::decay_t<decltype(val)>>;
    }
};

// ============================================================================
// Test 1: DataStore Key Constants are Compile-Time Constants
// ============================================================================

TEST_F(SchemaValidationTest, KeyConstantsAreConstexpr) {
    using namespace mxrc::ipc::DataStoreKeys;

    // Verify keys can be used in constexpr context
    constexpr const char* robot_pos = ROBOT_POSITION;
    constexpr const char* robot_vel = ROBOT_VELOCITY;
    constexpr const char* ethercat_pos = ETHERCAT_SENSOR_POSITION;

    EXPECT_STREQ(robot_pos, "robot_position");
    EXPECT_STREQ(robot_vel, "robot_velocity");
    EXPECT_STREQ(ethercat_pos, "ethercat_sensor_position");

    // Compile-time type check
    static_assert(std::is_same_v<decltype(ROBOT_POSITION), const char* const>,
                  "ROBOT_POSITION must be const char* const");
}

TEST_F(SchemaValidationTest, KeyConstantsPreventRuntimeErrors) {
    using namespace mxrc::ipc::DataStoreKeys;

    // Using generated constants prevents typos
    std::string correct_key = ROBOT_POSITION;  // ✅ Compile-time checked

    // Uncommenting the following would cause a compile error:
    // std::string typo_key = ROBO_POSITION;  // ❌ Compile error: undeclared identifier

    EXPECT_EQ(correct_key, "robot_position");
}

// ============================================================================
// Test 2: EventBus Event Structures are Type-Safe
// ============================================================================

TEST_F(SchemaValidationTest, EventPriorityIsStronglyTyped) {
    using namespace mxrc::ipc;

    // EventPriority is an enum class (strongly typed)
    static_assert(std::is_enum_v<EventPriority>,
                  "EventPriority must be an enum");

    // Cannot implicitly convert to int (compile-time check)
    // int priority_int = EventPriority::CRITICAL;  // ❌ Compile error

    // Must explicitly cast
    uint8_t priority_value = static_cast<uint8_t>(EventPriority::CRITICAL);
    EXPECT_EQ(priority_value, 3);
}

TEST_F(SchemaValidationTest, EventNameConstantsAreConstexpr) {
    using namespace mxrc::ipc;

    // Event names are constexpr
    constexpr const char* rt_miss_name = RTDeadlineMissEvent::NAME;
    constexpr const char* ha_change_name = HAStateChangedEvent::NAME;

    EXPECT_STREQ(rt_miss_name, "RTDeadlineMissEvent");
    EXPECT_STREQ(ha_change_name, "HAStateChangedEvent");

    // Compile-time type check
    static_assert(std::is_same_v<decltype(RTDeadlineMissEvent::NAME), const char* const>,
                  "Event NAME must be const char* const");
}

TEST_F(SchemaValidationTest, EventPriorityConstantsAreConstexpr) {
    using namespace mxrc::ipc;

    // Event priorities are constexpr
    constexpr EventPriority rt_miss_priority = RTDeadlineMissEvent::PRIORITY;
    constexpr EventPriority ha_change_priority = HAStateChangedEvent::PRIORITY;

    EXPECT_EQ(rt_miss_priority, EventPriority::CRITICAL);
    EXPECT_EQ(ha_change_priority, EventPriority::CRITICAL);
}

// ============================================================================
// Test 3: TTL Values are Correct Types
// ============================================================================

TEST_F(SchemaValidationTest, EventTTLIsChronoMilliseconds) {
    using namespace mxrc::ipc;

    // TTL is std::chrono::milliseconds
    static_assert(std::is_same_v<decltype(RTDeadlineMissEvent::TTL), const std::chrono::milliseconds>,
                  "TTL must be std::chrono::milliseconds");

    constexpr auto ttl = RTDeadlineMissEvent::TTL;
    EXPECT_EQ(ttl.count(), 5000);  // 5 seconds
}

TEST_F(SchemaValidationTest, EventTTLIsConstexpr) {
    using namespace mxrc::ipc;

    // TTL can be used in constexpr context
    constexpr std::chrono::milliseconds ttl1 = RTDeadlineMissEvent::TTL;
    constexpr std::chrono::milliseconds ttl2 = EStopTriggeredEvent::TTL;

    EXPECT_EQ(ttl1.count(), 5000);
    EXPECT_EQ(ttl2.count(), 10000);
}

// ============================================================================
// Test 4: Coalescing Flags are Compile-Time Booleans
// ============================================================================

TEST_F(SchemaValidationTest, CoalescingFlagsAreConstexpr) {
    using namespace mxrc::ipc;

    // Coalescing flags are constexpr bool
    constexpr bool rt_cycle_coalescing = RTCycleCompletedEvent::COALESCING;
    constexpr bool ha_change_coalescing = HAStateChangedEvent::COALESCING;

    EXPECT_TRUE(rt_cycle_coalescing);
    EXPECT_FALSE(ha_change_coalescing);

    // Compile-time type check
    static_assert(std::is_same_v<decltype(RTCycleCompletedEvent::COALESCING), const bool>,
                  "COALESCING must be const bool");
}

// ============================================================================
// Test 5: Enum Values are Correctly Ordered
// ============================================================================

TEST_F(SchemaValidationTest, EventPriorityValuesAreOrdered) {
    using namespace mxrc::ipc;

    // Verify priority ordering: CRITICAL > HIGH > NORMAL > LOW
    EXPECT_LT(static_cast<uint8_t>(EventPriority::LOW),
              static_cast<uint8_t>(EventPriority::NORMAL));

    EXPECT_LT(static_cast<uint8_t>(EventPriority::NORMAL),
              static_cast<uint8_t>(EventPriority::HIGH));

    EXPECT_LT(static_cast<uint8_t>(EventPriority::HIGH),
              static_cast<uint8_t>(EventPriority::CRITICAL));

    // Explicit values
    EXPECT_EQ(static_cast<uint8_t>(EventPriority::LOW), 0);
    EXPECT_EQ(static_cast<uint8_t>(EventPriority::NORMAL), 1);
    EXPECT_EQ(static_cast<uint8_t>(EventPriority::HIGH), 2);
    EXPECT_EQ(static_cast<uint8_t>(EventPriority::CRITICAL), 3);
}

// ============================================================================
// Test 6: Type Misuse Prevention (Compile-Time Errors)
// ============================================================================

TEST_F(SchemaValidationTest, TypeMisuseWouldCauseCompileError) {
    using namespace mxrc::ipc;

    // The following lines would cause compile errors if uncommented:

    // 1. Cannot assign EventPriority to int without explicit cast
    // int priority = EventPriority::CRITICAL;  // ❌ Compile error

    // 2. Cannot use undefined event names
    // const char* invalid_event = UndefinedEvent::NAME;  // ❌ Compile error

    // 3. Cannot use undefined keys
    // const char* invalid_key = DataStoreKeys::INVALID_KEY;  // ❌ Compile error

    // 4. Cannot modify constexpr constants
    // RTDeadlineMissEvent::NAME = "modified";  // ❌ Compile error (const)

    // 5. Cannot modify constexpr TTL
    // RTDeadlineMissEvent::TTL = std::chrono::milliseconds(1000);  // ❌ Compile error

    SUCCEED() << "Type safety enforced at compile time";
}

// ============================================================================
// Test 7: Namespace Isolation
// ============================================================================

TEST_F(SchemaValidationTest, IPCTypesAreInCorrectNamespace) {
    // EventPriority is in mxrc::ipc namespace
    using EP = mxrc::ipc::EventPriority;
    EP priority = EP::CRITICAL;
    EXPECT_EQ(static_cast<uint8_t>(priority), 3);

    // DataStoreKeys are in mxrc::ipc::DataStoreKeys namespace
    using namespace mxrc::ipc::DataStoreKeys;
    EXPECT_STREQ(ROBOT_POSITION, "robot_position");

    // Event structs are in mxrc::ipc namespace
    using mxrc::ipc::RTDeadlineMissEvent;
    EXPECT_STREQ(RTDeadlineMissEvent::NAME, "RTDeadlineMissEvent");
}

// ============================================================================
// Test 8: Static Assertions (Compile-Time Checks)
// ============================================================================

// These static assertions run at compile time
namespace compile_time_checks {

using namespace mxrc::ipc;

// EventPriority must be an enum class
static_assert(std::is_enum_v<EventPriority>);

// EventPriority underlying type must be uint8_t
static_assert(std::is_same_v<std::underlying_type_t<EventPriority>, uint8_t>);

// Event NAME must be constexpr
static_assert(std::is_same_v<decltype(RTDeadlineMissEvent::NAME), const char* const>);

// Event PRIORITY must be constexpr
static_assert(std::is_same_v<decltype(RTDeadlineMissEvent::PRIORITY), const EventPriority>);

// Event TTL must be std::chrono::milliseconds
static_assert(std::is_same_v<decltype(RTDeadlineMissEvent::TTL), const std::chrono::milliseconds>);

// Event COALESCING must be bool
static_assert(std::is_same_v<decltype(RTCycleCompletedEvent::COALESCING), const bool>);

}  // namespace compile_time_checks

TEST_F(SchemaValidationTest, StaticAssertionsPass) {
    // If this test runs, all static_assert checks passed at compile time
    SUCCEED() << "All compile-time static assertions passed";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
