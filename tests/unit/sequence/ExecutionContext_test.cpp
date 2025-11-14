// ExecutionContext 클래스 테스트
// 시퀀스 실행 중 동작들 간 상태 공유 기능 검증

#include "gtest/gtest.h"
#include "core/sequence/core/ExecutionContext.h"
#include <string>
#include <any>

namespace mxrc::core::sequence {

class ExecutionContextTest : public ::testing::Test {
protected:
    ExecutionContext context_;
};

// 실행 결과 저장 및 조회 테스트
TEST_F(ExecutionContextTest, SetAndGetActionResult) {
    std::string actionId = "action_1";
    int expectedResult = 42;
    
    context_.setActionResult(actionId, expectedResult);
    
    std::any result = context_.getActionResult(actionId);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(std::any_cast<int>(result), expectedResult);
}

// 존재하지 않는 결과 조회 테스트
TEST_F(ExecutionContextTest, GetNonExistentResult) {
    std::any result = context_.getActionResult("non_existent");
    
    EXPECT_FALSE(result.has_value());
}

// 결과 존재 여부 확인 테스트
TEST_F(ExecutionContextTest, HasActionResult) {
    std::string actionId = "action_1";
    
    EXPECT_FALSE(context_.hasActionResult(actionId));
    
    context_.setActionResult(actionId, 100);
    
    EXPECT_TRUE(context_.hasActionResult(actionId));
}

// 여러 동작 결과 저장 테스트
TEST_F(ExecutionContextTest, MultipleActionResults) {
    context_.setActionResult("action_1", 10);
    context_.setActionResult("action_2", std::string("result_2"));
    context_.setActionResult("action_3", 3.14f);

    const auto& results = context_.getAllResults();

    EXPECT_EQ(results.size(), 3);
    EXPECT_EQ(std::any_cast<int>(context_.getActionResult("action_1")), 10);
    EXPECT_EQ(std::any_cast<std::string>(context_.getActionResult("action_2")), "result_2");
    EXPECT_FLOAT_EQ(std::any_cast<float>(context_.getActionResult("action_3")), 3.14f);
}

// 컨텍스트 변수 설정 및 조회 테스트
TEST_F(ExecutionContextTest, SetAndGetVariable) {
    std::string key = "velocity";
    float value = 50.0f;
    
    context_.setVariable(key, value);
    
    std::any result = context_.getVariable(key);
    ASSERT_TRUE(result.has_value());
    EXPECT_FLOAT_EQ(std::any_cast<float>(result), value);
}

// 존재하지 않는 변수 조회 테스트
TEST_F(ExecutionContextTest, GetNonExistentVariable) {
    std::any result = context_.getVariable("non_existent");
    
    EXPECT_FALSE(result.has_value());
}

// 실행 ID 설정 및 조회 테스트
TEST_F(ExecutionContextTest, ExecutionId) {
    std::string executionId = "exec_123";
    
    context_.setExecutionId(executionId);
    
    EXPECT_EQ(context_.getExecutionId(), executionId);
}

// 컨텍스트 초기화 테스트
TEST_F(ExecutionContextTest, ClearContext) {
    context_.setActionResult("action_1", 100);
    context_.setVariable("key_1", "value_1");
    context_.setExecutionId("exec_123");
    
    EXPECT_EQ(context_.getAllResults().size(), 1);
    
    context_.clear();
    
    EXPECT_EQ(context_.getAllResults().size(), 0);
    EXPECT_FALSE(context_.getVariable("key_1").has_value());
    EXPECT_FALSE(context_.hasActionResult("action_1"));
}

// 결과 덮어쓰기 테스트
TEST_F(ExecutionContextTest, OverwriteActionResult) {
    std::string actionId = "action_1";
    
    context_.setActionResult(actionId, 100);
    context_.setActionResult(actionId, 200);
    
    std::any result = context_.getActionResult(actionId);
    EXPECT_EQ(std::any_cast<int>(result), 200);
}

// 복합 데이터 타입 저장 테스트
TEST_F(ExecutionContextTest, ComplexDataTypes) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    context_.setActionResult("vector_action", vec);
    
    std::any result = context_.getActionResult("vector_action");
    std::vector<int> retrieved = std::any_cast<std::vector<int>>(result);
    
    EXPECT_EQ(retrieved.size(), 5);
    EXPECT_EQ(retrieved[0], 1);
    EXPECT_EQ(retrieved[4], 5);
}

} // namespace mxrc::core::sequence

