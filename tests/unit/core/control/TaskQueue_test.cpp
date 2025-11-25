#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include "core/control/impl/TaskQueue.h"

using namespace mxrc::core::control;
using namespace std::chrono_literals;

/**
 * @brief TaskQueue 단위 테스트
 *
 * Feature 016: Pallet Shuttle Control System
 * Phase 4: User Story 5 - 행동 의사 결정
 *
 * 테스트 범위:
 * - T046: 우선순위 정렬 (Priority 기반 dequeue)
 * - FIFO 보장 (동일 우선순위 내)
 * - Thread-safety
 */

// Mock Task
class MockTask : public mxrc::core::task::ITask {
public:
    explicit MockTask(std::string id) : id_(std::move(id)), status_(mxrc::core::task::TaskStatus::IDLE) {}

    std::string getId() const override { return id_; }
    std::string start() override { return id_; }
    void stop() override {}
    void pause() override {}
    void resume() override {}
    mxrc::core::task::TaskStatus getStatus() const override { return status_; }
    float getProgress() const override { return 0.0f; }
    const mxrc::core::task::TaskDefinition& getDefinition() const override {
        static mxrc::core::task::TaskDefinition def("mock_task", "Mock Task");
        return def;
    }

private:
    std::string id_;
    mxrc::core::task::TaskStatus status_;
};

class TaskQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        queue_ = std::make_unique<TaskQueue>();
    }

    std::unique_ptr<TaskQueue> queue_;
};

// T046-1: 빈 큐 확인
TEST_F(TaskQueueTest, EmptyQueue) {
    EXPECT_TRUE(queue_->isEmpty());
    EXPECT_EQ(queue_->size(), 0);

    auto task = queue_->dequeue();
    EXPECT_FALSE(task.has_value());
}

// T046-2: Enqueue 및 Dequeue 기본 동작
TEST_F(TaskQueueTest, EnqueueDequeue) {
    auto task1 = std::make_shared<MockTask>("task1");

    bool result = queue_->enqueue(task1, Priority::NORMAL_TASK);
    EXPECT_TRUE(result);
    EXPECT_FALSE(queue_->isEmpty());
    EXPECT_EQ(queue_->size(), 1);

    auto dequeued = queue_->dequeue();
    ASSERT_TRUE(dequeued.has_value());
    EXPECT_EQ((*dequeued)->getId(), "task1");

    EXPECT_TRUE(queue_->isEmpty());
}

// T046-3: 우선순위 정렬 - EMERGENCY_STOP > NORMAL_TASK
TEST_F(TaskQueueTest, PrioritySorting_EmergencyFirst) {
    auto normal_task = std::make_shared<MockTask>("normal");
    auto emergency_task = std::make_shared<MockTask>("emergency");

    // 낮은 우선순위 먼저 enqueue
    queue_->enqueue(normal_task, Priority::NORMAL_TASK);
    std::this_thread::sleep_for(1ms);
    queue_->enqueue(emergency_task, Priority::EMERGENCY_STOP);

    // dequeue 시 EMERGENCY_STOP이 먼저 나와야 함
    auto task1 = queue_->dequeue();
    ASSERT_TRUE(task1.has_value());
    EXPECT_EQ((*task1)->getId(), "emergency");

    auto task2 = queue_->dequeue();
    ASSERT_TRUE(task2.has_value());
    EXPECT_EQ((*task2)->getId(), "normal");
}

// T046-4: 우선순위 정렬 - 5단계 모두 테스트
TEST_F(TaskQueueTest, PrioritySorting_AllLevels) {
    auto emergency = std::make_shared<MockTask>("emergency");
    auto safety = std::make_shared<MockTask>("safety");
    auto urgent = std::make_shared<MockTask>("urgent");
    auto normal = std::make_shared<MockTask>("normal");
    auto maintenance = std::make_shared<MockTask>("maintenance");

    // 역순으로 enqueue
    queue_->enqueue(maintenance, Priority::MAINTENANCE);
    std::this_thread::sleep_for(1ms);
    queue_->enqueue(normal, Priority::NORMAL_TASK);
    std::this_thread::sleep_for(1ms);
    queue_->enqueue(urgent, Priority::URGENT_TASK);
    std::this_thread::sleep_for(1ms);
    queue_->enqueue(safety, Priority::SAFETY_ISSUE);
    std::this_thread::sleep_for(1ms);
    queue_->enqueue(emergency, Priority::EMERGENCY_STOP);

    EXPECT_EQ(queue_->size(), 5);

    // Dequeue 순서 확인
    EXPECT_EQ((*queue_->dequeue())->getId(), "emergency");
    EXPECT_EQ((*queue_->dequeue())->getId(), "safety");
    EXPECT_EQ((*queue_->dequeue())->getId(), "urgent");
    EXPECT_EQ((*queue_->dequeue())->getId(), "normal");
    EXPECT_EQ((*queue_->dequeue())->getId(), "maintenance");
}

