// PlacePalletActionTest.cpp - PlacePalletAction 단위 테스트
// Copyright (C) 2025 MXRC Project
//
// Feature 016: Pallet Shuttle Control System (T059)

#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <thread>

#include "robot/pallet_shuttle/actions/PlacePalletAction.h"
#include "robot/pallet_shuttle/interfaces/IPalletShuttleStateAccessor.h"
#include "core/fieldbus/interfaces/IFieldbusDriver.h"
#include "core/action/dto/ActionResult.h"

using namespace mxrc;
using namespace mxrc::robot::pallet_shuttle;

// Mock StateAccessor for testing
class MockStateAccessor : public IPalletShuttleStateAccessor {
public:
    mutable Position current_position_{0, 0, 0, 0};
    mutable ShuttleState state_ = ShuttleState::IDLE;
    mutable std::optional<PalletInfo> loaded_pallet_;
    mutable uint32_t completed_tasks_ = 0;

    std::optional<Position> getCurrentPosition() const override {
        return current_position_;
    }

    std::optional<Position> getTargetPosition() const override {
        return std::nullopt;
    }

    bool updatePosition(const Position& position) override {
        current_position_ = position;
        return true;
    }

    bool setTargetPosition(const Position&) override {
        return true;
    }

    ShuttleState getState() const override {
        return state_;
    }

    bool setState(ShuttleState state) override {
        state_ = state;
        return true;
    }

    std::optional<PalletInfo> getLoadedPallet() const override {
        return loaded_pallet_;
    }

    bool updateLoadedPallet(const PalletInfo& pallet) override {
        loaded_pallet_ = pallet;
        return true;
    }

    bool clearLoadedPallet() override {
        loaded_pallet_.reset();
        return true;
    }

    double getCurrentSpeed() const override { return 0.0; }
    double getBatteryLevel() const override { return 1.0; }
    double getTotalDistance() const override { return 0.0; }

    uint32_t getCompletedTaskCount() const override {
        return completed_tasks_;
    }

    void incrementCompletedTaskCount() override {
        completed_tasks_++;
    }

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
    bool lower_command_received_ = false;
    bool release_command_received_ = false;
    bool place_surface_clear_ = true;
    double surface_height_ = 0.0;  // mm
    std::string last_command_;

    bool connect() override { return connected_; }
    bool disconnect() override { return true; }
    bool isConnected() const override { return connected_; }

    bool read(const std::string& address, std::any& value) override {
        if (address == "sensor/place_surface_clear") {
            value = place_surface_clear_;
            return true;
        } else if (address == "sensor/surface_height") {
            value = surface_height_;
            return true;
        } else if (address == "sensor/gripper_open") {
            value = true;  // 그리퍼가 열렸음
            return true;
        }
        return false;
    }

    bool write(const std::string& address, const std::any& value) override {
        last_command_ = address;

        if (address == "actuator/lower") {
            lower_command_received_ = true;
            return true;
        } else if (address == "actuator/release") {
            release_command_received_ = true;
            return true;
        }
        return false;
    }

    std::vector<std::string> scan() override { return {}; }
    std::string getDriverInfo() const override { return "MockDriver"; }
    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
};

class PlacePalletActionTest : public ::testing::Test {
protected:
    void SetUp() override {
        state_accessor_ = std::make_shared<MockStateAccessor>();
        fieldbus_driver_ = std::make_shared<MockFieldbusDriver>();

        // 기본적으로 팔렛을 적재한 상태로 설정
        PalletInfo loaded{"PLT-TEST", 50.0, true};
        state_accessor_->updateLoadedPallet(loaded);
    }

    std::shared_ptr<MockStateAccessor> state_accessor_;
    std::shared_ptr<MockFieldbusDriver> fieldbus_driver_;
};

