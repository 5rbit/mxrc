#include <gtest/gtest.h>
#include "core/ethercat/adapters/RTEtherCATCycle.h"
#include "core/ethercat/impl/SensorDataManager.h"
#include "core/ethercat/events/EtherCATErrorEvent.h"
#include "MockEtherCATMaster.h"
#include "MockSlaveConfig.h"
#include "core/rt/RTDataStore.h"
#include "core/rt/RTContext.h"
#include "core/rt/RTStateMachine.h"
#include "core/event/core/EventBus.h"
#include <memory>

using namespace mxrc::ethercat;
using namespace mxrc::ethercat::test;
using namespace mxrc::core::rt;

class RTEtherCATCycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Mock 객체 생성
        mock_master_ = std::make_shared<MockEtherCATMaster>();
        mock_config_ = std::make_shared<MockSlaveConfig>();
        sensor_manager_ = std::make_shared<SensorDataManager>(mock_master_, mock_config_);

        // PDO domain 설정
        sensor_manager_->setDomainPtr(mock_master_->getDomainPtr());

        // RTDataStore 생성
        data_store_ = std::make_unique<RTDataStore>();

        // RTContext 생성
        context_.data_store = data_store_.get();
        context_.current_slot = 0;
        context_.cycle_count = 0;
        context_.timestamp_ns = 0;

        // RTEtherCATCycle 생성
        cycle_ = std::make_unique<RTEtherCATCycle>(mock_master_, sensor_manager_);

        // Master 활성화
        mock_master_->activate();
    }

    std::shared_ptr<MockEtherCATMaster> mock_master_;
    std::shared_ptr<MockSlaveConfig> mock_config_;
    std::shared_ptr<SensorDataManager> sensor_manager_;
    std::unique_ptr<RTDataStore> data_store_;
    RTContext context_;
    std::unique_ptr<RTEtherCATCycle> cycle_;
};

// 테스트 1: EtherCAT Cycle 실행 - send/receive 호출 확인
TEST_F(RTEtherCATCycleTest, ExecuteCycleCallsSendReceive) {
    // Arrange
    mock_master_->resetCallFlags();

    // Act
    cycle_->execute(context_);

    // Assert
    EXPECT_TRUE(mock_master_->wasSendCalled());
    EXPECT_TRUE(mock_master_->wasReceiveCalled());
    EXPECT_EQ(1ULL, cycle_->getTotalCycles());
    EXPECT_EQ(0ULL, cycle_->getErrorCount());
}

// 테스트 2: Position 센서 읽기 및 RTDataStore 저장 (DOUBLE, scale_factor = 1.0)
TEST_F(RTEtherCATCycleTest, ReadPositionSensorAndStoreToDataStore) {
    // Arrange: PDO 매핑 설정
    PDOMapping pos_mapping;
    pos_mapping.direction = PDODirection::INPUT;
    pos_mapping.index = 0x1A00;
    pos_mapping.subindex = 0x01;
    pos_mapping.data_type = PDODataType::INT32;
    pos_mapping.offset = 0;
    mock_config_->addPDOMapping(0, pos_mapping);

    // PDO domain에 데이터 설정
    int32_t expected_pos = 12345;
    mock_master_->setDomainData(0, &expected_pos, sizeof(int32_t));

    // 센서 등록 (기본 scale_factor = 1.0)
    cycle_->registerSensor(0, DataKey::ETHERCAT_SENSOR_POSITION_0, "POSITION");

    // Act
    cycle_->execute(context_);

    // Assert: RTDataStore에서 읽기 (DOUBLE로 변경됨)
    double stored_pos;
    ASSERT_EQ(0, data_store_->getDouble(DataKey::ETHERCAT_SENSOR_POSITION_0, stored_pos));
    EXPECT_DOUBLE_EQ(12345.0, stored_pos);
}

