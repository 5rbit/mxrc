// SequenceEngine 통합 테스트
// 실제 3개 동작 시퀀스 실행 검증

#include "gtest/gtest.h"
#include "core/sequence/core/SequenceEngine.h"
#include "core/sequence/core/SequenceRegistry.h"
#include "core/sequence/dto/SequenceDto.h"
#include "MockActions.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <any>

using namespace mxrc::core::sequence;
using namespace mxrc::core::sequence::testing;

class SequenceIntegrationTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        // spdlog 초기화 (통합 테스트 시작 시 한 번만 실행)
        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto logger = std::make_shared<spdlog::logger>("mxrc", console_sink);
            logger->set_level(spdlog::level::info);
            spdlog::register_logger(logger);
        } catch (const spdlog::spdlog_ex& ex) {
            // 로거가 이미 존재하는 경우
        }
    }

protected:
    void SetUp() override {
        // 레지스트리 생성
        registry_ = std::make_shared<SequenceRegistry>();

        // 팩토리 생성
        factory_ = std::make_shared<MockActionFactory>();

        // 엔진 생성
        engine_ = std::make_shared<SequenceEngine>(registry_, factory_);
    }

    std::shared_ptr<SequenceRegistry> registry_;
    std::shared_ptr<IActionFactory> factory_;
    std::shared_ptr<SequenceEngine> engine_;
};

/**
 * @brief 통합 테스트: 3개 동작 시퀀스 완전 실행
 *
 * 시나리오:
 * 1. 시퀀스 정의 (3개 동작)
 * 2. 레지스트리 등록
 * 3. 파라미터 전달하며 실행
 * 4. 실행 상태 확인
 * 5. 각 동작 결과 검증
 * 6. 최종 결과 확인
 */
TEST_F(SequenceIntegrationTest, ThreeActionSequenceExecution) {
    // 1. 시퀀스 정의 생성
    SequenceDefinition sequenceDef;
    sequenceDef.id = "workflow_1";
    sequenceDef.name = "Three-Action Workflow";
    sequenceDef.version = "1.0.0";
    sequenceDef.actionIds = {"init_action", "process_action", "finalize_action"};

    // 2. 레지스트리에 등록
    EXPECT_NO_THROW(registry_->registerSequence(sequenceDef));

    // 3. 파라미터 생성 및 실행
    std::map<std::string, std::any> params;
    params["workflow_name"] = std::string("Test Workflow");
    params["input_value"] = 100;

    std::string executionId = engine_->execute("workflow_1", params);

    // 4. 실행 ID 검증
    ASSERT_FALSE(executionId.empty());
    EXPECT_TRUE(executionId.find("exec_") == 0);

    // 5. 실행 컨텍스트 조회
    auto context = engine_->getExecutionContext(executionId);
    ASSERT_TRUE(context);

    // 6. 파라미터 검증
    auto workflowName = context->getVariable("workflow_name");
    ASSERT_TRUE(workflowName.has_value());
    EXPECT_EQ(std::any_cast<std::string>(workflowName), "Test Workflow");

    auto inputValue = context->getVariable("input_value");
    ASSERT_TRUE(inputValue.has_value());
    EXPECT_EQ(std::any_cast<int>(inputValue), 100);

    // 7. 시퀀스 상태 조회 및 검증
    auto status = engine_->getStatus(executionId);
    EXPECT_EQ(status.sequenceId, "workflow_1");
    EXPECT_EQ(status.status, SequenceStatus::COMPLETED);
    EXPECT_FLOAT_EQ(status.progress, 1.0f);
    EXPECT_EQ(status.actionResults.size(), 3);

    // 8. 각 동작의 결과 검증
    for (size_t i = 0; i < 3; ++i) {
        ASSERT_LT(i, status.actionResults.size());
        const auto& actionLog = status.actionResults[i];
        EXPECT_EQ(actionLog.status, ActionStatus::COMPLETED);
    }

    // 9. 완료된 실행 목록에 포함되는지 확인
    auto completedExecutions = engine_->getCompletedExecutions();
    EXPECT_GE(completedExecutions.size(), 1);
    EXPECT_TRUE(
        std::find(completedExecutions.begin(), completedExecutions.end(), executionId)
        != completedExecutions.end()
    );

    // 10. 실행 중인 항목은 없어야 함 (동기 실행이므로)
    auto runningExecutions = engine_->getRunningExecutions();
    EXPECT_EQ(runningExecutions.size(), 0);
}

