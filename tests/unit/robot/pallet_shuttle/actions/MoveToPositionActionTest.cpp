// MoveToPositionActionTest.cpp - MoveToPositionAction 단위 테스트
// Copyright (C) 2025 MXRC Project
//
// Feature 016: Pallet Shuttle Control System (T057)

#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <thread>

#include "robot/pallet_shuttle/actions/MoveToPositionAction.h"
#include "robot/pallet_shuttle/interfaces/IPalletShuttleStateAccessor.h"
#include "core/fieldbus/interfaces/IFieldbusDriver.h"
#include "core/action/dto/ActionResult.h"

using namespace mxrc;
using namespace mxrc::robot::pallet_shuttle;

// Mock StateAccessor for testing
class MockStateAccessor : public IPalletShuttleStateAccessor {
public:
    // Position management
    mutable Position current_position_{0, 0, 0, 0};
    mutable Position target_position_{0, 0, 0, 0};
    mutable ShuttleState state_ = ShuttleState::IDLE;

    std::optional<Position> getCurrentPosition() const override {
        return current_position_;
    }

    std::optional<Position> getTargetPosition() const override {
        return target_position_;
    }

    bool updatePosition(const Position& position) override {
        current_position_ = position;
        return true;
    }

    bool setTargetPosition(const Position& position) override {
        target_position_ = position;
        return true;
    }

    ShuttleState getState() const override {
        return state_;
    }

    bool setState(ShuttleState state) override {
        state_ = state;
        return true;
    }

    // Stub implementations for other methods
    std::optional<PalletInfo> getLoadedPallet() const override { return std::nullopt; }
    bool updateLoadedPallet(const PalletInfo&) override { return true; }
    bool clearLoadedPallet() override { return true; }
    double getCurrentSpeed() const override { return 100.0; }
    double getBatteryLevel() const override { return 1.0; }
    double getTotalDistance() const override { return 0.0; }
    uint32_t getCompletedTaskCount() const override { return 0; }
    void incrementCompletedTaskCount() override {}
    std::chrono::system_clock::time_point getLastUpdateTime() const override {
        return std::chrono::system_clock::now();
    }
    std::optional<std::chrono::system_clock::time_point> getTaskStartTime() const override {
        return std::nullopt;
    }
    void setTaskStartTime(const std::chrono::system_clock::time_point&) override {}
    void clearTaskStartTime() override {}
};

// Mock FieldbusDriver for testing
class MockFieldbusDriver : public core::fieldbus::IFieldbusDriver {
public:
    bool connected_ = true;
    bool move_command_received_ = false;
    Position last_commanded_position_{0, 0, 0, 0};

    bool connect() override { return connected_; }
    bool disconnect() override { return true; }
    bool isConnected() const override { return connected_; }

    bool read(const std::string&, std::any&) override { return true; }

    bool write(const std::string& address, const std::any& value) override {
        if (address == "move_command") {
            move_command_received_ = true;
            // Extract position from value
            try {
                auto pos_map = std::any_cast<std::map<std::string, double>>(value);
                last_commanded_position_.x = pos_map["x"];
                last_commanded_position_.y = pos_map["y"];
                last_commanded_position_.z = pos_map["z"];
                last_commanded_position_.theta = pos_map.count("theta") ? pos_map["theta"] : 0.0;
            } catch (...) {
                return false;
            }
            return true;
        }
        return false;
    }

    std::vector<std::string> scan() override { return {}; }
    std::string getDriverInfo() const override { return "MockDriver"; }
    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
};

class MoveToPositionActionTest : public ::testing::Test {
protected:
    void SetUp() override {
        state_accessor_ = std::make_shared<MockStateAccessor>();
        fieldbus_driver_ = std::make_shared<MockFieldbusDriver>();
    }

    std::shared_ptr<MockStateAccessor> state_accessor_;
    std::shared_ptr<MockFieldbusDriver> fieldbus_driver_;
};

// 기본 이동 테스트
TEST_F(MoveToPositionActionTest, MoveToTargetPosition) {
    // Given: 목표 위치
    Position target{100.0, 200.0, 50.0, 0.0};

    // When: Action 생성 및 실행
    auto action = std::make_shared<MoveToPositionAction>(
        "move_001", target, state_accessor_, fieldbus_driver_);

    auto result = action->execute();

    // Then: 성공적으로 이동
    EXPECT_EQ(result.status, core::action::ActionStatus::SUCCESS);
    EXPECT_EQ(state_accessor_->getState(), ShuttleState::MOVING);
    EXPECT_TRUE(fieldbus_driver_->move_command_received_);
    EXPECT_DOUBLE_EQ(fieldbus_driver_->last_commanded_position_.x, target.x);
    EXPECT_DOUBLE_EQ(fieldbus_driver_->last_commanded_position_.y, target.y);
}

// 도착 확인 테스트
TEST_F(MoveToPositionActionTest, CheckArrival) {
    // Given: 목표 위치와 허용 오차
    Position target{100.0, 200.0, 0.0, 0.0};
    double tolerance = 5.0;  // 5mm 허용 오차

    auto action = std::make_shared<MoveToPositionAction>(
        "move_002", target, state_accessor_, fieldbus_driver_, tolerance);

    // 초기 실행
    auto result = action->execute();
    EXPECT_EQ(result.status, core::action::ActionStatus::SUCCESS);

    // When: 현재 위치를 목표 근처로 업데이트
    Position near_target{99.0, 201.0, 0.0, 0.0};  // 허용 오차 내
    state_accessor_->updatePosition(near_target);

    // Then: 도착 확인
    EXPECT_TRUE(action->hasArrived());

    // When: 허용 오차 밖
    Position far_position{90.0, 200.0, 0.0, 0.0};  // 10mm 떨어짐
    state_accessor_->updatePosition(far_position);

    // Then: 미도착
    EXPECT_FALSE(action->hasArrived());
}