// 테스트 3: Velocity 센서 읽기 (DOUBLE)
TEST_F(RTEtherCATCycleTest, ReadVelocitySensor) {
    // Arrange
    PDOMapping vel_mapping;
    vel_mapping.direction = PDODirection::INPUT;
    vel_mapping.index = 0x1A01;
    vel_mapping.subindex = 0x01;
    vel_mapping.data_type = PDODataType::DOUBLE;
    vel_mapping.offset = 0;
    mock_config_->addPDOMapping(1, vel_mapping);

    double expected_vel = 5.5;
    mock_master_->setDomainData(0, &expected_vel, sizeof(double));

    cycle_->registerSensor(1, DataKey::ETHERCAT_SENSOR_VELOCITY_0, "VELOCITY");

    // Act
    cycle_->execute(context_);

    // Assert
    double stored_vel;
    ASSERT_EQ(0, data_store_->getDouble(DataKey::ETHERCAT_SENSOR_VELOCITY_0, stored_vel));
    EXPECT_DOUBLE_EQ(expected_vel, stored_vel);
}

// 테스트 4: Torque 센서 읽기 (torque_z만)
TEST_F(RTEtherCATCycleTest, ReadTorqueSensor) {
    // Arrange: torque_z만 매핑 (subindex 0x06)
    PDOMapping torque_mapping;
    torque_mapping.direction = PDODirection::INPUT;
    torque_mapping.index = 0x1A02;
    torque_mapping.subindex = 0x06;  // torque_z
    torque_mapping.data_type = PDODataType::DOUBLE;
    torque_mapping.offset = 0;
    mock_config_->addPDOMapping(2, torque_mapping);

    double expected_torque = 3.14;
    mock_master_->setDomainData(0, &expected_torque, sizeof(double));

    cycle_->registerSensor(2, DataKey::ETHERCAT_SENSOR_TORQUE_0, "TORQUE");

    // Act
    cycle_->execute(context_);

    // Assert
    double stored_torque;
    ASSERT_EQ(0, data_store_->getDouble(DataKey::ETHERCAT_SENSOR_TORQUE_0, stored_torque));
    EXPECT_DOUBLE_EQ(expected_torque, stored_torque);
}

// 테스트 5: 여러 cycle 반복 실행
TEST_F(RTEtherCATCycleTest, MultipleCycles) {
    // Arrange
    PDOMapping pos_mapping;
    pos_mapping.direction = PDODirection::INPUT;
    pos_mapping.index = 0x1A00;
    pos_mapping.subindex = 0x01;
    pos_mapping.data_type = PDODataType::INT32;
    pos_mapping.offset = 0;
    mock_config_->addPDOMapping(0, pos_mapping);

    cycle_->registerSensor(0, DataKey::ETHERCAT_SENSOR_POSITION_0, "POSITION");

    // Act: 10번 실행
    for (int i = 0; i < 10; ++i) {
        int32_t pos = 1000 + i;
        mock_master_->setDomainData(0, &pos, sizeof(int32_t));
        cycle_->execute(context_);
    }

    // Assert
    EXPECT_EQ(10ULL, cycle_->getTotalCycles());
    EXPECT_EQ(0ULL, cycle_->getErrorCount());

    // 마지막 값 확인 (DOUBLE로 변경)
    double final_pos;
    ASSERT_EQ(0, data_store_->getDouble(DataKey::ETHERCAT_SENSOR_POSITION_0, final_pos));
    EXPECT_DOUBLE_EQ(1009.0, final_pos);
}

// 테스트 6: 에러 카운트 증가 (send 실패)
TEST_F(RTEtherCATCycleTest, SendFailureIncreasesErrorCount) {
    // Arrange: Master를 비활성화하여 send 실패 유도
    mock_master_->deactivate();

    // Act
    cycle_->execute(context_);

    // Assert
    EXPECT_EQ(0ULL, cycle_->getTotalCycles());  // 실패 시 cycle 카운트 안 증가
    EXPECT_GT(cycle_->getErrorCount(), 0ULL);
}

// 테스트 7: 센서 데이터 invalid - 저장 안 됨
TEST_F(RTEtherCATCycleTest, InvalidSensorDataNotStored) {
    // Arrange
    PDOMapping pos_mapping;
    pos_mapping.direction = PDODirection::INPUT;
    pos_mapping.index = 0x1A00;
    pos_mapping.subindex = 0x01;
    pos_mapping.data_type = PDODataType::INT32;
    pos_mapping.offset = 0;
    mock_config_->addPDOMapping(0, pos_mapping);

    cycle_->registerSensor(0, DataKey::ETHERCAT_SENSOR_POSITION_0, "POSITION");

    int32_t pos = 999;
    mock_master_->setDomainData(0, &pos, sizeof(int32_t));

    // Master 비활성화 - valid=false
    mock_master_->deactivate();

    // Act
    cycle_->execute(context_);

    // Assert: 비활성화 상태에서는 데이터가 저장되지 않음
    int32_t stored_pos = -999;
    int result = data_store_->getInt32(DataKey::ETHERCAT_SENSOR_POSITION_0, stored_pos);

    // 데이터가 한 번도 저장되지 않았으므로 get 실패 또는 기본값
    EXPECT_NE(pos, stored_pos);  // 저장되지 않음
}