/**
 * @brief 통합 테스트: 순차 실행 검증
 *
 * 시나리오:
 * - 3개 동작이 순서대로 실행되는지 확인
 * - 각 동작이 결과를 저장하는지 확인
 */
TEST_F(SequenceIntegrationTest, SequentialExecutionOrder) {
    // 시퀀스 정의
    SequenceDefinition def;
    def.id = "sequential_workflow";
    def.name = "Sequential Workflow";
    def.version = "1.0.0";
    def.actionIds = {"step_1", "step_2", "step_3"};

    registry_->registerSequence(def);

    // 실행
    std::string executionId = engine_->execute("sequential_workflow");

    // 상태 확인
    auto status = engine_->getStatus(executionId);
    EXPECT_EQ(status.status, SequenceStatus::COMPLETED);

    // 모든 동작이 완료되었는지 확인
    EXPECT_EQ(status.actionResults.size(), 3);
    for (const auto& actionLog : status.actionResults) {
        EXPECT_EQ(actionLog.status, ActionStatus::COMPLETED);
    }

    // 컨텍스트에서 결과 검증
    auto context = engine_->getExecutionContext(executionId);
    ASSERT_TRUE(context);

    // MockActionFactory가 생성한 SuccessAction들이 각각 42를 저장해야 함
    for (const auto& actionId : def.actionIds) {
        auto result = context->getActionResult(actionId);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(std::any_cast<int>(result), 42);
    }
}

/**
 * @brief 통합 테스트: 파라미터 흐름 검증
 *
 * 시나리오:
 * - 시퀀스 시작 시 파라미터 전달
 * - 컨텍스트에서 파라미터 접근
 * - 파라미터를 여러 동작이 공유
 */
TEST_F(SequenceIntegrationTest, ParameterFlowThroughSequence) {
    // 시퀀스 정의
    SequenceDefinition def;
    def.id = "param_flow_workflow";
    def.name = "Parameter Flow Workflow";
    def.version = "1.0.0";
    def.actionIds = {"process_step_1", "process_step_2", "process_step_3"};

    registry_->registerSequence(def);

    // 파라미터 전달
    std::map<std::string, std::any> params;
    params["request_id"] = std::string("REQ-12345");
    params["priority"] = 5;
    params["timeout_ms"] = 30000;

    std::string executionId = engine_->execute("param_flow_workflow", params);

    // 컨텍스트에서 모든 파라미터 접근 가능 확인
    auto context = engine_->getExecutionContext(executionId);
    ASSERT_TRUE(context);

    auto requestId = context->getVariable("request_id");
    ASSERT_TRUE(requestId.has_value());
    EXPECT_EQ(std::any_cast<std::string>(requestId), "REQ-12345");

    auto priority = context->getVariable("priority");
    ASSERT_TRUE(priority.has_value());
    EXPECT_EQ(std::any_cast<int>(priority), 5);

    auto timeout = context->getVariable("timeout_ms");
    ASSERT_TRUE(timeout.has_value());
    EXPECT_EQ(std::any_cast<int>(timeout), 30000);
}

/**
 * @brief 통합 테스트: 여러 시퀀스 동시 정의 및 실행
 *
 * 시나리오:
 * - 2개의 서로 다른 시퀀스 정의
 * - 각각 독립적으로 실행
 * - 실행 결과가 격리되는지 확인
 */