// T046-5: 동일 우선순위 내 FIFO 보장
TEST_F(TaskQueueTest, FIFO_WithinSamePriority) {
    auto task1 = std::make_shared<MockTask>("task1");
    auto task2 = std::make_shared<MockTask>("task2");
    auto task3 = std::make_shared<MockTask>("task3");

    queue_->enqueue(task1, Priority::NORMAL_TASK);
    std::this_thread::sleep_for(1ms);
    queue_->enqueue(task2, Priority::NORMAL_TASK);
    std::this_thread::sleep_for(1ms);
    queue_->enqueue(task3, Priority::NORMAL_TASK);

    // FIFO 순서 확인
    EXPECT_EQ((*queue_->dequeue())->getId(), "task1");
    EXPECT_EQ((*queue_->dequeue())->getId(), "task2");
    EXPECT_EQ((*queue_->dequeue())->getId(), "task3");
}

// T046-6: 중복 enqueue 방지
TEST_F(TaskQueueTest, PreventDuplicateEnqueue) {
    auto task = std::make_shared<MockTask>("task1");

    bool result1 = queue_->enqueue(task, Priority::NORMAL_TASK);
    EXPECT_TRUE(result1);
    EXPECT_EQ(queue_->size(), 1);

    // 같은 task를 다시 enqueue 시도
    bool result2 = queue_->enqueue(task, Priority::NORMAL_TASK);
    EXPECT_FALSE(result2);
    EXPECT_EQ(queue_->size(), 1);
}

// T046-7: Remove task by ID
TEST_F(TaskQueueTest, RemoveTaskById) {
    auto task1 = std::make_shared<MockTask>("task1");
    auto task2 = std::make_shared<MockTask>("task2");
    auto task3 = std::make_shared<MockTask>("task3");

    queue_->enqueue(task1, Priority::NORMAL_TASK);
    std::this_thread::sleep_for(1ms);
    queue_->enqueue(task2, Priority::NORMAL_TASK);
    std::this_thread::sleep_for(1ms);
    queue_->enqueue(task3, Priority::NORMAL_TASK);

    EXPECT_EQ(queue_->size(), 3);

    // task2 제거
    bool removed = queue_->remove("task2");
    EXPECT_TRUE(removed);
    EXPECT_EQ(queue_->size(), 2);

    // Dequeue 순서 확인 (task2 제외)
    EXPECT_EQ((*queue_->dequeue())->getId(), "task1");
    EXPECT_EQ((*queue_->dequeue())->getId(), "task3");
}

// T046-8: Remove non-existent task
TEST_F(TaskQueueTest, RemoveNonExistentTask) {
    auto task = std::make_shared<MockTask>("task1");
    queue_->enqueue(task, Priority::NORMAL_TASK);

    bool removed = queue_->remove("non_existent");
    EXPECT_FALSE(removed);
    EXPECT_EQ(queue_->size(), 1);
}

// T046-9: Clear all tasks
TEST_F(TaskQueueTest, ClearAllTasks) {
    auto task1 = std::make_shared<MockTask>("task1");
    auto task2 = std::make_shared<MockTask>("task2");
    auto task3 = std::make_shared<MockTask>("task3");

    queue_->enqueue(task1, Priority::EMERGENCY_STOP);
    std::this_thread::sleep_for(1ms);
    queue_->enqueue(task2, Priority::NORMAL_TASK);
    std::this_thread::sleep_for(1ms);
    queue_->enqueue(task3, Priority::MAINTENANCE);

    EXPECT_EQ(queue_->size(), 3);

    queue_->clear();

    EXPECT_TRUE(queue_->isEmpty());
    EXPECT_EQ(queue_->size(), 0);
}

// T046-10: Thread-safety - Concurrent enqueue
TEST_F(TaskQueueTest, ThreadSafety_ConcurrentEnqueue) {
    const int thread_count = 10;
    const int tasks_per_thread = 5;

    std::vector<std::thread> threads;

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([this, i, tasks_per_thread]() {
            for (int j = 0; j < tasks_per_thread; ++j) {
                auto task = std::make_shared<MockTask>(
                    "task_" + std::to_string(i) + "_" + std::to_string(j));
                queue_->enqueue(task, Priority::NORMAL_TASK);
                std::this_thread::sleep_for(1ms);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(queue_->size(), thread_count * tasks_per_thread);
}
