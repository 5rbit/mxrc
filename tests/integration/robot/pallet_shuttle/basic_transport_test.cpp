#include <gtest/gtest.h>
#include <memory>
#include <spdlog/spdlog.h>
#include "robot/pallet_shuttle/actions/MoveToPositionAction.h"
#include "robot/pallet_shuttle/actions/PickPalletAction.h"
#include "robot/pallet_shuttle/actions/PlacePalletAction.h"
#include "robot/pallet_shuttle/sequences/PalletTransportSequence.h"
#include "robot/pallet_shuttle/tasks/PalletTransportTask.h"
#include "core/action/util/ExecutionContext.h"

using namespace mxrc::robot::pallet_shuttle;
using namespace mxrc::core::action;

/**
 * @brief Pallet Shuttle 기본 운반 통합 테스트
 *
 * Feature 016: Pallet Shuttle Control System
 * Phase 5: User Story 1 - 팔렛 픽업 및 배치
 *
 * 테스트 범위:
 * - T062: 기본 운반 통합 테스트
 * - Action → Sequence → Task 전체 흐름 검증
 * - 실제 시나리오 시뮬레이션
 */
class BasicTransportIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        context_ = std::make_shared<ExecutionContext>();

        // 초기 로봇 위치 설정
        context_->set("current_x", "0");
        context_->set("current_y", "0");
        context_->set("current_theta", "0.0");
        context_->set("gripper_state", "open");
    }

    std::shared_ptr<ExecutionContext> context_;
};

// T062-1: 단일 Action 실행 검증
TEST_F(BasicTransportIntegrationTest, SingleMoveAction) {
    auto action = std::make_shared<MoveToPositionAction>(
        "move_test", 100.0, 200.0, 0.0
    );

    action->execute(*context_);

    EXPECT_EQ(action->getStatus(), ActionStatus::COMPLETED);
    EXPECT_FLOAT_EQ(action->getProgress(), 1.0f);
    EXPECT_EQ(context_->get("current_x"), "100");
    EXPECT_EQ(context_->get("current_y"), "200");
}

// T062-2: Pick & Place Action 연속 실행
TEST_F(BasicTransportIntegrationTest, PickAndPlaceActions) {
    // 픽업 위치로 이동
    auto move_to_pickup = std::make_shared<MoveToPositionAction>(
        "move1", 100.0, 200.0, 0.0
    );
    move_to_pickup->execute(*context_);
    EXPECT_EQ(move_to_pickup->getStatus(), ActionStatus::COMPLETED);

    // 팔렛 픽업
    auto pick = std::make_shared<PickPalletAction>("pick1", "PALLET_001");
    pick->execute(*context_);
    EXPECT_EQ(pick->getStatus(), ActionStatus::COMPLETED);
    EXPECT_EQ(context_->get("gripper_state"), "closed");
    EXPECT_EQ(context_->get("holding_pallet"), "PALLET_001");

    // 배치 위치로 이동
    auto move_to_place = std::make_shared<MoveToPositionAction>(
        "move2", 300.0, 400.0, 0.0
    );
    move_to_place->execute(*context_);
    EXPECT_EQ(move_to_place->getStatus(), ActionStatus::COMPLETED);

    // 팔렛 배치
    auto place = std::make_shared<PlacePalletAction>("place1");
    place->execute(*context_);
    EXPECT_EQ(place->getStatus(), ActionStatus::COMPLETED);
    EXPECT_EQ(context_->get("gripper_state"), "open");
    EXPECT_EQ(context_->get("holding_pallet"), "");
}

// T062-3: PalletTransportSequence 정의 검증
TEST_F(BasicTransportIntegrationTest, SequenceDefinitionValidation) {
    auto sequence = std::make_shared<PalletTransportSequence>(
        100.0, 200.0,  // pickup location
        300.0, 400.0,  // place location
        "PALLET_TEST"
    );

    auto def = sequence->getDefinition();

    EXPECT_EQ(def.id, "pallet_transport");
    EXPECT_EQ(def.name, "Pallet Transport Sequence");
    ASSERT_EQ(def.steps.size(), 4);

    // 단계 순서 검증
    EXPECT_EQ(def.steps[0].actionType, "MoveToPosition");
    EXPECT_EQ(def.steps[1].actionType, "PickPallet");
    EXPECT_EQ(def.steps[2].actionType, "MoveToPosition");
    EXPECT_EQ(def.steps[3].actionType, "PlacePallet");

    // 파라미터 검증
    EXPECT_EQ(def.steps[0].parameters.at("target_x"), "100");
    EXPECT_EQ(def.steps[0].parameters.at("target_y"), "200");
    EXPECT_EQ(def.steps[1].parameters.at("pallet_id"), "PALLET_TEST");
    EXPECT_EQ(def.steps[2].parameters.at("target_x"), "300");
    EXPECT_EQ(def.steps[2].parameters.at("target_y"), "400");

    // 타임아웃 검증
    EXPECT_GT(def.timeout.count(), 0);
}

