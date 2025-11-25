#include <gtest/gtest.h>
#include <memory>
#include "robot/pallet_shuttle/tasks/PalletTransportTask.h"
#include "core/task/dto/TaskStatus.h"

using namespace mxrc::robot::pallet_shuttle;
using namespace mxrc::core::task;

/**
 * @brief PalletTransportTask 단위 테스트
 *
 * Feature 016: Pallet Shuttle Control System
 * Phase 5: User Story 1 - 팔렛 픽업 및 배치
 *
 * 테스트 범위:
 * - T061: PalletTransportTask 단위 테스트
 * - Task 생성 및 시작
 * - 상태 관리
 * - 진행률 추적
 */
class PalletTransportTaskTest : public ::testing::Test {
protected:
    void SetUp() override {
        task_ = std::make_shared<PalletTransportTask>(
            "task_001",
            100.0, 200.0,  // pickup location
            300.0, 400.0,  // place location
            "PALLET_001"
        );
    }

    std::shared_ptr<PalletTransportTask> task_;
};

// T061-1: Task 생성 및 기본 정보
TEST_F(PalletTransportTaskTest, CreateTask_BasicInfo) {
    EXPECT_EQ(task_->getId(), "task_001");
    EXPECT_EQ(task_->getStatus(), TaskStatus::IDLE);
    EXPECT_FLOAT_EQ(task_->getProgress(), 0.0f);
}

// T061-2: Task 정의 조회
TEST_F(PalletTransportTaskTest, GetDefinition) {
    auto& def = task_->getDefinition();

    EXPECT_EQ(def.name, "Pallet Transport Task");
    EXPECT_FALSE(def.name.empty());
}

// T061-3: Task 시작
TEST_F(PalletTransportTaskTest, Start_Success) {
    std::string exec_id = task_->start();

    EXPECT_FALSE(exec_id.empty());
    EXPECT_EQ(task_->getStatus(), TaskStatus::RUNNING);
}

// T061-4: Task 중지
TEST_F(PalletTransportTaskTest, Stop_Success) {
    task_->start();
    task_->stop();

    EXPECT_TRUE(
        task_->getStatus() == TaskStatus::CANCELLED ||
        task_->getStatus() == TaskStatus::COMPLETED
    );
}

// T061-5: Task 일시정지
TEST_F(PalletTransportTaskTest, Pause_Success) {
    task_->start();
    task_->pause();

    EXPECT_EQ(task_->getStatus(), TaskStatus::PAUSED);
}

// T061-6: Task 재개
TEST_F(PalletTransportTaskTest, Resume_Success) {
    task_->start();
    task_->pause();
    task_->resume();

    EXPECT_EQ(task_->getStatus(), TaskStatus::RUNNING);
}

// T061-7: 진행률 추적
TEST_F(PalletTransportTaskTest, Progress_Tracking) {
    EXPECT_FLOAT_EQ(task_->getProgress(), 0.0f);

    task_->start();

    // 진행 중이므로 0보다 크고 1 이하
    float progress = task_->getProgress();
    EXPECT_GE(progress, 0.0f);
    EXPECT_LE(progress, 1.0f);
}

// T061-8: 이미 실행 중인 Task 재시작
TEST_F(PalletTransportTaskTest, Start_AlreadyRunning) {
    task_->start();

    // 이미 실행 중이면 예외
    EXPECT_THROW(task_->start(), std::runtime_error);
}

// T061-9: IDLE 상태가 아닌 Task pause
TEST_F(PalletTransportTaskTest, Pause_NotRunning) {
    // IDLE 상태에서 pause 시도
    EXPECT_THROW(task_->pause(), std::runtime_error);
}

// T061-10: PAUSED 상태가 아닌 Task resume
TEST_F(PalletTransportTaskTest, Resume_NotPaused) {
    // IDLE 상태에서 resume 시도
    EXPECT_THROW(task_->resume(), std::runtime_error);
}

// T061-11: 잘못된 파라미터로 Task 생성
TEST_F(PalletTransportTaskTest, CreateTask_InvalidParams) {
    // 빈 팔렛 ID
    EXPECT_THROW(
        std::make_shared<PalletTransportTask>(
            "task_002", 0.0, 0.0, 0.0, 0.0, ""
        ),
        std::invalid_argument
    );
}
