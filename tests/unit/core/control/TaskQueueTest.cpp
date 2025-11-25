// TaskQueueTest.cpp - TaskQueue 다중 작업 처리 단위 테스트
// Copyright (C) 2025 MXRC Project
//
// Feature 016: Pallet Shuttle Control System (T072-T073)
// Phase 6: Multi-pallet handling

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include "core/control/impl/TaskQueue.h"
#include "core/control/dto/BehaviorRequest.h"
#include "core/control/dto/Priority.h"

using namespace mxrc::core::control;

class TaskQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        queue_ = std::make_shared<TaskQueue>();
    }

    std::shared_ptr<TaskQueue> queue_;
};

// T072: 다중 작업 처리 테스트
TEST_F(TaskQueueTest, MultipleTaskHandling) {
    // Given: 5개의 작업을 다른 우선순위로 추가
    std::vector<BehaviorRequest> requests = {
        {"task1", Priority::NORMAL, "transport", nullptr},
        {"task2", Priority::HIGH, "urgent_transport", nullptr},
        {"task3", Priority::NORMAL, "transport", nullptr},
        {"task4", Priority::REALTIME, "emergency", nullptr},
        {"task5", Priority::LOW, "maintenance", nullptr}
    };

    // When: 작업들을 큐에 추가
    for (const auto& req : requests) {
        queue_->addRequest(req);
    }

    // Then: 우선순위 순서로 작업이 반환됨
    EXPECT_EQ(queue_->size(), 5);

    auto first = queue_->getNextRequest();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->behavior_id, "task4"); // REALTIME

    auto second = queue_->getNextRequest();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->behavior_id, "task2"); // HIGH

    auto third = queue_->getNextRequest();
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(third->behavior_id, "task1"); // NORMAL (FIFO)

    auto fourth = queue_->getNextRequest();
    ASSERT_TRUE(fourth.has_value());
    EXPECT_EQ(fourth->behavior_id, "task3"); // NORMAL (FIFO)

    auto fifth = queue_->getNextRequest();
    ASSERT_TRUE(fifth.has_value());
    EXPECT_EQ(fifth->behavior_id, "task5"); // LOW

    EXPECT_EQ(queue_->size(), 0);
}

// T073: 긴급 작업 삽입 테스트
TEST_F(TaskQueueTest, UrgentTaskInsertion) {
    // Given: 일반 작업들이 이미 큐에 있음
    queue_->addRequest({"normal1", Priority::NORMAL, "transport", nullptr});
    queue_->addRequest({"normal2", Priority::NORMAL, "transport", nullptr});
    queue_->addRequest({"low1", Priority::LOW, "maintenance", nullptr});

    // When: 긴급 작업 삽입
    queue_->addRequest({"urgent1", Priority::REALTIME, "emergency_stop", nullptr});
    queue_->addRequest({"high1", Priority::HIGH, "urgent_transport", nullptr});

    // Then: 긴급 작업이 먼저 처리됨
    auto first = queue_->getNextRequest();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->behavior_id, "urgent1");
    EXPECT_EQ(first->priority, Priority::REALTIME);

    auto second = queue_->getNextRequest();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->behavior_id, "high1");
    EXPECT_EQ(second->priority, Priority::HIGH);
}

// 작업 취소 테스트
TEST_F(TaskQueueTest, CancelTask) {
    // Given: 여러 작업 추가
    queue_->addRequest({"task1", Priority::NORMAL, "transport", nullptr});
    queue_->addRequest({"task2", Priority::HIGH, "transport", nullptr});
    queue_->addRequest({"task3", Priority::NORMAL, "transport", nullptr});

    // When: 특정 작업 취소
    bool canceled = queue_->cancelRequest("task2");

    // Then: 취소 성공 및 큐에서 제거됨
    EXPECT_TRUE(canceled);
    EXPECT_EQ(queue_->size(), 2);

    auto first = queue_->getNextRequest();
    EXPECT_EQ(first->behavior_id, "task1");

    auto second = queue_->getNextRequest();
    EXPECT_EQ(second->behavior_id, "task3");
}

// 동일 우선순위 FIFO 처리 테스트
TEST_F(TaskQueueTest, SamePriorityFIFO) {
    // Given: 동일한 우선순위의 작업들
    queue_->addRequest({"task1", Priority::NORMAL, "transport", nullptr});
    queue_->addRequest({"task2", Priority::NORMAL, "transport", nullptr});
    queue_->addRequest({"task3", Priority::NORMAL, "transport", nullptr});

    // Then: FIFO 순서로 처리
    EXPECT_EQ(queue_->getNextRequest()->behavior_id, "task1");
    EXPECT_EQ(queue_->getNextRequest()->behavior_id, "task2");
    EXPECT_EQ(queue_->getNextRequest()->behavior_id, "task3");
}

// 큐 초기화 테스트
TEST_F(TaskQueueTest, ClearQueue) {
    // Given: 작업들이 큐에 있음
    queue_->addRequest({"task1", Priority::NORMAL, "transport", nullptr});
    queue_->addRequest({"task2", Priority::HIGH, "transport", nullptr});

    // When: 큐 초기화
    queue_->clear();

    // Then: 큐가 비어있음
    EXPECT_EQ(queue_->size(), 0);
    EXPECT_FALSE(queue_->getNextRequest().has_value());
}

// Peek 기능 테스트 (작업을 제거하지 않고 확인)
TEST_F(TaskQueueTest, PeekNextTask) {
    // Given: 작업 추가
    queue_->addRequest({"task1", Priority::HIGH, "transport", nullptr});
    queue_->addRequest({"task2", Priority::NORMAL, "transport", nullptr});

    // When: Peek
    auto peeked = queue_->peekNextRequest();

    // Then: 작업이 제거되지 않음
    ASSERT_TRUE(peeked.has_value());
    EXPECT_EQ(peeked->behavior_id, "task1");
    EXPECT_EQ(queue_->size(), 2); // 여전히 2개

    // 실제로 가져오면 제거됨
    auto actual = queue_->getNextRequest();
    EXPECT_EQ(actual->behavior_id, "task1");
    EXPECT_EQ(queue_->size(), 1);
}