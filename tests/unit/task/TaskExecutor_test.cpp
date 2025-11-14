#include "gtest/gtest.h"
#include "core/taskmanager/TaskExecutor.h"
#include "core/taskmanager/interfaces/ITask.h"
#include <memory>
#include <chrono>
#include <thread>

namespace mxrc::core::taskmanager {

// Mock ITask for testing TaskExecutor
class MockExecutableTask : public ITask {
public:
    MockExecutableTask(const std::string& id, const std::string& name)
        : id_(id), name_(name), status_(TaskStatus::PENDING), progress_(0.0f) {}

    void execute() override {
        status_ = TaskStatus::RUNNING;
        // Simulate some work
        for (int i = 0; i <= 10; ++i) {
            if (status_ == TaskStatus::CANCELLING) {
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
            status_ = TaskStatus::CANCELLING;
        }
    }

    TaskStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    const std::string& getId() const override { return id_; }
    TaskDto toDto() const override { return TaskDto{id_, name_, status_, progress_}; }

private:
    std::string id_;
    std::string name_;
    TaskStatus status_;
    float progress_;
};

TEST(TaskExecutorTest, SubmitAndExecuteTask) {
    TaskExecutor executor;
    auto task = std::make_shared<MockExecutableTask>("task_1", "TestTask");

    executor.submit(task);

    // Give some time for the task to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);
    ASSERT_EQ(task->getProgress(), 1.0f);
}

TEST(TaskExecutorTest, CancelRunningTask) {
    TaskExecutor executor;
    auto task = std::make_shared<MockExecutableTask>("task_2", "CancellableTask");

    executor.submit(task);

    // Give some time for the task to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_EQ(task->getStatus(), TaskStatus::RUNNING);

    executor.cancel("task_2");

    // Give some time for the task to be cancelled
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ASSERT_EQ(task->getStatus(), TaskStatus::CANCELLED);
}

TEST(TaskExecutorTest, CancelNonExistentTask) {
    TaskExecutor executor;
    auto task = std::make_shared<MockExecutableTask>("task_3", "AnotherTask");

    executor.submit(task);
    
    // Attempt to cancel a task that doesn't exist
    executor.cancel("non_existent_task");

    // The original task should still complete
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);
}

} // namespace mxrc::core::taskmanager