// 기본 팔렛 배치 테스트
TEST_F(PlacePalletActionTest, SuccessfulPlacement) {
    // Given: 배치 위치에 도착한 상태
    Position place_position{300.0, 400.0, 0.0, 0.0};
    state_accessor_->updatePosition(place_position);

    // When: PlacePalletAction 실행
    auto action = std::make_shared<PlacePalletAction>(
        "place_001", place_position, state_accessor_, fieldbus_driver_);

    auto result = action->execute();

    // Then: 성공적으로 배치
    EXPECT_EQ(result.status, core::action::ActionStatus::SUCCESS);
    EXPECT_EQ(state_accessor_->getState(), ShuttleState::PLACING);
    EXPECT_TRUE(fieldbus_driver_->lower_command_received_);
    EXPECT_TRUE(fieldbus_driver_->release_command_received_);

    // 팔렛이 하역되었는지 확인
    EXPECT_FALSE(state_accessor_->getLoadedPallet().has_value());

    // 완료 카운트 증가 확인
    EXPECT_EQ(state_accessor_->getCompletedTaskCount(), 1);
}

// 팔렛 미적재 상태 테스트
TEST_F(PlacePalletActionTest, NoPalletLoaded) {
    // Given: 팔렛이 적재되지 않은 상태
    state_accessor_->clearLoadedPallet();

    Position place_position{300.0, 400.0, 0.0, 0.0};
    state_accessor_->updatePosition(place_position);

    // When: 배치 시도
    auto action = std::make_shared<PlacePalletAction>(
        "place_002", place_position, state_accessor_, fieldbus_driver_);

    auto result = action->execute();

    // Then: 실패 (팔렛 없음)
    EXPECT_EQ(result.status, core::action::ActionStatus::FAILURE);
    EXPECT_TRUE(result.error_message.find("no pallet loaded") != std::string::npos);
}

// 배치 공간 미확보 테스트
TEST_F(PlacePalletActionTest, PlaceSurfaceNotClear) {
    // Given: 배치 위치에 장애물 존재
    fieldbus_driver_->place_surface_clear_ = false;

    Position place_position{300.0, 400.0, 0.0, 0.0};
    state_accessor_->updatePosition(place_position);

    // When: 배치 시도
    auto action = std::make_shared<PlacePalletAction>(
        "place_003", place_position, state_accessor_, fieldbus_driver_);

    auto result = action->execute();

    // Then: 실패 (공간 미확보)
    EXPECT_EQ(result.status, core::action::ActionStatus::FAILURE);
    EXPECT_TRUE(result.error_message.find("surface not clear") != std::string::npos);
}

// 위치 불일치 테스트
TEST_F(PlacePalletActionTest, PositionMismatch) {
    // Given: 현재 위치와 배치 위치가 다름
    Position current_position{0.0, 0.0, 0.0, 0.0};
    Position place_position{300.0, 400.0, 0.0, 0.0};

    state_accessor_->updatePosition(current_position);

    // When: 배치 시도
    auto action = std::make_shared<PlacePalletAction>(
        "place_004", place_position, state_accessor_, fieldbus_driver_);

    auto result = action->execute();

    // Then: 실패 (위치 불일치)
    EXPECT_EQ(result.status, core::action::ActionStatus::FAILURE);
    EXPECT_TRUE(result.error_message.find("not at place position") != std::string::npos);
}

// 높이 조정 테스트
TEST_F(PlacePalletActionTest, HeightAdjustment) {
    // Given: 배치 표면 높이가 있음
    fieldbus_driver_->surface_height_ = 100.0;  // 100mm 높이

    Position place_position{300.0, 400.0, 0.0, 0.0};
    state_accessor_->updatePosition(place_position);

    // When: 배치 실행
    auto action = std::make_shared<PlacePalletAction>(
        "place_005", place_position, state_accessor_, fieldbus_driver_);

    auto result = action->execute();

    // Then: 성공 (높이 자동 조정)
    EXPECT_EQ(result.status, core::action::ActionStatus::SUCCESS);
    EXPECT_TRUE(fieldbus_driver_->lower_command_received_);
}