// T062-4: 수동 Sequence 실행 (각 단계 개별 실행)
TEST_F(BasicTransportIntegrationTest, ManualSequenceExecution) {
    auto sequence = std::make_shared<PalletTransportSequence>(
        150.0, 250.0,  // pickup
        350.0, 450.0,  // place
        "PALLET_MANUAL"
    );

    auto def = sequence->getDefinition();

    // Step 1: Move to pickup
    auto move1 = std::make_shared<MoveToPositionAction>(
        def.steps[0].actionId,
        std::stod(def.steps[0].parameters.at("target_x")),
        std::stod(def.steps[0].parameters.at("target_y")),
        std::stod(def.steps[0].parameters.at("target_theta"))
    );
    move1->execute(*context_);
    EXPECT_EQ(move1->getStatus(), ActionStatus::COMPLETED);

    // Step 2: Pick pallet
    auto pick = std::make_shared<PickPalletAction>(
        def.steps[1].actionId,
        def.steps[1].parameters.at("pallet_id")
    );
    pick->execute(*context_);
    EXPECT_EQ(pick->getStatus(), ActionStatus::COMPLETED);

    // Step 3: Move to place
    auto move2 = std::make_shared<MoveToPositionAction>(
        def.steps[2].actionId,
        std::stod(def.steps[2].parameters.at("target_x")),
        std::stod(def.steps[2].parameters.at("target_y")),
        std::stod(def.steps[2].parameters.at("target_theta"))
    );
    move2->execute(*context_);
    EXPECT_EQ(move2->getStatus(), ActionStatus::COMPLETED);

    // Step 4: Place pallet
    auto place = std::make_shared<PlacePalletAction>(
        def.steps[3].actionId
    );
    place->execute(*context_);
    EXPECT_EQ(place->getStatus(), ActionStatus::COMPLETED);

    // 최종 상태 검증
    EXPECT_EQ(context_->get("current_x"), "350");
    EXPECT_EQ(context_->get("current_y"), "450");
    EXPECT_EQ(context_->get("gripper_state"), "open");
    EXPECT_EQ(context_->get("holding_pallet"), "");
}

// T062-5: PalletTransportTask 기본 실행
TEST_F(BasicTransportIntegrationTest, TaskBasicExecution) {
    auto task = std::make_shared<PalletTransportTask>(
        "task_001",
        200.0, 300.0,  // pickup
        500.0, 600.0,  // place
        "PALLET_TASK"
    );

    EXPECT_EQ(task->getId(), "task_001");
    EXPECT_EQ(task->getStatus(), mxrc::core::task::TaskStatus::IDLE);

    // Task 시작
    task->start();
    EXPECT_EQ(task->getStatus(), mxrc::core::task::TaskStatus::RUNNING);

    // Task 정의 검증
    auto def = task->getDefinition();
    EXPECT_EQ(def.id, "task_001");
    EXPECT_EQ(def.name, "Pallet Transport Task");
}

// T062-6: Task 일시정지 및 재개
TEST_F(BasicTransportIntegrationTest, TaskPauseAndResume) {
    auto task = std::make_shared<PalletTransportTask>(
        "task_002",
        100.0, 100.0,
        200.0, 200.0,
        "PALLET_PAUSE"
    );

    // 시작
    task->start();
    EXPECT_EQ(task->getStatus(), mxrc::core::task::TaskStatus::RUNNING);

    // 일시정지
    task->pause();
    EXPECT_EQ(task->getStatus(), mxrc::core::task::TaskStatus::PAUSED);

    // 재개
    task->resume();
    EXPECT_EQ(task->getStatus(), mxrc::core::task::TaskStatus::RUNNING);
}

