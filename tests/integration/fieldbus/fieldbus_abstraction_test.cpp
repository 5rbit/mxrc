// fieldbus_abstraction_test.cpp - 필드버스 추상화 계층 통합 테스트
// 기능 019 - US4: T045

#include <gtest/gtest.h>
#include "core/fieldbus/factory/FieldbusFactory.h"
#include "core/fieldbus/interfaces/IFieldbus.h"
#include "core/fieldbus/drivers/MockDriver.h"
#include <vector>
#include <thread>
#include <chrono>

using namespace mxrc::core::fieldbus;

// 필드버스 통합 테스트 클래스
class FieldbusIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 깨끗한 상태 보장
        FieldbusFactory::clearProtocols();
    }

    void TearDown() override {
        // 정리
        if (fieldbus_) {
            fieldbus_->stop();
            fieldbus_.reset();
        }
    }

    IFieldbusPtr fieldbus_;
};

// ============================================================================
// T045: Mock 필드버스 모터 제어 시나리오 테스트
// ============================================================================

// Mock 드라이버의 기본 생명주기 테스트
TEST_F(FieldbusIntegrationTest, MockDriver_BasicLifecycle) {
    // 팩토리를 통해 Mock 필드버스 생성
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    fieldbus_ = FieldbusFactory::create(config);
    ASSERT_NE(fieldbus_, nullptr);

    // 초기화
    EXPECT_TRUE(fieldbus_->initialize());
    EXPECT_EQ(fieldbus_->getStatus(), FieldbusStatus::INITIALIZED);

    // 시작
    EXPECT_TRUE(fieldbus_->start());
    EXPECT_EQ(fieldbus_->getStatus(), FieldbusStatus::RUNNING);

    // 정지
    EXPECT_TRUE(fieldbus_->stop());
    EXPECT_EQ(fieldbus_->getStatus(), FieldbusStatus::STOPPED);
}

// Mock 드라이버의 센서 데이터 읽기 테스트
TEST_F(FieldbusIntegrationTest, MockDriver_SensorDataRead) {
    // Mock 필드버스 생성 및 초기화
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    fieldbus_ = FieldbusFactory::create(config);
    ASSERT_NE(fieldbus_, nullptr);
    ASSERT_TRUE(fieldbus_->initialize());
    ASSERT_TRUE(fieldbus_->start());

    // 센서 데이터 읽기
    std::vector<double> sensor_data;
    EXPECT_TRUE(fieldbus_->readSensors(sensor_data));

    // Mock 드라이버는 4개의 센서 값을 반환
    ASSERT_EQ(sensor_data.size(), 4u);

    // 센서 값이 예상 범위 내에 있는지 확인
    for (const auto& value : sensor_data) {
        EXPECT_GE(value, 0.0);
        EXPECT_LE(value, 100.0);
    }
}

// Mock 드라이버의 액추에이터 제어 테스트
TEST_F(FieldbusIntegrationTest, MockDriver_ActuatorControl) {
    // Mock 필드버스 생성 및 초기화
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    fieldbus_ = FieldbusFactory::create(config);
    ASSERT_NE(fieldbus_, nullptr);
    ASSERT_TRUE(fieldbus_->initialize());
    ASSERT_TRUE(fieldbus_->start());

    // 액추에이터 명령 쓰기
    std::vector<double> actuator_commands = {10.0, 20.0, 30.0, 40.0};
    EXPECT_TRUE(fieldbus_->writeActuators(actuator_commands));

    // 다시 읽어서 확인 (Mock 드라이버는 명령을 에코)
    std::vector<double> sensor_data;
    EXPECT_TRUE(fieldbus_->readSensors(sensor_data));
    ASSERT_EQ(sensor_data.size(), 4u);
}

// Mock 드라이버의 주기적 동작 테스트
TEST_F(FieldbusIntegrationTest, MockDriver_CyclicOperation) {
    // Mock 필드버스 생성 및 초기화
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    fieldbus_ = FieldbusFactory::create(config);
    ASSERT_NE(fieldbus_, nullptr);
    ASSERT_TRUE(fieldbus_->initialize());
    ASSERT_TRUE(fieldbus_->start());

    // 주기적 동작 시뮬레이션 (10 사이클)
    const int num_cycles = 10;
    std::vector<double> actuator_commands = {1.0, 2.0, 3.0, 4.0};

    for (int cycle = 0; cycle < num_cycles; ++cycle) {
        // 액추에이터 쓰기
        EXPECT_TRUE(fieldbus_->writeActuators(actuator_commands));

        // 센서 읽기
        std::vector<double> sensor_data;
        EXPECT_TRUE(fieldbus_->readSensors(sensor_data));
        EXPECT_EQ(sensor_data.size(), 4u);

        // 사이클 타임 시뮬레이션
        std::this_thread::sleep_for(std::chrono::microseconds(config.cycle_time_us));

        // 다음 사이클을 위한 명령 업데이트
        for (auto& cmd : actuator_commands) {
            cmd += 0.1;
        }
    }

    // 통계 확인
    auto stats = fieldbus_->getStatistics();
    EXPECT_GE(stats.total_cycles, static_cast<uint64_t>(num_cycles));
    EXPECT_EQ(stats.communication_errors, 0u);
}

