// FieldbusFactory_test.cpp - Unit tests for FieldbusFactory
// Feature 019 - US4: T044

#include <gtest/gtest.h>
#include "core/fieldbus/factory/FieldbusFactory.h"
#include "core/fieldbus/interfaces/IFieldbus.h"
#include "core/fieldbus/drivers/MockDriver.h"
#include "core/fieldbus/drivers/EtherCATDriver.h"

using namespace mxrc::core::fieldbus;

class FieldbusFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing protocols to ensure clean state
        FieldbusFactory::clearProtocols();
    }

    void TearDown() override {
        // Clean up after each test
        FieldbusFactory::clearProtocols();
    }
};

// ============================================================================
// T044: FieldbusFactory Driver Creation Tests
// ============================================================================

TEST_F(FieldbusFactoryTest, CreateMockDriver_Success) {
    // Create configuration for Mock protocol
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;

    // Create Mock fieldbus
    auto fieldbus = FieldbusFactory::create(config);

    // Verify creation succeeded
    ASSERT_NE(fieldbus, nullptr);
    EXPECT_EQ(fieldbus->getProtocolName(), "Mock");
}

TEST_F(FieldbusFactoryTest, CreateEtherCATDriver_Success) {
    // Create configuration for EtherCAT protocol
    FieldbusConfig config;
    config.protocol = "EtherCAT";
    config.config_file = "config/ethercat/test.yaml";
    config.cycle_time_us = 1000;

    // Create EtherCAT fieldbus
    auto fieldbus = FieldbusFactory::create(config);

    // Verify creation succeeded
    ASSERT_NE(fieldbus, nullptr);
    EXPECT_EQ(fieldbus->getProtocolName(), "EtherCAT");
}

TEST_F(FieldbusFactoryTest, CreateByName_OverridesConfigProtocol) {
    // Create configuration with one protocol
    FieldbusConfig config;
    config.protocol = "Wrong";  // This will be ignored
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;

    // Create by explicitly specifying protocol name
    auto fieldbus = FieldbusFactory::createByName("Mock", config);

    // Verify correct protocol was used
    ASSERT_NE(fieldbus, nullptr);
    EXPECT_EQ(fieldbus->getProtocolName(), "Mock");
}

TEST_F(FieldbusFactoryTest, CreateUnsupportedProtocol_ReturnsNull) {
    // Create configuration for non-existent protocol
    FieldbusConfig config;
    config.protocol = "NonExistent";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;

    // Attempt to create unsupported fieldbus
    auto fieldbus = FieldbusFactory::create(config);

    // Verify creation failed gracefully
    EXPECT_EQ(fieldbus, nullptr);
}

TEST_F(FieldbusFactoryTest, IsProtocolSupported_ChecksRegistration) {
    // Check built-in protocols
    EXPECT_TRUE(FieldbusFactory::isProtocolSupported("Mock"));
    EXPECT_TRUE(FieldbusFactory::isProtocolSupported("EtherCAT"));

    // Check non-existent protocol
    EXPECT_FALSE(FieldbusFactory::isProtocolSupported("NonExistent"));
}

TEST_F(FieldbusFactoryTest, GetSupportedProtocols_ReturnsBuiltIn) {
    // Get list of supported protocols
    auto protocols = FieldbusFactory::getSupportedProtocols();

    // Verify built-in protocols are present
    EXPECT_GE(protocols.size(), 2u);
    EXPECT_NE(std::find(protocols.begin(), protocols.end(), "Mock"), protocols.end());
    EXPECT_NE(std::find(protocols.begin(), protocols.end(), "EtherCAT"), protocols.end());
}

TEST_F(FieldbusFactoryTest, RegisterCustomProtocol_Success) {
    // Define custom protocol creator
    auto custom_creator = [](const FieldbusConfig& config) -> IFieldbusPtr {
        // Return Mock driver as placeholder
        return std::make_shared<MockDriver>(config);
    };

    // Register custom protocol
    bool registered = FieldbusFactory::registerProtocol("Custom", custom_creator);

    // Verify registration succeeded
    EXPECT_TRUE(registered);
    EXPECT_TRUE(FieldbusFactory::isProtocolSupported("Custom"));

    // Create using custom protocol
    FieldbusConfig config;
    config.protocol = "Custom";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;

    auto fieldbus = FieldbusFactory::create(config);
    ASSERT_NE(fieldbus, nullptr);
}

TEST_F(FieldbusFactoryTest, RegisterDuplicateProtocol_Fails) {
    // Try to register protocol that already exists
    auto creator = [](const FieldbusConfig& config) -> IFieldbusPtr {
        return std::make_shared<MockDriver>(config);
    };

    bool registered = FieldbusFactory::registerProtocol("Mock", creator);

    // Verify registration failed (Mock already exists)
    EXPECT_FALSE(registered);
}

TEST_F(FieldbusFactoryTest, UnregisterProtocol_Success) {
    // Register custom protocol first
    auto creator = [](const FieldbusConfig& config) -> IFieldbusPtr {
        return std::make_shared<MockDriver>(config);
    };
    FieldbusFactory::registerProtocol("Temp", creator);

    // Verify it's registered
    ASSERT_TRUE(FieldbusFactory::isProtocolSupported("Temp"));

    // Unregister it
    bool unregistered = FieldbusFactory::unregisterProtocol("Temp");

    // Verify it was removed
    EXPECT_TRUE(unregistered);
    EXPECT_FALSE(FieldbusFactory::isProtocolSupported("Temp"));
}

TEST_F(FieldbusFactoryTest, ClearProtocols_AllowsReinitialization) {
    // Verify protocols exist initially
    ASSERT_TRUE(FieldbusFactory::isProtocolSupported("Mock"));

    // Clear all protocols
    FieldbusFactory::clearProtocols();

    // After clearing, next access will re-initialize built-in protocols
    // This is the intended behavior - built-in protocols are always available
    EXPECT_TRUE(FieldbusFactory::isProtocolSupported("Mock"));
    EXPECT_TRUE(FieldbusFactory::isProtocolSupported("EtherCAT"));
    EXPECT_EQ(FieldbusFactory::getSupportedProtocols().size(), 2u);
}
