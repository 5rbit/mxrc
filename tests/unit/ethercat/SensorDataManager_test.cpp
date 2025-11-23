#include <gtest/gtest.h>
#include "core/ethercat/impl/SensorDataManager.h"
#include "MockEtherCATMaster.h"
#include "MockSlaveConfig.h"
#include <memory>

using namespace mxrc::ethercat;
using namespace mxrc::ethercat::test;

class SensorDataManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_master_ = std::make_shared<MockEtherCATMaster>();
        mock_config_ = std::make_shared<MockSlaveConfig>();
        manager_ = std::make_unique<SensorDataManager>(mock_master_, mock_config_);

        // PDO domain 포인터 설정
        manager_->setDomainPtr(mock_master_->getDomainPtr());

        // Master 활성화
        mock_master_->activate();
    }

    std::shared_ptr<MockEtherCATMaster> mock_master_;
    std::shared_ptr<MockSlaveConfig> mock_config_;
    std::unique_ptr<SensorDataManager> manager_;
};

// 테스트 1: Position 센서 읽기 - 성공 (position + velocity)
TEST_F(SensorDataManagerTest, ReadPositionSensorSuccess) {
    // Arrange: PDO 매핑 설정
    PDOMapping pos_mapping;
    pos_mapping.direction = PDODirection::INPUT;
    pos_mapping.index = 0x1A00;
    pos_mapping.subindex = 0x01;
    pos_mapping.data_type = PDODataType::INT32;
    pos_mapping.offset = 0;
    pos_mapping.bit_length = 32;

    PDOMapping vel_mapping;
    vel_mapping.direction = PDODirection::INPUT;
    vel_mapping.index = 0x1A00;
    vel_mapping.subindex = 0x02;
    vel_mapping.data_type = PDODataType::INT32;
    vel_mapping.offset = 4;
    vel_mapping.bit_length = 32;

    mock_config_->addPDOMapping(0, pos_mapping);
    mock_config_->addPDOMapping(0, vel_mapping);

    // PDO domain에 데이터 설정
    int32_t expected_pos = 123456;
    int32_t expected_vel = 7890;
    mock_master_->setDomainData(0, &expected_pos, sizeof(int32_t));
    mock_master_->setDomainData(4, &expected_vel, sizeof(int32_t));

    // Act
    PositionSensorData data;
    int result = manager_->readPositionSensor(0, data);

    // Assert
    EXPECT_EQ(0, result);
    EXPECT_EQ(expected_pos, data.position);
    EXPECT_EQ(expected_vel, data.velocity);
    EXPECT_TRUE(data.valid);
    EXPECT_EQ(0, data.slave_id);
    EXPECT_GT(data.timestamp, 0ULL);
}

// 테스트 2: Position 센서 - velocity 없는 경우
TEST_F(SensorDataManagerTest, ReadPositionSensorNoVelocity) {
    PDOMapping pos_mapping;
    pos_mapping.direction = PDODirection::INPUT;
    pos_mapping.index = 0x1A00;
    pos_mapping.subindex = 0x01;
    pos_mapping.data_type = PDODataType::INT32;
    pos_mapping.offset = 0;

    mock_config_->addPDOMapping(1, pos_mapping);

    int32_t expected_pos = 99999;
    mock_master_->setDomainData(0, &expected_pos, sizeof(int32_t));

    PositionSensorData data;
    int result = manager_->readPositionSensor(1, data);

    EXPECT_EQ(0, result);
    EXPECT_EQ(expected_pos, data.position);
    EXPECT_EQ(0, data.velocity);  // velocity는 0
}

// 테스트 3: Velocity 센서 읽기 - DOUBLE 타입
TEST_F(SensorDataManagerTest, ReadVelocitySensorSuccess) {
    PDOMapping vel_mapping;
    vel_mapping.direction = PDODirection::INPUT;
    vel_mapping.index = 0x1A01;
    vel_mapping.subindex = 0x01;
    vel_mapping.data_type = PDODataType::DOUBLE;
    vel_mapping.offset = 0;

    PDOMapping acc_mapping;
    acc_mapping.direction = PDODirection::INPUT;
    acc_mapping.index = 0x1A01;
    acc_mapping.subindex = 0x02;
    acc_mapping.data_type = PDODataType::DOUBLE;
    acc_mapping.offset = 8;

    mock_config_->addPDOMapping(2, vel_mapping);
    mock_config_->addPDOMapping(2, acc_mapping);

    double expected_vel = 5.5;
    double expected_acc = 0.25;
    mock_master_->setDomainData(0, &expected_vel, sizeof(double));
    mock_master_->setDomainData(8, &expected_acc, sizeof(double));

    VelocitySensorData data;
    int result = manager_->readVelocitySensor(2, data);

    EXPECT_EQ(0, result);
    EXPECT_DOUBLE_EQ(expected_vel, data.velocity);
    EXPECT_DOUBLE_EQ(expected_acc, data.acceleration);
    EXPECT_TRUE(data.valid);
    EXPECT_EQ(2, data.slave_id);
}

