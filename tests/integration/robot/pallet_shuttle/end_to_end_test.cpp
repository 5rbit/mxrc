// end_to_end_test.cpp - End-to-End 통합 테스트
// Copyright (C) 2025 MXRC Project
//
// Feature 016: Pallet Shuttle Control System (T093)
// Phase 9: Polish & Cross-Cutting Concerns

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>

// Core components
#include "core/control/impl/BehaviorArbiter.h"
#include "core/control/impl/TaskQueue.h"
#include "core/alarm/impl/AlarmManager.h"
#include "core/datastore/DataStore.h"
#include "core/event/core/EventBus.h"
#include "core/fieldbus/drivers/MockDriver.h"

// Pallet shuttle components
#include "robot/pallet_shuttle/state/PalletShuttleState.h"
#include "robot/pallet_shuttle/sequences/SafetyCheckSequence.h"
#include "robot/pallet_shuttle/control/PalletShuttleController.h"

using namespace mxrc;
using namespace mxrc::core;
using namespace mxrc::robot::pallet_shuttle;

class EndToEndTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize core infrastructure
        data_store_ = std::make_shared<datastore::DataStore>();
        event_bus_ = std::make_shared<event::EventBus>();

        // Initialize alarm system
        auto alarm_config = std::make_shared<alarm::AlarmConfiguration>("config/alarm-config.yaml");
        alarm_manager_ = std::make_shared<alarm::AlarmManager>(alarm_config, data_store_, event_bus_);

        // Initialize control system
        task_queue_ = std::make_shared<control::TaskQueue>();
        behavior_arbiter_ = std::make_shared<control::BehaviorArbiter>(
            task_queue_, alarm_manager_, data_store_);

        // Initialize fieldbus
        fieldbus_driver_ = std::make_shared<fieldbus::MockDriver>();
        fieldbus_driver_->connect();

        // Initialize pallet shuttle
        shuttle_state_ = std::make_shared<PalletShuttleState>(data_store_, event_bus_);
        controller_ = std::make_shared<PalletShuttleController>(
            shuttle_state_, behavior_arbiter_, alarm_manager_, fieldbus_driver_);

        // Start behavior arbiter
        behavior_arbiter_->start();
    }

    void TearDown() override {
        behavior_arbiter_->stop();
    }

    std::shared_ptr<datastore::DataStore> data_store_;
    std::shared_ptr<event::EventBus> event_bus_;
    std::shared_ptr<alarm::AlarmManager> alarm_manager_;
    std::shared_ptr<control::TaskQueue> task_queue_;
    std::shared_ptr<control::BehaviorArbiter> behavior_arbiter_;
    std::shared_ptr<fieldbus::MockDriver> fieldbus_driver_;
    std::shared_ptr<PalletShuttleState> shuttle_state_;
    std::shared_ptr<PalletShuttleController> controller_;
};

// T093: Complete workflow test - User Story 1 + 3
TEST_F(EndToEndTest, CompleteTransportWorkflow) {
    // Scenario: Multiple pallet transport tasks with priority handling

    // Given: System in AUTO mode
    behavior_arbiter_->transitionTo(control::ControlMode::AUTO);
    EXPECT_EQ(behavior_arbiter_->getCurrentMode(), control::ControlMode::AUTO);

    // When: Submit multiple transport tasks with different priorities
    controller_->submitTransportTask("TASK001", "PLT001",
        Position{0, 0, 0, 0}, Position{100, 200, 0, 0},
        control::Priority::NORMAL);

    controller_->submitTransportTask("TASK002", "PLT002",
        Position{100, 200, 0, 0}, Position{300, 400, 0, 0},
        control::Priority::HIGH);

    controller_->submitTransportTask("TASK003", "PLT003",
        Position{300, 400, 0, 0}, Position{500, 600, 0, 0},
        control::Priority::NORMAL);

    // Then: High priority task executes first
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto next_task = task_queue_->peekNextRequest();
    ASSERT_TRUE(next_task.has_value());
    EXPECT_EQ(next_task->priority, control::Priority::HIGH);

    // Verify state updates in DataStore
    auto current_state = data_store_->get<int>("pallet_shuttle/state");
    EXPECT_NE(current_state, static_cast<int>(ShuttleState::ERROR));
}

// User Story 2: Alarm handling workflow
TEST_F(EndToEndTest, AlarmHandlingWorkflow) {
    // Scenario: Warning alarm during operation

    // Given: System executing a task
    behavior_arbiter_->transitionTo(control::ControlMode::AUTO);
    controller_->submitTransportTask("TASK_ALARM", "PLT_TEST",
        Position{0, 0, 0, 0}, Position{100, 100, 0, 0},
        control::Priority::NORMAL);

    // When: Battery becomes low (warning condition)
    shuttle_state_->setBatteryLevel(0.15); // 15%
    alarm_manager_->raiseAlarm("W001", "Battery", "Low battery warning");

    // Then: System should transition to MAINT mode after task completion
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Verify warning alarm is active
    auto active_alarms = alarm_manager_->getActiveAlarmsBySeverity(alarm::AlarmSeverity::WARNING);
    EXPECT_FALSE(active_alarms.empty());

    // Verify appropriate mode transition
    EXPECT_NE(behavior_arbiter_->getCurrentMode(), control::ControlMode::FAULT);
}

