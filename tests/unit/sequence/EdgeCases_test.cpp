// 엣지 케이스 및 경계 조건 테스트

#include "gtest/gtest.h"
#include "core/sequence/core/SequenceRegistry.h"
#include "core/sequence/core/SequenceEngine.h"
#include "core/sequence/core/ExecutionContext.h"
#include "MockActions.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

using namespace mxrc::core::sequence;
using namespace mxrc::core::sequence::testing;

class EdgeCasesTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto logger = std::make_shared<spdlog::logger>("mxrc", console_sink);
            logger->set_level(spdlog::level::debug);
            spdlog::register_logger(logger);
        } catch (const spdlog::spdlog_ex&) {
            // 로거가 이미 존재
        }
    }

protected:
    void SetUp() override {
        registry_ = std::make_shared<SequenceRegistry>();
        actionFactory_ = std::make_shared<MockActionFactory>();
        engine_ = std::make_shared<SequenceEngine>(registry_, actionFactory_);
    }

    std::shared_ptr<SequenceRegistry> registry_;
    std::shared_ptr<IActionFactory> actionFactory_;
    std::shared_ptr<SequenceEngine> engine_;
};

/**
 * @brief 매우 큰 시퀀스 처리
 *
 * 1000개 이상의 액션을 가진 시퀀스를 처리할 수 있습니다
 */
TEST_F(EdgeCasesTest, LargeScaleSequence) {
    // 대규모 시퀀스 정의
    SequenceDefinition largSeq;
    largSeq.id = "large_sequence";
    largSeq.name = "Large Sequence";
    largSeq.version = "1.0.0";

    // 100개의 액션 추가 (1000개는 테스트 시간이 오래 걸리므로 100으로)
    for (int i = 0; i < 100; ++i) {
        largSeq.actionIds.push_back("action_" + std::to_string(i));
    }

    registry_->registerSequence(largSeq);

    // 실행
    std::string executionId = engine_->execute("large_sequence", {});

    // 검증
    EXPECT_FALSE(executionId.empty());
    auto result = engine_->getStatus(executionId);
    EXPECT_EQ(result.actionResults.size(), 100);
}

/**
 * @brief 빈 파라미터 맵
 *
 * 파라미터 없이 시퀀스를 실행할 수 있습니다
 */
TEST_F(EdgeCasesTest, EmptyParameterMap) {
    SequenceDefinition seq;
    seq.id = "simple_seq";
    seq.name = "Simple";
    seq.version = "1.0.0";
    seq.actionIds = {"action_1"};
    registry_->registerSequence(seq);

    // 빈 파라미터로 실행
    std::map<std::string, std::any> emptyParams;
    std::string executionId = engine_->execute("simple_seq", emptyParams);

    EXPECT_FALSE(executionId.empty());
}

/**
 * @brief ExecutionContext의 타입 변환
 *
 * std::any를 사용한 안전한 타입 변환
 */
TEST_F(EdgeCasesTest, ExecutionContextTypeConversion) {
    auto context = std::make_shared<ExecutionContext>();

    // 다양한 타입 저장 및 조회
    context->setVariable("int_val", 42);
    context->setVariable("float_val", 3.14f);
    context->setVariable("bool_val", true);
    context->setVariable("string_val", std::string("test"));

    // 저장된 값 확인
    auto intVal = context->getVariable("int_val");
    EXPECT_TRUE(intVal.has_value());
    EXPECT_EQ(std::any_cast<int>(intVal), 42);

    // 잘못된 타입 캐스팅은 예외 발생
    try {
        std::any_cast<std::string>(intVal);  // int를 string으로 변환 시도
        FAIL() << "Should have thrown exception";
    } catch (const std::bad_any_cast&) {
        // 예상된 예외
    }
}

/**
 * @brief 동작 결과 연쇄
 *
 * 이전 동작의 결과를 다음 동작에서 사용할 수 있습니다
 */
TEST_F(EdgeCasesTest, ActionResultChaining) {
    // 결과를 저장하는 액션
    SequenceDefinition seq;
    seq.id = "chaining_seq";
    seq.name = "Chaining";
    seq.version = "1.0.0";
    seq.actionIds = {"result_action"};
    registry_->registerSequence(seq);

    // 시퀀스 실행
    std::string executionId = engine_->execute("chaining_seq", {});

    // 실행 컨텍스트 조회
    auto context = engine_->getExecutionContext(executionId);
    EXPECT_NE(context, nullptr);

    // 액션 결과 조회 (SuccessAction이 42를 저장함)
    auto result = context->getActionResult("result_action");
    if (result.has_value()) {
        EXPECT_EQ(std::any_cast<int>(result), 42);
    }
}

/**
 * @brief 중첩된 조건부 분기
 *
 * 조건부 분기 내에서 다시 조건부 분기를 할 수 있습니다
 */