// 안전 거리 확인 테스트
TEST_F(PlacePalletActionTest, SafetyDistance) {
    // Given: 안전 거리 설정
    Position place_position{300.0, 400.0, 50.0, 0.0};  // Z=50mm
    state_accessor_->updatePosition(place_position);

    double safety_distance = 10.0;  // 10mm 안전 거리

    // When: 안전 거리를 고려한 배치
    auto action = std::make_shared<PlacePalletAction>(
        "place_006", place_position, state_accessor_, fieldbus_driver_, safety_distance);

    auto result = action->execute();

    // Then: 성공
    EXPECT_EQ(result.status, core::action::ActionStatus::SUCCESS);
}

// 배치 진행률 테스트
TEST_F(PlacePalletActionTest, PlacementProgress) {
    // Given: 배치 준비
    Position place_position{300.0, 400.0, 0.0, 0.0};
    state_accessor_->updatePosition(place_position);

    auto action = std::make_shared<PlacePalletAction>(
        "place_007", place_position, state_accessor_, fieldbus_driver_);

    // 초기 진행률
    EXPECT_DOUBLE_EQ(action->getProgress(), 0.0);

    // When: 실행 시작
    action->execute();

    // Then: 진행률 업데이트
    // 실제 구현에서는 하강 높이, 그리퍼 열림 상태 등으로 진행률 계산
    EXPECT_GT(action->getProgress(), 0.0);
}

// 취소 테스트
TEST_F(PlacePalletActionTest, CancelPlacement) {
    // Given: 배치 진행 중
    Position place_position{300.0, 400.0, 0.0, 0.0};
    state_accessor_->updatePosition(place_position);

    auto action = std::make_shared<PlacePalletAction>(
        "place_008", place_position, state_accessor_, fieldbus_driver_);

    action->execute();
    EXPECT_EQ(state_accessor_->getState(), ShuttleState::PLACING);

    // When: 취소
    action->cancel();

    // Then: IDLE 상태로 복귀, 팔렛은 여전히 적재 상태
    EXPECT_EQ(state_accessor_->getState(), ShuttleState::IDLE);
    EXPECT_TRUE(state_accessor_->getLoadedPallet().has_value());
}

// 연결 실패 테스트
TEST_F(PlacePalletActionTest, ConnectionFailure) {
    // Given: Fieldbus 연결 실패
    fieldbus_driver_->connected_ = false;

    Position place_position{300.0, 400.0, 0.0, 0.0};
    state_accessor_->updatePosition(place_position);

    // When: 배치 시도
    auto action = std::make_shared<PlacePalletAction>(
        "place_009", place_position, state_accessor_, fieldbus_driver_);

    auto result = action->execute();

    // Then: 실패 (연결 오류)
    EXPECT_EQ(result.status, core::action::ActionStatus::FAILURE);
    EXPECT_EQ(state_accessor_->getState(), ShuttleState::ERROR);
}

// 연속 배치 테스트
TEST_F(PlacePalletActionTest, ConsecutivePlacements) {
    // Given: 첫 번째 배치 완료
    Position place_position1{300.0, 400.0, 0.0, 0.0};
    state_accessor_->updatePosition(place_position1);

    auto action1 = std::make_shared<PlacePalletAction>(
        "place_010a", place_position1, state_accessor_, fieldbus_driver_);

    action1->execute();
    EXPECT_FALSE(state_accessor_->getLoadedPallet().has_value());

    // 새 팔렛 적재
    PalletInfo new_pallet{"PLT-NEW", 60.0, true};
    state_accessor_->updateLoadedPallet(new_pallet);

    // When: 두 번째 배치
    Position place_position2{500.0, 600.0, 0.0, 0.0};
    state_accessor_->updatePosition(place_position2);

    auto action2 = std::make_shared<PlacePalletAction>(
        "place_010b", place_position2, state_accessor_, fieldbus_driver_);

    auto result = action2->execute();

    // Then: 성공, 완료 카운트 증가
    EXPECT_EQ(result.status, core::action::ActionStatus::SUCCESS);
    EXPECT_EQ(state_accessor_->getCompletedTaskCount(), 2);
}