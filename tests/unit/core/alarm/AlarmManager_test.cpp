#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include "core/alarm/impl/AlarmManager.h"
#include "core/alarm/impl/AlarmConfiguration.h"

using namespace mxrc::core::alarm;
using namespace std::chrono_literals;

/**
 * @brief AlarmManager 단위 테스트
 *
 * Feature 016: Pallet Shuttle Control System
 * User Story 2: Alarm System
 *
 * 테스트 범위:
 * - Alarm 발생 (raiseAlarm)
 * - Alarm 조회 (getAlarm, getActiveAlarms)
 * - Alarm 확인 (acknowledgeAlarm)
 * - Alarm 해결 (resolveAlarm)
 * - Critical Alarm 확인 (hasCriticalAlarm)
 * - 재발 추적 (recurrence tracking)
 * - 심각도 상향 (severity escalation)
 * - 통계 (getStatistics)
 */
class AlarmManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // AlarmConfiguration 생성 및 설정
        config_ = std::make_shared<AlarmConfiguration>();

        // 테스트용 alarm 설정
        AlarmConfig critical_alarm;
        critical_alarm.code = "E001";
        critical_alarm.name = "Emergency Stop";
        critical_alarm.severity = AlarmSeverity::CRITICAL;
        critical_alarm.recurrence_window = std::chrono::seconds(60);
        critical_alarm.recurrence_threshold = 1;
        critical_alarm.auto_reset = false;

        AlarmConfig warning_alarm;
        warning_alarm.code = "W001";
        warning_alarm.name = "Battery Low";
        warning_alarm.severity = AlarmSeverity::WARNING;
        warning_alarm.recurrence_window = std::chrono::seconds(60);
        warning_alarm.recurrence_threshold = 3;
        warning_alarm.auto_reset = true;

        AlarmConfig info_alarm;
        info_alarm.code = "I001";
        info_alarm.name = "Task Started";
        info_alarm.severity = AlarmSeverity::INFO;
        info_alarm.recurrence_window = std::chrono::seconds(30);
        info_alarm.recurrence_threshold = 5;
        info_alarm.auto_reset = true;

        // 수동으로 config 추가 (loadFromFile 대신)
        config_->addConfig(critical_alarm);
        config_->addConfig(warning_alarm);
        config_->addConfig(info_alarm);

        manager_ = std::make_shared<AlarmManager>(config_);
    }

    std::shared_ptr<AlarmConfiguration> config_;
    std::shared_ptr<AlarmManager> manager_;
};

// T034-1: Alarm 발생 테스트
TEST_F(AlarmManagerTest, RaiseAlarm_Success) {
    auto alarm = manager_->raiseAlarm("E001", "TestComponent", "Test details");

    ASSERT_TRUE(alarm.has_value());
    EXPECT_EQ(alarm->alarm_code, "E001");
    EXPECT_EQ(alarm->alarm_name, "Emergency Stop");
    EXPECT_EQ(alarm->severity, AlarmSeverity::CRITICAL);
    EXPECT_EQ(alarm->source, "TestComponent");
    EXPECT_EQ(alarm->details, "Test details");
    EXPECT_TRUE(alarm->isActive());
}

// T034-2: 알 수 없는 Alarm 코드
TEST_F(AlarmManagerTest, RaiseAlarm_UnknownCode) {
    auto alarm = manager_->raiseAlarm("E999", "TestComponent");

    EXPECT_FALSE(alarm.has_value());
}

// T034-3: Critical Alarm 확인
TEST_F(AlarmManagerTest, HasCriticalAlarm_True) {
    manager_->raiseAlarm("E001", "TestComponent");

    EXPECT_TRUE(manager_->hasCriticalAlarm());
}

// T034-4: Critical Alarm 없음
TEST_F(AlarmManagerTest, HasCriticalAlarm_False) {
    manager_->raiseAlarm("W001", "TestComponent");

    EXPECT_FALSE(manager_->hasCriticalAlarm());
}

