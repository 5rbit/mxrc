// SafetyCheckSequenceTest.cpp - SafetyCheckSequence 단위 테스트
// Copyright (C) 2025 MXRC Project
//
// Feature 016: Pallet Shuttle Control System (T085)
// Phase 8: Periodic safety checks

#include <gtest/gtest.h>
#include <memory>
#include "robot/pallet_shuttle/sequences/SafetyCheckSequence.h"
#include "robot/pallet_shuttle/interfaces/IPalletShuttleStateAccessor.h"
#include "core/alarm/interfaces/IAlarmManager.h"
#include "core/fieldbus/interfaces/IFieldbusDriver.h"
#include "core/sequence/dto/SequenceStatus.h"

using namespace mxrc::robot::pallet_shuttle;
using namespace mxrc::core::alarm;
using namespace mxrc::core::fieldbus;
using namespace mxrc::core::sequence;

// Mock implementations
class MockStateAccessor : public IPalletShuttleStateAccessor {
public:
    double battery_level_ = 1.0;
    double total_distance_ = 10000.0; // 10km
    uint32_t completed_tasks_ = 100;
    Position current_position_{0, 0, 0, 0};

    std::optional<Position> getCurrentPosition() const override { return current_position_; }
    double getBatteryLevel() const override { return battery_level_; }
    double getTotalDistance() const override { return total_distance_; }
    uint32_t getCompletedTaskCount() const override { return completed_tasks_; }

    // Other methods with default implementations
    std::optional<Position> getTargetPosition() const override { return std::nullopt; }
    bool updatePosition(const Position&) override { return true; }
    bool setTargetPosition(const Position&) override { return true; }
    ShuttleState getState() const override { return ShuttleState::IDLE; }
    bool setState(ShuttleState) override { return true; }
    std::optional<PalletInfo> getLoadedPallet() const override { return std::nullopt; }
    bool updateLoadedPallet(const PalletInfo&) override { return true; }
    bool clearLoadedPallet() override { return true; }
    double getCurrentSpeed() const override { return 0; }
    void incrementCompletedTaskCount() override { completed_tasks_++; }
    std::chrono::system_clock::time_point getLastUpdateTime() const override {
        return std::chrono::system_clock::now();
    }
    std::optional<std::chrono::system_clock::time_point> getTaskStartTime() const override {
        return std::nullopt;
    }
    void setTaskStartTime(const std::chrono::system_clock::time_point&) override {}
    void clearTaskStartTime() override {}
};

class MockAlarmManager : public IAlarmManager {
public:
    std::vector<std::string> raised_alarms_;

    AlarmDto raiseAlarm(const std::string& code, const std::string& source,
                       const std::string& details = "") override {
        raised_alarms_.push_back(code);
        return AlarmDto{
            code + "_001", code, "Test Alarm",
            AlarmSeverity::INFO, AlarmState::ACTIVE,
            std::chrono::system_clock::now()
        };
    }

    std::vector<AlarmDto> getActiveAlarms() const override { return {}; }
    bool resolveAlarm(const std::string&) override { return true; }
    bool acknowledgeAlarm(const std::string&) override { return true; }
    bool resetAllAlarms() override { return true; }
    bool hasCriticalAlarm() const override { return false; }
    std::vector<AlarmDto> getActiveAlarmsBySeverity(AlarmSeverity) const override { return {}; }
};

class MockFieldbusDriver : public IFieldbusDriver {
public:
    bool sensors_ok_ = true;
    bool motors_ok_ = true;
    bool emergency_stop_ = false;

    bool connect() override { return true; }
    bool disconnect() override { return true; }
    bool isConnected() const override { return true; }

    bool read(const std::string& address, std::any& value) override {
        if (address == "sensor/safety/emergency_stop") {
            value = emergency_stop_;
            return true;
        } else if (address == "sensor/diagnostic/all_ok") {
            value = sensors_ok_;
            return true;
        } else if (address == "motor/diagnostic/status") {
            value = motors_ok_ ? "OK" : "FAULT";
            return true;
        }
        return false;
    }

    bool write(const std::string&, const std::any&) override { return true; }
    std::vector<std::string> scan() override { return {}; }
    std::string getDriverInfo() const override { return "MockDriver"; }
    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
};

class SafetyCheckSequenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        state_accessor_ = std::make_shared<MockStateAccessor>();
        alarm_manager_ = std::make_shared<MockAlarmManager>();
        fieldbus_driver_ = std::make_shared<MockFieldbusDriver>();
    }

    std::shared_ptr<MockStateAccessor> state_accessor_;
    std::shared_ptr<MockAlarmManager> alarm_manager_;
    std::shared_ptr<MockFieldbusDriver> fieldbus_driver_;
};

// T085: SafetyCheckSequence 기본 테스트
TEST_F(SafetyCheckSequenceTest, BasicSafetyCheck) {
    // Given: 정상 상태의 시스템
    auto sequence = std::make_shared<SafetyCheckSequence>(
        "safety_001", state_accessor_, alarm_manager_, fieldbus_driver_);

    // When: 안전 점검 실행
    auto result = sequence->execute();

    // Then: 점검 성공
    EXPECT_EQ(result.status, SequenceStatus::COMPLETED);
    EXPECT_EQ(sequence->getCheckResults().size(), 6); // 6개 항목 점검
}

