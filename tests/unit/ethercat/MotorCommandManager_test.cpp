#include <gtest/gtest.h>
#include "core/ethercat/impl/MotorCommandManager.h"
#include "MockEtherCATMaster.h"
#include "MockSlaveConfig.h"
#include <memory>

using namespace mxrc::ethercat;
using namespace mxrc::ethercat::test;

class MotorCommandManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_master_ = std::make_shared<MockEtherCATMaster>();
        mock_config_ = std::make_shared<MockSlaveConfig>();

        manager_ = std::make_unique<MotorCommandManager>(mock_master_, mock_config_);

        // PDO domain 포인터 설정
        manager_->setDomainPtr(mock_master_->getDomainPtr());

        // Master 활성화
        mock_master_->activate();
    }

    void TearDown() override {
        manager_.reset();
        mock_config_.reset();
        mock_master_.reset();
    }

    std::shared_ptr<MockEtherCATMaster> mock_master_;
    std::shared_ptr<MockSlaveConfig> mock_config_;
    std::unique_ptr<MotorCommandManager> manager_;
};

// 테스트 1: BLDC 모터 - VELOCITY 모드 명령 전송
TEST_F(MotorCommandManagerTest, WriteBLDCVelocityCommand) {
    // Arrange: BLDC 모터용 PDO 매핑 설정
    // 0x1602:01 - Control Word (UINT16)
    // 0x1602:02 - Target Velocity (INT32, RPM)
    PDOMapping control_mapping;
    control_mapping.direction = PDODirection::OUTPUT;
    control_mapping.index = 0x1602;
    control_mapping.subindex = 0x01;
    control_mapping.data_type = PDODataType::UINT16;
    control_mapping.offset = 0;
    mock_config_->addPDOMapping(10, control_mapping);

    PDOMapping velocity_mapping;
    velocity_mapping.direction = PDODirection::OUTPUT;
    velocity_mapping.index = 0x1602;
    velocity_mapping.subindex = 0x02;
    velocity_mapping.data_type = PDODataType::INT32;
    velocity_mapping.offset = 2;
    mock_config_->addPDOMapping(10, velocity_mapping);

    // BLDC 명령 생성
    BLDCMotorCommand cmd;
    cmd.slave_id = 10;
    cmd.target_velocity = 1500.0;  // 1500 RPM
    cmd.control_mode = ControlMode::VELOCITY;
    cmd.enable = true;

    // Act
    int result = manager_->writeBLDCCommand(cmd);

    // Assert
    EXPECT_EQ(0, result);

    // Control Word 확인 (enable=true, mode=VELOCITY)
    uint16_t control_word;
    mock_master_->getDomainData(0, &control_word, sizeof(uint16_t));
    EXPECT_NE(0, control_word);  // enable 비트가 설정됨

    // Target Velocity 확인
    int32_t velocity;
    mock_master_->getDomainData(2, &velocity, sizeof(int32_t));
    EXPECT_EQ(1500, velocity);
}

// 테스트 2: BLDC 모터 - TORQUE 모드 명령 전송
TEST_F(MotorCommandManagerTest, WriteBLDCTorqueCommand) {
    // Arrange
    PDOMapping control_mapping;
    control_mapping.direction = PDODirection::OUTPUT;
    control_mapping.index = 0x1602;
    control_mapping.subindex = 0x01;
    control_mapping.data_type = PDODataType::UINT16;
    control_mapping.offset = 0;
    mock_config_->addPDOMapping(10, control_mapping);

    PDOMapping torque_mapping;
    torque_mapping.direction = PDODirection::OUTPUT;
    torque_mapping.index = 0x1602;
    torque_mapping.subindex = 0x03;
    torque_mapping.data_type = PDODataType::INT16;
    torque_mapping.offset = 6;
    mock_config_->addPDOMapping(10, torque_mapping);

    BLDCMotorCommand cmd;
    cmd.slave_id = 10;
    cmd.target_torque = 5.5;  // 5.5 Nm
    cmd.control_mode = ControlMode::TORQUE;
    cmd.enable = true;

    // Act
    int result = manager_->writeBLDCCommand(cmd);

    // Assert
    EXPECT_EQ(0, result);

    int16_t torque;
    mock_master_->getDomainData(6, &torque, sizeof(int16_t));
    EXPECT_EQ(5, torque);  // 5.5 → 5 (정수 변환)
}