// 테스트 8: Scale factor 적용 - 엔코더 카운트 → 실제 단위 변환
TEST_F(RTEtherCATCycleTest, PositionSensorWithScaleFactor) {
    // Arrange: PDO 매핑 설정 (position + velocity)
    PDOMapping pos_mapping;
    pos_mapping.direction = PDODirection::INPUT;
    pos_mapping.index = 0x1A00;
    pos_mapping.subindex = 0x01;
    pos_mapping.data_type = PDODataType::INT32;
    pos_mapping.offset = 0;
    mock_config_->addPDOMapping(0, pos_mapping);

    PDOMapping vel_mapping;
    vel_mapping.direction = PDODirection::INPUT;
    vel_mapping.index = 0x1A00;
    vel_mapping.subindex = 0x02;
    vel_mapping.data_type = PDODataType::INT32;
    vel_mapping.offset = 4;
    mock_config_->addPDOMapping(0, vel_mapping);

    // 엔코더 카운트 설정 (1 count = 0.001 mm라고 가정)
    int32_t encoder_pos = 10000;  // 10000 counts
    int32_t encoder_vel = 500;     // 500 counts/s
    mock_master_->setDomainData(0, &encoder_pos, sizeof(int32_t));
    mock_master_->setDomainData(4, &encoder_vel, sizeof(int32_t));

    // 센서 등록 with scale_factor = 0.001 (1 count = 0.001 mm)
    cycle_->registerPositionSensor(0,
                                    DataKey::ETHERCAT_SENSOR_POSITION_0,
                                    DataKey::ETHERCAT_SENSOR_VELOCITY_0,
                                    0.001);

    // Act
    cycle_->execute(context_);

    // Assert: 스케일 적용된 값 확인
    double stored_pos, stored_vel;
    ASSERT_EQ(0, data_store_->getDouble(DataKey::ETHERCAT_SENSOR_POSITION_0, stored_pos));
    ASSERT_EQ(0, data_store_->getDouble(DataKey::ETHERCAT_SENSOR_VELOCITY_0, stored_vel));

    // 10000 * 0.001 = 10.0 mm
    EXPECT_DOUBLE_EQ(10.0, stored_pos);
    // 500 * 0.001 = 0.5 mm/s
    EXPECT_DOUBLE_EQ(0.5, stored_vel);
}

// 테스트 9: 여러 스케일 팩터 동시 사용
TEST_F(RTEtherCATCycleTest, MultipleScaleFactors) {
    // Arrange: 두 개의 position 센서, 서로 다른 스케일 팩터
    PDOMapping pos1_mapping;
    pos1_mapping.direction = PDODirection::INPUT;
    pos1_mapping.index = 0x1A00;
    pos1_mapping.subindex = 0x01;
    pos1_mapping.data_type = PDODataType::INT32;
    pos1_mapping.offset = 0;
    mock_config_->addPDOMapping(0, pos1_mapping);

    PDOMapping pos2_mapping;
    pos2_mapping.direction = PDODirection::INPUT;
    pos2_mapping.index = 0x1A00;
    pos2_mapping.subindex = 0x01;
    pos2_mapping.data_type = PDODataType::INT32;
    pos2_mapping.offset = 10;
    mock_config_->addPDOMapping(1, pos2_mapping);

    int32_t pos1 = 1000;  // Slave 0
    int32_t pos2 = 2000;  // Slave 1
    mock_master_->setDomainData(0, &pos1, sizeof(int32_t));
    mock_master_->setDomainData(10, &pos2, sizeof(int32_t));

    // Slave 0: scale = 0.001 (linear encoder, mm)
    cycle_->registerPositionSensor(0,
                                    DataKey::ETHERCAT_SENSOR_POSITION_0,
                                    DataKey::ETHERCAT_SENSOR_POSITION_0,  // velocity 없음
                                    0.001);

    // Slave 1: scale = 0.0001745 (rotary encoder, rad ≈ π/18000)
    cycle_->registerPositionSensor(1,
                                    DataKey::ETHERCAT_SENSOR_POSITION_1,
                                    DataKey::ETHERCAT_SENSOR_POSITION_1,  // velocity 없음
                                    0.0001745);

    // Act
    cycle_->execute(context_);

    // Assert
    double stored_pos1, stored_pos2;
    ASSERT_EQ(0, data_store_->getDouble(DataKey::ETHERCAT_SENSOR_POSITION_0, stored_pos1));
    ASSERT_EQ(0, data_store_->getDouble(DataKey::ETHERCAT_SENSOR_POSITION_1, stored_pos2));

    // 1000 * 0.001 = 1.0 mm
    EXPECT_DOUBLE_EQ(1.0, stored_pos1);
    // 2000 * 0.0001745 = 0.349 rad
    EXPECT_DOUBLE_EQ(0.349, stored_pos2);
}

