// IPC Schema Code Generation Pipeline Integration Test
// Feature 019: Architecture Improvements
// Tests: T016 - Schema validation, code generation, and type safety

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>

// Generated headers from IPC schema
#include "ipc/DataStoreKeys.h"
#include "ipc/EventBusEvents.h"

namespace fs = std::filesystem;

/**
 * @brief Integration test suite for IPC schema code generation pipeline
 *
 * Validates:
 * 1. YAML schema → C++ code generation workflow
 * 2. Generated headers are valid and compilable
 * 3. Type-safe key constants are accessible
 * 4. EventBus event structures are correct
 */
class IPCSchemaIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Verify generated files exist
        generated_ipc_dir_ = fs::path(CMAKE_BINARY_DIR) / "generated" / "ipc";
        datastore_keys_path_ = generated_ipc_dir_ / "DataStoreKeys.h";
        eventbus_events_path_ = generated_ipc_dir_ / "EventBusEvents.h";
    }

    fs::path generated_ipc_dir_;
    fs::path datastore_keys_path_;
    fs::path eventbus_events_path_;
};

// ============================================================================
// Test 1: Code Generation Pipeline
// ============================================================================

TEST_F(IPCSchemaIntegrationTest, GeneratedFilesExist) {
    ASSERT_TRUE(fs::exists(generated_ipc_dir_))
        << "Generated IPC directory does not exist: " << generated_ipc_dir_;

    ASSERT_TRUE(fs::exists(datastore_keys_path_))
        << "DataStoreKeys.h not generated: " << datastore_keys_path_;

    ASSERT_TRUE(fs::exists(eventbus_events_path_))
        << "EventBusEvents.h not generated: " << eventbus_events_path_;
}

TEST_F(IPCSchemaIntegrationTest, GeneratedHeadersAreValid) {
    // This test passes if the file compiles (headers are already included)
    SUCCEED() << "Generated headers compiled successfully";
}

// ============================================================================
// Test 2: DataStore Key Constants
// ============================================================================

TEST_F(IPCSchemaIntegrationTest, DataStoreKeysAreAccessible) {
    using namespace mxrc::ipc::DataStoreKeys;

    // Test Hot Key constants (64-axis motor data)
    EXPECT_STREQ(ROBOT_POSITION, "robot_position");
    EXPECT_STREQ(ROBOT_VELOCITY, "robot_velocity");
    EXPECT_STREQ(ROBOT_ACCELERATION, "robot_acceleration");

    // Test EtherCAT 64-axis motor keys
    EXPECT_STREQ(ETHERCAT_SENSOR_POSITION, "ethercat_sensor_position");
    EXPECT_STREQ(ETHERCAT_SENSOR_VELOCITY, "ethercat_sensor_velocity");
    EXPECT_STREQ(ETHERCAT_TARGET_POSITION, "ethercat_target_position");
    EXPECT_STREQ(ETHERCAT_TARGET_VELOCITY, "ethercat_target_velocity");
    EXPECT_STREQ(ETHERCAT_MOTOR_TORQUE, "ethercat_motor_torque");

    // Test EtherCAT 64-module IO keys
    EXPECT_STREQ(ETHERCAT_DIGITAL_INPUT, "ethercat_digital_input");
    EXPECT_STREQ(ETHERCAT_DIGITAL_OUTPUT, "ethercat_digital_output");
    EXPECT_STREQ(ETHERCAT_ANALOG_INPUT, "ethercat_analog_input");
    EXPECT_STREQ(ETHERCAT_ANALOG_OUTPUT, "ethercat_analog_output");

    // Test RT performance keys
    EXPECT_STREQ(RT_CYCLE_TIME_US, "rt_cycle_time_us");
    EXPECT_STREQ(RT_DEADLINE_MISS_COUNT, "rt_deadline_miss_count");

    // Test task and HA state keys
    EXPECT_STREQ(TASK_CURRENT_ID, "task_current_id");
    EXPECT_STREQ(TASK_STATUS, "task_status");
    EXPECT_STREQ(HA_CURRENT_STATE, "ha_current_state");
}

TEST_F(IPCSchemaIntegrationTest, KeyConstantsAreConstexpr) {
    using namespace mxrc::ipc::DataStoreKeys;

    // Compile-time constant verification (constexpr can be used in constant expressions)
    constexpr const char* robot_pos = ROBOT_POSITION;
    constexpr const char* ethercat_pos = ETHERCAT_SENSOR_POSITION;

    EXPECT_STREQ(robot_pos, "robot_position");
    EXPECT_STREQ(ethercat_pos, "ethercat_sensor_position");
}

// ============================================================================
// Test 3: EventBus Event Structures
// ============================================================================

