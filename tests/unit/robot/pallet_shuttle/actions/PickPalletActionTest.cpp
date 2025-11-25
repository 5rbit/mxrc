// PickPalletActionTest.cpp - PickPalletAction 단위 테스트
// Copyright (C) 2025 MXRC Project
//
// Feature 016: Pallet Shuttle Control System (T058)

#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <thread>

#include "robot/pallet_shuttle/actions/PickPalletAction.h"
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
    mutable double battery_level_ = 1.0;

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
    double getBatteryLevel() const override { return battery_level_; }
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
    bool lift_command_received_ = false;
    bool gripper_command_received_ = false;
    bool pallet_detected_ = true;
    double detected_weight_ = 50.0;  // kg
    std::string last_command_;

    bool connect() override { return connected_; }
    bool disconnect() override { return true; }
    bool isConnected() const override { return connected_; }

    bool read(const std::string& address, std::any& value) override {
        if (address == "sensor/pallet_present") {
            value = pallet_detected_;
            return true;
        } else if (address == "sensor/pallet_weight") {
            value = detected_weight_;
            return true;
        } else if (address == "sensor/lift_position") {
            value = 100.0;  // mm
            return true;
        }
        return false;
    }

    bool write(const std::string& address, const std::any& value) override {
        last_command_ = address;

        if (address == "actuator/lift") {
            lift_command_received_ = true;
            return true;
        } else if (address == "actuator/gripper") {
            gripper_command_received_ = true;
            return true;
        }
        return false;
    }

    std::vector<std::string> scan() override { return {}; }
    std::string getDriverInfo() const override { return "MockDriver"; }
    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
};

class PickPalletActionTest : public ::testing::Test {
protected:
    void SetUp() override {
        state_accessor_ = std::make_shared<MockStateAccessor>();
        fieldbus_driver_ = std::make_shared<MockFieldbusDriver>();
    }

    std::shared_ptr<MockStateAccessor> state_accessor_;
    std::shared_ptr<MockFieldbusDriver> fieldbus_driver_;
};

// 기본 팔렛 픽업 테스트
TEST_F(PickPalletActionTest, SuccessfulPickup) {
    // Given: 픽업할 팔렛 정보
    std::string pallet_id = "PLT-001";
    Position pickup_position{100.0, 200.0, 0.0, 0.0};

    // 현재 위치를 픽업 위치로 설정
    state_accessor_->updatePosition(pickup_position);

    // When: PickPalletAction 실행
    auto action = std::make_shared<PickPalletAction>(
        "pick_001", pallet_id, pickup_position, state_accessor_, fieldbus_driver_);

    auto result = action->execute();

    // Then: 성공적으로 픽업
    EXPECT_EQ(result.status, core::action::ActionStatus::SUCCESS);
    EXPECT_EQ(state_accessor_->getState(), ShuttleState::PICKING);
    EXPECT_TRUE(fieldbus_driver_->lift_command_received_);
    EXPECT_TRUE(fieldbus_driver_->gripper_command_received_);

    // 팔렛이 적재되었는지 확인
    auto loaded = state_accessor_->getLoadedPallet();
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->pallet_id, pallet_id);
    EXPECT_TRUE(loaded->is_loaded);
    EXPECT_DOUBLE_EQ(loaded->weight, fieldbus_driver_->detected_weight_);
}

// 팔렛 미감지 테스트
TEST_F(PickPalletActionTest, PalletNotDetected) {
    // Given: 팔렛이 감지되지 않음
    fieldbus_driver_->pallet_detected_ = false;

    std::string pallet_id = "PLT-002";
    Position pickup_position{100.0, 200.0, 0.0, 0.0};
    state_accessor_->updatePosition(pickup_position);

    // When: PickPalletAction 실행
    auto action = std::make_shared<PickPalletAction>(
        "pick_002", pallet_id, pickup_position, state_accessor_, fieldbus_driver_);

    auto result = action->execute();

    // Then: 실패
    EXPECT_EQ(result.status, core::action::ActionStatus::FAILURE);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_FALSE(state_accessor_->getLoadedPallet().has_value());
}

// 이미 팔렛이 적재된 상태 테스트
TEST_F(PickPalletActionTest, AlreadyLoaded) {
    // Given: 이미 다른 팔렛이 적재됨
    PalletInfo existing_pallet{"PLT-EXISTING", 30.0, true};
    state_accessor_->updateLoadedPallet(existing_pallet);

    std::string new_pallet_id = "PLT-003";
    Position pickup_position{100.0, 200.0, 0.0, 0.0};
    state_accessor_->updatePosition(pickup_position);

    // When: 새 팔렛 픽업 시도
    auto action = std::make_shared<PickPalletAction>(
        "pick_003", new_pallet_id, pickup_position, state_accessor_, fieldbus_driver_);

    auto result = action->execute();

    // Then: 실패 (이미 적재됨)
    EXPECT_EQ(result.status, core::action::ActionStatus::FAILURE);
    EXPECT_TRUE(result.error_message.find("already loaded") != std::string::npos);
}

