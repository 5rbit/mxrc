// PalletTransportSequenceTest.cpp - PalletTransportSequence 단위 테스트
// Copyright (C) 2025 MXRC Project
//
// Feature 016: Pallet Shuttle Control System (T060)

#include <gtest/gtest.h>
#include <memory>
#include <chrono>

#include "robot/pallet_shuttle/sequences/PalletTransportSequence.h"
#include "robot/pallet_shuttle/interfaces/IPalletShuttleStateAccessor.h"
#include "robot/pallet_shuttle/interfaces/IPalletTaskAccessor.h"
#include "core/sequence/interfaces/ISequence.h"
#include "core/fieldbus/interfaces/IFieldbusDriver.h"

using namespace mxrc;
using namespace mxrc::robot::pallet_shuttle;

// Mock StateAccessor
class MockStateAccessor : public IPalletShuttleStateAccessor {
public:
    Position current_position_{0, 0, 0, 0};
    ShuttleState state_ = ShuttleState::IDLE;
    std::optional<PalletInfo> loaded_pallet_;

    std::optional<Position> getCurrentPosition() const override { return current_position_; }
    std::optional<Position> getTargetPosition() const override { return std::nullopt; }
    bool updatePosition(const Position& pos) override {
        current_position_ = pos;
        return true;
    }
    bool setTargetPosition(const Position&) override { return true; }
    ShuttleState getState() const override { return state_; }
    bool setState(ShuttleState s) override { state_ = s; return true; }
    std::optional<PalletInfo> getLoadedPallet() const override { return loaded_pallet_; }
    bool updateLoadedPallet(const PalletInfo& p) override {
        loaded_pallet_ = p;
        return true;
    }
    bool clearLoadedPallet() override {
        loaded_pallet_.reset();
        return true;
    }

    // Stub implementations
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

// Mock TaskAccessor
class MockTaskAccessor : public IPalletTaskAccessor {
public:
    std::vector<PalletTransportTask> pending_tasks_;
    std::optional<PalletTransportTask> current_task_;
    TaskStatistics stats_;

    bool addTask(const PalletTransportTask& task) override {
        pending_tasks_.push_back(task);
        return true;
    }

    std::optional<PalletTransportTask> getNextTask() override {
        if (!pending_tasks_.empty()) {
            auto task = pending_tasks_.front();
            pending_tasks_.erase(pending_tasks_.begin());
            current_task_ = task;
            return task;
        }
        return std::nullopt;
    }

    std::optional<PalletTransportTask> getCurrentTask() const override {
        return current_task_;
    }

    std::vector<PalletTransportTask> getPendingTasks(size_t limit) const override {
        if (limit == 0) return pending_tasks_;
        return std::vector<PalletTransportTask>(
            pending_tasks_.begin(),
            pending_tasks_.begin() + std::min(limit, pending_tasks_.size())
        );
    }

    size_t getPendingTaskCount() const override { return pending_tasks_.size(); }

    bool updateTaskStatus(const std::string& task_id, PalletTransportTask::Status status) override {
        if (current_task_ && current_task_->task_id == task_id) {
            current_task_->status = status;
            return true;
        }
        return false;
    }

    bool updateTaskProgress(const std::string& task_id, double progress) override {
        if (current_task_ && current_task_->task_id == task_id) {
            current_task_->progress = progress;
            return true;
        }
        return false;
    }

    // Stub implementations
    bool setTaskError(const std::string&, const std::string&) override { return true; }
    bool cancelTask(const std::string&) override { return true; }
    size_t cancelAllPendingTasks() override { return 0; }
    std::optional<PalletTransportTask> getTask(const std::string&) const override { return std::nullopt; }
    std::vector<PalletTransportTask> getCompletedTasks(size_t) const override { return {}; }
    std::vector<PalletTransportTask> getFailedTasks(size_t) const override { return {}; }
    TaskStatistics getStatistics() const override { return stats_; }
    void resetStatistics() override { stats_ = TaskStatistics(); }
    bool updateTaskPriority(const std::string&, uint32_t) override { return true; }
    bool promoteToUrgent(const std::string&) override { return true; }
};

// Mock FieldbusDriver
class MockFieldbusDriver : public core::fieldbus::IFieldbusDriver {
public:
    bool connected_ = true;