// 테스트 3: BLDC 모터 - enable=false (안전 상태)
TEST_F(MotorCommandManagerTest, WriteBLDCDisabledCommand) {
    // Arrange
    PDOMapping control_mapping;
    control_mapping.direction = PDODirection::OUTPUT;
    control_mapping.index = 0x1602;
    control_mapping.subindex = 0x01;
    control_mapping.data_type = PDODataType::UINT16;
    control_mapping.offset = 0;
    mock_config_->addPDOMapping(10, control_mapping);

    BLDCMotorCommand cmd;
    cmd.slave_id = 10;
    cmd.enable = false;  // 비활성화
    cmd.control_mode = ControlMode::DISABLED;

    // Act
    int result = manager_->writeBLDCCommand(cmd);

    // Assert
    EXPECT_EQ(0, result);

    // Control Word가 0이어야 함 (disabled)
    uint16_t control_word;
    mock_master_->getDomainData(0, &control_word, sizeof(uint16_t));
    EXPECT_EQ(0, control_word);
}

// 테스트 4: BLDC 모터 - 범위 초과 거부
TEST_F(MotorCommandManagerTest, WriteBLDCOutOfRangeRejected) {
    // Arrange
    PDOMapping control_mapping;
    control_mapping.direction = PDODirection::OUTPUT;
    control_mapping.index = 0x1602;
    control_mapping.subindex = 0x01;
    control_mapping.data_type = PDODataType::UINT16;
    control_mapping.offset = 0;
    mock_config_->addPDOMapping(10, control_mapping);

    BLDCMotorCommand cmd;
    cmd.slave_id = 10;
    cmd.target_velocity = 15000.0;  // 범위 초과 (>10000 RPM)
    cmd.control_mode = ControlMode::VELOCITY;
    cmd.enable = true;

    // Act
    int result = manager_->writeBLDCCommand(cmd);

    // Assert: 실패해야 함
    EXPECT_NE(0, result);
}

// 테스트 5: Servo 드라이버 - POSITION 모드 명령 전송
TEST_F(MotorCommandManagerTest, WriteServoPositionCommand) {
    // Arrange: Servo용 PDO 매핑
    // 0x1603:01 - Control Word
    // 0x1603:02 - Target Position (DOUBLE, radian)
    // 0x1603:03 - Max Velocity (DOUBLE)
    PDOMapping control_mapping;
    control_mapping.direction = PDODirection::OUTPUT;
    control_mapping.index = 0x1603;
    control_mapping.subindex = 0x01;
    control_mapping.data_type = PDODataType::UINT16;
    control_mapping.offset = 0;
    mock_config_->addPDOMapping(11, control_mapping);

    PDOMapping position_mapping;
    position_mapping.direction = PDODirection::OUTPUT;
    position_mapping.index = 0x1603;
    position_mapping.subindex = 0x02;
    position_mapping.data_type = PDODataType::DOUBLE;
    position_mapping.offset = 2;
    mock_config_->addPDOMapping(11, position_mapping);

    PDOMapping max_vel_mapping;
    max_vel_mapping.direction = PDODirection::OUTPUT;
    max_vel_mapping.index = 0x1603;
    max_vel_mapping.subindex = 0x03;
    max_vel_mapping.data_type = PDODataType::DOUBLE;
    max_vel_mapping.offset = 10;
    mock_config_->addPDOMapping(11, max_vel_mapping);

    ServoDriverCommand cmd;
    cmd.slave_id = 11;
    cmd.target_position = 1.57;  // π/2 rad
    cmd.target_velocity = 2.0;   // 2 rad/s
    cmd.max_velocity = 5.0;
    cmd.control_mode = ControlMode::POSITION;
    cmd.enable = true;

    // Act
    int result = manager_->writeServoCommand(cmd);

    // Assert
    EXPECT_EQ(0, result);

    double position, max_vel;
    mock_master_->getDomainData(2, &position, sizeof(double));
    mock_master_->getDomainData(10, &max_vel, sizeof(double));

    EXPECT_DOUBLE_EQ(1.57, position);
    EXPECT_DOUBLE_EQ(5.0, max_vel);
}