// 무게 초과 테스트
TEST_F(PickPalletActionTest, WeightExceeded) {
    // Given: 매우 무거운 팔렛
    fieldbus_driver_->detected_weight_ = 2000.0;  // 2톤

    std::string pallet_id = "PLT-HEAVY";
    Position pickup_position{100.0, 200.0, 0.0, 0.0};
    state_accessor_->updatePosition(pickup_position);

    // 최대 중량 제한을 1000kg으로 설정
    auto action = std::make_shared<PickPalletAction>(
        "pick_004", pallet_id, pickup_position, state_accessor_, fieldbus_driver_, 1000.0);

    // When: 픽업 시도
    auto result = action->execute();

    // Then: 실패 (무게 초과)
    EXPECT_EQ(result.status, core::action::ActionStatus::FAILURE);
    EXPECT_TRUE(result.error_message.find("weight exceeded") != std::string::npos);
}

// 위치 불일치 테스트
TEST_F(PickPalletActionTest, PositionMismatch) {
    // Given: 현재 위치와 픽업 위치가 다름
    Position current_position{0.0, 0.0, 0.0, 0.0};
    Position pickup_position{100.0, 200.0, 0.0, 0.0};

    state_accessor_->updatePosition(current_position);

    // When: 픽업 시도
    auto action = std::make_shared<PickPalletAction>(
        "pick_005", "PLT-005", pickup_position, state_accessor_, fieldbus_driver_);

    auto result = action->execute();

    // Then: 실패 (위치 불일치)
    EXPECT_EQ(result.status, core::action::ActionStatus::FAILURE);
    EXPECT_TRUE(result.error_message.find("not at pickup position") != std::string::npos);
}

// 배터리 부족 테스트
TEST_F(PickPalletActionTest, LowBattery) {
    // Given: 배터리 부족
    state_accessor_->battery_level_ = 0.05;  // 5%

    std::string pallet_id = "PLT-006";
    Position pickup_position{100.0, 200.0, 0.0, 0.0};
    state_accessor_->updatePosition(pickup_position);

    // 최소 배터리 레벨 10%로 설정
    auto action = std::make_shared<PickPalletAction>(
        "pick_006", pallet_id, pickup_position, state_accessor_, fieldbus_driver_,
        1000.0, 0.10);

    // When: 픽업 시도
    auto result = action->execute();

    // Then: 실패 (배터리 부족)
    EXPECT_EQ(result.status, core::action::ActionStatus::FAILURE);
    EXPECT_TRUE(result.error_message.find("battery low") != std::string::npos);
}

// 픽업 진행률 테스트
TEST_F(PickPalletActionTest, PickupProgress) {
    // Given: 픽업 준비
    std::string pallet_id = "PLT-007";
    Position pickup_position{100.0, 200.0, 0.0, 0.0};
    state_accessor_->updatePosition(pickup_position);

    auto action = std::make_shared<PickPalletAction>(
        "pick_007", pallet_id, pickup_position, state_accessor_, fieldbus_driver_);

    // 초기 진행률
    EXPECT_DOUBLE_EQ(action->getProgress(), 0.0);

    // When: 실행 시작
    action->execute();

    // Then: 진행률 업데이트
    // 실제 구현에서는 리프트 높이, 그리퍼 상태 등으로 진행률 계산
    EXPECT_GT(action->getProgress(), 0.0);
}

// 취소 테스트
TEST_F(PickPalletActionTest, CancelPickup) {
    // Given: 픽업 진행 중
    std::string pallet_id = "PLT-008";
    Position pickup_position{100.0, 200.0, 0.0, 0.0};
    state_accessor_->updatePosition(pickup_position);

    auto action = std::make_shared<PickPalletAction>(
        "pick_008", pallet_id, pickup_position, state_accessor_, fieldbus_driver_);

    action->execute();
    EXPECT_EQ(state_accessor_->getState(), ShuttleState::PICKING);

    // When: 취소
    action->cancel();

    // Then: IDLE 상태로 복귀, 팔렛 미적재
    EXPECT_EQ(state_accessor_->getState(), ShuttleState::IDLE);
    EXPECT_FALSE(state_accessor_->getLoadedPallet().has_value());
}

// 연결 실패 테스트
TEST_F(PickPalletActionTest, ConnectionFailure) {
    // Given: Fieldbus 연결 실패
    fieldbus_driver_->connected_ = false;

    std::string pallet_id = "PLT-009";
    Position pickup_position{100.0, 200.0, 0.0, 0.0};
    state_accessor_->updatePosition(pickup_position);

    // When: 픽업 시도
    auto action = std::make_shared<PickPalletAction>(
        "pick_009", pallet_id, pickup_position, state_accessor_, fieldbus_driver_);

    auto result = action->execute();

    // Then: 실패 (연결 오류)
    EXPECT_EQ(result.status, core::action::ActionStatus::FAILURE);
    EXPECT_EQ(state_accessor_->getState(), ShuttleState::ERROR);
}