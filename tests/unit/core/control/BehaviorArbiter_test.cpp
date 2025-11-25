#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include "core/control/impl/BehaviorArbiter.h"
#include "core/alarm/impl/AlarmManager.h"
#include "core/alarm/impl/AlarmConfiguration.h"

using namespace mxrc::core::control;
using namespace mxrc::core::alarm;
using namespace std::chrono_literals;

/**
 * @brief BehaviorArbiter 단위 테스트
 *
 * Feature 016: Pallet Shuttle Control System
 * Phase 4: User Story 5 - 행동 의사 결정 및 모드 전환
 *
 * 테스트 범위:
 * - T045: 우선순위 선택 (Priority 기반 behavior 선택)
 * - T047: ControlMode 상태 전환 검증
 * - Tick 메커니즘 (100ms 주기)
 * - 선점(Preemption) 처리
 * - Critical Alarm 시 FAULT 모드 전환
 */

// Mock Task for testing
class MockTask : public mxrc::core::task::ITask {
public:
    explicit MockTask(std::string id) : id_(std::move(id)), status_(mxrc::core::task::TaskStatus::IDLE) {}

    std::string getId() const override { return id_; }

    std::string start() override {
        status_ = mxrc::core::task::TaskStatus::RUNNING;
        return id_;
    }

    void stop() override {
        status_ = mxrc::core::task::TaskStatus::CANCELLED;
    }

    void pause() override {
        if (status_ == mxrc::core::task::TaskStatus::RUNNING) {
            status_ = mxrc::core::task::TaskStatus::PAUSED;
        }
    }

    void resume() override {
        if (status_ == mxrc::core::task::TaskStatus::PAUSED) {
            status_ = mxrc::core::task::TaskStatus::RUNNING;
        }
    }

    mxrc::core::task::TaskStatus getStatus() const override { return status_; }

    float getProgress() const override { return 0.5f; }

    const mxrc::core::task::TaskDefinition& getDefinition() const override {
        static mxrc::core::task::TaskDefinition def("mock_task", "Mock Task");
        return def;
    }

private:
    std::string id_;
    mxrc::core::task::TaskStatus status_;
};

class BehaviorArbiterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // AlarmManager 생성 (BehaviorArbiter 의존성)
        auto config = std::make_shared<AlarmConfiguration>();

        AlarmConfig critical_alarm;
        critical_alarm.code = "E001";
        critical_alarm.name = "Emergency Stop";
        critical_alarm.severity = AlarmSeverity::CRITICAL;
        critical_alarm.recurrence_window = std::chrono::seconds(60);
        critical_alarm.recurrence_threshold = 1;
        critical_alarm.auto_reset = false;

        config->addConfig(critical_alarm);

        alarm_manager_ = std::make_shared<AlarmManager>(config);
        arbiter_ = std::make_unique<BehaviorArbiter>(alarm_manager_);
    }

    std::shared_ptr<AlarmManager> alarm_manager_;
    std::unique_ptr<BehaviorArbiter> arbiter_;
};

// T045-1: 우선순위 선택 - EMERGENCY_STOP이 가장 높은 우선순위
TEST_F(BehaviorArbiterTest, PrioritySelection_EmergencyStopHighest) {
    // NORMAL_TASK 요청
    auto normal_task = std::make_shared<MockTask>("normal_task");
    BehaviorRequest normal_req("normal_behavior", Priority::NORMAL_TASK, normal_task, "test");

    // EMERGENCY_STOP 요청
    auto emergency_task = std::make_shared<MockTask>("emergency_task");
    BehaviorRequest emergency_req("emergency_behavior", Priority::EMERGENCY_STOP, emergency_task, "test");

    // 낮은 우선순위 먼저 요청
    EXPECT_TRUE(arbiter_->requestBehavior(normal_req));
    std::this_thread::sleep_for(2ms);
    EXPECT_TRUE(arbiter_->requestBehavior(emergency_req));

    // tick() 호출 시 EMERGENCY_STOP이 선택되어야 함
    arbiter_->tick();

    // Emergency task가 시작되었는지 확인
    EXPECT_EQ(emergency_task->getStatus(), mxrc::core::task::TaskStatus::RUNNING);

    // EMERGENCY_STOP priority behavior is executed but doesn't auto-transition to FAULT
    // (FAULT transition only happens when critical alarm is raised)
    EXPECT_EQ(arbiter_->getCurrentMode(), ControlMode::STANDBY);
}

// T045-2: 우선순위 선택 - URGENT_TASK > NORMAL_TASK
TEST_F(BehaviorArbiterTest, PrioritySelection_UrgentOverNormal) {
    auto normal_task = std::make_shared<MockTask>("normal_task");
    BehaviorRequest normal_req("normal", Priority::NORMAL_TASK, normal_task, "test");

    auto urgent_task = std::make_shared<MockTask>("urgent_task");
    BehaviorRequest urgent_req("urgent", Priority::URGENT_TASK, urgent_task, "test");

    EXPECT_TRUE(arbiter_->requestBehavior(normal_req));
    std::this_thread::sleep_for(2ms);
    EXPECT_TRUE(arbiter_->requestBehavior(urgent_req));

    arbiter_->tick();

    // URGENT_TASK가 먼저 실행되어야 함
    EXPECT_EQ(urgent_task->getStatus(), mxrc::core::task::TaskStatus::RUNNING);
}

// T047-1: ControlMode 전환 - STANDBY → AUTO
TEST_F(BehaviorArbiterTest, ModeTransition_StandbyToAuto) {
    // 초기 상태는 STANDBY
    EXPECT_EQ(arbiter_->getCurrentMode(), ControlMode::STANDBY);

    // AUTO 모드로 전환
    bool result = arbiter_->transitionTo(ControlMode::AUTO);

    EXPECT_TRUE(result);
    EXPECT_EQ(arbiter_->getCurrentMode(), ControlMode::AUTO);
}