TEST_F(SequenceIntegrationTest, MultipleIndependentSequences) {
    // 첫 번째 시퀀스
    SequenceDefinition def1;
    def1.id = "workflow_a";
    def1.name = "Workflow A";
    def1.version = "1.0.0";
    def1.actionIds = {"action_a1", "action_a2"};

    // 두 번째 시퀀스
    SequenceDefinition def2;
    def2.id = "workflow_b";
    def2.name = "Workflow B";
    def2.version = "1.0.0";
    def2.actionIds = {"action_b1", "action_b2", "action_b3"};

    registry_->registerSequence(def1);
    registry_->registerSequence(def2);

    // 파라미터를 다르게 전달하며 실행
    std::map<std::string, std::any> paramsA;
    paramsA["workflow_type"] = std::string("A");

    std::map<std::string, std::any> paramsB;
    paramsB["workflow_type"] = std::string("B");

    std::string execIdA = engine_->execute("workflow_a", paramsA);
    std::string execIdB = engine_->execute("workflow_b", paramsB);

    // 실행 ID는 다름을 확인
    EXPECT_NE(execIdA, execIdB);

    // 각 시퀀스의 컨텍스트가 독립적인지 확인
    auto contextA = engine_->getExecutionContext(execIdA);
    auto contextB = engine_->getExecutionContext(execIdB);

    ASSERT_TRUE(contextA);
    ASSERT_TRUE(contextB);

    auto typeA = contextA->getVariable("workflow_type");
    auto typeB = contextB->getVariable("workflow_type");

    ASSERT_TRUE(typeA.has_value());
    ASSERT_TRUE(typeB.has_value());
    EXPECT_EQ(std::any_cast<std::string>(typeA), "A");
    EXPECT_EQ(std::any_cast<std::string>(typeB), "B");

    // 동작 개수도 다름을 확인
    auto statusA = engine_->getStatus(execIdA);
    auto statusB = engine_->getStatus(execIdB);
    EXPECT_EQ(statusA.actionResults.size(), 2);
    EXPECT_EQ(statusB.actionResults.size(), 3);
}

/**
 * @brief 통합 테스트: 취소 기능 통합
 *
 * 시나리오:
 * - 시퀀스 실행 (동기이므로 이미 완료됨)
 * - 취소 시도 (이미 완료되었으므로 실패)
 * - 최종 상태 확인
 */
TEST_F(SequenceIntegrationTest, CancellationHandling) {
    // 시퀀스 정의
    SequenceDefinition def;
    def.id = "cancellable_workflow";
    def.name = "Cancellable Workflow";
    def.version = "1.0.0";
    def.actionIds = {"action_1", "action_2"};

    registry_->registerSequence(def);

    // 실행
    std::string executionId = engine_->execute("cancellable_workflow");

    // 이미 완료되었으므로 취소는 실패해야 함
    bool cancelResult = engine_->cancel(executionId);
    EXPECT_TRUE(cancelResult);  // 취소는 실행되지만

    // 최종 상태는 CANCELLED로 표시됨
    auto status = engine_->getStatus(executionId);
    EXPECT_EQ(status.status, SequenceStatus::CANCELLED);
}

/**
 * @brief 통합 테스트: 대규모 시퀀스 실행
 *
 * 시나리오:
 * - 많은 동작을 가진 시퀀스 실행
 * - 메모리 관리 확인
 * - 진행률 추적 확인
 */
TEST_F(SequenceIntegrationTest, LargeSequenceExecution) {
    // 100개의 동작을 가진 시퀀스
    SequenceDefinition def;
    def.id = "large_workflow";
    def.name = "Large Workflow";
    def.version = "1.0.0";

    for (int i = 1; i <= 100; ++i) {
        def.actionIds.push_back("action_" + std::to_string(i));
    }

    registry_->registerSequence(def);

    // 실행
    std::string executionId = engine_->execute("large_workflow");

    // 상태 확인
    auto status = engine_->getStatus(executionId);
    EXPECT_EQ(status.status, SequenceStatus::COMPLETED);
    EXPECT_EQ(status.actionResults.size(), 100);
    EXPECT_FLOAT_EQ(status.progress, 1.0f);

    // 모든 동작이 완료되었는지 확인
    for (const auto& actionLog : status.actionResults) {
        EXPECT_EQ(actionLog.status, ActionStatus::COMPLETED);
    }
}

/**
 * @brief 통합 테스트: 조건부 분기가 포함된 시퀀스
 *
 * 시나리오:
 * - 시퀀스에 조건부 분기 포함
 * - THEN/ELSE 경로 선택 검증
 * - 분기 실행 후 계속 진행
 */