// 테스트 6: Servo 드라이버 - VELOCITY 모드
TEST_F(MotorCommandManagerTest, WriteServoVelocityCommand) {
    // Arrange
    PDOMapping control_mapping;
    control_mapping.direction = PDODirection::OUTPUT;
    control_mapping.index = 0x1603;
    control_mapping.subindex = 0x01;
    control_mapping.data_type = PDODataType::UINT16;
    control_mapping.offset = 0;
    mock_config_->addPDOMapping(11, control_mapping);

    PDOMapping velocity_mapping;
    velocity_mapping.direction = PDODirection::OUTPUT;
    velocity_mapping.index = 0x1603;
    velocity_mapping.subindex = 0x04;
    velocity_mapping.data_type = PDODataType::DOUBLE;
    velocity_mapping.offset = 18;
    mock_config_->addPDOMapping(11, velocity_mapping);

    ServoDriverCommand cmd;
    cmd.slave_id = 11;
    cmd.target_velocity = 3.14;  // 3.14 rad/s
    cmd.max_velocity = 10.0;
    cmd.control_mode = ControlMode::VELOCITY;
    cmd.enable = true;

    // Act
    int result = manager_->writeServoCommand(cmd);

    // Assert
    EXPECT_EQ(0, result);

    double velocity;
    mock_master_->getDomainData(18, &velocity, sizeof(double));
    EXPECT_DOUBLE_EQ(3.14, velocity);
}

// 테스트 7: Servo 드라이버 - TORQUE 모드
TEST_F(MotorCommandManagerTest, WriteServoTorqueCommand) {
    // Arrange
    PDOMapping control_mapping;
    control_mapping.direction = PDODirection::OUTPUT;
    control_mapping.index = 0x1603;
    control_mapping.subindex = 0x01;
    control_mapping.data_type = PDODataType::UINT16;
    control_mapping.offset = 0;
    mock_config_->addPDOMapping(11, control_mapping);

    PDOMapping torque_mapping;
    torque_mapping.direction = PDODirection::OUTPUT;
    torque_mapping.index = 0x1603;
    torque_mapping.subindex = 0x05;
    torque_mapping.data_type = PDODataType::DOUBLE;
    torque_mapping.offset = 26;
    mock_config_->addPDOMapping(11, torque_mapping);

    ServoDriverCommand cmd;
    cmd.slave_id = 11;
    cmd.target_torque = 50.0;  // 50 Nm
    cmd.max_torque = 100.0;
    cmd.control_mode = ControlMode::TORQUE;
    cmd.enable = true;

    // Act
    int result = manager_->writeServoCommand(cmd);

    // Assert
    EXPECT_EQ(0, result);

    double torque;
    mock_master_->getDomainData(26, &torque, sizeof(double));
    EXPECT_DOUBLE_EQ(50.0, torque);
}

// 테스트 8: Servo 드라이버 - 범위 초과 거부
TEST_F(MotorCommandManagerTest, WriteServoOutOfRangeRejected) {
    // Arrange
    PDOMapping control_mapping;
    control_mapping.direction = PDODirection::OUTPUT;
    control_mapping.index = 0x1603;
    control_mapping.subindex = 0x01;
    control_mapping.data_type = PDODataType::UINT16;
    control_mapping.offset = 0;
    mock_config_->addPDOMapping(11, control_mapping);

    ServoDriverCommand cmd;
    cmd.slave_id = 11;
    cmd.target_velocity = 15.0;  // max_velocity 초과
    cmd.max_velocity = 10.0;
    cmd.control_mode = ControlMode::VELOCITY;
    cmd.enable = true;

    // Act
    int result = manager_->writeServoCommand(cmd);

    // Assert: 실패해야 함
    EXPECT_NE(0, result);
}

// 테스트 9: PDO 매핑 없음 - 실패
TEST_F(MotorCommandManagerTest, WriteBLDCNoMappingFails) {
    // Arrange: PDO 매핑을 설정하지 않음
    BLDCMotorCommand cmd;
    cmd.slave_id = 99;  // 존재하지 않는 slave
    cmd.control_mode = ControlMode::VELOCITY;
    cmd.enable = true;

    // Act
    int result = manager_->writeBLDCCommand(cmd);

    // Assert: 실패
    EXPECT_NE(0, result);
}

// 테스트 10: Master 비활성화 상태에서도 명령 전송 가능 (PDO 준비)
TEST_F(MotorCommandManagerTest, WriteCommandWhenMasterInactive) {
    // Arrange
    mock_master_->deactivate();

    PDOMapping control_mapping;
    control_mapping.direction = PDODirection::OUTPUT;
    control_mapping.index = 0x1602;
    control_mapping.subindex = 0x01;
    control_mapping.data_type = PDODataType::UINT16;
    control_mapping.offset = 0;
    mock_config_->addPDOMapping(10, control_mapping);

    BLDCMotorCommand cmd;
    cmd.slave_id = 10;
    cmd.enable = false;
    cmd.control_mode = ControlMode::DISABLED;

    // Act: 비활성화 명령은 항상 성공해야 함
    int result = manager_->writeBLDCCommand(cmd);

    // Assert
    EXPECT_EQ(0, result);
}