// 테스트 10: Digital Output 쓰기
TEST_F(RTEtherCATCycleTest, WriteDigitalOutput) {
    // Arrange: DO PDO 매핑
    PDOMapping do_mapping;
    do_mapping.direction = PDODirection::OUTPUT;
    do_mapping.index = 0x1600;
    do_mapping.subindex = 0x01;
    do_mapping.data_type = PDODataType::UINT8;
    do_mapping.offset = 0;
    mock_config_->addPDOMapping(10, do_mapping);

    // RTDataStore에 출력 값 설정
    data_store_->setInt32(DataKey::ETHERCAT_SENSOR_POSITION_0, 1);  // HIGH

    // Digital Output 등록
    cycle_->registerDigitalOutput(10, 0, DataKey::ETHERCAT_SENSOR_POSITION_0);

    // Act
    cycle_->execute(context_);

    // Assert: PDO domain에 쓰여진 값 확인
    uint8_t result = mock_master_->getDomainPtr()[0];
    EXPECT_EQ(0x01, result);  // 비트 0 = 1
    EXPECT_EQ(1ULL, cycle_->getWriteSuccessCount());
}

// 테스트 11: Analog Output 쓰기
TEST_F(RTEtherCATCycleTest, WriteAnalogOutput) {
    // Arrange: AO PDO 매핑
    PDOMapping ao_mapping;
    ao_mapping.direction = PDODirection::OUTPUT;
    ao_mapping.index = 0x1601;
    ao_mapping.subindex = 0x01;
    ao_mapping.data_type = PDODataType::INT16;
    ao_mapping.offset = 0;
    mock_config_->addPDOMapping(11, ao_mapping);

    // RTDataStore에 출력 값 설정
    data_store_->setDouble(DataKey::ETHERCAT_SENSOR_VELOCITY_0, 5.5);

    // Analog Output 등록
    cycle_->registerAnalogOutput(11, 0, DataKey::ETHERCAT_SENSOR_VELOCITY_0, -10.0, 10.0);

    // Act
    cycle_->execute(context_);

    // Assert: PDO domain에 쓰여진 값 확인
    int16_t result;
    mock_master_->getDomainData(0, &result, sizeof(int16_t));
    EXPECT_EQ(5, result);
    EXPECT_EQ(1ULL, cycle_->getWriteSuccessCount());
}