// Mock 드라이버의 오류 처리 테스트
TEST_F(FieldbusIntegrationTest, MockDriver_ErrorHandling) {
    // Mock 필드버스 생성
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    fieldbus_ = FieldbusFactory::create(config);
    ASSERT_NE(fieldbus_, nullptr);

    // 초기화 전에 읽기/쓰기 시도 - 정상적으로 실패해야 함
    std::vector<double> data;
    EXPECT_FALSE(fieldbus_->readSensors(data));
    EXPECT_FALSE(fieldbus_->writeActuators(data));

    // 초기화 및 시작
    ASSERT_TRUE(fieldbus_->initialize());
    ASSERT_TRUE(fieldbus_->start());

    // 이제 작업이 성공해야 함
    EXPECT_TRUE(fieldbus_->readSensors(data));
    EXPECT_TRUE(fieldbus_->writeActuators({1.0, 2.0, 3.0, 4.0}));
}

// Mock 드라이버의 통계 추적 테스트
TEST_F(FieldbusIntegrationTest, MockDriver_StatisticsTracking) {
    // Mock 필드버스 생성 및 초기화
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    fieldbus_ = FieldbusFactory::create(config);
    ASSERT_NE(fieldbus_, nullptr);
    ASSERT_TRUE(fieldbus_->initialize());
    ASSERT_TRUE(fieldbus_->start());

    // 초기 통계 가져오기
    auto stats_before = fieldbus_->getStatistics();
    uint64_t initial_cycles = stats_before.total_cycles;

    // 일부 작업 수행
    const int operations = 5;
    for (int i = 0; i < operations; ++i) {
        std::vector<double> data;
        fieldbus_->readSensors(data);
        fieldbus_->writeActuators({1.0, 2.0, 3.0, 4.0});
    }

    // 업데이트된 통계 가져오기
    auto stats_after = fieldbus_->getStatistics();

    // 통계가 업데이트되었는지 확인
    EXPECT_GT(stats_after.total_cycles, initial_cycles);
    EXPECT_EQ(stats_after.communication_errors, 0u);
}

// 여러 드라이버 인스턴스 테스트
TEST_F(FieldbusIntegrationTest, MultipleDriverInstances) {
    // 두 개의 별도 Mock 필드버스 인스턴스 생성
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

    // 둘 다 초기화 및 시작
    EXPECT_TRUE(fieldbus1->initialize());
    EXPECT_TRUE(fieldbus1->start());
    EXPECT_TRUE(fieldbus2->initialize());
    EXPECT_TRUE(fieldbus2->start());

    // 둘 다 독립적으로 작동해야 함
    std::vector<double> data1, data2;
    EXPECT_TRUE(fieldbus1->readSensors(data1));
    EXPECT_TRUE(fieldbus2->readSensors(data2));

    // 정리
    fieldbus1->stop();
    fieldbus2->stop();
}

// 프로토콜 전환 테스트
TEST_F(FieldbusIntegrationTest, ProtocolSwitching) {
    // Mock 프로토콜로 시작
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    auto mock_fieldbus = FieldbusFactory::create(config);
    ASSERT_NE(mock_fieldbus, nullptr);
    EXPECT_EQ(mock_fieldbus->getProtocolName(), "Mock");

    // 초기화 및 실행
    ASSERT_TRUE(mock_fieldbus->initialize());
    ASSERT_TRUE(mock_fieldbus->start());
    std::vector<double> data;
    EXPECT_TRUE(mock_fieldbus->readSensors(data));
    mock_fieldbus->stop();

    // EtherCAT 프로토콜로 전환 (초기화는 실패하지만 전환을 보여줌)
    config.protocol = "EtherCAT";
    auto ethercat_fieldbus = FieldbusFactory::create(config);
    ASSERT_NE(ethercat_fieldbus, nullptr);
    EXPECT_EQ(ethercat_fieldbus->getProtocolName(), "EtherCAT");

    // 참고: EtherCAT 초기화는 실제 하드웨어 없이는 실패하지만
    // 팩토리 패턴은 원활한 프로토콜 전환을 허용함
}

// 반복적인 시작/정지 테스트
TEST_F(FieldbusIntegrationTest, RepeatedStartStop) {
    // Mock 필드버스 생성
    FieldbusConfig config;
    config.protocol = "Mock";
    config.config_file = "test.yaml";
    config.cycle_time_us = 1000;
    config.device_count = 4;

    fieldbus_ = FieldbusFactory::create(config);
    ASSERT_NE(fieldbus_, nullptr);
    ASSERT_TRUE(fieldbus_->initialize());

    // 반복적으로 시작 및 정지
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(fieldbus_->start());
        EXPECT_EQ(fieldbus_->getStatus(), FieldbusStatus::RUNNING);

        // 일부 작업 수행
        std::vector<double> data;
        EXPECT_TRUE(fieldbus_->readSensors(data));

        EXPECT_TRUE(fieldbus_->stop());
        EXPECT_EQ(fieldbus_->getStatus(), FieldbusStatus::STOPPED);
    }
}
