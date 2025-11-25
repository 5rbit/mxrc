// alarm_handling_test.cpp - Pallet Shuttle Alarm 처리 통합 테스트
// Copyright (C) 2025 MXRC Project
//
// Critical Alarm 발생 시 즉시 중단 테스트
// Feature 016: Pallet Shuttle Control System (T037)

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>

#include "core/alarm/impl/AlarmManager.h"
#include "core/alarm/impl/AlarmConfiguration.h"
#include "core/control/impl/BehaviorArbiter.h"
#include "core/control/dto/BehaviorRequest.h"
#include "core/control/dto/Priority.h"
#include "core/control/dto/ControlMode.h"
#include "core/task/core/Task.h"
#include "core/datastore/DataStore.h"
#include "core/event/core/EventBus.h"

using namespace mxrc::core;

class AlarmHandlingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 설정 파일 생성
        createAlarmConfig();

        // DataStore와 EventBus 생성
        data_store_ = DataStore::create();
        event_bus_ = std::make_shared<event::EventBus>();

        // AlarmConfiguration 생성
        alarm_config_ = std::make_shared<alarm::AlarmConfiguration>(config_file_);

        // AlarmManager 생성
        alarm_manager_ = std::make_shared<alarm::AlarmManager>(
            alarm_config_, data_store_, event_bus_);

        // BehaviorArbiter 생성
        behavior_arbiter_ = std::make_shared<control::BehaviorArbiter>(
            alarm_manager_, data_store_);
    }

    void TearDown() override {
        // 설정 파일 삭제
        std::remove(config_file_.c_str());
    }

    void createAlarmConfig() {
        config_file_ = "/tmp/test_alarm_config.yaml";

        std::string config_content = R"(
# Test Alarm Configuration
alarms:
  - code: "ALM001"
    name: "비상 정지"
    severity: CRITICAL
    description: "비상 정지 버튼 눌림"
    escalation_threshold: 0

  - code: "ALM002"
    name: "모터 과열"
    severity: WARNING
    description: "모터 온도 임계값 초과"
    escalation_threshold: 3
    escalation_severity: CRITICAL

  - code: "ALM003"
    name: "통신 지연"
    severity: INFO
    description: "통신 지연 경고"
    escalation_threshold: 5
    escalation_severity: WARNING

recurrence_window_minutes: 5
)";

        std::ofstream file(config_file_);
        file << config_content;
        file.close();
    }

    std::shared_ptr<task::Task> createDummyTask(const std::string& id) {
        auto task = std::make_shared<task::Task>(id);
        task->setName("Test Task " + id);
        task->setWorkType(task::WorkType::ACTION);
        task->setExecutionMode(task::ExecutionMode::ONCE);
        return task;
    }

protected:
    std::string config_file_;
    std::shared_ptr<DataStore> data_store_;
    std::shared_ptr<event::EventBus> event_bus_;
    std::shared_ptr<alarm::IAlarmConfiguration> alarm_config_;
    std::shared_ptr<alarm::AlarmManager> alarm_manager_;
    std::shared_ptr<control::BehaviorArbiter> behavior_arbiter_;
};

// T037: Critical Alarm 발생 시 즉시 중단
TEST_F(AlarmHandlingTest, CriticalAlarmImmediateStop) {
    // 1. Normal task 실행
    control::BehaviorRequest normal_request;
    normal_request.behavior_id = "task_001";
    normal_request.priority = control::Priority::NORMAL_TASK;
    normal_request.task = createDummyTask("task_001");

    // Behavior 요청
    ASSERT_TRUE(behavior_arbiter_->requestBehavior(normal_request));

    // Task 시작되도록 tick
    behavior_arbiter_->tick();

    // 현재 task가 실행 중인지 확인
    EXPECT_EQ(behavior_arbiter_->getCurrentTaskId(), "task_001");
    EXPECT_EQ(behavior_arbiter_->getCurrentMode(), control::ControlMode::MANUAL);

    // 2. Critical Alarm 발생
    auto alarm = alarm_manager_->raiseAlarm("ALM001", "test_source", "Emergency stop pressed");
    ASSERT_TRUE(alarm.has_value());
    EXPECT_EQ(alarm->severity, alarm::AlarmSeverity::CRITICAL);

    // 3. BehaviorArbiter가 alarm을 감지하도록 tick
    behavior_arbiter_->tick();

    // 4. FAULT 모드로 전환되었는지 확인
    EXPECT_EQ(behavior_arbiter_->getCurrentMode(), control::ControlMode::FAULT);

    // 5. 현재 task가 중단되었는지 확인
    EXPECT_EQ(behavior_arbiter_->getCurrentTaskId(), "");
    EXPECT_EQ(behavior_arbiter_->getPendingBehaviorCount(), 0);
}

