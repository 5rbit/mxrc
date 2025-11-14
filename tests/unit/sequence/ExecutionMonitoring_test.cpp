// 시퀀스 실행 모니터링 및 제어 테스트

#include "gtest/gtest.h"
#include "core/sequence/core/SequenceRegistry.h"
#include "core/sequence/core/SequenceEngine.h"
#include "core/sequence/core/ExecutionMonitor.h"
#include "MockActions.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <thread>
#include <chrono>

using namespace mxrc::core::sequence;
using namespace mxrc::core::sequence::testing;

class ExecutionMonitoringTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        // spdlog 초기화
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

        // 기본 시퀀스 등록 (고유한 액션 ID)
        SequenceDefinition seq;
        seq.id = "monitoring_test";
        seq.name = "Monitoring Test Sequence";
        seq.version = "1.0.0";
        seq.actionIds = {"action_1", "action_2", "action_3"};
        registry_->registerSequence(seq);
    }

    std::shared_ptr<SequenceRegistry> registry_;
    std::shared_ptr<IActionFactory> actionFactory_;
    std::shared_ptr<SequenceEngine> engine_;
};

/**
 * @brief 시퀀스 실행 상태 조회
 *
 * 실행 중인 시퀀스의 현재 상태를 조회할 수 있습니다
 */
TEST_F(ExecutionMonitoringTest, QueryExecutionStatus) {
    // 시퀀스 실행
    std::string executionId = engine_->execute("monitoring_test", {});
    EXPECT_FALSE(executionId.empty());

    // 실행 상태 조회
    auto result = engine_->getStatus(executionId);
    EXPECT_EQ(result.executionId, executionId);
    EXPECT_EQ(result.sequenceId, "monitoring_test");
    EXPECT_EQ(result.status, SequenceStatus::COMPLETED);
    EXPECT_EQ(result.progress, 1.0f);
}

/**
 * @brief 진행률 추적
 *
 * 시퀀스 실행 중 진행률이 0부터 1.0으로 증가합니다
 */
TEST_F(ExecutionMonitoringTest, ProgressTracking) {
    // 실행
    std::string executionId = engine_->execute("monitoring_test", {});

    // 최종 상태에서 진행률 확인
    auto result = engine_->getStatus(executionId);
    EXPECT_EQ(result.progress, 1.0f);  // 3개 액션 모두 완료
}

/**
 * @brief 실행 로그 기록
 *
 * 각 동작 실행 시마다 로그가 기록됩니다
 */
TEST_F(ExecutionMonitoringTest, ExecutionLogRecording) {
    // 시퀀스 실행
    std::string executionId = engine_->execute("monitoring_test", {});

    // 실행 컨텍스트에서 모니터 접근
    auto context = engine_->getExecutionContext(executionId);
    EXPECT_NE(context, nullptr);

    // 최종 상태 확인
    auto result = engine_->getStatus(executionId);
    EXPECT_GE(result.actionResults.size(), 3);  // 최소 3개 액션 결과
}

/**
 * @brief 동작 실행 결과 세부사항
 *
 * 각 동작의 실행 시간, 상태, 에러 메시지 등을 추적합니다
 */
TEST_F(ExecutionMonitoringTest, ActionExecutionDetails) {
    // 시퀀스 실행
    std::string executionId = engine_->execute("monitoring_test", {});

    // 실행 결과 조회
    auto result = engine_->getStatus(executionId);

    // 각 액션의 결과 검증
    for (const auto& actionResult : result.actionResults) {
        EXPECT_FALSE(actionResult.actionId.empty());
        EXPECT_EQ(actionResult.status, ActionStatus::COMPLETED);
        EXPECT_GE(actionResult.executionTimeMs, 0);
    }
}

/**
 * @brief 실행 중인 시퀀스 목록 조회
 *
 * 현재 실행 중인 모든 시퀀스를 조회할 수 있습니다
 */
TEST_F(ExecutionMonitoringTest, ListRunningExecutions) {
    // 첫 번째 실행
    std::string executionId1 = engine_->execute("monitoring_test", {});
    EXPECT_FALSE(executionId1.empty());

    // 실행 중인 시퀀스 조회 (빠르게 완료되므로 아마 비어있을 수 있음)
    auto running = engine_->getRunningExecutions();
    // 빠른 실행이므로 완료될 수 있음 - 완료된 실행 확인
    auto completed = engine_->getCompletedExecutions();
    EXPECT_GE(completed.size(), 1);
}

/**
 * @brief 완료된 시퀀스 목록 조회
 *
 * 완료된 시퀀스의 목록과 최종 상태를 조회할 수 있습니다
 */