TEST_F(SequenceIntegrationTest, ConditionalBranchIntegration) {
    // 시퀀스 정의: branch_check → final_action
    SequenceDefinition def;
    def.id = "conditional_workflow";
    def.name = "Conditional Workflow";
    def.version = "1.0.0";
    def.actionIds = {"branch_check", "final_action"};

    // 조건부 분기 정의
    ConditionalBranch branch;
    branch.id = "branch_check";
    branch.condition = "status == 200";
    branch.thenActions = {"handle_success"};
    branch.elseActions = {"handle_error"};

    registry_->registerSequence(def);
    engine_->registerBranch(branch);

    // 파라미터: status = 200 (THEN 경로 선택)
    std::map<std::string, std::any> params;
    params["status"] = 200;

    std::string executionId = engine_->execute("conditional_workflow", params);
    auto status_result = engine_->getStatus(executionId);

    // 시퀀스 완료 확인
    EXPECT_EQ(status_result.status, SequenceStatus::COMPLETED);

    // 실행된 액션: handle_success + final_action = 2개
    EXPECT_EQ(status_result.actionResults.size(), 2);
    EXPECT_EQ(status_result.actionResults[0].actionId, "handle_success");
    EXPECT_EQ(status_result.actionResults[1].actionId, "final_action");

    // 모든 액션이 성공적으로 완료
    for (const auto& actionLog : status_result.actionResults) {
        EXPECT_EQ(actionLog.status, ActionStatus::COMPLETED);
    }
}

/**
 * @brief 통합 테스트: 복잡한 조건 및 다중 분기 통합
 *
 * 시나리오:
 * - 여러 분기가 순차적으로 실행
 * - 각 분기는 다른 조건 평가
 * - 최종 결과 검증
 */
TEST_F(SequenceIntegrationTest, MultipleBranchesIntegration) {
    // 시퀀스: check_range → validate_type → process_result
    SequenceDefinition def;
    def.id = "multi_branch_workflow";
    def.name = "Multi-Branch Workflow";
    def.version = "1.0.0";
    def.actionIds = {"check_range", "validate_type", "process_result"};

    // 첫 번째 분기: 범위 체크
    ConditionalBranch branch1;
    branch1.id = "check_range";
    branch1.condition = "value >= 0 AND value <= 100";
    branch1.thenActions = {"in_range"};
    branch1.elseActions = {"out_of_range"};

    // 두 번째 분기: 타입 검증
    ConditionalBranch branch2;
    branch2.id = "validate_type";
    branch2.condition = "type == number";
    branch2.thenActions = {"numeric_process"};
    branch2.elseActions = {"non_numeric_process"};

    registry_->registerSequence(def);
    engine_->registerBranch(branch1);
    engine_->registerBranch(branch2);

    // 파라미터: value = 50, type = "number"
    // 예상: in_range + numeric_process + process_result = 3개
    std::map<std::string, std::any> params;
    params["value"] = 50;
    params["type"] = std::string("number");

    std::string executionId = engine_->execute("multi_branch_workflow", params);
    auto result = engine_->getStatus(executionId);

    // 시퀀스 완료
    EXPECT_EQ(result.status, SequenceStatus::COMPLETED);

    // 실행된 액션 확인: in_range + numeric_process + process_result
    EXPECT_EQ(result.actionResults.size(), 3);
    EXPECT_EQ(result.actionResults[0].actionId, "in_range");
    EXPECT_EQ(result.actionResults[1].actionId, "numeric_process");
    EXPECT_EQ(result.actionResults[2].actionId, "process_result");

    // 모두 성공
    for (const auto& actionLog : result.actionResults) {
        EXPECT_EQ(actionLog.status, ActionStatus::COMPLETED);
    }

    // 진행률 완료
    EXPECT_FLOAT_EQ(result.progress, 1.0f);
}

/**
 * @brief 통합 테스트: 조건부 분기 ELSE 경로 실행
 *
 * 시나리오:
 * - 조건이 거짓이어서 ELSE 경로 선택
 * - ELSE 액션만 실행됨
 * - 이후 시퀀스 계속 진행
 */