// Warning Alarm 발생 시 현재 작업 완료 후 SAFE_MODE 전환
TEST_F(AlarmHandlingTest, WarningAlarmWaitForTaskCompletion) {
    // 1. Normal task 실행
    control::BehaviorRequest normal_request;
    normal_request.behavior_id = "task_002";
    normal_request.priority = control::Priority::NORMAL_TASK;
    normal_request.task = createDummyTask("task_002");
    normal_request.task->start();  // 실제로 시작

    // Behavior 요청
    ASSERT_TRUE(behavior_arbiter_->requestBehavior(normal_request));

    // Task 시작되도록 tick
    behavior_arbiter_->tick();

    // 현재 task가 실행 중인지 확인
    EXPECT_EQ(behavior_arbiter_->getCurrentTaskId(), "task_002");
    EXPECT_NE(behavior_arbiter_->getCurrentMode(), control::ControlMode::FAULT);

    // 2. Warning Alarm 발생
    auto alarm = alarm_manager_->raiseAlarm("ALM002", "test_source", "Motor overheating");
    ASSERT_TRUE(alarm.has_value());
    EXPECT_EQ(alarm->severity, alarm::AlarmSeverity::WARNING);

    // 3. BehaviorArbiter가 alarm을 감지하도록 tick
    behavior_arbiter_->tick();

    // 4. 아직 SAFE_MODE로 전환되지 않았는지 확인 (현재 task 진행 중)
    EXPECT_NE(behavior_arbiter_->getCurrentMode(), control::ControlMode::SAFE_MODE);
    EXPECT_EQ(behavior_arbiter_->getCurrentTaskId(), "task_002");

    // 5. Task 완료
    normal_request.task->setStatus(task::TaskStatus::COMPLETED);

    // 6. 다음 tick에서 SAFE_MODE로 전환되는지 확인
    behavior_arbiter_->tick();
    EXPECT_EQ(behavior_arbiter_->getCurrentMode(), control::ControlMode::SAFE_MODE);
    EXPECT_EQ(behavior_arbiter_->getCurrentTaskId(), "");
}

// Critical Alarm이 있을 때 새로운 task 요청 거부
TEST_F(AlarmHandlingTest, RejectNewTasksWithCriticalAlarm) {
    // 1. Critical Alarm 발생
    auto alarm = alarm_manager_->raiseAlarm("ALM001", "test_source", "Emergency stop");
    ASSERT_TRUE(alarm.has_value());

    // BehaviorArbiter가 alarm을 감지하도록 tick
    behavior_arbiter_->tick();

    // FAULT 모드 확인
    EXPECT_EQ(behavior_arbiter_->getCurrentMode(), control::ControlMode::FAULT);

    // 2. 새로운 task 요청 시도
    control::BehaviorRequest new_request;
    new_request.behavior_id = "task_003";
    new_request.priority = control::Priority::NORMAL_TASK;
    new_request.task = createDummyTask("task_003");

    // Behavior 요청
    ASSERT_TRUE(behavior_arbiter_->requestBehavior(new_request));

    // tick
    behavior_arbiter_->tick();

    // 3. Task가 시작되지 않았는지 확인
    EXPECT_EQ(behavior_arbiter_->getCurrentTaskId(), "");
}

