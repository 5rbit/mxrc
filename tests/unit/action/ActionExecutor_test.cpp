#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "core/action/core/ActionExecutor.h"
#include "core/action/impl/DelayAction.h"
#include "core/action/impl/MoveAction.h"
#include "core/action/util/ExecutionContext.h"

using namespace mxrc::core::action;

/**
 * @brief ActionExecutor 단위 테스트
 */
class ActionExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // ✓ shared_ptr 사용 (ActionExecutor는 weak_from_this() 사용)
        executor = std::make_shared<ActionExecutor>();
        context = std::make_unique<ExecutionContext>();
    }

    std::shared_ptr<ActionExecutor> executor;  // ✓ shared_ptr로 변경
    std::unique_ptr<ExecutionContext> context;
};

// Action 성공적으로 실행
TEST_F(ActionExecutorTest, ExecuteActionSuccessfully) {
    auto action = std::make_shared<DelayAction>("delay1", 50);

    auto result = executor->execute(action, *context);

    EXPECT_EQ(result.actionId, "delay1");
    EXPECT_EQ(result.status, ActionStatus::COMPLETED);
    EXPECT_FLOAT_EQ(result.progress, 1.0f);
    EXPECT_TRUE(result.isSuccessful());
    EXPECT_FALSE(result.isFailed());
}

// 타임아웃 체크
TEST_F(ActionExecutorTest, TimeoutCancelsAction) {
    auto action = std::make_shared<DelayAction>("delay_long", 5000);

    // 100ms 타임아웃으로 5초 대기 Action 실행
    auto result = executor->execute(action, *context, std::chrono::milliseconds(100));

    // 타임아웃 발생 확인
    EXPECT_EQ(result.status, ActionStatus::TIMEOUT);
    EXPECT_TRUE(result.isFailed());
}

// Action 취소
TEST_F(ActionExecutorTest, CancelAction) {
    auto action = std::make_shared<DelayAction>("delay2", 1000);

    // 별도 스레드에서 취소
    std::thread cancelThread([this, &action]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        executor->cancel(action);
    });

    auto result = executor->execute(action, *context);

    cancelThread.join();

    EXPECT_TRUE(result.status == ActionStatus::CANCELLED ||
                result.status == ActionStatus::COMPLETED);
}

// MoveAction 실행
TEST_F(ActionExecutorTest, ExecuteMoveAction) {
    auto action = std::make_shared<MoveAction>("move1", 10.0, 20.0, 30.0);

    auto result = executor->execute(action, *context);

    EXPECT_EQ(result.actionId, "move1");
    EXPECT_EQ(result.status, ActionStatus::COMPLETED);

    // 컨텍스트에 위치 정보가 저장되었는지 확인
    auto posX = context->getVariable("last_position_x");
    ASSERT_TRUE(posX.has_value());
    EXPECT_DOUBLE_EQ(std::any_cast<double>(posX.value()), 10.0);
}

// 타임아웃 시 즉시 취소 확인 (개선된 비동기 실행)
TEST_F(ActionExecutorTest, TimeoutCancelsActionImmediately) {
    auto action = std::make_shared<DelayAction>("delay_very_long", 10000);

    auto startTime = std::chrono::steady_clock::now();

    // 200ms 타임아웃으로 10초 대기 Action 실행
    auto result = executor->execute(action, *context, std::chrono::milliseconds(200));

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);

    // 타임아웃 발생 확인
    EXPECT_EQ(result.status, ActionStatus::TIMEOUT);
    EXPECT_TRUE(result.isFailed());

    // 실행 시간이 타임아웃 시간과 가까운지 확인
    // DelayAction은 10개 스텝으로 나누므로 10000ms/10 = 1000ms 스텝
    // 타임아웃 200ms + 취소 처리 대기 100ms + 현재 스텝 완료 최대 1000ms = 1300ms 이내
    EXPECT_LT(elapsed.count(), 1500);
}

// 타임아웃 없이 정상 완료
TEST_F(ActionExecutorTest, ExecuteWithoutTimeout) {
    auto action = std::make_shared<DelayAction>("delay_short", 50);

    // 타임아웃 없음 (0 = 무제한)
    auto result = executor->execute(action, *context, std::chrono::milliseconds(0));

    EXPECT_EQ(result.status, ActionStatus::COMPLETED);
    EXPECT_TRUE(result.isSuccessful());
}

