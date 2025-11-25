// PalletShuttleStateTest.cpp - PalletShuttleState 상태 조회 테스트
// Copyright (C) 2025 MXRC Project
//
// Feature 016: Pallet Shuttle Control System (T079)
// Phase 7: Status monitoring

#include <gtest/gtest.h>
#include <memory>
#include "robot/pallet_shuttle/state/PalletShuttleState.h"
#include "robot/pallet_shuttle/interfaces/IPalletShuttleStateAccessor.h"
#include "core/datastore/DataStore.h"
#include "core/event/core/EventBus.h"

using namespace mxrc::robot::pallet_shuttle;
using namespace mxrc::core::datastore;
using namespace mxrc::core::event;

class PalletShuttleStateTest : public ::testing::Test {
protected:
    void SetUp() override {
        data_store_ = std::make_shared<DataStore>();
        event_bus_ = std::make_shared<EventBus>();
        state_ = std::make_shared<PalletShuttleState>(data_store_, event_bus_);
    }

    std::shared_ptr<DataStore> data_store_;
    std::shared_ptr<EventBus> event_bus_;
    std::shared_ptr<PalletShuttleState> state_;
};

// T079: 상태 조회 테스트
TEST_F(PalletShuttleStateTest, StateQuery) {
    // Given: 초기 상태 설정
    Position initial_pos{100, 200, 50, 0};
    state_->updatePosition(initial_pos);
    state_->setState(ShuttleState::MOVING);
    state_->setBatteryLevel(0.85);
    state_->setCurrentSpeed(150.0);

    // When: 상태 조회
    auto current_pos = state_->getCurrentPosition();
    auto current_state = state_->getState();
    auto battery = state_->getBatteryLevel();
    auto speed = state_->getCurrentSpeed();

    // Then: 정확한 상태 반환
    ASSERT_TRUE(current_pos.has_value());
    EXPECT_DOUBLE_EQ(current_pos->x, 100);
    EXPECT_DOUBLE_EQ(current_pos->y, 200);
    EXPECT_DOUBLE_EQ(current_pos->z, 50);

    EXPECT_EQ(current_state, ShuttleState::MOVING);
    EXPECT_DOUBLE_EQ(battery, 0.85);
    EXPECT_DOUBLE_EQ(speed, 150.0);
}

// 팔렛 상태 관리 테스트
TEST_F(PalletShuttleStateTest, PalletStateManagement) {
    // Given: 팔렛 정보
    PalletInfo pallet{"PLT-001", 45.5, true};

    // When: 팔렛 적재
    bool loaded = state_->updateLoadedPallet(pallet);

    // Then: 팔렛 정보 조회 가능
    EXPECT_TRUE(loaded);
    auto loaded_pallet = state_->getLoadedPallet();
    ASSERT_TRUE(loaded_pallet.has_value());
    EXPECT_EQ(loaded_pallet->pallet_id, "PLT-001");
    EXPECT_DOUBLE_EQ(loaded_pallet->weight, 45.5);

    // When: 팔렛 하역
    bool cleared = state_->clearLoadedPallet();

    // Then: 팔렛 정보 없음
    EXPECT_TRUE(cleared);
    EXPECT_FALSE(state_->getLoadedPallet().has_value());
}

// 메트릭 추적 테스트
TEST_F(PalletShuttleStateTest, MetricsTracking) {
    // Given: 초기 메트릭
    double initial_distance = state_->getTotalDistance();
    uint32_t initial_count = state_->getCompletedTaskCount();

    // When: 이동 및 작업 완료
    state_->addDistance(150.5);
    state_->addDistance(200.3);
    state_->incrementCompletedTaskCount();
    state_->incrementCompletedTaskCount();

    // Then: 메트릭 업데이트
    EXPECT_DOUBLE_EQ(state_->getTotalDistance() - initial_distance, 350.8);
    EXPECT_EQ(state_->getCompletedTaskCount() - initial_count, 2);
}

// 상태 전환 유효성 테스트
TEST_F(PalletShuttleStateTest, StateTransitionValidation) {
    // Given: IDLE 상태
    state_->setState(ShuttleState::IDLE);

    // When: 유효한 전환
    bool valid_transition = state_->canTransitionTo(ShuttleState::MOVING);

    // Then: 전환 가능
    EXPECT_TRUE(valid_transition);

    // When: MOVING 상태에서 ERROR로 전환 (항상 가능)
    state_->setState(ShuttleState::MOVING);
    bool error_transition = state_->canTransitionTo(ShuttleState::ERROR);

    // Then: ERROR는 항상 전환 가능
    EXPECT_TRUE(error_transition);
}

// DataStore 동기화 테스트
TEST_F(PalletShuttleStateTest, DataStoreSync) {
    // Given: 상태 업데이트
    Position pos{300, 400, 100, 45};
    state_->updatePosition(pos);
    state_->setState(ShuttleState::PICKING);

    // When: DataStore에서 직접 읽기
    auto stored_x = data_store_->get<double>("pallet_shuttle/position/current/x");
    auto stored_y = data_store_->get<double>("pallet_shuttle/position/current/y");
    auto stored_state = data_store_->get<int>("pallet_shuttle/state");

    // Then: 동기화 확인
    EXPECT_DOUBLE_EQ(stored_x, 300);
    EXPECT_DOUBLE_EQ(stored_y, 400);
    EXPECT_EQ(stored_state, static_cast<int>(ShuttleState::PICKING));
}

// 작업 시간 추적 테스트
TEST_F(PalletShuttleStateTest, TaskTimeTracking) {
    // Given: 작업 시작
    auto start_time = std::chrono::system_clock::now();
    state_->setTaskStartTime(start_time);

    // When: 작업 시간 조회
    auto task_start = state_->getTaskStartTime();

    // Then: 시작 시간 기록됨
    ASSERT_TRUE(task_start.has_value());
    EXPECT_EQ(task_start.value(), start_time);

    // When: 작업 완료
    state_->clearTaskStartTime();

    // Then: 시작 시간 삭제됨
    EXPECT_FALSE(state_->getTaskStartTime().has_value());
}

// 알람 상태 통합 테스트
TEST_F(PalletShuttleStateTest, AlarmStateIntegration) {
    // Given: 정상 상태
    state_->setState(ShuttleState::IDLE);

    // When: 알람 발생 시뮬레이션 (배터리 부족)
    state_->setBatteryLevel(0.05); // 5%

    // Then: 저전력 경고 확인 가능
    bool low_battery = state_->isLowBattery();
    EXPECT_TRUE(low_battery);

    // When: ERROR 상태 전환
    state_->setState(ShuttleState::ERROR);
    state_->setErrorMessage("Battery critically low");

    // Then: 에러 메시지 조회
    auto error_msg = state_->getErrorMessage();
    ASSERT_TRUE(error_msg.has_value());
    EXPECT_EQ(error_msg.value(), "Battery critically low");
}