// User Story 4: Real-time monitoring
TEST_F(EndToEndTest, RealtimeStateMonitoring) {
    // Scenario: Monitor state changes during operation

    // Given: Event listener for state changes
    bool state_changed = false;
    Position last_position;

    auto listener = event_bus_->subscribe(event::EventType::POSITION_UPDATED,
        [&state_changed, &last_position](const std::shared_ptr<event::EventBase>& event) {
            state_changed = true;
            // Extract position from event
        });

    // When: Update position
    Position new_pos{150, 250, 0, 45};
    shuttle_state_->updatePosition(new_pos);

    // Then: Event should be published
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(state_changed);

    // Verify DataStore consistency
    auto stored_x = data_store_->get<double>("pallet_shuttle/position/current/x");
    EXPECT_DOUBLE_EQ(stored_x, 150);
}

// User Story 6: Periodic safety checks
TEST_F(EndToEndTest, PeriodicSafetyChecks) {
    // Scenario: Execute safety check sequence

    // Given: Safety check sequence
    auto safety_sequence = std::make_shared<SafetyCheckSequence>(
        "SAFETY001", shuttle_state_, alarm_manager_, fieldbus_driver_);

    // Set up maintenance due condition
    shuttle_state_->addDistance(51000); // 51km

    // When: Execute safety check
    auto result = safety_sequence->execute();

    // Then: Check completes with maintenance warning
    EXPECT_EQ(result.status, sequence::SequenceStatus::COMPLETED);

    auto check_results = safety_sequence->getCheckResults();
    EXPECT_FALSE(check_results.empty());

    // Verify maintenance alarm raised
    auto info_alarms = alarm_manager_->getActiveAlarmsBySeverity(alarm::AlarmSeverity::INFO);
    bool maintenance_alarm_found = false;
    for (const auto& alarm : info_alarms) {
        if (alarm.alarm_code == "I001") {
            maintenance_alarm_found = true;
            break;
        }
    }
    EXPECT_TRUE(maintenance_alarm_found);
}

// Critical failure handling
TEST_F(EndToEndTest, CriticalFailureHandling) {
    // Scenario: Emergency stop activated

    // Given: System in AUTO mode with active task
    behavior_arbiter_->transitionTo(control::ControlMode::AUTO);

    // When: Emergency stop activated
    fieldbus_driver_->setEmergencyStop(true);
    alarm_manager_->raiseAlarm("E001", "Safety", "Emergency stop activated");

    // Then: System should immediately transition to FAULT
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(behavior_arbiter_->getCurrentMode(), control::ControlMode::FAULT);

    // Verify all tasks cancelled
    EXPECT_EQ(task_queue_->size(), 0);
}

// Performance test: Response time
TEST_F(EndToEndTest, CriticalAlarmResponseTime) {
    // T092: Critical alarm response < 100ms

    auto start = std::chrono::steady_clock::now();

    // Raise critical alarm
    alarm_manager_->raiseAlarm("E002", "System", "Critical system failure");

    // Wait for mode transition
    while (behavior_arbiter_->getCurrentMode() != control::ControlMode::FAULT) {
        if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(100)) {
            FAIL() << "Critical alarm response time exceeded 100ms";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    EXPECT_LT(response_time.count(), 100) << "Response time: " << response_time.count() << "ms";
}

// Performance test: DataStore write
TEST_F(EndToEndTest, DataStoreWritePerformance) {
    // T092: DataStore write < 50ms

    const int num_writes = 100;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_writes; ++i) {
        data_store_->set(fmt::format("test/key_{}", i), i, datastore::DataType::RobotState);
    }

    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    double avg_time = total_time.count() / static_cast<double>(num_writes);
    EXPECT_LT(avg_time, 50) << "Average write time: " << avg_time << "ms";
}

// Multi-threaded stress test
TEST_F(EndToEndTest, ConcurrentOperations) {
    // Test concurrent task submissions and state updates

    std::vector<std::thread> threads;
    std::atomic<int> completed_count{0};

    // Thread 1: Submit tasks
    threads.emplace_back([this, &completed_count]() {
        for (int i = 0; i < 10; ++i) {
            controller_->submitTransportTask(
                fmt::format("TASK_T1_{}", i),
                fmt::format("PLT_T1_{}", i),
                Position{0, 0, 0, 0},
                Position{100, 100, 0, 0},
                control::Priority::NORMAL);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        completed_count++;
    });

    // Thread 2: Update states
    threads.emplace_back([this, &completed_count]() {
        for (int i = 0; i < 20; ++i) {
            Position pos{i * 10.0, i * 20.0, 0, 0};
            shuttle_state_->updatePosition(pos);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        completed_count++;
    });

    // Thread 3: Safety checks
    threads.emplace_back([this, &completed_count]() {
        for (int i = 0; i < 3; ++i) {
            auto safety_seq = std::make_shared<SafetyCheckSequence>(
                fmt::format("SAFETY_T3_{}", i),
                shuttle_state_, alarm_manager_, fieldbus_driver_);
            safety_seq->execute();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        completed_count++;
    });

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed_count, 3);

    // Verify system stability
    EXPECT_NE(behavior_arbiter_->getCurrentMode(), control::ControlMode::FAULT);
}