// ========== 종료 안정성 테스트 ==========

// 소멸자가 실행 중인 액션을 안전하게 취소
TEST_F(ActionExecutorTest, DestructorCancelsRunningActions) {
    auto action1 = std::make_shared<DelayAction>("delay_long1", 2000);
    auto action2 = std::make_shared<DelayAction>("delay_long2", 2000);
    auto action3 = std::make_shared<DelayAction>("delay_long3", 2000);

    // 비동기로 여러 액션 시작
    std::string id1 = executor->executeAsync(action1, *context);
    std::string id2 = executor->executeAsync(action2, *context);
    std::string id3 = executor->executeAsync(action3, *context);

    // 모두 실행 중인지 확인
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(executor->isRunning(id1));
    EXPECT_TRUE(executor->isRunning(id2));
    EXPECT_TRUE(executor->isRunning(id3));

    auto startTime = std::chrono::steady_clock::now();

    // executor 소멸 - 모든 실행 중인 액션을 취소하고 스레드를 정리해야 함
    executor.reset();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);

    // 소멸자가 빠르게 완료되어야 함 (액션이 완료될 때까지 기다리지 않음)
    // 각 액션의 현재 스텝 완료 대기 시간: 2000ms/10 = 200ms per step
    // 3개 액션 취소 + 스레드 정리: 약 1000ms 이내
    EXPECT_LT(elapsed.count(), 1500);
}

// clearCompletedActions로 완료된 액션 정리
TEST_F(ActionExecutorTest, ClearCompletedActionsRemovesFinishedActions) {
    auto action1 = std::make_shared<DelayAction>("delay1", 50);
    auto action2 = std::make_shared<DelayAction>("delay2", 50);
    auto action3 = std::make_shared<DelayAction>("delay3", 2000);

    // 2개는 짧게, 1개는 길게 실행
    std::string id1 = executor->executeAsync(action1, *context);
    std::string id2 = executor->executeAsync(action2, *context);
    std::string id3 = executor->executeAsync(action3, *context);

    // 짧은 액션들이 완료될 때까지 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 정리 전에는 모두 맵에 존재
    auto result1 = executor->getResult(id1);
    auto result2 = executor->getResult(id2);
    auto result3 = executor->getResult(id3);

    EXPECT_EQ(result1.status, ActionStatus::COMPLETED);
    EXPECT_EQ(result2.status, ActionStatus::COMPLETED);
    EXPECT_EQ(result3.status, ActionStatus::RUNNING);

    // 완료된 액션만 정리
    int cleared = executor->clearCompletedActions();
    EXPECT_EQ(cleared, 2);  // action1, action2만 제거

    // id3는 여전히 실행 중이어야 함
    EXPECT_TRUE(executor->isRunning(id3));

    // 정리된 액션은 조회 시 "not found" 반환
    auto result1After = executor->getResult(id1);
    EXPECT_EQ(result1After.status, ActionStatus::FAILED);
    EXPECT_EQ(result1After.errorMessage, "Action not found");

    // 긴 액션 취소 및 정리
    executor->cancel(id3);
    executor->waitForCompletion(id3);
    int cleared2 = executor->clearCompletedActions();
    EXPECT_EQ(cleared2, 1);  // action3 제거
}