TEST_F(IPCSchemaIntegrationTest, EventBusEventStructsExist) {
    using namespace mxrc::ipc;

    // Test event name constants
    EXPECT_STREQ(RTDeadlineMissEvent::NAME, "RTDeadlineMissEvent");
    EXPECT_STREQ(HAStateChangedEvent::NAME, "HAStateChangedEvent");
    EXPECT_STREQ(TaskCompletedEvent::NAME, "TaskCompletedEvent");
    EXPECT_STREQ(TaskFailedEvent::NAME, "TaskFailedEvent");
}

TEST_F(IPCSchemaIntegrationTest, EventPriorityLevelsAreCorrect) {
    using namespace mxrc::ipc;

    // CRITICAL priority events
    EXPECT_EQ(RTDeadlineMissEvent::PRIORITY, EventPriority::CRITICAL);
    EXPECT_EQ(HAStateChangedEvent::PRIORITY, EventPriority::CRITICAL);
    EXPECT_EQ(EStopTriggeredEvent::PRIORITY, EventPriority::CRITICAL);

    // HIGH priority events
    EXPECT_EQ(TaskFailedEvent::PRIORITY, EventPriority::HIGH);

    // NORMAL priority events
    EXPECT_EQ(TaskCompletedEvent::PRIORITY, EventPriority::NORMAL);
    EXPECT_EQ(TaskStartedEvent::PRIORITY, EventPriority::NORMAL);
}

TEST_F(IPCSchemaIntegrationTest, EventTTLValuesAreCorrect) {
    using namespace mxrc::ipc;

    // Events with TTL
    EXPECT_EQ(RTDeadlineMissEvent::TTL.count(), 5000);  // 5 seconds
    EXPECT_EQ(EStopTriggeredEvent::TTL.count(), 10000);  // 10 seconds
}

TEST_F(IPCSchemaIntegrationTest, EventCoalescingFlagsAreCorrect) {
    using namespace mxrc::ipc;

    // Events with coalescing enabled
    EXPECT_TRUE(RTCycleCompletedEvent::COALESCING);

    // Events without coalescing
    EXPECT_FALSE(HAStateChangedEvent::COALESCING);
    EXPECT_FALSE(EStopTriggeredEvent::COALESCING);
}

// ============================================================================
// Test 4: Type Safety (Compile-Time Checks)
// ============================================================================

TEST_F(IPCSchemaIntegrationTest, KeyConstantsPreventTypos) {
    using namespace mxrc::ipc::DataStoreKeys;

    // Using generated constants prevents runtime typo errors
    std::string key1 = ROBOT_POSITION;  // Compile-time checked
    // std::string key2 = "robo_position";  // Runtime typo (not caught)

    EXPECT_EQ(key1, "robot_position");
}

TEST_F(IPCSchemaIntegrationTest, EventPriorityEnumIsTypeSafe) {
    using namespace mxrc::ipc;

    // EventPriority is a strongly-typed enum
    EventPriority p1 = EventPriority::CRITICAL;
    EventPriority p2 = EventPriority::LOW;

    EXPECT_NE(p1, p2);
    EXPECT_EQ(static_cast<uint8_t>(EventPriority::CRITICAL), 3);
    EXPECT_EQ(static_cast<uint8_t>(EventPriority::LOW), 0);
}

// ============================================================================
// Test 5: Schema Versioning
// ============================================================================

TEST_F(IPCSchemaIntegrationTest, SchemaVersionIsEmbedded) {
    // Read generated header to check schema version comment
    std::ifstream file(datastore_keys_path_);
    ASSERT_TRUE(file.is_open()) << "Failed to open DataStoreKeys.h";

    std::string line;
    bool found_version = false;
    while (std::getline(file, line)) {
        if (line.find("Schema version: 1.0.0") != std::string::npos) {
            found_version = true;
            break;
        }
    }

    EXPECT_TRUE(found_version) << "Schema version 1.0.0 not found in generated header";
}

// ============================================================================
// Test 6: Hot Key Count Verification
// ============================================================================

TEST_F(IPCSchemaIntegrationTest, HotKeyCountWithinLimit) {
    // Count Hot Key markers in DataStoreKeys.h
    std::ifstream file(datastore_keys_path_);
    ASSERT_TRUE(file.is_open());

    size_t hot_key_count = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("HOT KEY") != std::string::npos) {
            hot_key_count++;
        }
    }

    // Verify Hot Key count is within limit (MAX_HOT_KEYS=32)
    EXPECT_LE(hot_key_count, 32) << "Hot Key count exceeds limit (32)";
    EXPECT_EQ(hot_key_count, 14) << "Expected 14 Hot Keys as per ipc-schema.yaml";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