// T047-2: ControlMode 전환 - 잘못된 전환 거부
TEST_F(BehaviorArbiterTest, ModeTransition_InvalidTransitionRejected) {
    // STANDBY → INIT는 잘못된 전환
    bool result = arbiter_->transitionTo(ControlMode::INIT);

    EXPECT_FALSE(result);
    EXPECT_EQ(arbiter_->getCurrentMode(), ControlMode::STANDBY);
}

// T047-3: ControlMode 전환 - FAULT는 항상 가능
TEST_F(BehaviorArbiterTest, ModeTransition_FaultAlwaysAllowed) {
    // AUTO 모드로 전환
    arbiter_->transitionTo(ControlMode::AUTO);

    // 어느 상태에서든 FAULT로 전환 가능
    bool result = arbiter_->transitionTo(ControlMode::FAULT);

    EXPECT_TRUE(result);
    EXPECT_EQ(arbiter_->getCurrentMode(), ControlMode::FAULT);
}

// T045-3: Critical Alarm 발생 시 자동 FAULT 전환
TEST_F(BehaviorArbiterTest, CriticalAlarm_AutoFaultTransition) {
    // AUTO 모드로 전환
    arbiter_->transitionTo(ControlMode::AUTO);
    EXPECT_EQ(arbiter_->getCurrentMode(), ControlMode::AUTO);

    // Critical Alarm 발생
    alarm_manager_->raiseAlarm("E001", "TestComponent");

    // tick() 호출 시 FAULT로 자동 전환
    arbiter_->tick();

    EXPECT_EQ(arbiter_->getCurrentMode(), ControlMode::FAULT);
}

// T045-4: Pause/Resume 기능
TEST_F(BehaviorArbiterTest, PauseResume) {
    auto task = std::make_shared<MockTask>("test_task");
    BehaviorRequest req("test", Priority::NORMAL_TASK, task, "test");

    arbiter_->requestBehavior(req);
    arbiter_->tick();

    EXPECT_EQ(task->getStatus(), mxrc::core::task::TaskStatus::RUNNING);

    // Pause
    bool paused = arbiter_->pause();
    EXPECT_TRUE(paused);
    EXPECT_EQ(task->getStatus(), mxrc::core::task::TaskStatus::PAUSED);

    // Resume
    bool resumed = arbiter_->resume();
    EXPECT_TRUE(resumed);
    EXPECT_EQ(task->getStatus(), mxrc::core::task::TaskStatus::RUNNING);
}

// T045-5: Pending behavior count 확인
TEST_F(BehaviorArbiterTest, GetPendingBehaviorCount) {
    EXPECT_EQ(arbiter_->getPendingBehaviorCount(), 0);

    auto task1 = std::make_shared<MockTask>("task1");
    BehaviorRequest req1("behavior1", Priority::NORMAL_TASK, task1, "test");

    auto task2 = std::make_shared<MockTask>("task2");
    BehaviorRequest req2("behavior2", Priority::URGENT_TASK, task2, "test");

    arbiter_->requestBehavior(req1);
    std::this_thread::sleep_for(2ms);
    arbiter_->requestBehavior(req2);

    EXPECT_EQ(arbiter_->getPendingBehaviorCount(), 2);

    // tick() 후 하나 감소
    arbiter_->tick();
    EXPECT_EQ(arbiter_->getPendingBehaviorCount(), 1);
}

// T045-6: Clear pending behaviors
TEST_F(BehaviorArbiterTest, ClearPendingBehaviors) {
    auto task1 = std::make_shared<MockTask>("task1");
    BehaviorRequest req1("behavior1", Priority::NORMAL_TASK, task1, "test");

    auto task2 = std::make_shared<MockTask>("task2");
    BehaviorRequest req2("behavior2", Priority::URGENT_TASK, task2, "test");

    arbiter_->requestBehavior(req1);
    std::this_thread::sleep_for(2ms);
    arbiter_->requestBehavior(req2);

    EXPECT_EQ(arbiter_->getPendingBehaviorCount(), 2);

    arbiter_->clearPendingBehaviors();

    EXPECT_EQ(arbiter_->getPendingBehaviorCount(), 0);
}

// T045-7: Cancel behavior
TEST_F(BehaviorArbiterTest, CancelBehavior) {
    auto task = std::make_shared<MockTask>("test_task");
    BehaviorRequest req("test_behavior", Priority::NORMAL_TASK, task, "test");
    req.cancellable = true;

    arbiter_->requestBehavior(req);
    arbiter_->tick();

    EXPECT_EQ(task->getStatus(), mxrc::core::task::TaskStatus::RUNNING);

    // Cancel
    bool cancelled = arbiter_->cancelBehavior("test_behavior");
    EXPECT_TRUE(cancelled);
    EXPECT_EQ(task->getStatus(), mxrc::core::task::TaskStatus::CANCELLED);
}

// T045-8: Non-cancellable behavior
TEST_F(BehaviorArbiterTest, NonCancellableBehavior) {
    auto task = std::make_shared<MockTask>("test_task");
    BehaviorRequest req("test_behavior", Priority::NORMAL_TASK, task, "test");
    req.cancellable = false;  // Not cancellable

    arbiter_->requestBehavior(req);
    arbiter_->tick();

    // Cancel 시도
    bool cancelled = arbiter_->cancelBehavior("test_behavior");
    EXPECT_FALSE(cancelled);
    EXPECT_EQ(task->getStatus(), mxrc::core::task::TaskStatus::RUNNING);
}
