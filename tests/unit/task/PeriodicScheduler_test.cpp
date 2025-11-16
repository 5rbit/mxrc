// PeriodicScheduler 단위 테스트
// Phase 3B-2: 주기적 실행 테스트

#include "gtest/gtest.h"
#include "core/task/core/PeriodicScheduler.h"
#include "core/action/util/ExecutionContext.h"
#include <atomic>
#include <chrono>
#include <thread>

namespace mxrc::core::task {

using namespace mxrc::core::action;

/**
 * @brief PeriodicScheduler 테스트
 */
class PeriodicSchedulerTest : public ::testing::Test {
protected:
    void SetUp() override {
        scheduler = std::make_unique<PeriodicScheduler>();
    }

    void TearDown() override {
        scheduler->stopAll();
        scheduler.reset();
    }

    std::unique_ptr<PeriodicScheduler> scheduler;
};

/**
 * @brief 주기적 실행 기본 테스트
 */
TEST_F(PeriodicSchedulerTest, BasicPeriodicExecution) {
    std::atomic<int> count{0};

    auto callback = [&count](ExecutionContext& ctx) {
        count++;
    };

    scheduler->start("task1", std::chrono::milliseconds(100), callback);

    // 실행 확인
    EXPECT_TRUE(scheduler->isRunning("task1"));

    // 300ms 대기 (약 3회 실행 예상)
    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    EXPECT_GE(count, 2);  // 최소 2회 이상 실행
    EXPECT_LE(count, 4);  // 최대 4회 이하 실행

    scheduler->stop("task1");
    EXPECT_FALSE(scheduler->isRunning("task1"));
}

/**
 * @brief 실행 횟수 추적 테스트
 */
TEST_F(PeriodicSchedulerTest, ExecutionCountTracking) {
    std::atomic<int> count{0};

    auto callback = [&count](ExecutionContext& ctx) {
        count++;
    };

    scheduler->start("task2", std::chrono::milliseconds(50), callback);

    std::this_thread::sleep_for(std::chrono::milliseconds(180));

    int execCount = scheduler->getExecutionCount("task2");
    EXPECT_GE(execCount, 2);
    EXPECT_LE(execCount, 4);

    scheduler->stop("task2");
}

/**
 * @brief 여러 Task 동시 실행 테스트
 */
TEST_F(PeriodicSchedulerTest, MultipleTasksExecution) {
    std::atomic<int> count1{0};
    std::atomic<int> count2{0};

    scheduler->start("task1", std::chrono::milliseconds(100),
        [&count1](ExecutionContext& ctx) { count1++; });

    scheduler->start("task2", std::chrono::milliseconds(150),
        [&count2](ExecutionContext& ctx) { count2++; });

    EXPECT_TRUE(scheduler->isRunning("task1"));
    EXPECT_TRUE(scheduler->isRunning("task2"));

    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    EXPECT_GE(count1, 2);
    EXPECT_GE(count2, 1);

    scheduler->stopAll();
    EXPECT_FALSE(scheduler->isRunning("task1"));
    EXPECT_FALSE(scheduler->isRunning("task2"));
}

/**
 * @brief 스케줄 재시작 테스트
 */
TEST_F(PeriodicSchedulerTest, RestartSchedule) {
    std::atomic<int> count{0};

    auto callback = [&count](ExecutionContext& ctx) {
        count++;
    };

    scheduler->start("task1", std::chrono::milliseconds(100), callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    int firstCount = count;
    EXPECT_GE(firstCount, 1);

    // 재시작
    scheduler->start("task1", std::chrono::milliseconds(100), callback);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    EXPECT_GT(count, firstCount);

    scheduler->stop("task1");
}

/**
 * @brief ExecutionContext 사용 테스트
 */
TEST_F(PeriodicSchedulerTest, ExecutionContextUsage) {
    std::atomic<int> sum{0};

    auto callback = [&sum](ExecutionContext& ctx) {
        // Context에 변수 저장
        int current = 0;
        auto prev = ctx.getVariable("prev_value");
        if (prev.has_value()) {
            current = std::any_cast<int>(prev.value()) + 1;
        }
        ctx.setVariable("prev_value", current);
        sum += current;
    };

    scheduler->start("task1", std::chrono::milliseconds(100), callback);

    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    EXPECT_GT(sum, 0);  // 누적 합이 증가했는지 확인

    scheduler->stop("task1");
}

/**
 * @brief 스케줄 중지 테스트
 */
TEST_F(PeriodicSchedulerTest, StopNonExistentTask) {
    // 존재하지 않는 Task 중지 시도 (오류 없이 처리)
    scheduler->stop("non_existent");
    EXPECT_FALSE(scheduler->isRunning("non_existent"));
}

/**
 * @brief 콜백 예외 처리 테스트
 */
TEST_F(PeriodicSchedulerTest, CallbackExceptionHandling) {
    std::atomic<int> count{0};

    auto callback = [&count](ExecutionContext& ctx) {
        count++;
        if (count == 2) {
            throw std::runtime_error("Test exception");
        }
    };

    scheduler->start("task1", std::chrono::milliseconds(100), callback);

    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    // 예외 발생해도 계속 실행됨
    EXPECT_GE(count, 3);

    scheduler->stop("task1");
}

/**
 * @brief 짧은 간격 실행 테스트
 */
TEST_F(PeriodicSchedulerTest, ShortIntervalExecution) {
    std::atomic<int> count{0};

    scheduler->start("task1", std::chrono::milliseconds(10),
        [&count](ExecutionContext& ctx) { count++; });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 10ms 간격으로 100ms 동안 실행 (약 10회)
    EXPECT_GE(count, 5);

    scheduler->stop("task1");
}

/**
 * @brief stopAll 테스트
 */
TEST_F(PeriodicSchedulerTest, StopAllTasks) {
    scheduler->start("task1", std::chrono::milliseconds(100),
        [](ExecutionContext& ctx) {});

    scheduler->start("task2", std::chrono::milliseconds(100),
        [](ExecutionContext& ctx) {});

    scheduler->start("task3", std::chrono::milliseconds(100),
        [](ExecutionContext& ctx) {});

    EXPECT_TRUE(scheduler->isRunning("task1"));
    EXPECT_TRUE(scheduler->isRunning("task2"));
    EXPECT_TRUE(scheduler->isRunning("task3"));

    scheduler->stopAll();

    EXPECT_FALSE(scheduler->isRunning("task1"));
    EXPECT_FALSE(scheduler->isRunning("task2"));
    EXPECT_FALSE(scheduler->isRunning("task3"));
}

} // namespace mxrc::core::task
