// TaskMonitor 단위 테스트
// Phase 3C: Task 모니터링 기능 테스트

#include "gtest/gtest.h"
#include "core/task/core/TaskMonitor.h"
#include <thread>
#include <chrono>

namespace mxrc::core::task {

/**
 * @brief TaskMonitor 테스트
 */
class TaskMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        monitor = std::make_unique<TaskMonitor>();
    }

    void TearDown() override {
        monitor->clear();
        monitor.reset();
    }

    std::unique_ptr<TaskMonitor> monitor;
};

/**
 * @brief Task 시작 및 조회 테스트
 */
TEST_F(TaskMonitorTest, StartTaskAndGetInfo) {
    monitor->startTask("task1");

    auto info = monitor->getTaskInfo("task1");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->taskId, "task1");
    EXPECT_EQ(info->status, TaskStatus::RUNNING);
    EXPECT_FLOAT_EQ(info->progress, 0.0f);
    EXPECT_EQ(info->retryCount, 0);
}

/**
 * @brief 진행률 업데이트 테스트
 */
TEST_F(TaskMonitorTest, UpdateProgress) {
    monitor->startTask("task1");

    monitor->updateProgress("task1", 0.5f);

    auto info = monitor->getTaskInfo("task1");
    ASSERT_TRUE(info.has_value());
    EXPECT_FLOAT_EQ(info->progress, 0.5f);

    monitor->updateProgress("task1", 1.0f);

    info = monitor->getTaskInfo("task1");
    ASSERT_TRUE(info.has_value());
    EXPECT_FLOAT_EQ(info->progress, 1.0f);
}

/**
 * @brief 상태 업데이트 테스트
 */
TEST_F(TaskMonitorTest, UpdateStatus) {
    monitor->startTask("task1");

    monitor->updateStatus("task1", TaskStatus::PAUSED);

    auto info = monitor->getTaskInfo("task1");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->status, TaskStatus::PAUSED);
}

/**
 * @brief Task 종료 테스트
 */
TEST_F(TaskMonitorTest, EndTask) {
    monitor->startTask("task1");
    monitor->updateProgress("task1", 0.8f);

    monitor->endTask("task1", TaskStatus::COMPLETED);

    auto info = monitor->getTaskInfo("task1");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->status, TaskStatus::COMPLETED);
    EXPECT_FLOAT_EQ(info->progress, 1.0f);  // 완료 시 progress는 1.0
}

/**
 * @brief Task 실패 테스트
 */
TEST_F(TaskMonitorTest, EndTaskWithFailure) {
    monitor->startTask("task1");

    monitor->endTask("task1", TaskStatus::FAILED, "Test error");

    auto info = monitor->getTaskInfo("task1");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->status, TaskStatus::FAILED);
    EXPECT_EQ(info->errorMessage, "Test error");
}

/**
 * @brief 재시도 횟수 테스트
 */
TEST_F(TaskMonitorTest, IncrementRetryCount) {
    monitor->startTask("task1");

    monitor->incrementRetryCount("task1");
    auto info = monitor->getTaskInfo("task1");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->retryCount, 1);

    monitor->incrementRetryCount("task1");
    info = monitor->getTaskInfo("task1");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->retryCount, 2);
}

/**
 * @brief 실행 중인 Task 수 조회 테스트
 */
TEST_F(TaskMonitorTest, GetRunningTaskCount) {
    monitor->startTask("task1");
    monitor->startTask("task2");
    monitor->startTask("task3");

    EXPECT_EQ(monitor->getRunningTaskCount(), 3);

    monitor->endTask("task1", TaskStatus::COMPLETED);

    EXPECT_EQ(monitor->getRunningTaskCount(), 2);
}

/**
 * @brief 완료된 Task 수 조회 테스트
 */
TEST_F(TaskMonitorTest, GetCompletedTaskCount) {
    monitor->startTask("task1");
    monitor->startTask("task2");
    monitor->startTask("task3");

    monitor->endTask("task1", TaskStatus::COMPLETED);
    monitor->endTask("task2", TaskStatus::COMPLETED);

    EXPECT_EQ(monitor->getCompletedTaskCount(), 2);
}

/**
 * @brief 실패한 Task 수 조회 테스트
 */
TEST_F(TaskMonitorTest, GetFailedTaskCount) {
    monitor->startTask("task1");
    monitor->startTask("task2");
    monitor->startTask("task3");

    monitor->endTask("task1", TaskStatus::FAILED, "Error 1");
    monitor->endTask("task2", TaskStatus::FAILED, "Timeout");

    EXPECT_EQ(monitor->getFailedTaskCount(), 2);
}

/**
 * @brief 실행 시간 측정 테스트
 */
