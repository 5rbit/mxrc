#include <gtest/gtest.h>
#include "core/action/core/ActionRegistry.h"

using namespace mxrc::core::action;

/**
 * @brief ActionRegistry 단위 테스트
 */
class ActionRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry = std::make_unique<ActionRegistry>();
    }

    std::unique_ptr<ActionRegistry> registry;
};

// Action 정의 등록 및 조회
TEST_F(ActionRegistryTest, RegisterAndRetrieveDefinition) {
    ActionDefinition def("action1", "Delay");
    def.addParameter("delay_ms", "100");
    def.setTimeout(1000);

    registry->registerDefinition(def);

    auto retrieved = registry->getDefinition("action1");

    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->id, "action1");
    EXPECT_EQ(retrieved->type, "Delay");
    EXPECT_EQ(retrieved->parameters.at("delay_ms"), "100");
    EXPECT_EQ(retrieved->timeout.count(), 1000);
}

// Action 타입 등록
TEST_F(ActionRegistryTest, RegisterType) {
    registry->registerType("Delay", "Delay action for testing");
    registry->registerType("Move", "Move robot to position");

    EXPECT_TRUE(registry->hasType("Delay"));
    EXPECT_TRUE(registry->hasType("Move"));
    EXPECT_FALSE(registry->hasType("Unknown"));
}

// Action 정의 존재 여부 확인
TEST_F(ActionRegistryTest, CheckDefinitionExists) {
    ActionDefinition def("action1", "Delay");
    registry->registerDefinition(def);

    EXPECT_TRUE(registry->hasDefinition("action1"));
    EXPECT_FALSE(registry->hasDefinition("action2"));
}

// 모든 정의 ID 조회
TEST_F(ActionRegistryTest, GetAllDefinitionIds) {
    ActionDefinition def1("action1", "Delay");
    ActionDefinition def2("action2", "Move");

    registry->registerDefinition(def1);
    registry->registerDefinition(def2);

    auto ids = registry->getAllDefinitionIds();

    EXPECT_EQ(ids.size(), 2);
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "action1") != ids.end());
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "action2") != ids.end());
}

// 모든 타입 조회
TEST_F(ActionRegistryTest, GetAllTypes) {
    registry->registerType("Delay", "Delay action");
    registry->registerType("Move", "Move action");

    auto types = registry->getAllTypes();

    EXPECT_EQ(types.size(), 2);
    EXPECT_TRUE(std::find(types.begin(), types.end(), "Delay") != types.end());
    EXPECT_TRUE(std::find(types.begin(), types.end(), "Move") != types.end());
}

// 정의 덮어쓰기
TEST_F(ActionRegistryTest, OverwriteDefinition) {
    ActionDefinition def1("action1", "Delay");
    def1.addParameter("delay_ms", "100");

    registry->registerDefinition(def1);

    ActionDefinition def2("action1", "Move");
    def2.addParameter("x", "10");

    registry->registerDefinition(def2);

    auto retrieved = registry->getDefinition("action1");

    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->type, "Move");
    EXPECT_TRUE(retrieved->parameters.find("x") != retrieved->parameters.end());
    EXPECT_TRUE(retrieved->parameters.find("delay_ms") == retrieved->parameters.end());
}