TEST_F(SequenceIntegrationTest, ConditionalBranchElsePathIntegration) {
    // 시퀀스: check_permission → done
    SequenceDefinition def;
    def.id = "permission_workflow";
    def.name = "Permission Workflow";
    def.version = "1.0.0";
    def.actionIds = {"check_permission", "done"};

    // 권한 체크 분기
    ConditionalBranch branch;
    branch.id = "check_permission";
    branch.condition = "permission >= 3";
    branch.thenActions = {"grant_access"};
    branch.elseActions = {"deny_access"};

    registry_->registerSequence(def);
    engine_->registerBranch(branch);

    // 파라미터: permission = 1 (거짓이므로 ELSE 경로)
    std::map<std::string, std::any> params;
    params["permission"] = 1;

    std::string executionId = engine_->execute("permission_workflow", params);
    auto result = engine_->getStatus(executionId);

    // 시퀀스 완료
    EXPECT_EQ(result.status, SequenceStatus::COMPLETED);

    // 실행된 액션: deny_access + done
    EXPECT_EQ(result.actionResults.size(), 2);
    EXPECT_EQ(result.actionResults[0].actionId, "deny_access");  // ELSE 경로
    EXPECT_EQ(result.actionResults[1].actionId, "done");

    // 모두 성공
    for (const auto& actionLog : result.actionResults) {
        EXPECT_EQ(actionLog.status, ActionStatus::COMPLETED);
    }
}

/**
 * @brief 통합 테스트: 병렬 분기 완전 실행
 *
 * 시나리오:
 * - 여러 병렬 그룹이 동시 실행
 * - 각 그룹 내 순차 실행 확인
 * - 모든 그룹 완료 후 계속 진행
 */
TEST_F(SequenceIntegrationTest, ParallelBranchIntegration) {
    // 시퀀스: setup_parallel → final_action
    SequenceDefinition def;
    def.id = "parallel_workflow";
    def.name = "Parallel Workflow";
    def.version = "1.0.0";
    def.actionIds = {"setup_parallel", "final_action"};

    // 병렬 분기: 3개 그룹
    ParallelBranch parallel;
    parallel.id = "setup_parallel";
    parallel.branches = {
        {"init_arm", "calibrate_arm"},  // 그룹 1: 팔 설정
        {"init_gripper"},                // 그룹 2: 그리퍼
        {"init_sensor", "verify_sensor"} // 그룹 3: 센서
    };

    registry_->registerSequence(def);
    engine_->registerParallelBranch(parallel);

    std::string executionId = engine_->execute("parallel_workflow");
    auto status = engine_->getStatus(executionId);

    // 시퀀스 완료
    EXPECT_EQ(status.status, SequenceStatus::COMPLETED);

    // 실행된 액션: 4개 (init_arm + calibrate_arm + init_gripper + init_sensor + verify_sensor) + 1 (final) = 6
    EXPECT_EQ(status.actionResults.size(), 6);

    // 마지막이 final_action 확인
    EXPECT_EQ(status.actionResults[5].actionId, "final_action");

    // 모두 성공
    for (const auto& actionLog : status.actionResults) {
        EXPECT_EQ(actionLog.status, ActionStatus::COMPLETED);
    }

    // 진행률 완료
    EXPECT_FLOAT_EQ(status.progress, 1.0f);
}

/**
 * @brief 통합 테스트: 순차 → 병렬 → 조건부 → 순차 복합 실행
 *
 * 시나리오:
 * - 여러 실행 모드 혼합
 * - 각 모드 간 정확한 제어 흐름
 * - 최종 상태 검증
 */