// 테스트 4: Torque 센서 읽기 - 6축
TEST_F(SensorDataManagerTest, ReadTorqueSensor6Axis) {
    // 6축 매핑 설정
    for (int i = 1; i <= 6; ++i) {
        PDOMapping mapping;
        mapping.direction = PDODirection::INPUT;
        mapping.index = 0x1A02;
        mapping.subindex = i;
        mapping.data_type = PDODataType::DOUBLE;
        mapping.offset = (i - 1) * 8;  // 8 bytes per DOUBLE
        mock_config_->addPDOMapping(3, mapping);
    }

    // 테스트 데이터
    double force_x = 10.5, force_y = 20.5, force_z = 30.5;
    double torque_x = 1.5, torque_y = 2.5, torque_z = 3.5;

    mock_master_->setDomainData(0, &force_x, sizeof(double));
    mock_master_->setDomainData(8, &force_y, sizeof(double));
    mock_master_->setDomainData(16, &force_z, sizeof(double));
    mock_master_->setDomainData(24, &torque_x, sizeof(double));
    mock_master_->setDomainData(32, &torque_y, sizeof(double));
    mock_master_->setDomainData(40, &torque_z, sizeof(double));

    TorqueSensorData data;
    int result = manager_->readTorqueSensor(3, data);

    EXPECT_EQ(0, result);
    EXPECT_DOUBLE_EQ(force_x, data.force_x);
    EXPECT_DOUBLE_EQ(force_y, data.force_y);
    EXPECT_DOUBLE_EQ(force_z, data.force_z);
    EXPECT_DOUBLE_EQ(torque_x, data.torque_x);
    EXPECT_DOUBLE_EQ(torque_y, data.torque_y);
    EXPECT_DOUBLE_EQ(torque_z, data.torque_z);
    EXPECT_TRUE(data.valid);
    EXPECT_EQ(3, data.slave_id);
}

// 테스트 5: Digital Input - 채널 0 읽기 (8bit 비트맵)
TEST_F(SensorDataManagerTest, ReadDigitalInputChannel0) {
    PDOMapping di_mapping;
    di_mapping.direction = PDODirection::INPUT;
    di_mapping.index = 0x1A03;
    di_mapping.subindex = 0x01;
    di_mapping.data_type = PDODataType::UINT8;
    di_mapping.offset = 0;

    mock_config_->addPDOMapping(4, di_mapping);

    // 비트맵: 0b10101010 (채널 1, 3, 5, 7 = HIGH)
    uint8_t bitmap = 0b10101010;
    mock_master_->setDomainData(0, &bitmap, sizeof(uint8_t));

    // 채널 0 읽기 (bit 0 = 0, LOW)
    DigitalInputData data;
    int result = manager_->readDigitalInput(4, 0, data);

    EXPECT_EQ(0, result);
    EXPECT_EQ(0, data.channel);
    EXPECT_FALSE(data.value);  // bit 0 = 0
    EXPECT_TRUE(data.valid);
}

// 테스트 6: Digital Input - 채널 3 읽기
TEST_F(SensorDataManagerTest, ReadDigitalInputChannel3) {
    PDOMapping di_mapping;
    di_mapping.direction = PDODirection::INPUT;
    di_mapping.index = 0x1A03;
    di_mapping.subindex = 0x01;
    di_mapping.data_type = PDODataType::UINT8;
    di_mapping.offset = 0;

    mock_config_->addPDOMapping(4, di_mapping);

    uint8_t bitmap = 0b10101010;
    mock_master_->setDomainData(0, &bitmap, sizeof(uint8_t));

    // 채널 3 읽기 (bit 3 = 1, HIGH)
    DigitalInputData data;
    int result = manager_->readDigitalInput(4, 3, data);

    EXPECT_EQ(0, result);
    EXPECT_EQ(3, data.channel);
    EXPECT_TRUE(data.value);  // bit 3 = 1
}

