#include <gtest/gtest.h>
#include "core/action/core/ActionExecutor.h"
#include "core/action/core/ActionFactory.h"
#include "core/action/core/ActionRegistry.h"
#include "core/action/impl/DelayAction.h"
#include "core/action/impl/MoveAction.h"
#include "core/action/util/ExecutionContext.h"

using namespace mxrc::core::action;

/**
 * @brief Action Layer 통합 테스트
 *
 * ActionRegistry, ActionFactory, ActionExecutor를 함께 사용하는 시나리오 테스트
 */
class ActionIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry = std::make_unique<ActionRegistry>();
        factory = std::make_unique<ActionFactory>();
        executor = std::make_unique<ActionExecutor>();
        context = std::make_unique<ExecutionContext>();

        // Delay Action 타입 등록
        registry->registerType("Delay", "Delay for specified milliseconds");
        factory->registerFactory("Delay", [](const std::string& id, const auto& params) {
            long delayMs = 100;
            auto it = params.find("delay_ms");
            if (it != params.end()) {
                delayMs = std::stol(it->second);
            }
            return std::make_shared<DelayAction>(id, delayMs);
        });

        // Move Action 타입 등록
        registry->registerType("Move", "Move robot to target position");
        factory->registerFactory("Move", [](const std::string& id, const auto& params) {
            double x = 0.0, y = 0.0, z = 0.0;
            auto itX = params.find("x");
            auto itY = params.find("y");
            auto itZ = params.find("z");
            if (itX != params.end()) x = std::stod(itX->second);
            if (itY != params.end()) y = std::stod(itY->second);
            if (itZ != params.end()) z = std::stod(itZ->second);
            return std::make_shared<MoveAction>(id, x, y, z);
        });
    }

    std::unique_ptr<ActionRegistry> registry;
    std::unique_ptr<ActionFactory> factory;
    std::unique_ptr<ActionExecutor> executor;
    std::unique_ptr<ExecutionContext> context;
};

// Action 실행 및 결과 확인
TEST_F(ActionIntegrationTest, ExecuteActionAndVerifyResult) {
    // Action 정의 생성 및 등록
    ActionDefinition def("delay1", "Delay");
    def.addParameter("delay_ms", "50");
    def.setTimeout(1000);

    registry->registerDefinition(def);

    // Factory로 Action 생성
    std::map<std::string, std::string> params = {
        {"id", "delay1"},
        {"delay_ms", "50"}
    };

    auto action = factory->createAction("Delay", params);
    ASSERT_NE(action, nullptr);

    // Executor로 실행
    auto result = executor->execute(action, *context);

    EXPECT_EQ(result.actionId, "delay1");
    EXPECT_EQ(result.status, ActionStatus::COMPLETED);
    EXPECT_TRUE(result.isSuccessful());
}

// 여러 Action 순차 실행
TEST_F(ActionIntegrationTest, ExecuteMultipleActionsSequentially) {
    // 3개의 Action 생성 및 실행
    std::vector<std::string> actionIds = {"action1", "action2", "action3"};

    for (const auto& id : actionIds) {
        std::map<std::string, std::string> params = {
            {"id", id},
            {"delay_ms", "30"}
        };

        auto action = factory->createAction("Delay", params);
        auto result = executor->execute(action, *context);

        EXPECT_EQ(result.status, ActionStatus::COMPLETED);
        EXPECT_EQ(result.actionId, id);
    }
}

// Action 간 데이터 전달
TEST_F(ActionIntegrationTest, DataSharingBetweenActions) {
    // Move Action 실행
    std::map<std::string, std::string> moveParams = {
        {"id", "move1"},
        {"x", "100.5"},
        {"y", "200.3"},
        {"z", "50.0"}
    };

    auto moveAction = factory->createAction("Move", moveParams);
    auto moveResult = executor->execute(moveAction, *context);

    EXPECT_EQ(moveResult.status, ActionStatus::COMPLETED);

    // Context에서 위치 정보 확인
    auto posX = context->getVariable("last_position_x");
    ASSERT_TRUE(posX.has_value());
    EXPECT_DOUBLE_EQ(std::any_cast<double>(posX.value()), 100.5);

    auto posY = context->getVariable("last_position_y");
    ASSERT_TRUE(posY.has_value());
    EXPECT_DOUBLE_EQ(std::any_cast<double>(posY.value()), 200.3);

    auto posZ = context->getVariable("last_position_z");
    ASSERT_TRUE(posZ.has_value());
    EXPECT_DOUBLE_EQ(std::any_cast<double>(posZ.value()), 50.0);
}

// Action 에러 처리
TEST_F(ActionIntegrationTest, HandleActionErrors) {
    // 존재하지 않는 타입으로 Action 생성 시도
    std::map<std::string, std::string> params = {{"id", "unknown1"}};

    EXPECT_THROW(factory->createAction("UnknownType", params), std::runtime_error);
}
