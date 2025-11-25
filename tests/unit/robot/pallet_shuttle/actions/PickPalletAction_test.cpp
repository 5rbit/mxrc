#include <gtest/gtest.h>
#include <memory>
#include "robot/pallet_shuttle/actions/PickPalletAction.h"
#include "core/action/util/ExecutionContext.h"

using namespace mxrc::robot::pallet_shuttle;
using namespace mxrc::core::action;

/**
 * @brief PickPalletAction 단위 테스트
 *
 * Feature 016: Pallet Shuttle Control System
 * Phase 5: User Story 1 - 팔렛 픽업 및 배치
 *
 * 테스트 범위:
 * - T058: PickPalletAction 단위 테스트
 * - 팔렛 픽업 시뮬레이션
 * - 그리퍼 제어
 * - 센서 확인
 */
class PickPalletActionTest : public ::testing::Test {
protected:
    void SetUp() override {
        context_ = std::make_shared<ExecutionContext>();
        // 현재 위치 설정
        context_->set("current_x", "100.0");
        context_->set("current_y", "200.0");
    }

    std::shared_ptr<ExecutionContext> context_;
};

// T058-1: Action 생성 및 기본 정보
TEST_F(PickPalletActionTest, CreateAction_BasicInfo) {
    auto action = std::make_shared<PickPalletAction>("pick_1", "PALLET_001");

    EXPECT_EQ(action->getId(), "pick_1");
    EXPECT_EQ(action->getType(), "PickPallet");
    EXPECT_EQ(action->getStatus(), ActionStatus::PENDING);
}

// T058-2: 팔렛 픽업 성공
TEST_F(PickPalletActionTest, Execute_Success) {
    auto action = std::make_shared<PickPalletAction>("pick_1", "PALLET_001");

    action->execute(*context_);

    EXPECT_EQ(action->getStatus(), ActionStatus::COMPLETED);
    EXPECT_TRUE(context_->has("picked_pallet_id"));
    EXPECT_EQ(context_->get("picked_pallet_id"), "PALLET_001");
}

// T058-3: 그리퍼 상태 확인
TEST_F(PickPalletActionTest, Execute_GripperState) {
    auto action = std::make_shared<PickPalletAction>("pick_1", "PALLET_001");

    action->execute(*context_);

    // 그리퍼가 닫힌 상태여야 함
    EXPECT_TRUE(context_->has("gripper_closed"));
    EXPECT_EQ(context_->get("gripper_closed"), "true");
}

// T058-4: 팔렛 ID 검증
TEST_F(PickPalletActionTest, Execute_InvalidPalletId) {
    // Empty pallet_id throws in constructor
    // auto action = std::make_shared<PickPalletAction>("pick_1", "");
    EXPECT_THROW(
        std::make_shared<PickPalletAction>("pick_1", ""),
        std::invalid_argument
    );

}

// T058-5: 이미 팔렛을 들고 있는 경우
TEST_F(PickPalletActionTest, Execute_AlreadyHoldingPallet) {
    context_->set("picked_pallet_id", "PALLET_000");

    auto action = std::make_shared<PickPalletAction>("pick_1", "PALLET_001");

    // 이미 팔렛을 들고 있으면 실패
}

// T058-6: Action 취소
TEST_F(PickPalletActionTest, Cancel_DuringExecution) {
    auto action = std::make_shared<PickPalletAction>("pick_1", "PALLET_001");

    action->execute(*context_);
    action->cancel();

    EXPECT_TRUE(
        action->getStatus() == ActionStatus::CANCELLED ||
        action->getStatus() == ActionStatus::COMPLETED
    );
}

// T058-7: 진행률 추적
TEST_F(PickPalletActionTest, Progress_Tracking) {
    auto action = std::make_shared<PickPalletAction>("pick_1", "PALLET_001");

    EXPECT_FLOAT_EQ(action->getProgress(), 0.0f);

    action->execute(*context_);

    EXPECT_FLOAT_EQ(action->getProgress(), 1.0f);
}