// 테스트 7: Analog Input - 채널 0 읽기 (INT16)
TEST_F(SensorDataManagerTest, ReadAnalogInputChannel0) {
    PDOMapping ai_mapping;
    ai_mapping.direction = PDODirection::INPUT;
    ai_mapping.index = 0x1A04;
    ai_mapping.subindex = 0x01;  // channel 0
    ai_mapping.data_type = PDODataType::INT16;
    ai_mapping.offset = 0;

    mock_config_->addPDOMapping(5, ai_mapping);

    int16_t expected_value = 1234;
    mock_master_->setDomainData(0, &expected_value, sizeof(int16_t));

    AnalogInputData data;
    int result = manager_->readAnalogInput(5, 0, data);

    EXPECT_EQ(0, result);
    EXPECT_EQ(0, data.channel);
    EXPECT_DOUBLE_EQ(1234.0, data.value);
    EXPECT_TRUE(data.valid);
}

// 테스트 8: Analog Input - 채널 2 읽기 (INT32)
TEST_F(SensorDataManagerTest, ReadAnalogInputChannel2Int32) {
    PDOMapping ai_mapping;
    ai_mapping.direction = PDODirection::INPUT;
    ai_mapping.index = 0x1A04;
    ai_mapping.subindex = 0x03;  // channel 2 (0x01 + 2)
    ai_mapping.data_type = PDODataType::INT32;
    ai_mapping.offset = 0;

    mock_config_->addPDOMapping(5, ai_mapping);

    int32_t expected_value = 999888;
    mock_master_->setDomainData(0, &expected_value, sizeof(int32_t));

    AnalogInputData data;
    int result = manager_->readAnalogInput(5, 2, data);

    EXPECT_EQ(0, result);
    EXPECT_EQ(2, data.channel);
    EXPECT_DOUBLE_EQ(999888.0, data.value);
}

// 테스트 9: PDO 매핑 없음 - 실패
TEST_F(SensorDataManagerTest, ReadPositionSensorNoMapping) {
    PositionSensorData data;
    int result = manager_->readPositionSensor(99, data);

    EXPECT_NE(0, result);
}

// 테스트 10: Master 비활성화 - valid = false
TEST_F(SensorDataManagerTest, ReadPositionSensorMasterInactive) {
    PDOMapping pos_mapping;
    pos_mapping.direction = PDODirection::INPUT;
    pos_mapping.index = 0x1A00;
    pos_mapping.subindex = 0x01;
    pos_mapping.data_type = PDODataType::INT32;
    pos_mapping.offset = 0;

    mock_config_->addPDOMapping(0, pos_mapping);

    int32_t pos = 12345;
    mock_master_->setDomainData(0, &pos, sizeof(int32_t));

    // Master 비활성화
    mock_master_->deactivate();

    PositionSensorData data;
    int result = manager_->readPositionSensor(0, data);

    EXPECT_EQ(0, result);
    EXPECT_FALSE(data.valid);  // 비활성화 상태
}

// 테스트 11: 음수 값 읽기
TEST_F(SensorDataManagerTest, ReadPositionSensorNegativeValue) {
    PDOMapping pos_mapping;
    pos_mapping.direction = PDODirection::INPUT;
    pos_mapping.index = 0x1A00;
    pos_mapping.subindex = 0x01;
    pos_mapping.data_type = PDODataType::INT32;
    pos_mapping.offset = 0;

    mock_config_->addPDOMapping(0, pos_mapping);

    int32_t negative_pos = -123456;
    mock_master_->setDomainData(0, &negative_pos, sizeof(int32_t));

    PositionSensorData data;
    EXPECT_EQ(0, manager_->readPositionSensor(0, data));
    EXPECT_EQ(negative_pos, data.position);
}

// 테스트 12: 여러 slave 동시 읽기
TEST_F(SensorDataManagerTest, ReadMultipleSlaves) {
    // Slave 0: Position
    PDOMapping pos_mapping;
    pos_mapping.direction = PDODirection::INPUT;
    pos_mapping.index = 0x1A00;
    pos_mapping.subindex = 0x01;
    pos_mapping.data_type = PDODataType::INT32;
    pos_mapping.offset = 0;
    mock_config_->addPDOMapping(0, pos_mapping);

    // Slave 1: Digital Input
    PDOMapping di_mapping;
    di_mapping.direction = PDODirection::INPUT;
    di_mapping.index = 0x1A03;
    di_mapping.subindex = 0x01;
    di_mapping.data_type = PDODataType::UINT8;
    di_mapping.offset = 10;
    mock_config_->addPDOMapping(1, di_mapping);

    int32_t pos = 999;
    uint8_t di = 0xFF;
    mock_master_->setDomainData(0, &pos, sizeof(int32_t));
    mock_master_->setDomainData(10, &di, sizeof(uint8_t));

    PositionSensorData pos_data;
    DigitalInputData di_data;

    EXPECT_EQ(0, manager_->readPositionSensor(0, pos_data));
    EXPECT_EQ(0, manager_->readDigitalInput(1, 0, di_data));

    EXPECT_EQ(pos, pos_data.position);
    EXPECT_TRUE(di_data.value);  // bit 0 = 1
    EXPECT_EQ(0, pos_data.slave_id);
    EXPECT_EQ(1, di_data.slave_id);
}

