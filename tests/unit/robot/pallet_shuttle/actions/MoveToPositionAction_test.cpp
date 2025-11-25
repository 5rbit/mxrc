#include <gtest/gtest.h>
#include <memory>
#include "robot/pallet_shuttle/actions/MoveToPositionAction.h"
#include "core/action/util/ExecutionContext.h"

using namespace mxrc::robot::pallet_shuttle;
using namespace mxrc::core::action;

/**
 * @brief MoveToPositionAction 단위 테스트
 *
 * Feature 016: Pallet Shuttle Control System
 * Phase 5: User Story 1 - 팔렛 픽업 및 배치
 *
 * 테스트 범위:
 * - T057: MoveToPositionAction 단위 테스트
 * - 위치 이동 시뮬레이션
 * - 진행률 추적
 * - 취소 처리
 * - 오류 처리
 */
class MoveToPositionActionTest : public ::testing::Test {
protected:
    void SetUp() override {
        context_ = std::make_shared<ExecutionContext>();
    }

    std::shared_ptr<ExecutionContext> context_;
};

// T057-1: Action 생성 및 기본 정보 조회
TEST_F(MoveToPositionActionTest, CreateAction_BasicInfo) {
    auto action = std::make_shared<MoveToPositionAction>(
        "move_1",
        100.0,  // target_x
        200.0,  // target_y
        0.0     // target_theta
    );

    EXPECT_EQ(action->getId(), "move_1");
    EXPECT_EQ(action->getType(), "MoveToPosition");
    EXPECT_EQ(action->getStatus(), ActionStatus::PENDING);
    EXPECT_FLOAT_EQ(action->getProgress(), 0.0f);
}

// T057-2: Action 실행 성공
TEST_F(MoveToPositionActionTest, Execute_Success) {
    auto action = std::make_shared<MoveToPositionAction>(
        "move_1", 100.0, 200.0, 0.0
    );

    // 실행 전 상태
    EXPECT_EQ(action->getStatus(), ActionStatus::PENDING);

    // 실행
    action->execute(*context_);

    // 실행 후 상태
    EXPECT_EQ(action->getStatus(), ActionStatus::COMPLETED);
    EXPECT_FLOAT_EQ(action->getProgress(), 1.0f);
}

// T057-3: Action 취소
TEST_F(MoveToPositionActionTest, Cancel_DuringExecution) {
    auto action = std::make_shared<MoveToPositionAction>(
        "move_1", 100.0, 200.0, 0.0
    );

    // 실행 시작 (비동기 시뮬레이션)
    action->execute(*context_);

    // 취소
    action->cancel();

    EXPECT_TRUE(
        action->getStatus() == ActionStatus::CANCELLED ||
        action->getStatus() == ActionStatus::COMPLETED
    );
}

// T057-4: 잘못된 목표 위치 (음수 좌표)
TEST_F(MoveToPositionActionTest, Execute_InvalidPosition) {
    auto action = std::make_shared<MoveToPositionAction>(
        "move_1", -100.0, -200.0, 0.0
    );

    // 음수 좌표도 허용 (실제 로봇 좌표계에서는 가능)
    EXPECT_NO_THROW(action->execute(*context_));
}

// T057-5: ExecutionContext에 결과 저장 확인
TEST_F(MoveToPositionActionTest, Execute_ContextUpdate) {
    auto action = std::make_shared<MoveToPositionAction>(
        "move_1", 100.0, 200.0, 0.0
    );

    action->execute(*context_);

    // Context에 현재 위치가 저장되어야 함
    EXPECT_TRUE(context_->has("current_x"));
    EXPECT_TRUE(context_->has("current_y"));
    EXPECT_TRUE(context_->has("current_theta"));
}

// T057-6: 진행률 추적 (시뮬레이션)
TEST_F(MoveToPositionActionTest, Progress_Tracking) {
    auto action = std::make_shared<MoveToPositionAction>(
        "move_1", 100.0, 200.0, 0.0
    );

    // 초기 진행률
    EXPECT_FLOAT_EQ(action->getProgress(), 0.0f);

    // 실행 후 진행률
    action->execute(*context_);
    EXPECT_FLOAT_EQ(action->getProgress(), 1.0f);
}

// T057-7: 동일한 Action 재실행
TEST_F(MoveToPositionActionTest, Execute_Twice) {
    auto action = std::make_shared<MoveToPositionAction>(
        "move_1", 100.0, 200.0, 0.0
    );

    // 첫 번째 실행
    action->execute(*context_);
    EXPECT_EQ(action->getStatus(), ActionStatus::COMPLETED);

    // 두 번째 실행 (이미 완료된 Action)
    // 구현에 따라 예외를 던지거나 무시할 수 있음
    // 여기서는 예외를 던지는 것으로 가정
    EXPECT_THROW(action->execute(*context_), std::runtime_error);
}

// T057-8: 큰 거리 이동 (경계 조건)
TEST_F(MoveToPositionActionTest, Execute_LargeDistance) {
    auto action = std::make_shared<MoveToPositionAction>(
        "move_1", 10000.0, 10000.0, 3.14159
    );

    EXPECT_NO_THROW(action->execute(*context_));
    EXPECT_EQ(action->getStatus(), ActionStatus::COMPLETED);
}