// 진행률 계산 테스트
TEST_F(MoveToPositionActionTest, ProgressCalculation) {
    // Given: 시작 위치와 목표 위치
    Position start{0.0, 0.0, 0.0, 0.0};
    Position target{100.0, 0.0, 0.0, 0.0};

    state_accessor_->updatePosition(start);

    auto action = std::make_shared<MoveToPositionAction>(
        "move_003", target, state_accessor_, fieldbus_driver_);

    // 초기 진행률
    EXPECT_DOUBLE_EQ(action->getProgress(), 0.0);

    // When: 50% 지점으로 이동
    Position halfway{50.0, 0.0, 0.0, 0.0};
    state_accessor_->updatePosition(halfway);

    // Then: 50% 진행
    EXPECT_NEAR(action->getProgress(), 0.5, 0.01);

    // When: 목표 도달
    state_accessor_->updatePosition(target);

    // Then: 100% 진행
    EXPECT_DOUBLE_EQ(action->getProgress(), 1.0);
}

// 연결 실패 테스트
TEST_F(MoveToPositionActionTest, ConnectionFailure) {
    // Given: 연결 실패 상태
    fieldbus_driver_->connected_ = false;

    Position target{100.0, 200.0, 0.0, 0.0};
    auto action = std::make_shared<MoveToPositionAction>(
        "move_004", target, state_accessor_, fieldbus_driver_);

    // When: Action 실행
    auto result = action->execute();

    // Then: 실패 반환
    EXPECT_EQ(result.status, core::action::ActionStatus::FAILURE);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_EQ(state_accessor_->getState(), ShuttleState::ERROR);
}

// 취소 테스트
TEST_F(MoveToPositionActionTest, CancelMovement) {
    // Given: 이동 중인 Action
    Position target{100.0, 200.0, 0.0, 0.0};
    auto action = std::make_shared<MoveToPositionAction>(
        "move_005", target, state_accessor_, fieldbus_driver_);

    action->execute();
    EXPECT_EQ(state_accessor_->getState(), ShuttleState::MOVING);

    // When: 취소
    action->cancel();

    // Then: IDLE 상태로 전환
    EXPECT_EQ(state_accessor_->getState(), ShuttleState::IDLE);
}

// 타임아웃 테스트
TEST_F(MoveToPositionActionTest, Timeout) {
    // Given: 매우 짧은 타임아웃
    Position target{100.0, 200.0, 0.0, 0.0};
    std::chrono::milliseconds timeout(1);  // 1ms 타임아웃

    auto action = std::make_shared<MoveToPositionAction>(
        "move_006", target, state_accessor_, fieldbus_driver_, 5.0, timeout);

    // When: 실행하고 타임아웃까지 대기
    action->execute();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Then: 타임아웃 확인
    EXPECT_TRUE(action->isTimedOut());
}

// 장애물 감지 시뮬레이션
TEST_F(MoveToPositionActionTest, ObstacleDetection) {
    // Given: 이동 중 장애물 감지 (Error 상태 시뮬레이션)
    Position target{100.0, 200.0, 0.0, 0.0};
    auto action = std::make_shared<MoveToPositionAction>(
        "move_007", target, state_accessor_, fieldbus_driver_);

    action->execute();

    // When: 외부에서 ERROR 상태로 변경 (장애물 감지 시뮬레이션)
    state_accessor_->setState(ShuttleState::ERROR);

    // Then: Action이 오류 감지
    auto status = action->getStatus();
    EXPECT_EQ(status, core::action::ActionStatus::FAILURE);
}

// 경로 재계산 테스트
TEST_F(MoveToPositionActionTest, PathRecalculation) {
    // Given: 초기 목표
    Position initial_target{100.0, 100.0, 0.0, 0.0};
    auto action = std::make_shared<MoveToPositionAction>(
        "move_008", initial_target, state_accessor_, fieldbus_driver_);

    action->execute();

    // When: 새로운 목표로 업데이트
    Position new_target{200.0, 200.0, 0.0, 0.0};
    EXPECT_TRUE(action->updateTarget(new_target));

    // Then: 새 목표가 설정됨
    auto current_target = state_accessor_->getTargetPosition();
    ASSERT_TRUE(current_target.has_value());
    EXPECT_DOUBLE_EQ(current_target->x, new_target.x);
    EXPECT_DOUBLE_EQ(current_target->y, new_target.y);
}

// 정밀도 테스트
TEST_F(MoveToPositionActionTest, PrecisionMovement) {
    // Given: 매우 정밀한 이동 (0.1mm 허용 오차)
    Position target{100.123, 200.456, 50.789, 0.0};
    double precision_tolerance = 0.1;

    auto action = std::make_shared<MoveToPositionAction>(
        "move_009", target, state_accessor_, fieldbus_driver_, precision_tolerance);

    action->execute();

    // When: 정밀 위치 도달
    Position precise_position{100.15, 200.48, 50.75, 0.0};  // 허용 오차 내
    state_accessor_->updatePosition(precise_position);

    // Then: 도착 확인
    EXPECT_TRUE(action->hasArrived());
}