// 배터리 부족 감지 테스트
TEST_F(SafetyCheckSequenceTest, LowBatteryDetection) {
    // Given: 배터리 부족 상태
    state_accessor_->battery_level_ = 0.15; // 15%

    auto sequence = std::make_shared<SafetyCheckSequence>(
        "safety_002", state_accessor_, alarm_manager_, fieldbus_driver_);

    // When: 안전 점검 실행
    sequence->execute();

    // Then: 배터리 경고 알람 발생
    EXPECT_TRUE(std::find(alarm_manager_->raised_alarms_.begin(),
                          alarm_manager_->raised_alarms_.end(),
                          "W001") != alarm_manager_->raised_alarms_.end());
}

// 정비 주기 도달 테스트
TEST_F(SafetyCheckSequenceTest, MaintenanceDueDetection) {
    // Given: 정비 주기 도달 (50km 또는 500 작업)
    state_accessor_->total_distance_ = 51000.0; // 51km
    state_accessor_->completed_tasks_ = 501;

    auto sequence = std::make_shared<SafetyCheckSequence>(
        "safety_003", state_accessor_, alarm_manager_, fieldbus_driver_);

    // When: 안전 점검 실행
    sequence->execute();

    // Then: 정비 필요 알람 발생
    EXPECT_TRUE(std::find(alarm_manager_->raised_alarms_.begin(),
                          alarm_manager_->raised_alarms_.end(),
                          "I001") != alarm_manager_->raised_alarms_.end());
}

// 센서 이상 감지 테스트
TEST_F(SafetyCheckSequenceTest, SensorFaultDetection) {
    // Given: 센서 이상 상태
    fieldbus_driver_->sensors_ok_ = false;

    auto sequence = std::make_shared<SafetyCheckSequence>(
        "safety_004", state_accessor_, alarm_manager_, fieldbus_driver_);

    // When: 안전 점검 실행
    auto result = sequence->execute();

    // Then: 점검 실패 및 경고 알람
    auto checks = sequence->getCheckResults();
    auto sensor_check = std::find_if(checks.begin(), checks.end(),
        [](const auto& check) { return check.name == "Sensor Diagnostics"; });

    ASSERT_TRUE(sensor_check != checks.end());
    EXPECT_FALSE(sensor_check->passed);
    EXPECT_FALSE(alarm_manager_->raised_alarms_.empty());
}

// 비상 정지 상태 테스트
TEST_F(SafetyCheckSequenceTest, EmergencyStopDetection) {
    // Given: 비상 정지 활성화
    fieldbus_driver_->emergency_stop_ = true;

    auto sequence = std::make_shared<SafetyCheckSequence>(
        "safety_005", state_accessor_, alarm_manager_, fieldbus_driver_);

    // When: 안전 점검 실행
    auto result = sequence->execute();

    // Then: 점검 실패 및 Critical 알람
    auto checks = sequence->getCheckResults();
    auto estop_check = std::find_if(checks.begin(), checks.end(),
        [](const auto& check) { return check.name == "Emergency Stop"; });

    ASSERT_TRUE(estop_check != checks.end());
    EXPECT_FALSE(estop_check->passed);
    EXPECT_TRUE(std::find(alarm_manager_->raised_alarms_.begin(),
                          alarm_manager_->raised_alarms_.end(),
                          "E001") != alarm_manager_->raised_alarms_.end());
}

// 모든 점검 통과 테스트
TEST_F(SafetyCheckSequenceTest, AllChecksPass) {
    // Given: 모든 시스템 정상
    state_accessor_->battery_level_ = 0.95;
    state_accessor_->total_distance_ = 5000.0; // 5km
    state_accessor_->completed_tasks_ = 50;
    fieldbus_driver_->sensors_ok_ = true;
    fieldbus_driver_->motors_ok_ = true;
    fieldbus_driver_->emergency_stop_ = false;

    auto sequence = std::make_shared<SafetyCheckSequence>(
        "safety_006", state_accessor_, alarm_manager_, fieldbus_driver_);

    // When: 안전 점검 실행
    auto result = sequence->execute();

    // Then: 모든 점검 통과, 알람 없음
    EXPECT_EQ(result.status, SequenceStatus::COMPLETED);

    auto checks = sequence->getCheckResults();
    for (const auto& check : checks) {
        EXPECT_TRUE(check.passed) << "Failed check: " << check.name;
    }

    EXPECT_TRUE(alarm_manager_->raised_alarms_.empty());
}

// 점검 취소 테스트
TEST_F(SafetyCheckSequenceTest, CancelSafetyCheck) {
    auto sequence = std::make_shared<SafetyCheckSequence>(
        "safety_007", state_accessor_, alarm_manager_, fieldbus_driver_);

    // Given: 점검 시작
    sequence->start();

    // When: 점검 취소
    sequence->cancel();

    // Then: 취소 상태
    EXPECT_EQ(sequence->getStatus(), SequenceStatus::CANCELLED);
}