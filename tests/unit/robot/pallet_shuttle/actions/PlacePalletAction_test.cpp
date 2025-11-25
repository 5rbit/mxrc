#include <gtest/gtest.h>
#include <memory>
#include "robot/pallet_shuttle/actions/PlacePalletAction.h"
#include "core/action/util/ExecutionContext.h"

using namespace mxrc::robot::pallet_shuttle;
using namespace mxrc::core::action;

/**
 * @brief PlacePalletAction 단위 테스트
 *
 * Feature 016: Pallet Shuttle Control System
 * Phase 5: User Story 1 - 팔렛 픽업 및 배치
 *
 * 테스트 범위:
 * - T059: PlacePalletAction 단위 테스트
 * - 팔렛 배치 시뮬레이션
 * - 그리퍼 제어
 * - 상태 업데이트
 */
class PlacePalletActionTest : public ::testing::Test {
protected:
    void SetUp() override {
        context_ = std::make_shared<ExecutionContext>();
        // 현재 위치 및 픽업된 팔렛 설정
        context_->set("current_x", "300.0");
        context_->set("current_y", "400.0");
        context_->set("picked_pallet_id", "PALLET_001");
        context_->set("gripper_closed", "true");
    }

    std::shared_ptr<ExecutionContext> context_;
};

// T059-1: Action 생성 및 기본 정보
TEST_F(PlacePalletActionTest, CreateAction_BasicInfo) {
    auto action = std::make_shared<PlacePalletAction>("place_1");

    EXPECT_EQ(action->getId(), "place_1");
    EXPECT_EQ(action->getType(), "PlacePallet");
    EXPECT_EQ(action->getStatus(), ActionStatus::PENDING);
}

// T059-2: 팔렛 배치 성공
TEST_F(PlacePalletActionTest, Execute_Success) {
    auto action = std::make_shared<PlacePalletAction>("place_1");

    action->execute(*context_);

    EXPECT_EQ(action->getStatus(), ActionStatus::COMPLETED);
    // 배치 후 팔렛 ID 제거
    EXPECT_FALSE(context_->has("picked_pallet_id") &&
                 !context_->get("picked_pallet_id").empty());
}

// T059-3: 그리퍼 상태 확인
TEST_F(PlacePalletActionTest, Execute_GripperState) {
    auto action = std::make_shared<PlacePalletAction>("place_1");

    action->execute(*context_);

    // 그리퍼가 열린 상태여야 함
    EXPECT_TRUE(context_->has("gripper_closed"));
    EXPECT_EQ(context_->get("gripper_closed"), "false");
}

// T059-4: 팔렛을 들고 있지 않은 경우
TEST_F(PlacePalletActionTest, Execute_NoPalletHeld) {
    context_->set("picked_pallet_id", "");

    auto action = std::make_shared<PlacePalletAction>("place_1");

    // 팔렛을 들고 있지 않으면 실패
    EXPECT_THROW(action->execute(*context_), std::runtime_error);
}

// T059-5: Action 취소
TEST_F(PlacePalletActionTest, Cancel_DuringExecution) {
    auto action = std::make_shared<PlacePalletAction>("place_1");

    action->execute(*context_);
    action->cancel();

    EXPECT_TRUE(
        action->getStatus() == ActionStatus::CANCELLED ||
        action->getStatus() == ActionStatus::COMPLETED
    );
}

// T059-6: 진행률 추적
TEST_F(PlacePalletActionTest, Progress_Tracking) {
    auto action = std::make_shared<PlacePalletAction>("place_1");

    EXPECT_FLOAT_EQ(action->getProgress(), 0.0f);

    action->execute(*context_);

    EXPECT_FLOAT_EQ(action->getProgress(), 1.0f);
}

// T059-7: 배치 위치 기록
TEST_F(PlacePalletActionTest, Execute_RecordPlacementLocation) {
    auto action = std::make_shared<PlacePalletAction>("place_1");

    action->execute(*context_);

    // 배치 위치 기록
    EXPECT_TRUE(context_->has("last_place_x"));
    EXPECT_TRUE(context_->has("last_place_y"));
}