// 타임아웃 스레드가 안전하게 종료되는지 확인
TEST_F(ActionExecutorTest, TimeoutThreadsTerminateCleanly) {
    auto action1 = std::make_shared<DelayAction>("delay_timeout1", 5000);
    auto action2 = std::make_shared<DelayAction>("delay_timeout2", 5000);
    auto action3 = std::make_shared<DelayAction>("delay_timeout3", 5000);

    // 타임아웃 설정하여 비동기 실행
    std::string id1 = executor->executeAsync(action1, *context, std::chrono::milliseconds(100));
    std::string id2 = executor->executeAsync(action2, *context, std::chrono::milliseconds(150));
    std::string id3 = executor->executeAsync(action3, *context, std::chrono::milliseconds(200));

    // 타임아웃이 발생하고 모든 액션이 완전히 완료될 때까지 충분히 대기
    // 타임아웃 + 현재 스텝 완료 대기 시간 고려
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    // 모두 타임아웃되었는지 확인
    auto result1 = executor->getResult(id1);
    auto result2 = executor->getResult(id2);
    auto result3 = executor->getResult(id3);

    EXPECT_EQ(result1.status, ActionStatus::TIMEOUT);
    EXPECT_EQ(result2.status, ActionStatus::TIMEOUT);
    EXPECT_EQ(result3.status, ActionStatus::TIMEOUT);

    auto startTime = std::chrono::steady_clock::now();

    // 타임아웃 스레드가 모두 정리되어야 함
    int cleared = executor->clearCompletedActions();
    EXPECT_EQ(cleared, 3);

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);

    // 스레드 정리가 빠르게 완료되어야 함 (이미 타임아웃되어 중지됨)
    EXPECT_LT(elapsed.count(), 300);
}

// 동시에 여러 액션 취소 시 데드락 없이 안전하게 종료
TEST_F(ActionExecutorTest, ConcurrentCancellationNoDeadlock) {
    std::vector<std::shared_ptr<DelayAction>> actions;
    std::vector<std::string> actionIds;

    // 10개의 긴 액션 시작
    for (int i = 0; i < 10; i++) {
        auto action = std::make_shared<DelayAction>("delay_concurrent_" + std::to_string(i), 5000);
        actions.push_back(action);
        actionIds.push_back(executor->executeAsync(action, *context));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 모두 실행 중인지 확인
    for (const auto& id : actionIds) {
        EXPECT_TRUE(executor->isRunning(id));
    }

    auto startTime = std::chrono::steady_clock::now();

    // 여러 스레드에서 동시에 취소
    std::vector<std::thread> cancelThreads;
    for (const auto& id : actionIds) {
        cancelThreads.emplace_back([this, id]() {
            executor->cancel(id);
        });
    }

    // 모든 취소 스레드 종료 대기
    for (auto& t : cancelThreads) {
        t.join();
    }

    // 모든 액션 완료 대기
    for (const auto& id : actionIds) {
        executor->waitForCompletion(id);
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);

    // 동시 취소가 데드락 없이 빠르게 완료되어야 함
    EXPECT_LT(elapsed.count(), 3000);

    // 모든 액션이 취소되었는지 확인
    for (const auto& id : actionIds) {
        auto result = executor->getResult(id);
        EXPECT_TRUE(result.status == ActionStatus::CANCELLED ||
                    result.status == ActionStatus::TIMEOUT);
    }
}

// 메모리 누수 방지: 많은 액션 실행 후 정리
TEST_F(ActionExecutorTest, NoMemoryLeakWithManyActions) {
    // 100개의 짧은 액션 순차 실행
    for (int i = 0; i < 100; i++) {
        auto action = std::make_shared<DelayAction>("delay_mem_" + std::to_string(i), 10);
        auto result = executor->execute(action, *context);
        EXPECT_EQ(result.status, ActionStatus::COMPLETED);
    }

    // clearCompletedActions가 없어도 execute()는 자동으로 정리함
    // 맵이 비어있어야 함 (execute는 완료 후 자동 제거)
    int cleared = executor->clearCompletedActions();
    EXPECT_EQ(cleared, 0);  // 이미 모두 정리되었음
}

// 비동기 실행 후 정리하지 않으면 메모리에 누적
TEST_F(ActionExecutorTest, AsyncExecutionRequiresManualCleanup) {
    std::vector<std::string> actionIds;

    // 100개의 짧은 액션 비동기 실행
    for (int i = 0; i < 100; i++) {
        auto action = std::make_shared<DelayAction>("delay_async_" + std::to_string(i), 10);
        actionIds.push_back(executor->executeAsync(action, *context));
    }

    // 모든 액션 완료 대기
    for (const auto& id : actionIds) {
        executor->waitForCompletion(id);
    }

    // clearCompletedActions로 정리해야 함
    int cleared = executor->clearCompletedActions();
    EXPECT_EQ(cleared, 100);  // 100개 모두 정리

    // 두 번째 호출은 0 반환
    int clearedAgain = executor->clearCompletedActions();
    EXPECT_EQ(clearedAgain, 0);
}
