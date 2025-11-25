#include <gtest/gtest.h>
#include <memory>
#include "core/alarm/impl/AlarmConfiguration.h"

using namespace mxrc::core::alarm;

/**
 * @brief AlarmConfiguration 단위 테스트
 *
 * Feature 016: Pallet Shuttle Control System
 * User Story 2: Alarm System
 *
 * 테스트 범위:
 * - YAML 파싱 (T035)
 * - Alarm 설정 조회
 * - 심각도 상향 조건 확인
 * - 설정 검증
 */
class AlarmConfigurationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = std::make_shared<AlarmConfiguration>();
    }

    std::shared_ptr<AlarmConfiguration> config_;
};

// T035-1: 수동으로 추가된 Alarm 설정 조회
TEST_F(AlarmConfigurationTest, GetAlarmConfig_ManuallyAdded) {
    AlarmConfig test_alarm;
    test_alarm.code = "E001";
    test_alarm.name = "Emergency Stop";
    test_alarm.severity = AlarmSeverity::CRITICAL;
    test_alarm.recurrence_window = std::chrono::seconds(60);
    test_alarm.recurrence_threshold = 1;
    test_alarm.auto_reset = false;

    config_->addConfig(test_alarm);

    auto result = config_->getAlarmConfig("E001");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->code, "E001");
    EXPECT_EQ(result->name, "Emergency Stop");
    EXPECT_EQ(result->severity, AlarmSeverity::CRITICAL);
}

// T035-2: 존재하지 않는 Alarm 코드 조회
TEST_F(AlarmConfigurationTest, GetAlarmConfig_NotFound) {
    auto result = config_->getAlarmConfig("E999");
    EXPECT_FALSE(result.has_value());
}

// T035-3: hasAlarmConfig 확인
TEST_F(AlarmConfigurationTest, HasAlarmConfig) {
    AlarmConfig test_alarm;
    test_alarm.code = "W001";
    test_alarm.name = "Battery Low";
    test_alarm.severity = AlarmSeverity::WARNING;

    config_->addConfig(test_alarm);

    EXPECT_TRUE(config_->hasAlarmConfig("W001"));
    EXPECT_FALSE(config_->hasAlarmConfig("W999"));
}

// T035-4: getAllConfigs 조회
TEST_F(AlarmConfigurationTest, GetAllConfigs) {
    AlarmConfig alarm1;
    alarm1.code = "E001";
    alarm1.name = "Emergency";
    alarm1.severity = AlarmSeverity::CRITICAL;

    AlarmConfig alarm2;
    alarm2.code = "W001";
    alarm2.name = "Warning";
    alarm2.severity = AlarmSeverity::WARNING;

    AlarmConfig alarm3;
    alarm3.code = "I001";
    alarm3.name = "Info";
    alarm3.severity = AlarmSeverity::INFO;

    config_->addConfig(alarm1);
    config_->addConfig(alarm2);
    config_->addConfig(alarm3);

    auto all_configs = config_->getAllConfigs();
    EXPECT_EQ(all_configs.size(), 3);
}

// T035-5: 심각도 상향 조건 - threshold 미만
TEST_F(AlarmConfigurationTest, ShouldEscalateSeverity_BelowThreshold) {
    AlarmConfig alarm;
    alarm.code = "W001";
    alarm.name = "Battery Low";
    alarm.severity = AlarmSeverity::WARNING;
    alarm.recurrence_threshold = 3;

    config_->addConfig(alarm);

    // recurrence_count = 2 (threshold 3 미만)
    auto severity = config_->shouldEscalateSeverity("W001", 2);
    EXPECT_EQ(severity, AlarmSeverity::WARNING);  // 상향 안 됨
}

// T035-6: 심각도 상향 조건 - threshold 도달
TEST_F(AlarmConfigurationTest, ShouldEscalateSeverity_AtThreshold) {
    AlarmConfig alarm;
    alarm.code = "W001";
    alarm.name = "Battery Low";
    alarm.severity = AlarmSeverity::WARNING;
    alarm.recurrence_threshold = 3;

    config_->addConfig(alarm);

    // recurrence_count = 3 (threshold 도달)
    auto severity = config_->shouldEscalateSeverity("W001", 3);
    EXPECT_EQ(severity, AlarmSeverity::CRITICAL);  // WARNING → CRITICAL
}

// T035-7: 심각도 상향 조건 - threshold 초과
TEST_F(AlarmConfigurationTest, ShouldEscalateSeverity_AboveThreshold) {
    AlarmConfig alarm;
    alarm.code = "W001";
    alarm.name = "Battery Low";
    alarm.severity = AlarmSeverity::WARNING;
    alarm.recurrence_threshold = 3;

    config_->addConfig(alarm);

    // recurrence_count = 5 (threshold 초과)
    auto severity = config_->shouldEscalateSeverity("W001", 5);
    EXPECT_EQ(severity, AlarmSeverity::CRITICAL);  // WARNING → CRITICAL
}

// T035-8: INFO 심각도 상향
TEST_F(AlarmConfigurationTest, ShouldEscalateSeverity_InfoToWarning) {
    AlarmConfig alarm;
    alarm.code = "I001";
    alarm.name = "Task Started";
    alarm.severity = AlarmSeverity::INFO;
    alarm.recurrence_threshold = 5;

    config_->addConfig(alarm);

    // threshold 도달 시 INFO → WARNING
    auto severity = config_->shouldEscalateSeverity("I001", 5);
    EXPECT_EQ(severity, AlarmSeverity::WARNING);
}

// T035-9: CRITICAL은 더 이상 상향 안 됨
TEST_F(AlarmConfigurationTest, ShouldEscalateSeverity_CriticalNoEscalation) {
    AlarmConfig alarm;
    alarm.code = "E001";
    alarm.name = "Emergency";
    alarm.severity = AlarmSeverity::CRITICAL;
    alarm.recurrence_threshold = 1;

    config_->addConfig(alarm);

    // CRITICAL은 더 이상 상향 안 됨
    auto severity = config_->shouldEscalateSeverity("E001", 10);
    EXPECT_EQ(severity, AlarmSeverity::CRITICAL);
}

// T035-10: 설정 검증 - 빈 설정은 유효하지 않음
TEST_F(AlarmConfigurationTest, Validate_EmptyConfig) {
    // 빈 설정은 유효하지 않음
    EXPECT_FALSE(config_->validate());
}

// T035-11: 설정 검증 - 정상 설정
TEST_F(AlarmConfigurationTest, Validate_ValidConfig) {
    AlarmConfig alarm1;
    alarm1.code = "E001";
    alarm1.name = "Emergency";
    alarm1.severity = AlarmSeverity::CRITICAL;

    AlarmConfig alarm2;
    alarm2.code = "W001";
    alarm2.name = "Warning";
    alarm2.severity = AlarmSeverity::WARNING;

    config_->addConfig(alarm1);
    config_->addConfig(alarm2);

    EXPECT_TRUE(config_->validate());
}