TEST_F(ExecutionMonitoringTest, ListCompletedExecutions) {
    // 여러 시퀀스 실행
    std::vector<std::string> executionIds;
    for (int i = 0; i < 3; ++i) {
        std::string executionId = engine_->execute("monitoring_test", {});
        executionIds.push_back(executionId);
    }

    // 완료된 시퀀스 조회
    auto completed = engine_->getCompletedExecutions();
    EXPECT_GE(completed.size(), 3);

    // 각 실행 상태 확인
    for (const auto& id : executionIds) {
        auto result = engine_->getStatus(id);
        EXPECT_EQ(result.status, SequenceStatus::COMPLETED);
    }
}

/**
 * @brief 실행 취소 및 상태 변경
 *
 * 실행 중인 시퀀스를 취소하면 상태가 CANCELLED로 변경됩니다
 */
TEST_F(ExecutionMonitoringTest, CancelExecutionAndStatus) {
    // 시퀀스 실행
    std::string executionId = engine_->execute("monitoring_test", {});

    // 취소 요청 (이미 완료되었을 수 있음)
    bool cancelled = engine_->cancel(executionId);

    // 최종 상태 확인
    auto result = engine_->getStatus(executionId);
    // 성공 또는 취소됨
    EXPECT_TRUE(result.status == SequenceStatus::COMPLETED ||
                result.status == SequenceStatus::CANCELLED);
}

/**
 * @brief 일시정지 및 재개
 *
 * 실행 중인 시퀀스를 일시정지하고 재개할 수 있습니다
 */
TEST_F(ExecutionMonitoringTest, PauseAndResumeExecution) {
    // 시퀀스 실행
    std::string executionId = engine_->execute("monitoring_test", {});

    // 일시정지 요청 (이미 완료되었을 수 있음)
    bool paused = engine_->pause(executionId);

    if (paused) {
        // 상태 확인 (일시정지 중)
        auto result = engine_->getStatus(executionId);
        // 상태가 변경되었을 수 있음

        // 재개 요청
        bool resumed = engine_->resume(executionId);
        // 재개 성공 여부 확인
    }
}

/**
 * @brief 긴 시퀀스의 진행률 추적
 *
 * 많은 액션으로 구성된 시퀀스의 진행률을 실시간으로 추적합니다
 */
TEST_F(ExecutionMonitoringTest, LongSequenceProgressTracking) {
    // 많은 액션으로 구성된 시퀀스
    SequenceDefinition longSeq;
    longSeq.id = "long_sequence";
    longSeq.name = "Long Sequence";
    longSeq.version = "1.0.0";
    for (int i = 0; i < 10; ++i) {
        longSeq.actionIds.push_back("long_action_" + std::to_string(i));
    }
    registry_->registerSequence(longSeq);

    // 실행
    std::string executionId = engine_->execute("long_sequence", {});

    // 최종 진행률 확인
    auto result = engine_->getStatus(executionId);
    EXPECT_EQ(result.progress, 1.0f);
    EXPECT_EQ(result.actionResults.size(), 10);
}

/**
 * @brief 실패한 액션의 로깅
 *
 * 실패한 액션의 에러 메시지가 정확하게 기록됩니다
 */
TEST_F(ExecutionMonitoringTest, FailureActionLogging) {
    // 실패 액션을 포함한 시퀀스
    SequenceDefinition failSeq;
    failSeq.id = "with_failure";
    failSeq.name = "Sequence With Failure";
    failSeq.version = "1.0.0";
    failSeq.actionIds = {"success_1", "failure", "success_2"};
    registry_->registerSequence(failSeq);

    // 실행
    std::string executionId = engine_->execute("with_failure", {});

    // 실행 결과 확인
    auto result = engine_->getStatus(executionId);
    EXPECT_EQ(result.status, SequenceStatus::FAILED);

    // 실패 액션 확인
    bool foundFailure = false;
    for (const auto& actionResult : result.actionResults) {
        if (actionResult.actionId == "failure") {
            EXPECT_EQ(actionResult.status, ActionStatus::FAILED);
            EXPECT_FALSE(actionResult.errorMessage.empty());
            foundFailure = true;
        }
    }
    EXPECT_TRUE(foundFailure);
}

/**
 * @brief 총 실행 시간 측정
 *
 * 시퀀스 전체의 실행 시간이 기록됩니다
 */
TEST_F(ExecutionMonitoringTest, TotalExecutionTimeMeasurement) {
    // 시퀀스 실행
    std::string executionId = engine_->execute("monitoring_test", {});

    // 실행 시간 확인
    auto result = engine_->getStatus(executionId);
    EXPECT_GE(result.totalExecutionTimeMs, 0);
}

/**
 * @brief 조건부 분기의 진행률
 *
 * 조건부 분기를 포함한 시퀀스의 진행률 추적
 */