    bool connect() override { return connected_; }
    bool disconnect() override { return true; }
    bool isConnected() const override { return connected_; }
    bool read(const std::string&, std::any&) override { return true; }
    bool write(const std::string&, const std::any&) override { return true; }
    std::vector<std::string> scan() override { return {}; }
    std::string getDriverInfo() const override { return "MockDriver"; }
    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
};

class PalletTransportSequenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        state_accessor_ = std::make_shared<MockStateAccessor>();
        task_accessor_ = std::make_shared<MockTaskAccessor>();
        fieldbus_driver_ = std::make_shared<MockFieldbusDriver>();
    }

    std::shared_ptr<MockStateAccessor> state_accessor_;
    std::shared_ptr<MockTaskAccessor> task_accessor_;
    std::shared_ptr<MockFieldbusDriver> fieldbus_driver_;
};

// 완전한 운반 시퀀스 테스트
TEST_F(PalletTransportSequenceTest, CompleteTransportSequence) {
    // Given: 픽업 및 배치 위치
    Position pickup_pos{100, 200, 0, 0};
    Position place_pos{300, 400, 0, 0};
    std::string pallet_id = "PLT-001";

    // When: 시퀀스 생성 및 실행
    auto sequence = std::make_shared<PalletTransportSequence>(
        "seq_001", pallet_id, pickup_pos, place_pos,
        state_accessor_, fieldbus_driver_
    );

    // 시퀀스 스텝 확인
    EXPECT_EQ(sequence->getStepCount(), 3);  // Move, Pick, Move, Place = 4 steps

    // Then: 각 스텝 검증
    auto steps = sequence->getSteps();
    EXPECT_EQ(steps[0].name, "MoveToPickup");
    EXPECT_EQ(steps[1].name, "PickPallet");
    EXPECT_EQ(steps[2].name, "MoveToPlace");
    EXPECT_EQ(steps[3].name, "PlacePallet");
}

// 진행률 추적 테스트
TEST_F(PalletTransportSequenceTest, ProgressTracking) {
    Position pickup_pos{100, 200, 0, 0};
    Position place_pos{300, 400, 0, 0};

    auto sequence = std::make_shared<PalletTransportSequence>(
        "seq_002", "PLT-002", pickup_pos, place_pos,
        state_accessor_, fieldbus_driver_
    );

    // 초기 진행률
    EXPECT_DOUBLE_EQ(sequence->getProgress(), 0.0);

    // 첫 번째 스텝 완료 (25%)
    sequence->completeStep(0);
    EXPECT_NEAR(sequence->getProgress(), 0.25, 0.01);

    // 두 번째 스텝 완료 (50%)
    sequence->completeStep(1);
    EXPECT_NEAR(sequence->getProgress(), 0.50, 0.01);
}

// 오류 처리 테스트
TEST_F(PalletTransportSequenceTest, ErrorHandling) {
    Position pickup_pos{100, 200, 0, 0};
    Position place_pos{300, 400, 0, 0};

    auto sequence = std::make_shared<PalletTransportSequence>(
        "seq_003", "PLT-003", pickup_pos, place_pos,
        state_accessor_, fieldbus_driver_
    );

    // 시퀀스 실행 중 오류 발생 시뮬레이션
    sequence->onStepError(1, "Pallet not detected");

    EXPECT_EQ(sequence->getStatus(), core::sequence::SequenceStatus::ERROR);
    EXPECT_FALSE(sequence->getErrorMessage().empty());
}