// 테스트 13: Digital Output - 채널 0 쓰기
TEST_F(SensorDataManagerTest, WriteDigitalOutputChannel0) {
    PDOMapping do_mapping;
    do_mapping.direction = PDODirection::OUTPUT;
    do_mapping.index = 0x1600;
    do_mapping.subindex = 0x01;
    do_mapping.data_type = PDODataType::UINT8;
    do_mapping.offset = 0;

    mock_config_->addPDOMapping(6, do_mapping);

    // 초기 bitmap: 0x00
    uint8_t bitmap = 0x00;
    mock_master_->setDomainData(0, &bitmap, sizeof(uint8_t));

    // 채널 0을 HIGH로 설정
    DigitalOutputData data;
    data.channel = 0;
    data.value = true;
    data.slave_id = 6;

    EXPECT_EQ(0, manager_->writeDigitalOutput(6, 0, data));

    // 결과 확인: 비트 0이 1로 설정됨
    uint8_t result = mock_master_->getDomainPtr()[0];
    EXPECT_EQ(0x01, result);
}

// 테스트 14: Digital Output - 여러 채널 동시 설정
TEST_F(SensorDataManagerTest, WriteDigitalOutputMultipleChannels) {
    PDOMapping do_mapping;
    do_mapping.direction = PDODirection::OUTPUT;
    do_mapping.index = 0x1600;
    do_mapping.subindex = 0x01;
    do_mapping.data_type = PDODataType::UINT8;
    do_mapping.offset = 0;

    mock_config_->addPDOMapping(6, do_mapping);

    uint8_t bitmap = 0x00;
    mock_master_->setDomainData(0, &bitmap, sizeof(uint8_t));

    // 채널 0, 2, 4를 HIGH로 설정
    DigitalOutputData data;
    data.slave_id = 6;

    data.channel = 0;
    data.value = true;
    manager_->writeDigitalOutput(6, 0, data);

    data.channel = 2;
    data.value = true;
    manager_->writeDigitalOutput(6, 2, data);

    data.channel = 4;
    data.value = true;
    manager_->writeDigitalOutput(6, 4, data);

    // 결과 확인: 0b00010101 = 0x15
    uint8_t result = mock_master_->getDomainPtr()[0];
    EXPECT_EQ(0x15, result);
}

// 테스트 15: Analog Output - 채널 0 쓰기 (INT16)
TEST_F(SensorDataManagerTest, WriteAnalogOutputChannel0) {
    PDOMapping ao_mapping;
    ao_mapping.direction = PDODirection::OUTPUT;
    ao_mapping.index = 0x1601;
    ao_mapping.subindex = 0x01;  // channel 0
    ao_mapping.data_type = PDODataType::INT16;
    ao_mapping.offset = 0;

    mock_config_->addPDOMapping(7, ao_mapping);

    // 5.0V 출력
    AnalogOutputData data;
    data.channel = 0;
    data.value = 5.0;
    data.min_value = -10.0;
    data.max_value = 10.0;
    data.slave_id = 7;

    EXPECT_EQ(0, manager_->writeAnalogOutput(7, 0, data));

    // 결과 확인
    int16_t result;
    mock_master_->getDomainData(0, &result, sizeof(int16_t));
    EXPECT_EQ(5, result);
}

// 테스트 16: Analog Output - 범위 초과 거부
TEST_F(SensorDataManagerTest, WriteAnalogOutputOutOfRange) {
    PDOMapping ao_mapping;
    ao_mapping.direction = PDODirection::OUTPUT;
    ao_mapping.index = 0x1601;
    ao_mapping.subindex = 0x01;
    ao_mapping.data_type = PDODataType::INT16;
    ao_mapping.offset = 0;

    mock_config_->addPDOMapping(7, ao_mapping);

    // 범위 초과 값
    AnalogOutputData data;
    data.channel = 0;
    data.value = 15.0;  // 범위 초과
    data.min_value = -10.0;
    data.max_value = 10.0;
    data.slave_id = 7;

    // 범위 초과 시 실패
    EXPECT_NE(0, manager_->writeAnalogOutput(7, 0, data));
}
