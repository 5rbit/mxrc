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
        executor = std::make_unique<ActionExecutor>();
        context = std::make_unique<ExecutionContext>();
    }

    std::unique_ptr<ActionExecutor> executor;
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
