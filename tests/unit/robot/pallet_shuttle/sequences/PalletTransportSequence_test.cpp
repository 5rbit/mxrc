#include <gtest/gtest.h>
#include <memory>
#include "robot/pallet_shuttle/sequences/PalletTransportSequence.h"

using namespace mxrc::robot::pallet_shuttle;

/**
 * @brief PalletTransportSequence 단위 테스트
 *
 * Feature 016: Pallet Shuttle Control System
 * Phase 5: User Story 1 - 팔렛 픽업 및 배치
 *
 * 테스트 범위:
 * - T060: PalletTransportSequence 단위 테스트
 * - Sequence 정의 검증
 * - Action 단계 순서 확인
 * - 파라미터 전달
 */
class PalletTransportSequenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        sequence_ = std::make_shared<PalletTransportSequence>();
    }

    std::shared_ptr<PalletTransportSequence> sequence_;
};

// T060-1: Sequence 정의 조회
TEST_F(PalletTransportSequenceTest, GetDefinition_BasicInfo) {
    auto def = sequence_->getDefinition();

    EXPECT_EQ(def.id, "pallet_transport");
    EXPECT_EQ(def.name, "Pallet Transport Sequence");
    EXPECT_FALSE(def.steps.empty());
}

// T060-2: Action 단계 순서 확인
TEST_F(PalletTransportSequenceTest, GetDefinition_StepOrder) {
    auto def = sequence_->getDefinition();

    // 예상 순서: Move → Pick → Move → Place
    ASSERT_GE(def.steps.size(), 4);
    EXPECT_EQ(def.steps[0].actionType, "MoveToPosition");
    EXPECT_EQ(def.steps[1].actionType, "PickPallet");
    EXPECT_EQ(def.steps[2].actionType, "MoveToPosition");
    EXPECT_EQ(def.steps[3].actionType, "PlacePallet");
}

// T060-3: 픽업 위치 파라미터
TEST_F(PalletTransportSequenceTest, GetDefinition_PickupLocationParams) {
    auto def = sequence_->getDefinition();

    // 첫 번째 Move 액션 (픽업 위치로 이동)
    auto& move_to_pickup = def.steps[0];
    EXPECT_TRUE(move_to_pickup.parameters.count("target_x") > 0);
    EXPECT_TRUE(move_to_pickup.parameters.count("target_y") > 0);
}

// T060-4: PickPallet 파라미터
TEST_F(PalletTransportSequenceTest, GetDefinition_PickPalletParams) {
    auto def = sequence_->getDefinition();

    auto& pick = def.steps[1];
    EXPECT_TRUE(pick.parameters.count("pallet_id") > 0);
}

// T060-5: 배치 위치 파라미터
TEST_F(PalletTransportSequenceTest, GetDefinition_PlacementLocationParams) {
    auto def = sequence_->getDefinition();

    // 두 번째 Move 액션 (배치 위치로 이동)
    auto& move_to_place = def.steps[2];
    EXPECT_TRUE(move_to_place.parameters.count("target_x") > 0);
    EXPECT_TRUE(move_to_place.parameters.count("target_y") > 0);
}

// T060-6: Sequence 타임아웃 설정
TEST_F(PalletTransportSequenceTest, GetDefinition_Timeout) {
    auto def = sequence_->getDefinition();

    // 타임아웃이 설정되어 있어야 함 (0이 아님)
    EXPECT_GT(def.timeout.count(), 0);
}

// T060-7: 재시도 정책 확인
TEST_F(PalletTransportSequenceTest, GetDefinition_RetryPolicy) {
    auto def = sequence_->getDefinition();

    // 재시도 정책이 있을 수 있음 (optional)
    if (def.retryPolicy.has_value()) {
        EXPECT_GT(def.retryPolicy->maxRetries, 0);
    }
}

// T060-8: 커스텀 파라미터로 Sequence 생성
TEST_F(PalletTransportSequenceTest, CreateWithCustomParams) {
    auto custom_seq = std::make_shared<PalletTransportSequence>(
        100.0, 200.0,  // pickup location
        300.0, 400.0,  // place location
        "PALLET_CUSTOM"
    );

    auto def = custom_seq->getDefinition();
    EXPECT_EQ(def.steps[0].parameters["target_x"], "100");
    EXPECT_EQ(def.steps[0].parameters["target_y"], "200");
}