TEST_F(TaskMonitorTest, ElapsedTimeTracking) {
    monitor->startTask("task1");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto info = monitor->getTaskInfo("task1");
    ASSERT_TRUE(info.has_value());
    EXPECT_GE(info->getElapsedMs(), 100);
    EXPECT_LE(info->getElapsedMs(), 150);

    monitor->endTask("task1", TaskStatus::COMPLETED);

    info = monitor->getTaskInfo("task1");
    ASSERT_TRUE(info.has_value());
    EXPECT_GE(info->getElapsedMs(), 100);
}

/**
 * @brief 여러 Task 동시 추적 테스트
 */
TEST_F(TaskMonitorTest, MultipleTasksTracking) {
    monitor->startTask("task1");
    monitor->startTask("task2");
    monitor->startTask("task3");

    monitor->updateProgress("task1", 0.3f);
    monitor->updateProgress("task2", 0.5f);
    monitor->updateProgress("task3", 0.7f);

    auto info1 = monitor->getTaskInfo("task1");
    auto info2 = monitor->getTaskInfo("task2");
    auto info3 = monitor->getTaskInfo("task3");

    ASSERT_TRUE(info1.has_value());
    ASSERT_TRUE(info2.has_value());
    ASSERT_TRUE(info3.has_value());

    EXPECT_FLOAT_EQ(info1->progress, 0.3f);
    EXPECT_FLOAT_EQ(info2->progress, 0.5f);
    EXPECT_FLOAT_EQ(info3->progress, 0.7f);

    EXPECT_EQ(monitor->getRunningTaskCount(), 3);
}

/**
 * @brief Task 제거 테스트
 */
TEST_F(TaskMonitorTest, RemoveTask) {
    monitor->startTask("task1");
    monitor->startTask("task2");

    EXPECT_TRUE(monitor->getTaskInfo("task1").has_value());

    monitor->removeTask("task1");

    EXPECT_FALSE(monitor->getTaskInfo("task1").has_value());
    EXPECT_TRUE(monitor->getTaskInfo("task2").has_value());
}

/**
 * @brief 전체 초기화 테스트
 */
TEST_F(TaskMonitorTest, ClearAllTasks) {
    monitor->startTask("task1");
    monitor->startTask("task2");
    monitor->startTask("task3");

    EXPECT_EQ(monitor->getRunningTaskCount(), 3);

    monitor->clear();

    EXPECT_EQ(monitor->getRunningTaskCount(), 0);
    EXPECT_FALSE(monitor->getTaskInfo("task1").has_value());
    EXPECT_FALSE(monitor->getTaskInfo("task2").has_value());
    EXPECT_FALSE(monitor->getTaskInfo("task3").has_value());
}

/**
 * @brief 존재하지 않는 Task 업데이트 테스트
 */
TEST_F(TaskMonitorTest, UpdateNonExistentTask) {
    // 존재하지 않는 Task 업데이트 시도 (오류 없이 처리)
    monitor->updateProgress("non_existent", 0.5f);
    monitor->updateStatus("non_existent", TaskStatus::COMPLETED);
    monitor->incrementRetryCount("non_existent");

    // 정상 종료 확인
    EXPECT_TRUE(true);
}

/**
 * @brief Task 진행률 및 상태 전환 시나리오 테스트
 */
TEST_F(TaskMonitorTest, TaskLifecycleScenario) {
    // Task 시작
    monitor->startTask("lifecycle_task");

    auto info = monitor->getTaskInfo("lifecycle_task");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->status, TaskStatus::RUNNING);
    EXPECT_FLOAT_EQ(info->progress, 0.0f);

    // 진행률 업데이트
    monitor->updateProgress("lifecycle_task", 0.25f);
    info = monitor->getTaskInfo("lifecycle_task");
    EXPECT_FLOAT_EQ(info->progress, 0.25f);

    monitor->updateProgress("lifecycle_task", 0.50f);
    info = monitor->getTaskInfo("lifecycle_task");
    EXPECT_FLOAT_EQ(info->progress, 0.50f);

    // 일시정지
    monitor->updateStatus("lifecycle_task", TaskStatus::PAUSED);
    info = monitor->getTaskInfo("lifecycle_task");
    EXPECT_EQ(info->status, TaskStatus::PAUSED);

    // 재개
    monitor->updateStatus("lifecycle_task", TaskStatus::RUNNING);
    info = monitor->getTaskInfo("lifecycle_task");
    EXPECT_EQ(info->status, TaskStatus::RUNNING);

    // 완료
    monitor->endTask("lifecycle_task", TaskStatus::COMPLETED);
    info = monitor->getTaskInfo("lifecycle_task");
    EXPECT_EQ(info->status, TaskStatus::COMPLETED);
    EXPECT_FLOAT_EQ(info->progress, 1.0f);
}

} // namespace mxrc::core::task