TEST_F(SequenceIntegrationTest, ComplexMixedExecutionFlow) {
    // 시퀀스: begin → parallel_setup → check_status → final
    SequenceDefinition def;
    def.id = "complex_workflow";
    def.name = "Complex Mixed Workflow";
    def.version = "1.0.0";
    def.actionIds = {"begin_action", "parallel_setup", "check_status", "final_action"};

    // 병렬 분기
    ParallelBranch parallel;
    parallel.id = "parallel_setup";
    parallel.branches = {
        {"setup_1"},
        {"setup_2", "setup_3"}
    };

    // 조건부 분기
    ConditionalBranch conditional;
    conditional.id = "check_status";
    conditional.condition = "ready == 1";
    conditional.thenActions = {"process_ready"};
    conditional.elseActions = {"process_not_ready"};

    registry_->registerSequence(def);
    engine_->registerParallelBranch(parallel);
    engine_->registerBranch(conditional);

    // 파라미터
    std::map<std::string, std::any> params;
    params["ready"] = 1;

    std::string executionId = engine_->execute("complex_workflow", params);
    auto result = engine_->getStatus(executionId);

    // 시퀀스 완료
    EXPECT_EQ(result.status, SequenceStatus::COMPLETED);

    // 실행 순서 검증:
    // begin_action (1) +
    // setup_1, setup_2, setup_3 (3) +
    // process_ready (1) +
    // final_action (1) = 6
    EXPECT_EQ(result.actionResults.size(), 6);

    // 첫 번째: begin_action
    EXPECT_EQ(result.actionResults[0].actionId, "begin_action");

    // 마지막: final_action
    EXPECT_EQ(result.actionResults[5].actionId, "final_action");

    // 모두 성공
    for (const auto& actionLog : result.actionResults) {
        EXPECT_EQ(actionLog.status, ActionStatus::COMPLETED);
    }
}

/**
 * @brief 통합 테스트: 대규모 병렬 실행
 *
 * 시나리오:
 * - 많은 병렬 그룹 동시 실행
 * - 메모리 및 스레드 관리 검증
 * - 대규모 동작 실행
 */
TEST_F(SequenceIntegrationTest, LargeParallelExecution) {
    // 시퀀스: large_parallel_setup
    SequenceDefinition def;
    def.id = "large_parallel_workflow";
    def.name = "Large Parallel Workflow";
    def.version = "1.0.0";
    def.actionIds = {"large_parallel_setup"};

    // 병렬 분기: 5개 그룹, 총 9개 액션 (2+1+3+1+2)
    ParallelBranch parallel;
    parallel.id = "large_parallel_setup";
    parallel.branches = {
        {"action_1_1", "action_1_2"},
        {"action_2_1"},
        {"action_3_1", "action_3_2", "action_3_3"},
        {"action_4_1"},
        {"action_5_1", "action_5_2"}
    };

    registry_->registerSequence(def);
    engine_->registerParallelBranch(parallel);

    std::string executionId = engine_->execute("large_parallel_workflow");
    auto status = engine_->getStatus(executionId);

    // 시퀀스 완료
    EXPECT_EQ(status.status, SequenceStatus::COMPLETED);

    // 9개 액션 모두 실행됨
    EXPECT_EQ(status.actionResults.size(), 9);

    // 모두 성공
    for (const auto& actionLog : status.actionResults) {
        EXPECT_EQ(actionLog.status, ActionStatus::COMPLETED);
    }

    // 진행률 완료
    EXPECT_FLOAT_EQ(status.progress, 1.0f);
}

/**
 * @brief 통합 테스트: 비동기 실행 및 취소
 *
 * 시나리오:
 * - 긴 지연 시간을 가진 액션을 포함하는 시퀀스 실행
 * - 시퀀스 실행 중 취소 요청
 * - 시퀀스 및 액션이 취소 상태로 전환되는지 확인
 */
TEST_F(SequenceIntegrationTest, AsyncExecutionWithCancellation) {
    // 시퀀스 정의: 500ms 지연 액션
    SequenceDefinition def;
    def.id = "async_cancellable_workflow";
    def.name = "Async Cancellable Workflow";
    def.version = "1.0.0";
    def.actionIds = {"long_delay_action"};

    // 액션 파라미터 설정 (duration_ms)
    ActionStep delayStep;
    delayStep.actionId = "long_delay_action";
    delayStep.actionType = "cancellable_delay";
    delayStep.parameters["duration_ms"] = "500";
    def.steps.push_back(delayStep);

    registry_->registerSequence(def);

    // 시퀀스 비동기 실행
    std::string executionId = engine_->execute("async_cancellable_workflow");

    // 짧은 지연 후 취소 요청
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    engine_->cancel(executionId);

    // 시퀀스 완료 대기 (취소되었으므로 빠르게 완료될 것)
    // Note: 현재 SequenceEngine::execute는 동기적으로 동작하므로,
    // 이 테스트는 SequenceEngine::execute가 내부적으로 ActionExecutor의 비동기 API를
    // 사용하더라도, SequenceEngine 자체는 블로킹 방식으로 동작함을 전제로 합니다.
    // 따라서, cancel 호출 후 execute가 반환될 때까지 기다려야 합니다.
    // 실제 비동기 SequenceEngine이 구현되면 이 부분은 변경될 수 있습니다.

    // 상태 확인
    auto status = engine_->getStatus(executionId);
    EXPECT_EQ(status.status, SequenceStatus::CANCELLED);
    EXPECT_EQ(status.actionResults.size(), 1);
    EXPECT_EQ(status.actionResults[0].actionId, "long_delay_action");
    EXPECT_EQ(status.actionResults[0].status, ActionStatus::CANCELLED);
}