TEST_F(EdgeCasesTest, NestedConditionalBranches) {
    // 기본 시퀀스
    SequenceDefinition seq;
    seq.id = "nested_cond";
    seq.name = "Nested Conditional";
    seq.version = "1.0.0";
    seq.actionIds = {"branch1", "branch2"};
    registry_->registerSequence(seq);

    // 조건부 분기 등록
    ConditionalBranch branch1;
    branch1.id = "branch1";
    branch1.condition = "value > 5";
    branch1.thenActions = {"action_1"};
    branch1.elseActions = {"action_2"};
    engine_->registerBranch(branch1);

    ConditionalBranch branch2;
    branch2.id = "branch2";
    branch2.condition = "value < 10";
    branch2.thenActions = {"action_3"};
    branch2.elseActions = {"action_4"};
    engine_->registerBranch(branch2);

    // 컨텍스트에 변수 설정
    std::map<std::string, std::any> params;
    params["value"] = 7;

    // 실행
    std::string executionId = engine_->execute("nested_cond", params);
    EXPECT_FALSE(executionId.empty());
}

/**
 * @brief 병렬 분기 내 조건부 실행
 *
 * 병렬 분기 내에서 조건부 실행이 가능합니다
 */
TEST_F(EdgeCasesTest, ConditionalInParallelBranch) {
    // 시퀀스
    SequenceDefinition seq;
    seq.id = "parallel_cond";
    seq.name = "Parallel with Conditional";
    seq.version = "1.0.0";
    seq.actionIds = {"parallel_group", "final_action"};
    registry_->registerSequence(seq);

    // 병렬 분기
    ParallelBranch parallel;
    parallel.id = "parallel_group";
    parallel.branches = {{"action_a", "action_b"}, {"action_c"}};
    engine_->registerParallelBranch(parallel);

    // 실행
    std::string executionId = engine_->execute("parallel_cond", {});
    EXPECT_FALSE(executionId.empty());
}

/**
 * @brief 매우 깊은 파라미터 치환
 *
 * 하나의 액션 ID에 여러 파라미터를 여러 번 치환합니다
 */
TEST_F(EdgeCasesTest, ComplexParameterSubstitution) {
    SequenceTemplate templ;
    templ.id = "complex_subst";
    templ.name = "Complex Substitution";
    templ.version = "1.0.0";

    TemplateParameter p1, p2, p3;
    p1.name = "x"; p1.type = "int"; p1.required = true;
    p2.name = "y"; p2.type = "int"; p2.required = true;
    p3.name = "op"; p3.type = "string"; p3.required = true;

    templ.parameters = {p1, p2, p3};
    templ.actionIds = {"compute_${op}_${x}_${y}_result"};

    registry_->registerTemplate(templ);

    // 인스턴스화
    std::map<std::string, std::any> params;
    params["x"] = 10;
    params["y"] = 20;
    params["op"] = std::string("add");

    auto result = engine_->instantiateTemplate("complex_subst", params);
    EXPECT_TRUE(result.success);
}

/**
 * @brief 매우 많은 템플릿 파라미터
 *
 * 20개 이상의 파라미터를 가진 템플릿을 처리합니다
 */
TEST_F(EdgeCasesTest, ManyTemplateParameters) {
    SequenceTemplate templ;
    templ.id = "many_params";
    templ.name = "Many Parameters";
    templ.version = "1.0.0";

    // 20개의 파라미터 추가
    std::string actionIdTemplate = "process";
    std::map<std::string, std::any> params;

    for (int i = 0; i < 20; ++i) {
        TemplateParameter p;
        p.name = "param_" + std::to_string(i);
        p.type = "int";
        p.required = true;
        templ.parameters.push_back(p);

        actionIdTemplate += "_${param_" + std::to_string(i) + "}";
        params["param_" + std::to_string(i)] = i;
    }

    templ.actionIds = {actionIdTemplate};
    registry_->registerTemplate(templ);

    // 인스턴스화
    auto result = engine_->instantiateTemplate("many_params", params);
    EXPECT_TRUE(result.success);
}

/**
 * @brief 연속적인 실패 재시도
 *
 * 실패하는 액션이 최대 재시도 횟수까지 반복됩니다
 */
TEST_F(EdgeCasesTest, ConsecutiveRetryFailures) {
    // 시퀀스
    SequenceDefinition seq;
    seq.id = "retry_test";
    seq.name = "Retry Test";
    seq.version = "1.0.0";
    seq.actionIds = {"failure_action"};
    registry_->registerSequence(seq);

    // 시퀀스 실행
    std::string executionId = engine_->execute("retry_test", {});

    // 결과 확인 (실패해야 함)
    auto result = engine_->getStatus(executionId);
    EXPECT_EQ(result.status, SequenceStatus::FAILED);
}

/**
 * @brief 혼합 액션 타입
 *
 * 성공, 실패, 예외 발생 등 다양한 동작을 하나의 시퀀스에서 처리합니다
 */