// T034-5: 활성 Alarm 조회
TEST_F(AlarmManagerTest, GetActiveAlarms_MultipleAlarms) {
    manager_->raiseAlarm("E001", "Component1");
    manager_->raiseAlarm("W001", "Component2");
    manager_->raiseAlarm("I001", "Component3");

    auto active_alarms = manager_->getActiveAlarms();

    EXPECT_EQ(active_alarms.size(), 3);

    // 심각도 순서 확인 (CRITICAL → WARNING → INFO)
    EXPECT_EQ(active_alarms[0].severity, AlarmSeverity::CRITICAL);
    EXPECT_EQ(active_alarms[1].severity, AlarmSeverity::WARNING);
    EXPECT_EQ(active_alarms[2].severity, AlarmSeverity::INFO);
}

// T034-6: 심각도별 Alarm 조회
TEST_F(AlarmManagerTest, GetActiveAlarmsBySeverity) {
    manager_->raiseAlarm("E001", "Component1");
    std::this_thread::sleep_for(2ms);  // 타임스탬프 중복 방지
    manager_->raiseAlarm("W001", "Component2");
    std::this_thread::sleep_for(2ms);
    manager_->raiseAlarm("W001", "Component3");  // 두 번째 WARNING

    auto critical = manager_->getActiveAlarmsBySeverity(AlarmSeverity::CRITICAL);
    auto warning = manager_->getActiveAlarmsBySeverity(AlarmSeverity::WARNING);

    EXPECT_EQ(critical.size(), 1);
    EXPECT_EQ(warning.size(), 2);
}

// T034-7: Alarm 확인 (acknowledge)
TEST_F(AlarmManagerTest, AcknowledgeAlarm_Success) {
    auto alarm = manager_->raiseAlarm("E001", "TestComponent");
    ASSERT_TRUE(alarm.has_value());

    bool result = manager_->acknowledgeAlarm(alarm->alarm_id, "operator1");

    EXPECT_TRUE(result);
}

// T034-8: 존재하지 않는 Alarm 확인
TEST_F(AlarmManagerTest, AcknowledgeAlarm_NotFound) {
    bool result = manager_->acknowledgeAlarm("invalid_id", "operator1");

    EXPECT_FALSE(result);
}

// T034-9: Alarm 해결 (resolve)
TEST_F(AlarmManagerTest, ResolveAlarm_Success) {
    auto alarm = manager_->raiseAlarm("E001", "TestComponent");
    ASSERT_TRUE(alarm.has_value());

    bool result = manager_->resolveAlarm(alarm->alarm_id);

    EXPECT_TRUE(result);
    EXPECT_FALSE(manager_->hasCriticalAlarm());
}

// T034-10: 이미 해결된 Alarm 재해결
TEST_F(AlarmManagerTest, ResolveAlarm_AlreadyResolved) {
    auto alarm = manager_->raiseAlarm("E001", "TestComponent");
    ASSERT_TRUE(alarm.has_value());

    manager_->resolveAlarm(alarm->alarm_id);
    bool result = manager_->resolveAlarm(alarm->alarm_id);

    EXPECT_FALSE(result);  // 이미 해결됨
}

// T034-11: 모든 Alarm 리셋
TEST_F(AlarmManagerTest, ResetAllAlarms) {
    manager_->raiseAlarm("E001", "Component1");
    manager_->raiseAlarm("W001", "Component2");
    manager_->raiseAlarm("I001", "Component3");

    size_t count = manager_->resetAllAlarms();

    EXPECT_EQ(count, 3);
    EXPECT_EQ(manager_->getActiveAlarms().size(), 0);
    EXPECT_FALSE(manager_->hasCriticalAlarm());
}