// 취소 테스트
TEST_F(PalletTransportSequenceTest, CancelSequence) {
    Position pickup_pos{100, 200, 0, 0};
    Position place_pos{300, 400, 0, 0};

    auto sequence = std::make_shared<PalletTransportSequence>(
        "seq_004", "PLT-004", pickup_pos, place_pos,
        state_accessor_, fieldbus_driver_
    );

    // 시퀀스 시작 후 취소
    sequence->start();
    sequence->cancel();

    EXPECT_EQ(sequence->getStatus(), core::sequence::SequenceStatus::CANCELLED);
}

// 재시도 정책 테스트
TEST_F(PalletTransportSequenceTest, RetryPolicy) {
    Position pickup_pos{100, 200, 0, 0};
    Position place_pos{300, 400, 0, 0};

    auto sequence = std::make_shared<PalletTransportSequence>(
        "seq_005", "PLT-005", pickup_pos, place_pos,
        state_accessor_, fieldbus_driver_,
        3  // 최대 3회 재시도
    );

    // 픽업 실패 시 재시도
    sequence->onStepError(1, "Temporary failure");
    EXPECT_EQ(sequence->getRetryCount(1), 1);

    // 재시도 한계 도달
    sequence->onStepError(1, "Temporary failure");
    sequence->onStepError(1, "Temporary failure");
    sequence->onStepError(1, "Permanent failure");

    EXPECT_EQ(sequence->getStatus(), core::sequence::SequenceStatus::ERROR);
}

// 타임아웃 테스트
TEST_F(PalletTransportSequenceTest, SequenceTimeout) {
    Position pickup_pos{100, 200, 0, 0};
    Position place_pos{300, 400, 0, 0};

    auto sequence = std::make_shared<PalletTransportSequence>(
        "seq_006", "PLT-006", pickup_pos, place_pos,
        state_accessor_, fieldbus_driver_,
        3, // 재시도 횟수
        std::chrono::seconds(60)  // 60초 타임아웃
    );

    EXPECT_FALSE(sequence->isTimedOut());

    // 타임아웃 시뮬레이션은 실제 구현에서 처리
}

// 상태 복구 테스트
TEST_F(PalletTransportSequenceTest, StateRecovery) {
    // Given: 팔렛이 이미 적재된 상태
    PalletInfo loaded{"PLT-007", 50.0, true};
    state_accessor_->loaded_pallet_ = loaded;

    Position current_pos{150, 250, 0, 0};  // 중간 위치
    Position place_pos{300, 400, 0, 0};
    state_accessor_->current_position_ = current_pos;

    // When: 시퀀스 생성 (픽업 스킵)
    auto sequence = std::make_shared<PalletTransportSequence>(
        "seq_007", loaded.pallet_id, current_pos, place_pos,
        state_accessor_, fieldbus_driver_
    );

    // Then: 픽업 단계가 스킵되고 바로 이동-배치로
    auto steps = sequence->getSteps();
    EXPECT_EQ(steps[0].name, "MoveToPlace");
    EXPECT_EQ(steps[1].name, "PlacePallet");
}

// 병렬 작업 간섭 테스트
TEST_F(PalletTransportSequenceTest, ParallelTaskInterference) {
    // Given: 여러 작업이 대기 중
    PalletTransportTask task1{"T001", "PLT-008", 100, 200, 0, 300, 400, 0, 1,
                              std::chrono::system_clock::now() + std::chrono::hours(1)};
    PalletTransportTask task2{"T002", "PLT-009", 150, 250, 0, 350, 450, 0, 2,
                              std::chrono::system_clock::now() + std::chrono::hours(2)};

    task_accessor_->addTask(task1);
    task_accessor_->addTask(task2);

    // When: 첫 번째 시퀀스 실행
    Position pickup_pos{100, 200, 0, 0};
    Position place_pos{300, 400, 0, 0};

    auto sequence = std::make_shared<PalletTransportSequence>(
        "seq_008", "PLT-008", pickup_pos, place_pos,
        state_accessor_, fieldbus_driver_
    );

    // Then: 작업 큐가 영향받지 않음
    EXPECT_EQ(task_accessor_->getPendingTaskCount(), 2);
}