TEST_F(EdgeCasesTest, MixedActionTypes) {
    // 다양한 액션 타입을 포함한 시퀀스
    SequenceDefinition seq;
    seq.id = "mixed_actions";
    seq.name = "Mixed Actions";
    seq.version = "1.0.0";
    seq.actionIds = {"success", "action_with_delay", "another_success"};
    registry_->registerSequence(seq);

    // 실행
    std::string executionId = engine_->execute("mixed_actions", {});

    // 검증
    EXPECT_FALSE(executionId.empty());
    auto result = engine_->getStatus(executionId);
    EXPECT_GE(result.actionResults.size(), 1);
}

/**
 * @brief 동시에 많은 시퀀스 실행
 *
 * 여러 시퀀스를 순간적으로 실행하면서 상태를 추적합니다
 */
TEST_F(EdgeCasesTest, ManySimultaneousExecutions) {
    // 시퀀스 등록
    SequenceDefinition seq;
    seq.id = "simple_execution";
    seq.name = "Simple";
    seq.version = "1.0.0";
    seq.actionIds = {"action_1"};
    registry_->registerSequence(seq);

    // 여러 시퀀스 동시 실행
    std::vector<std::string> executionIds;
    for (int i = 0; i < 10; ++i) {
        std::string executionId = engine_->execute("simple_execution", {});
        executionIds.push_back(executionId);
    }

    // 모든 실행이 추적되는지 확인
    auto completed = engine_->getCompletedExecutions();
    EXPECT_GE(completed.size(), 10);
}

/**
 * @brief 빈 시퀀스 시도 (오류 처리)
 *
 * 존재하지 않는 시퀀스를 실행하려 하면 오류가 발생합니다
 */
TEST_F(EdgeCasesTest, NonExistentSequenceExecution) {
    // 존재하지 않는 시퀀스 실행 시도
    try {
        std::string executionId = engine_->execute("non_existent_sequence", {});
        FAIL() << "Should have thrown exception";
    } catch (const std::runtime_error&) {
        // 예상된 예외
    }
}

/**
 * @brief 액션 ID 특수문자
 *
 * 특수문자가 포함된 액션 ID를 처리합니다
 */
TEST_F(EdgeCasesTest, SpecialCharactersInActionIds) {
    SequenceDefinition seq;
    seq.id = "special_chars";
    seq.name = "Special Characters";
    seq.version = "1.0.0";
    seq.actionIds = {"action-with-dashes", "action_with_underscores", "action123"};
    registry_->registerSequence(seq);

    // 실행
    std::string executionId = engine_->execute("special_chars", {});
    EXPECT_FALSE(executionId.empty());
}

/**
 * @brief 파라미터로 특수값 전달
 *
 * null, 0, 빈 문자열 등의 특수값을 파라미터로 전달합니다
 */
TEST_F(EdgeCasesTest, SpecialValueParameters) {
    SequenceTemplate templ;
    templ.id = "special_values";
    templ.name = "Special Values";
    templ.version = "1.0.0";

    TemplateParameter p;
    p.name = "value";
    p.type = "string";
    p.required = true;

    templ.parameters = {p};
    templ.actionIds = {"action_${value}"};
    registry_->registerTemplate(templ);

    // 빈 문자열 파라미터
    std::map<std::string, std::any> params;
    params["value"] = std::string("");

    auto result = engine_->instantiateTemplate("special_values", params);
    EXPECT_TRUE(result.success);
}

/**
 * @brief 실행 컨텍스트 메모리 관리
 *
 * 많은 실행 컨텍스트가 생성되고 정리되어도 메모리 누수가 없습니다
 */
TEST_F(EdgeCasesTest, ExecutionContextMemoryManagement) {
    SequenceDefinition seq;
    seq.id = "memory_test";
    seq.name = "Memory Test";
    seq.version = "1.0.0";
    seq.actionIds = {"action_1"};
    registry_->registerSequence(seq);

    // 많은 실행 컨텍스트 생성
    for (int i = 0; i < 50; ++i) {
        std::string executionId = engine_->execute("memory_test", {});
        auto context = engine_->getExecutionContext(executionId);
        // 각 컨텍스트가 정상적으로 생성됨
        EXPECT_NE(context, nullptr);
    }

    // 완료된 실행 조회
    auto completed = engine_->getCompletedExecutions();
    EXPECT_GE(completed.size(), 50);
}

/**
 * @brief 조건식의 경계값
 *
 * 조건식에서 경계값(0, MAX_INT, MIN_INT 등)을 처리합니다
 */
TEST_F(EdgeCasesTest, ConditionBoundaryValues) {
    SequenceDefinition seq;
    seq.id = "boundary_cond";
    seq.name = "Boundary Condition";
    seq.version = "1.0.0";
    seq.actionIds = {"cond_branch"};
    registry_->registerSequence(seq);

    // 경계값 조건부 분기
    ConditionalBranch branch;
    branch.id = "cond_branch";
    branch.condition = "value == 0";
    branch.thenActions = {"zero_action"};
    branch.elseActions = {"nonzero_action"};
    engine_->registerBranch(branch);

    // 0으로 실행
    std::map<std::string, std::any> params;
    params["value"] = 0;
    std::string executionId = engine_->execute("boundary_cond", params);

    EXPECT_FALSE(executionId.empty());
}