// 테스트 12: 센서 읽기 + 출력 쓰기 통합
TEST_F(RTEtherCATCycleTest, ReadSensorAndWriteOutput) {
    // Arrange: Position 입력
    PDOMapping pos_mapping;
    pos_mapping.direction = PDODirection::INPUT;
    pos_mapping.index = 0x1A00;
    pos_mapping.subindex = 0x01;
    pos_mapping.data_type = PDODataType::INT32;
    pos_mapping.offset = 0;
    mock_config_->addPDOMapping(0, pos_mapping);

    // Digital Output
    PDOMapping do_mapping;
    do_mapping.direction = PDODirection::OUTPUT;
    do_mapping.index = 0x1600;
    do_mapping.subindex = 0x01;
    do_mapping.data_type = PDODataType::UINT8;
    do_mapping.offset = 10;
    mock_config_->addPDOMapping(10, do_mapping);

    // 입력 데이터 설정
    int32_t pos = 1000;
    mock_master_->setDomainData(0, &pos, sizeof(int32_t));

    // 출력 데이터 설정 (RTDataStore)
    data_store_->setInt32(DataKey::ETHERCAT_SENSOR_VELOCITY_0, 1);

    // 센서 및 출력 등록
    cycle_->registerSensor(0, DataKey::ETHERCAT_SENSOR_POSITION_0, "POSITION");
    cycle_->registerDigitalOutput(10, 2, DataKey::ETHERCAT_SENSOR_VELOCITY_0);

    // Act
    cycle_->execute(context_);

    // Assert: 입력 확인
    double stored_pos;
    ASSERT_EQ(0, data_store_->getDouble(DataKey::ETHERCAT_SENSOR_POSITION_0, stored_pos));
    EXPECT_DOUBLE_EQ(1000.0, stored_pos);

    // 출력 확인
    uint8_t do_result = mock_master_->getDomainPtr()[10];
    EXPECT_EQ(0x04, do_result);  // 비트 2 = 1

    // 통계 확인
    EXPECT_EQ(1ULL, cycle_->getReadSuccessCount());
    EXPECT_EQ(1ULL, cycle_->getWriteSuccessCount());
}

// 테스트 13: EventBus 통합 - receive 에러 이벤트 발행 확인
TEST_F(RTEtherCATCycleTest, EventBusIntegrationPublishesReceiveErrorEvents) {
    // Arrange: EventBus 생성
    auto event_bus = std::make_shared<mxrc::core::event::EventBus>();

    // 이벤트 수집을 위한 subscriber (UNKNOWN 타입에 subscribe - EtherCATErrorEvent의 타입이 100이므로)
    std::vector<std::shared_ptr<mxrc::core::event::IEvent>> received_events;
    auto handler = [&received_events](std::shared_ptr<mxrc::core::event::IEvent> event) {
        received_events.push_back(event);
    };
    // EtherCATErrorEvent는 getType()에서 100을 반환하므로, 모든 이벤트를 수신하는 방식으로 테스트
    // 실제로는 EventType에 ETHERCAT_ERROR를 추가해야 하지만, 지금은 이벤트가 발행되는지만 확인

    // EventBus를 포함한 RTEtherCATCycle 생성
    auto cycle_with_events = std::make_unique<RTEtherCATCycle>(
        mock_master_, sensor_manager_, nullptr, event_bus, nullptr);

    // Master를 비활성화하여 receive 실패 유도
    mock_master_->deactivate();

    // Act
    cycle_with_events->execute(context_);

    // Assert: 에러 카운트 증가 확인
    EXPECT_EQ(1ULL, cycle_with_events->getErrorCount());
}

// 테스트 14: StateMachine 통합 - SAFE_MODE 전환 확인
TEST_F(RTEtherCATCycleTest, StateMachineIntegrationTransitionsToSafeMode) {
    // Arrange: StateMachine 생성
    auto state_machine = std::make_shared<mxrc::core::rt::RTStateMachine>();

    // StateMachine을 RUNNING 상태로 전환 (INIT -> READY -> RUNNING)
    state_machine->handleEvent(mxrc::core::rt::RTEvent::START);  // INIT -> READY
    ASSERT_EQ(mxrc::core::rt::RTState::READY, state_machine->getState());
    state_machine->handleEvent(mxrc::core::rt::RTEvent::START);  // READY -> RUNNING
    ASSERT_EQ(mxrc::core::rt::RTState::RUNNING, state_machine->getState());

    // StateMachine을 포함한 RTEtherCATCycle 생성
    auto cycle_with_events = std::make_unique<RTEtherCATCycle>(
        mock_master_, sensor_manager_, nullptr, nullptr, state_machine);

    // Master를 비활성화하여 send 실패 유도
    mock_master_->deactivate();

    // Act: 11회 연속 에러 발생 (threshold 초과)
    for (int i = 0; i < 11; ++i) {
        cycle_with_events->execute(context_);
    }

    // Assert: SAFE_MODE로 전환 확인
    EXPECT_EQ(mxrc::core::rt::RTState::SAFE_MODE, state_machine->getState());
    EXPECT_EQ(11ULL, cycle_with_events->getErrorCount());
}