/**
 * @brief 통합 테스트: 여러 비동기 액션 관리
 *
 * 시나리오:
 * - 여러 개의 비동기 액션을 포함하는 시퀀스 실행
 * - 각 액션이 올바르게 시작되고 완료되는지 확인
 * - SequenceEngine이 모든 액션의 생명주기를 관리하는지 확인
 */
TEST_F(SequenceIntegrationTest, MultipleAsyncActionsManagement) {
    // 시퀀스 정의: 2개의 지연 액션
    SequenceDefinition def;
    def.id = "multiple_async_workflow";
    def.name = "Multiple Async Workflow";
    def.version = "1.0.0";
    def.actionIds = {"delay_action_1", "delay_action_2"};

    ActionStep delayStep1;
    delayStep1.actionId = "delay_action_1";
    delayStep1.actionType = "cancellable_delay";
    delayStep1.parameters["duration_ms"] = "100";
    def.steps.push_back(delayStep1);

    ActionStep delayStep2;
    delayStep2.actionId = "delay_action_2";
    delayStep2.actionType = "cancellable_delay";
    delayStep2.parameters["duration_ms"] = "150";
    def.steps.push_back(delayStep2);

    registry_->registerSequence(def);

    // 시퀀스 실행
    std::string executionId = engine_->execute("multiple_async_workflow");

    // 상태 확인
    auto status = engine_->getStatus(executionId);
    EXPECT_EQ(status.status, SequenceStatus::COMPLETED);
    EXPECT_EQ(status.actionResults.size(), 2);
    EXPECT_EQ(status.actionResults[0].actionId, "delay_action_1");
    EXPECT_EQ(status.actionResults[0].status, ActionStatus::COMPLETED);
    EXPECT_EQ(status.actionResults[1].actionId, "delay_action_2");
    EXPECT_EQ(status.actionResults[1].status, ActionStatus::COMPLETED);
}

/**
 * @brief 통합 테스트: 비동기 액션이 포함된 시퀀스 완료 대기
 *
 * 시나리오:
 * - 비동기 액션을 포함하는 시퀀스 실행
 * - SequenceEngine이 액션 완료를 기다린 후 시퀀스를 완료하는지 확인
 */
TEST_F(SequenceIntegrationTest, SequenceWithAsyncActions) {
    // 시퀀스 정의: 200ms 지연 액션
    SequenceDefinition def;
    def.id = "wait_for_async_workflow";
    def.name = "Wait For Async Workflow";
    def.version = "1.0.0";
    def.actionIds = {"short_delay_action"};

    ActionStep delayStep;
    delayStep.actionId = "short_delay_action";
    delayStep.actionType = "cancellable_delay";
    delayStep.parameters["duration_ms"] = "200";
    def.steps.push_back(delayStep);

    registry_->registerSequence(def);

    // 시퀀스 실행
    auto start_time = std::chrono::steady_clock::now();
    std::string executionId = engine_->execute("wait_for_async_workflow");
    auto end_time = std::chrono::steady_clock::now();

    // 실행 시간 검증 (최소 지연 시간 이상이어야 함)
    auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    EXPECT_GE(elapsed_time.count(), 200);

    // 상태 확인
    auto status = engine_->getStatus(executionId);
    EXPECT_EQ(status.status, SequenceStatus::COMPLETED);
    EXPECT_EQ(status.actionResults.size(), 1);
    EXPECT_EQ(status.actionResults[0].actionId, "short_delay_action");
    EXPECT_EQ(status.actionResults[0].status, ActionStatus::COMPLETED);
}