// T034-12: 통계 확인
TEST_F(AlarmManagerTest, GetStatistics) {
    manager_->raiseAlarm("E001", "Component1");
    manager_->raiseAlarm("W001", "Component2");
    manager_->raiseAlarm("I001", "Component3");

    auto stats = manager_->getStatistics();

    EXPECT_EQ(stats.total_raised, 3);
    EXPECT_EQ(stats.active_count, 3);
    EXPECT_EQ(stats.critical_count, 1);
    EXPECT_EQ(stats.warning_count, 1);
    EXPECT_EQ(stats.info_count, 1);

    // Alarm 하나 해결
    auto alarm = manager_->getActiveAlarms()[0];
    manager_->resolveAlarm(alarm.alarm_id);

    stats = manager_->getStatistics();
    EXPECT_EQ(stats.active_count, 2);
    EXPECT_EQ(stats.resolved_count, 1);
}

// T036: 재발 추적 테스트
TEST_F(AlarmManagerTest, RecurrenceTracking_WithinWindow) {
    // 첫 번째 발생
    auto alarm1 = manager_->raiseAlarm("W001", "Component1");
    ASSERT_TRUE(alarm1.has_value());
    EXPECT_EQ(alarm1->recurrence_count, 1);

    // 짧은 시간 후 재발 (window 내)
    std::this_thread::sleep_for(2ms);
    auto alarm2 = manager_->raiseAlarm("W001", "Component1");
    ASSERT_TRUE(alarm2.has_value());
    EXPECT_EQ(alarm2->recurrence_count, 2);

    // 또 재발
    std::this_thread::sleep_for(2ms);
    auto alarm3 = manager_->raiseAlarm("W001", "Component1");
    ASSERT_TRUE(alarm3.has_value());
    EXPECT_EQ(alarm3->recurrence_count, 3);
}

// T037: 심각도 상향 테스트
TEST_F(AlarmManagerTest, SeverityEscalation_ThresholdExceeded) {
    // W001의 recurrence_threshold는 3

    // 1번째 발생 - WARNING 유지
    auto alarm1 = manager_->raiseAlarm("W001", "Component1");
    ASSERT_TRUE(alarm1.has_value());
    EXPECT_EQ(alarm1->severity, AlarmSeverity::WARNING);
    EXPECT_EQ(alarm1->recurrence_count, 1);

    // 2번째 발생 - WARNING 유지
    std::this_thread::sleep_for(2ms);
    auto alarm2 = manager_->raiseAlarm("W001", "Component1");
    EXPECT_EQ(alarm2->severity, AlarmSeverity::WARNING);
    EXPECT_EQ(alarm2->recurrence_count, 2);

    // 3번째 발생 - CRITICAL로 상향 (threshold 도달: recurrence >= 3)
    std::this_thread::sleep_for(2ms);
    auto alarm3 = manager_->raiseAlarm("W001", "Component1");
    EXPECT_EQ(alarm3->severity, AlarmSeverity::CRITICAL);
    EXPECT_EQ(alarm3->recurrence_count, 3);

    // 4번째 발생 - CRITICAL 유지
    std::this_thread::sleep_for(2ms);
    auto alarm4 = manager_->raiseAlarm("W001", "Component1");
    EXPECT_EQ(alarm4->severity, AlarmSeverity::CRITICAL);
    EXPECT_EQ(alarm4->recurrence_count, 4);
}

// Thread-safety 테스트
TEST_F(AlarmManagerTest, ConcurrentAlarmRaising) {
    const int thread_count = 10;
    const int alarms_per_thread = 5;

    std::vector<std::thread> threads;

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([this, i, alarms_per_thread]() {
            for (int j = 0; j < alarms_per_thread; ++j) {
                manager_->raiseAlarm("I001", "Thread_" + std::to_string(i));
                std::this_thread::sleep_for(1ms);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto stats = manager_->getStatistics();
    EXPECT_EQ(stats.total_raised, thread_count * alarms_per_thread);
}