// T062-7: Task 중지
TEST_F(BasicTransportIntegrationTest, TaskStop) {
    auto task = std::make_shared<PalletTransportTask>(
        "task_003",
        100.0, 100.0,
        200.0, 200.0,
        "PALLET_STOP"
    );

    task->start();
    EXPECT_EQ(task->getStatus(), mxrc::core::task::TaskStatus::RUNNING);

    task->stop();
    EXPECT_EQ(task->getStatus(), mxrc::core::task::TaskStatus::CANCELLED);
}

// T062-8: 여러 위치로 순차 운반
TEST_F(BasicTransportIntegrationTest, MultipleTransports) {
    std::vector<std::tuple<double, double, double, double, std::string>> transports = {
        {100, 100, 200, 200, "PALLET_A"},
        {200, 200, 300, 300, "PALLET_B"},
        {300, 300, 400, 400, "PALLET_C"}
    };

    for (const auto& [px, py, plx, ply, pallet_id] : transports) {
        // Move to pickup
        auto move1 = std::make_shared<MoveToPositionAction>(
            "move_" + pallet_id, px, py, 0.0
        );
        move1->execute(*context_);
        EXPECT_EQ(move1->getStatus(), ActionStatus::COMPLETED);

        // Pick
        auto pick = std::make_shared<PickPalletAction>("pick_" + pallet_id, pallet_id);
        pick->execute(*context_);
        EXPECT_EQ(pick->getStatus(), ActionStatus::COMPLETED);

        // Move to place
        auto move2 = std::make_shared<MoveToPositionAction>(
            "move_place_" + pallet_id, plx, ply, 0.0
        );
        move2->execute(*context_);
        EXPECT_EQ(move2->getStatus(), ActionStatus::COMPLETED);

        // Place
        auto place = std::make_shared<PlacePalletAction>("place_" + pallet_id);
        place->execute(*context_);
        EXPECT_EQ(place->getStatus(), ActionStatus::COMPLETED);

        EXPECT_EQ(context_->get("holding_pallet"), "");
    }

    // 최종 위치 검증
    EXPECT_EQ(context_->get("current_x"), "400");
    EXPECT_EQ(context_->get("current_y"), "400");
}

// T062-9: Context 데이터 전달 검증
TEST_F(BasicTransportIntegrationTest, ContextDataPropagation) {
    // 초기 컨텍스트 설정
    context_->set("robot_id", "ROBOT_001");
    context_->set("session_id", "SESSION_123");

    auto move = std::make_shared<MoveToPositionAction>("move1", 100, 100, 0);
    move->execute(*context_);

    // Context 데이터가 유지되는지 확인
    EXPECT_EQ(context_->get("robot_id"), "ROBOT_001");
    EXPECT_EQ(context_->get("session_id"), "SESSION_123");
    EXPECT_EQ(context_->get("current_x"), "100");

    auto pick = std::make_shared<PickPalletAction>("pick1", "PALLET_CTX");
    pick->execute(*context_);

    // 이전 데이터와 새 데이터 모두 존재
    EXPECT_EQ(context_->get("robot_id"), "ROBOT_001");
    EXPECT_EQ(context_->get("holding_pallet"), "PALLET_CTX");
}

// T062-10: 에러 상황 - 팔렛 없이 배치 시도
TEST_F(BasicTransportIntegrationTest, ErrorHandling_PlaceWithoutPallet) {
    // 그리퍼가 비어있는 상태에서 배치 시도
    auto place = std::make_shared<PlacePalletAction>("place_error");

    EXPECT_THROW(place->execute(*context_), std::runtime_error);
}

// T062-11: 에러 상황 - 이미 팔렛을 들고 있는 상태에서 픽업
TEST_F(BasicTransportIntegrationTest, ErrorHandling_PickWithPallet) {
    // 먼저 팔렛 픽업
    context_->set("holding_pallet", "PALLET_ALREADY");
    context_->set("gripper_state", "closed");

    // 다른 팔렛 픽업 시도
    auto pick = std::make_shared<PickPalletAction>("pick_error", "PALLET_NEW");

    EXPECT_THROW(pick->execute(*context_), std::runtime_error);
}

// T062-12: 진행률 추적
TEST_F(BasicTransportIntegrationTest, ProgressTracking) {
    auto move = std::make_shared<MoveToPositionAction>("move_progress", 1000, 1000, 0);

    EXPECT_FLOAT_EQ(move->getProgress(), 0.0f);

    move->execute(*context_);

    EXPECT_FLOAT_EQ(move->getProgress(), 1.0f);
}