// Alarm 심각도 상향 조정 테스트
TEST_F(AlarmHandlingTest, AlarmSeverityEscalation) {
    // 1. Warning Alarm 반복 발생
    for (int i = 0; i < 3; ++i) {
        auto alarm = alarm_manager_->raiseAlarm("ALM002", "test_source",
                                                std::string("Occurrence ") + std::to_string(i + 1));
        ASSERT_TRUE(alarm.has_value());

        if (i < 2) {
            // 처음 2번은 WARNING
            EXPECT_EQ(alarm->severity, alarm::AlarmSeverity::WARNING);
        } else {
            // 3번째는 CRITICAL로 상향
            EXPECT_EQ(alarm->severity, alarm::AlarmSeverity::CRITICAL);
        }

        // 짧은 대기 (실제 시간 윈도우 시뮬레이션)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Critical로 상향되었으므로 hasCriticalAlarm이 true여야 함
    EXPECT_TRUE(alarm_manager_->hasCriticalAlarm());
}

// DataStore 통합 테스트
TEST_F(AlarmHandlingTest, AlarmDataStoreIntegration) {
    // 1. Alarm 발생
    auto alarm = alarm_manager_->raiseAlarm("ALM001", "test_source", "Test alarm");
    ASSERT_TRUE(alarm.has_value());

    // 2. DataStore에서 alarm 정보 조회
    std::string key = "alarm/" + alarm->alarmId;
    auto data = data_store_->get(key);
    ASSERT_TRUE(data.has_value());

    // 3. 활성 alarm 카운트 확인
    auto count_data = data_store_->get("alarm/active_count");
    ASSERT_TRUE(count_data.has_value());

    try {
        int active_count = std::any_cast<int>(count_data.value());
        EXPECT_EQ(active_count, 1);
    } catch (const std::bad_any_cast& e) {
        FAIL() << "Failed to cast active_count: " << e.what();
    }

    // 4. Alarm 해제
    ASSERT_TRUE(alarm_manager_->resolveAlarm(alarm->alarmId));

    // 5. DataStore에서 alarm이 제거되었는지 확인
    auto removed_data = data_store_->get(key);
    EXPECT_FALSE(removed_data.has_value());

    // 6. 활성 카운트가 0인지 확인
    count_data = data_store_->get("alarm/active_count");
    ASSERT_TRUE(count_data.has_value());

    try {
        int active_count = std::any_cast<int>(count_data.value());
        EXPECT_EQ(active_count, 0);
    } catch (const std::bad_any_cast& e) {
        FAIL() << "Failed to cast active_count after resolve: " << e.what();
    }
}

// BehaviorArbiter의 ControlMode DataStore 통합 테스트
TEST_F(AlarmHandlingTest, ControlModeDataStoreIntegration) {
    // 1. 초기 모드 확인
    EXPECT_EQ(behavior_arbiter_->getCurrentMode(), control::ControlMode::STANDBY);

    // 2. MANUAL 모드로 전환
    ASSERT_TRUE(behavior_arbiter_->transitionTo(control::ControlMode::MANUAL));

    // 3. DataStore에서 현재 모드 확인
    auto mode_data = data_store_->get("control/current_mode");
    ASSERT_TRUE(mode_data.has_value());

    try {
        int stored_mode = std::any_cast<int>(mode_data.value());
        EXPECT_EQ(stored_mode, static_cast<int>(control::ControlMode::MANUAL));
    } catch (const std::bad_any_cast& e) {
        FAIL() << "Failed to cast current_mode: " << e.what();
    }

    // 4. 모드 전환 카운터 확인
    auto counter_data = data_store_->get("control/mode_transitions_count");
    ASSERT_TRUE(counter_data.has_value());

    try {
        int transitions = std::any_cast<int>(counter_data.value());
        EXPECT_GT(transitions, 0);
    } catch (const std::bad_any_cast& e) {
        FAIL() << "Failed to cast transitions count: " << e.what();
    }
}