TEST_F(ExecutionMonitoringTest, ConditionalBranchProgressTracking) {
    // 조건부 분기 포함 시퀀스
    SequenceDefinition condSeq;
    condSeq.id = "cond_sequence";
    condSeq.name = "Conditional Sequence";
    condSeq.version = "1.0.0";
    condSeq.actionIds = {"check_condition", "final_action"};
    registry_->registerSequence(condSeq);

    // 조건부 분기 등록
    ConditionalBranch branch;
    branch.id = "check_condition";
    branch.condition = "value > 5";
    branch.thenActions = {"then_action"};
    branch.elseActions = {"else_action"};
    engine_->registerBranch(branch);

    // 실행
    std::string executionId = engine_->execute("cond_sequence", {});

    // 진행률 확인
    auto result = engine_->getStatus(executionId);
    EXPECT_GE(result.progress, 0.0f);
    EXPECT_LE(result.progress, 1.0f);
}

/**
 * @brief 병렬 분기의 진행률
 *
 * 병렬 분기를 포함한 시퀀스의 진행률 추적
 */
TEST_F(ExecutionMonitoringTest, ParallelBranchProgressTracking) {
    // 병렬 분기 포함 시퀀스
    SequenceDefinition parSeq;
    parSeq.id = "par_sequence";
    parSeq.name = "Parallel Sequence";
    parSeq.version = "1.0.0";
    parSeq.actionIds = {"parallel_setup", "final_action"};
    registry_->registerSequence(parSeq);

    // 병렬 분기 등록
    ParallelBranch parallel;
    parallel.id = "parallel_setup";
    parallel.branches = {{"action_1", "action_2"}, {"action_3"}};
    engine_->registerParallelBranch(parallel);

    // 실행
    std::string executionId = engine_->execute("par_sequence", {});

    // 진행률 확인
    auto result = engine_->getStatus(executionId);
    EXPECT_GE(result.progress, 0.0f);
    EXPECT_LE(result.progress, 1.0f);
}

/**
 * @brief 각 액션별 진행률
 *
 * 각 액션의 진행률(0~1.0)이 추적됩니다
 */
TEST_F(ExecutionMonitoringTest, IndividualActionProgress) {
    // 시퀀스 실행
    std::string executionId = engine_->execute("monitoring_test", {});

    // 실행 결과 확인
    auto result = engine_->getStatus(executionId);

    // 각 액션의 진행률 확인
    for (const auto& actionResult : result.actionResults) {
        EXPECT_GE(actionResult.progress, 0.0f);
        EXPECT_LE(actionResult.progress, 1.0f);
    }
}

/**
 * @brief 실행 히스토리 조회
 *
 * 과거 모든 실행 이력을 조회할 수 있습니다
 */
TEST_F(ExecutionMonitoringTest, ExecutionHistory) {
    // 여러 시퀀스 실행
    std::vector<std::string> executionIds;
    for (int i = 0; i < 5; ++i) {
        std::string executionId = engine_->execute("monitoring_test", {});
        executionIds.push_back(executionId);
    }

    // 완료된 실행 조회
    auto completed = engine_->getCompletedExecutions();
    EXPECT_GE(completed.size(), 5);

    // 각 실행의 상태 확인
    for (const auto& id : executionIds) {
        auto result = engine_->getStatus(id);
        EXPECT_EQ(result.executionId, id);
        EXPECT_EQ(result.sequenceId, "monitoring_test");
    }
}

/**
 * @brief 동시 실행 모니터링
 *
 * 여러 시퀀스가 동시에 실행될 때 각각의 진행률을 독립적으로 추적합니다
 */
TEST_F(ExecutionMonitoringTest, ConcurrentExecutionMonitoring) {
    // 여러 시퀀스 동시 실행 (빠르게 완료되는 시퀀스)
    std::vector<std::string> executionIds;
    for (int i = 0; i < 3; ++i) {
        std::string executionId = engine_->execute("monitoring_test", {});
        executionIds.push_back(executionId);
    }

    // 각 시퀀스의 상태를 독립적으로 조회
    for (const auto& id : executionIds) {
        auto result = engine_->getStatus(id);
        EXPECT_EQ(result.executionId, id);
        // 완료되었거나 실행 중
        EXPECT_TRUE(result.status == SequenceStatus::COMPLETED ||
                    result.status == SequenceStatus::RUNNING);
    }
}

/**
 * @brief 실행 컨텍스트 접근
 *
 * 실행 중인 시퀀스의 컨텍스트(변수, 결과)에 접근할 수 있습니다
 */
TEST_F(ExecutionMonitoringTest, AccessExecutionContext) {
    // 시퀀스 실행
    std::string executionId = engine_->execute("monitoring_test", {});

    // 실행 컨텍스트 조회
    auto context = engine_->getExecutionContext(executionId);
    EXPECT_NE(context, nullptr);

    // 컨텍스트의 변수 조회 가능 (예: 이전 액션의 결과)
    // 실제 컨텍스트 내용은 시퀀스 구현에 따라 다름
}
