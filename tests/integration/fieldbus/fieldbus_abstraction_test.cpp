// fieldbus_abstraction_test.cpp - Integration tests for Fieldbus abstraction layer
// Feature 019 - US4: T045

#include <gtest/gtest.h>
#include "core/fieldbus/factory/FieldbusFactory.h"
#include "core/fieldbus/interfaces/IFieldbus.h"
#include "core/fieldbus/drivers/MockDriver.h"
#include <vector>
#include <thread>
#include <chrono>

using namespace mxrc::core::fieldbus;

class FieldbusIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure clean state
        FieldbusFactory::clearProtocols();
    }

    void TearDown() override {
        // Cleanup
        if (fieldbus_) {
            fieldbus_->stop();
            fieldbus_.reset();
        }
    }

    IFieldbusPtr fieldbus_;
};

// ============================================================================
// T045: Mock Fieldbus Motor Control Scenario Tests
// ============================================================================

TEST_F(FieldbusIntegrationTest, MockDriver_BasicLifecycle) {
    // Create Mock fieldbus through factory
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    fieldbus_ = FieldbusFactory::create(config);
    ASSERT_NE(fieldbus_, nullptr);

    // Initialize
    EXPECT_TRUE(fieldbus_->initialize());
    EXPECT_EQ(fieldbus_->getStatus(), FieldbusStatus::INITIALIZED);

    // Start
    EXPECT_TRUE(fieldbus_->start());
    EXPECT_EQ(fieldbus_->getStatus(), FieldbusStatus::RUNNING);

    // Stop
    EXPECT_TRUE(fieldbus_->stop());
    EXPECT_EQ(fieldbus_->getStatus(), FieldbusStatus::STOPPED);
}

TEST_F(FieldbusIntegrationTest, MockDriver_SensorDataRead) {
    // Create and initialize Mock fieldbus
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    fieldbus_ = FieldbusFactory::create(config);
    ASSERT_NE(fieldbus_, nullptr);
    ASSERT_TRUE(fieldbus_->initialize());
    ASSERT_TRUE(fieldbus_->start());

    // Read sensor data
    std::vector<double> sensor_data;
    EXPECT_TRUE(fieldbus_->readSensors(sensor_data));

    // Mock driver returns 4 sensor values
    ASSERT_EQ(sensor_data.size(), 4u);

    // Verify sensor values are in expected range
    for (const auto& value : sensor_data) {
        EXPECT_GE(value, 0.0);
        EXPECT_LE(value, 100.0);
    }
}

TEST_F(FieldbusIntegrationTest, MockDriver_ActuatorControl) {
    // Create and initialize Mock fieldbus
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    fieldbus_ = FieldbusFactory::create(config);
    ASSERT_NE(fieldbus_, nullptr);
    ASSERT_TRUE(fieldbus_->initialize());
    ASSERT_TRUE(fieldbus_->start());

    // Write actuator commands
    std::vector<double> actuator_commands = {10.0, 20.0, 30.0, 40.0};
    EXPECT_TRUE(fieldbus_->writeActuators(actuator_commands));

    // Read back to verify (Mock driver echoes commands)
    std::vector<double> sensor_data;
    EXPECT_TRUE(fieldbus_->readSensors(sensor_data));
    ASSERT_EQ(sensor_data.size(), 4u);
}

TEST_F(FieldbusIntegrationTest, MockDriver_CyclicOperation) {
    // Create and initialize Mock fieldbus
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    fieldbus_ = FieldbusFactory::create(config);
    ASSERT_NE(fieldbus_, nullptr);
    ASSERT_TRUE(fieldbus_->initialize());
    ASSERT_TRUE(fieldbus_->start());

    // Simulate cyclic operation (10 cycles)
    const int num_cycles = 10;
    std::vector<double> actuator_commands = {1.0, 2.0, 3.0, 4.0};

    for (int cycle = 0; cycle < num_cycles; ++cycle) {
        // Write actuators
        EXPECT_TRUE(fieldbus_->writeActuators(actuator_commands));

        // Read sensors
        std::vector<double> sensor_data;
        EXPECT_TRUE(fieldbus_->readSensors(sensor_data));
        EXPECT_EQ(sensor_data.size(), 4u);

        // Simulate cycle time
        std::this_thread::sleep_for(std::chrono::microseconds(config.cycle_time_us));

        // Update commands for next cycle
        for (auto& cmd : actuator_commands) {
            cmd += 0.1;
        }
    }

    // Check statistics
    auto stats = fieldbus_->getStatistics();
    EXPECT_GE(stats.total_cycles, static_cast<uint64_t>(num_cycles));
    EXPECT_EQ(stats.communication_errors, 0u);
}

