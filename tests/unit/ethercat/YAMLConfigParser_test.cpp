#include <gtest/gtest.h>
#include "core/ethercat/util/YAMLConfigParser.h"
#include <fstream>
#include <string>

using namespace mxrc::ethercat;

class YAMLConfigParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 임시 YAML 파일 생성
        createTestYAML();
    }

    void TearDown() override {
        // 테스트 파일 정리
        std::remove(test_yaml_path.c_str());
    }

    void createTestYAML() {
        test_yaml_path = "/tmp/test_ethercat_slaves.yaml";
        std::ofstream file(test_yaml_path);
        file << R"(
master:
  index: 0
  cycle_time_ns: 10000000
  priority: 99
  cpu_affinity: 1

slaves:
  - alias: 0
    position: 0
    vendor_id: 0x00000002
    product_code: 0x044c2c52
    device_name: "Test_Encoder"
    device_type: SENSOR
    pdo_mappings:
      - direction: INPUT
        index: 0x1A00
        subindex: 0x01
        data_type: INT32
        offset: 0
        description: "Position value"
      - direction: INPUT
        index: 0x1A00
        subindex: 0x02
        data_type: INT32
        offset: 4
        description: "Velocity value"

  - alias: 1
    position: 1
    vendor_id: 0x000000ab
    product_code: 0x00000028
    device_name: "Test_ServoDriver"
    device_type: MOTOR
    pdo_mappings:
      - direction: OUTPUT
        index: 0x1600
        subindex: 0x01
        data_type: INT32
        offset: 0
        description: "Target position"

dc_config:
  enable: true
  reference_slave: 0
  sync0_cycle_time: 10000000
  sync0_shift_time: 0
)";
        file.close();
    }

    std::string test_yaml_path;
};

// 테스트 1: YAML 파일 로드 성공
TEST_F(YAMLConfigParserTest, LoadFromFileSuccess) {
    YAMLConfigParser parser;
    EXPECT_EQ(0, parser.loadFromFile(test_yaml_path));
}

// 테스트 2: 존재하지 않는 파일 로드 실패
TEST_F(YAMLConfigParserTest, LoadFromFileNotFound) {
    YAMLConfigParser parser;
    EXPECT_NE(0, parser.loadFromFile("/nonexistent/file.yaml"));
}

// 테스트 3: Slave 설정 파싱
TEST_F(YAMLConfigParserTest, ParseSlaveConfig) {
    YAMLConfigParser parser;
    parser.loadFromFile(test_yaml_path);

    // Slave 개수 확인
    EXPECT_EQ(2, parser.getSlaveCount());

    // 첫 번째 slave 설정 확인
    const SlaveConfig* slave0 = parser.getSlaveConfig(0);
    ASSERT_NE(nullptr, slave0);
    EXPECT_EQ(0, slave0->alias);
    EXPECT_EQ(0, slave0->position);
    EXPECT_EQ(0x00000002u, slave0->vendor_id);
    EXPECT_EQ(0x044c2c52u, slave0->product_code);
    EXPECT_EQ("Test_Encoder", slave0->device_name);
    EXPECT_EQ(DeviceType::SENSOR, slave0->device_type);

    // 두 번째 slave 설정 확인
    const SlaveConfig* slave1 = parser.getSlaveConfig(1);
    ASSERT_NE(nullptr, slave1);
    EXPECT_EQ(1, slave1->alias);
    EXPECT_EQ("Test_ServoDriver", slave1->device_name);
    EXPECT_EQ(DeviceType::MOTOR, slave1->device_type);
}

// 테스트 4: PDO 매핑 파싱
TEST_F(YAMLConfigParserTest, ParsePDOMapping) {
    YAMLConfigParser parser;
    parser.loadFromFile(test_yaml_path);

    // 첫 번째 slave의 PDO 매핑 확인
    const auto& mappings = parser.getPDOMappings(0);
    EXPECT_EQ(2, mappings.size());

    // 첫 번째 PDO 매핑
    EXPECT_EQ(PDODirection::INPUT, mappings[0].direction);
    EXPECT_EQ(0x1A00, mappings[0].index);
    EXPECT_EQ(0x01, mappings[0].subindex);
    EXPECT_EQ(PDODataType::INT32, mappings[0].data_type);
    EXPECT_EQ(0u, mappings[0].offset);
    EXPECT_EQ("Position value", mappings[0].description);

    // 두 번째 PDO 매핑
    EXPECT_EQ(0x1A00, mappings[1].index);
    EXPECT_EQ(0x02, mappings[1].subindex);
    EXPECT_EQ(4u, mappings[1].offset);
}

// 테스트 5: DC 설정 파싱
TEST_F(YAMLConfigParserTest, ParseDCConfig) {
    YAMLConfigParser parser;
    parser.loadFromFile(test_yaml_path);

    DCConfiguration dc_config;
    EXPECT_EQ(0, parser.getDCConfig(dc_config));

    EXPECT_TRUE(dc_config.enable);
    EXPECT_EQ(0, dc_config.reference_slave);
    EXPECT_EQ(10000000u, dc_config.sync0_cycle_time);
    EXPECT_EQ(0, dc_config.sync0_shift_time);
}

// 테스트 6: Master 설정 파싱
TEST_F(YAMLConfigParserTest, ParseMasterConfig) {
    YAMLConfigParser parser;
    parser.loadFromFile(test_yaml_path);

    EXPECT_EQ(0, parser.getMasterIndex());
    EXPECT_EQ(10000000u, parser.getCycleTimeNs());
}

// 테스트 7: 잘못된 device_type 처리
TEST_F(YAMLConfigParserTest, InvalidDeviceType) {
    std::string invalid_yaml = "/tmp/test_invalid.yaml";
    std::ofstream file(invalid_yaml);
    file << R"(
master:
  index: 0
  cycle_time_ns: 10000000

slaves:
  - alias: 0
    position: 0
    vendor_id: 0x00000002
    product_code: 0x044c2c52
    device_name: "Test"
    device_type: INVALID_TYPE
)";
    file.close();

    YAMLConfigParser parser;
    EXPECT_EQ(0, parser.loadFromFile(invalid_yaml));

    const SlaveConfig* slave = parser.getSlaveConfig(0);
    ASSERT_NE(nullptr, slave);
    EXPECT_EQ(DeviceType::UNKNOWN, slave->device_type);

    std::remove(invalid_yaml.c_str());
}
