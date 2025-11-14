// TaskExecutor 클래스 테스트.
// ITask 구현 태스크의 비동기 실행 및 관리 기능 검증.
// 태스크 제출, 실행, 취소, 진행률 추적, 동시 실행 등 핵심 기능 테스트.

#include "gtest/gtest.h"
#include "core/taskmanager/TaskExecutor.h"
#include "core/taskmanager/interfaces/ITask.h"
#include <memory>
#include <chrono>
#include <thread>

namespace mxrc::core::taskmanager {

// TaskExecutor 테스트용 Mock ITask
class MockExecutableTask : public ITask {
public:
    MockExecutableTask(const std::string& id, const std::string& name)
        : id_(id), name_(name), status_(TaskStatus::PENDING), progress_(0.0f), should_cancel_(false) {}

    void execute() override {
        status_ = TaskStatus::RUNNING;
        // 작업 시뮬레이션
        for (int i = 0; i <= 10; ++i) {
            if (should_cancel_) {
                status_ = TaskStatus::CANCELLED;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            progress_ = static_cast<float>(i) / 10.0f;
        }
        status_ = TaskStatus::COMPLETED;
    }

    void cancel() override {
        if (status_ == TaskStatus::RUNNING || status_ == TaskStatus::PENDING) {
            should_cancel_ = true;
        }
    }

    void pause() override {}

    std::string getType() const override { return name_; }
    std::map<std::string, std::string> getParameters() const override { return {}; }

    TaskStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    const std::string& getId() const override { return id_; }
    TaskDto toDto() const override { return TaskDto{id_, name_, status_, progress_}; }

private:
    std::string id_;
    std::string name_;
    TaskStatus status_;
    float progress_;
    bool should_cancel_;
};

// 태스크 제출 및 정상 실행 테스트
TEST(TaskExecutorTest, SubmitAndExecuteTask) {
    TaskExecutor executor;
    auto task = std::make_shared<MockExecutableTask>("task_1", "TestTask");

    executor.submit(task);

    // 태스크 실행 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);
    ASSERT_EQ(task->getProgress(), 1.0f);
}

// 실행 중인 태스크 취소 테스트
TEST(TaskExecutorTest, CancelRunningTask) {
    TaskExecutor executor;
    auto task = std::make_shared<MockExecutableTask>("task_2", "CancellableTask");

    executor.submit(task);

    // 태스크 시작 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_EQ(task->getStatus(), TaskStatus::RUNNING);

    executor.cancel("task_2");

    // 태스크 취소 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ASSERT_EQ(task->getStatus(), TaskStatus::CANCELLED);
}

// 존재하지 않는 태스크 취소 시도 테스트
TEST(TaskExecutorTest, CancelNonExistentTask) {
    TaskExecutor executor;
    auto task = std::make_shared<MockExecutableTask>("task_3", "AnotherTask");

    executor.submit(task);

    // 존재하지 않는 태스크 취소 시도
    executor.cancel("non_existent_task");

    // 기존 태스크는 완료되어야 함
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);
}

// ID로 태스크 조회 테스트
TEST(TaskExecutorTest, GetTask) {
    TaskExecutor executor;
    auto task = std::make_shared<MockExecutableTask>("task_4", "RetrievableTask");

    executor.submit(task);

    // ID로 태스크 검색
    auto retrievedTask = executor.getTask("task_4");
    ASSERT_NE(retrievedTask, nullptr);
    ASSERT_EQ(retrievedTask->getId(), "task_4");

    // 존재하지 않는 태스크 검색 시도
    auto nonExistent = executor.getTask("non_existent");
    ASSERT_EQ(nonExistent, nullptr);
}

// 여러 태스크 동시 실행 테스트
TEST(TaskExecutorTest, MultipleTasksExecution) {
    TaskExecutor executor;

    // 여러 태스크 제출
    auto task1 = std::make_shared<MockExecutableTask>("multi_task_1", "Task1");
    auto task2 = std::make_shared<MockExecutableTask>("multi_task_2", "Task2");
    auto task3 = std::make_shared<MockExecutableTask>("multi_task_3", "Task3");

    executor.submit(task1);
    executor.submit(task2);
    executor.submit(task3);

    // 모든 태스크 완료 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 모든 태스크 완료 확인
    ASSERT_EQ(task1->getStatus(), TaskStatus::COMPLETED);
    ASSERT_EQ(task2->getStatus(), TaskStatus::COMPLETED);
    ASSERT_EQ(task3->getStatus(), TaskStatus::COMPLETED);
}

// 여러 태스크 동시 취소 테스트
TEST(TaskExecutorTest, CancelMultipleTasks) {
    TaskExecutor executor;

    auto task1 = std::make_shared<MockExecutableTask>("cancel_multi_1", "Task1");
    auto task2 = std::make_shared<MockExecutableTask>("cancel_multi_2", "Task2");

    executor.submit(task1);
    executor.submit(task2);

    // 태스크 시작 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 두 태스크 모두 취소
    executor.cancel("cancel_multi_1");
    executor.cancel("cancel_multi_2");

    // 취소 완료 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 두 태스크 모두 취소 확인
    ASSERT_EQ(task1->getStatus(), TaskStatus::CANCELLED);
    ASSERT_EQ(task2->getStatus(), TaskStatus::CANCELLED);
}

// 태스크 진행률 추적 테스트
TEST(TaskExecutorTest, TaskProgressTracking) {
    TaskExecutor executor;
    auto task = std::make_shared<MockExecutableTask>("progress_task", "ProgressTask");

    executor.submit(task);

    // 다른 간격으로 진행률 확인
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    float midProgress = task->getProgress();
    ASSERT_GT(midProgress, 0.0f); // 진행률 0.0f 초과 확인

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);
    ASSERT_EQ(task->getProgress(), 1.0f); // 진행률 1.0f 확인
}