TEST_F(FieldbusIntegrationTest, MockDriver_ErrorHandling) {
    // Create Mock fieldbus
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    fieldbus_ = FieldbusFactory::create(config);
    ASSERT_NE(fieldbus_, nullptr);

    // Try to read/write before initialization - should fail gracefully
    std::vector<double> data;
    EXPECT_FALSE(fieldbus_->readSensors(data));
    EXPECT_FALSE(fieldbus_->writeActuators(data));

    // Initialize and start
    ASSERT_TRUE(fieldbus_->initialize());
    ASSERT_TRUE(fieldbus_->start());

    // Now operations should succeed
    EXPECT_TRUE(fieldbus_->readSensors(data));
    EXPECT_TRUE(fieldbus_->writeActuators({1.0, 2.0, 3.0, 4.0}));
}

TEST_F(FieldbusIntegrationTest, MockDriver_StatisticsTracking) {
    // Create and initialize Mock fieldbus
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    fieldbus_ = FieldbusFactory::create(config);
    ASSERT_NE(fieldbus_, nullptr);
    ASSERT_TRUE(fieldbus_->initialize());
    ASSERT_TRUE(fieldbus_->start());

    // Get initial statistics
    auto stats_before = fieldbus_->getStatistics();
    uint64_t initial_cycles = stats_before.total_cycles;

    // Perform some operations
    const int operations = 5;
    for (int i = 0; i < operations; ++i) {
        std::vector<double> data;
        fieldbus_->readSensors(data);
        fieldbus_->writeActuators({1.0, 2.0, 3.0, 4.0});
    }

    // Get updated statistics
    auto stats_after = fieldbus_->getStatistics();

    // Verify statistics were updated
    EXPECT_GT(stats_after.total_cycles, initial_cycles);
    EXPECT_EQ(stats_after.communication_errors, 0u);
}

TEST_F(FieldbusIntegrationTest, MultipleDriverInstances) {
    // Create two separate Mock fieldbus instances
    FieldbusConfig config1;
    config1.protocol = "Mock";
    config1.config_file = "test1.yaml";
    config1.cycle_time_us = 1000;
    config1.device_count = 4;

    FieldbusConfig config2;
    config2.protocol = "Mock";
    config2.config_file = "test2.yaml";
    config2.cycle_time_us = 2000;
    config2.device_count = 4;

    auto fieldbus1 = FieldbusFactory::create(config1);
    auto fieldbus2 = FieldbusFactory::create(config2);

    ASSERT_NE(fieldbus1, nullptr);
    ASSERT_NE(fieldbus2, nullptr);

    // Initialize and start both
    EXPECT_TRUE(fieldbus1->initialize());
    EXPECT_TRUE(fieldbus1->start());
    EXPECT_TRUE(fieldbus2->initialize());
    EXPECT_TRUE(fieldbus2->start());

    // Both should operate independently
    std::vector<double> data1, data2;
    EXPECT_TRUE(fieldbus1->readSensors(data1));
    EXPECT_TRUE(fieldbus2->readSensors(data2));

    // Cleanup
    fieldbus1->stop();
    fieldbus2->stop();
}

TEST_F(FieldbusIntegrationTest, ProtocolSwitching) {
    // Start with Mock protocol
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    auto mock_fieldbus = FieldbusFactory::create(config);
    ASSERT_NE(mock_fieldbus, nullptr);
    EXPECT_EQ(mock_fieldbus->getProtocolName(), "Mock");

    // Initialize and run
    ASSERT_TRUE(mock_fieldbus->initialize());
    ASSERT_TRUE(mock_fieldbus->start());
    std::vector<double> data;
    EXPECT_TRUE(mock_fieldbus->readSensors(data));
    mock_fieldbus->stop();

    // Switch to EtherCAT protocol (will fail initialization but demonstrates switching)
    config.protocol = "EtherCAT";
    auto ethercat_fieldbus = FieldbusFactory::create(config);
    ASSERT_NE(ethercat_fieldbus, nullptr);
    EXPECT_EQ(ethercat_fieldbus->getProtocolName(), "EtherCAT");

    // Note: EtherCAT initialization will fail without real hardware
    // but the factory pattern allows seamless protocol switching
}

TEST_F(FieldbusIntegrationTest, RepeatedStartStop) {
    // Create Mock fieldbus
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    fieldbus_ = FieldbusFactory::create(config);
    ASSERT_NE(fieldbus_, nullptr);
    ASSERT_TRUE(fieldbus_->initialize());

    // Repeatedly start and stop
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(fieldbus_->start());
        EXPECT_EQ(fieldbus_->getStatus(), FieldbusStatus::RUNNING);

        // Do some work
        std::vector<double> data;
        EXPECT_TRUE(fieldbus_->readSensors(data));

        EXPECT_TRUE(fieldbus_->stop());
        EXPECT_EQ(fieldbus_->getStatus(), FieldbusStatus::STOPPED);
    }
}
