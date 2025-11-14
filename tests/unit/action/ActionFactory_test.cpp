#include <gtest/gtest.h>
#include "core/action/core/ActionFactory.h"
#include "core/action/impl/DelayAction.h"
#include "core/action/impl/MoveAction.h"

using namespace mxrc::core::action;

/**
 * @brief ActionFactory 단위 테스트
 */
class ActionFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        factory = std::make_unique<ActionFactory>();

        // DelayAction 팩토리 등록
        factory->registerFactory("Delay", [](const std::string& id, const auto& params) {
            long delayMs = 100;  // 기본값
            auto it = params.find("delay_ms");
            if (it != params.end()) {
                delayMs = std::stol(it->second);
            }
            return std::make_shared<DelayAction>(id, delayMs);
        });

        // MoveAction 팩토리 등록
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

    std::unique_ptr<ActionFactory> factory;
};

// Action 생성 with 파라미터
TEST_F(ActionFactoryTest, CreateActionWithParameters) {
    std::map<std::string, std::string> params = {
        {"id", "delay1"},
        {"delay_ms", "200"}
    };

    auto action = factory->createAction("Delay", params);

    ASSERT_NE(action, nullptr);
    EXPECT_EQ(action->getId(), "delay1");
    EXPECT_EQ(action->getType(), "Delay");
}

// 등록되지 않은 타입으로 생성 시도
TEST_F(ActionFactoryTest, CreateUnknownTypeThrowsError) {
    std::map<std::string, std::string> params = {{"id", "unknown1"}};

    EXPECT_THROW(factory->createAction("Unknown", params), std::runtime_error);
}

// ID 없이 생성 시도
TEST_F(ActionFactoryTest, CreateWithoutIdThrowsError) {
    std::map<std::string, std::string> params = {{"delay_ms", "100"}};

    EXPECT_THROW(factory->createAction("Delay", params), std::runtime_error);
}

// 등록된 타입 확인
TEST_F(ActionFactoryTest, HasRegisteredTypes) {
    EXPECT_TRUE(factory->hasType("Delay"));
    EXPECT_TRUE(factory->hasType("Move"));
    EXPECT_FALSE(factory->hasType("Unknown"));
}

// 모든 등록된 타입 조회
TEST_F(ActionFactoryTest, GetAllRegisteredTypes) {
    auto types = factory->getRegisteredTypes();

    EXPECT_EQ(types.size(), 2);
    EXPECT_TRUE(std::find(types.begin(), types.end(), "Delay") != types.end());
    EXPECT_TRUE(std::find(types.begin(), types.end(), "Move") != types.end());
}