// 많은 태스크 동시 실행 스트레스 테스트
TEST(TaskExecutorTest, ManyTasksStress) {
    TaskExecutor executor;
    const int NUM_TASKS = 50;

    std::vector<std::shared_ptr<MockExecutableTask>> tasks;

    // 많은 수의 태스크 제출
    for (int i = 0; i < NUM_TASKS; ++i) {
        auto task = std::make_shared<MockExecutableTask>("stress_task_" + std::to_string(i), "StressTask");
        tasks.push_back(task);
        executor.submit(task);
    }

    // 모든 태스크 완료 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 모든 태스크 완료 확인
    int completedCount = 0;
    for (const auto& task : tasks) {
        if (task->getStatus() == TaskStatus::COMPLETED) {
            completedCount++;
        }
    }

    ASSERT_EQ(completedCount, NUM_TASKS);
}

// 완료된 태스크 조회 테스트
TEST(TaskExecutorTest, RetrieveTaskAfterCompletion) {
    TaskExecutor executor;
    auto task = std::make_shared<MockExecutableTask>("retrieve_after_completion", "RetrievableTask");

    executor.submit(task);

    // 완료 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);

    // 완료된 태스크 검색
    auto retrieved = executor.getTask("retrieve_after_completion");
    ASSERT_NE(retrieved, nullptr);
    ASSERT_EQ(retrieved->getStatus(), TaskStatus::COMPLETED);
}

// 존재하지 않는 태스크 조회 테스트
TEST(TaskExecutorTest, GetNonExistentTask) {
    TaskExecutor executor;

    // 제출되지 않은 태스크 검색 시도
    auto retrieved = executor.getTask("never_submitted");
    ASSERT_EQ(retrieved, nullptr);
}

// 이미 취소된 태스크 재취소 시도 테스트
TEST(TaskExecutorTest, CancelAlreadyCancelledTask) {
    TaskExecutor executor;
    auto task = std::make_shared<MockExecutableTask>("double_cancel", "DoubleCancelTask");

    executor.submit(task);

    // 실행 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 한 번 취소
    executor.cancel("double_cancel");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_EQ(task->getStatus(), TaskStatus::CANCELLED);

    // 재취소 시도 (예외 미발생)
    ASSERT_NO_THROW(executor.cancel("double_cancel"));
}

} // namespace mxrc::core::taskmanager
