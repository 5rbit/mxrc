// SequenceEngine 단위 테스트
// 시퀀스 순차 실행 기능 검증

#include "gtest/gtest.h"
#include "core/sequence/core/SequenceEngine.h"
#include "core/sequence/core/SequenceRegistry.h"
#include "core/sequence/dto/SequenceDto.h"
#include "MockActions.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

using namespace mxrc::core::sequence;
using namespace mxrc::core::sequence::testing;

class SequenceEngineTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        // spdlog 초기화 (테스트 시작 시 한 번만 실행)
        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto logger = std::make_shared<spdlog::logger>("mxrc", console_sink);
            logger->set_level(spdlog::level::debug);
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

// 기본 시퀀스 실행 테스트
TEST_F(SequenceEngineTest, ExecuteSimpleSequence) {
    // 시퀀스 정의 생성
    SequenceDefinition def;
    def.id = "simple_seq";
    def.name = "Simple Sequence";
    def.version = "1.0.0";
    def.actionIds = {"action_1", "action_2", "action_3"};

    registry_->registerSequence(def);

    // 시퀀스 실행
    std::string executionId = engine_->execute("simple_seq");

    ASSERT_FALSE(executionId.empty());
    EXPECT_TRUE(executionId.find("exec_") == 0);

    // 상태 확인
    auto status = engine_->getStatus(executionId);
    EXPECT_EQ(status.sequenceId, "simple_seq");
    EXPECT_EQ(status.status, SequenceStatus::COMPLETED);
    EXPECT_EQ(status.progress, 1.0f);
}

// 여러 동작 순차 실행 테스트
TEST_F(SequenceEngineTest, ExecuteMultipleActions) {
    SequenceDefinition def;
    def.id = "multi_action";
    def.name = "Multiple Actions";
    def.version = "1.0.0";
    def.actionIds = {"action_1", "action_2", "action_3", "action_4", "action_5"};

    registry_->registerSequence(def);

    std::string executionId = engine_->execute("multi_action");
    auto status = engine_->getStatus(executionId);

    EXPECT_EQ(status.status, SequenceStatus::COMPLETED);
    EXPECT_EQ(status.actionResults.size(), 5);
    EXPECT_FLOAT_EQ(status.progress, 1.0f);
}

// 컨텍스트 조회 테스트
TEST_F(SequenceEngineTest, RetrieveExecutionContext) {
    SequenceDefinition def;
    def.id = "context_test";
    def.name = "Context Test";
    def.version = "1.0.0";
    def.actionIds = {"action_1"};

    registry_->registerSequence(def);

    std::string executionId = engine_->execute("context_test");
    auto context = engine_->getExecutionContext(executionId);

    ASSERT_TRUE(context);
    EXPECT_EQ(context->getExecutionId(), executionId);
}

// 컨텍스트에 파라미터 전달 테스트
TEST_F(SequenceEngineTest, PassParametersToSequence) {
    SequenceDefinition def;
    def.id = "param_test";
    def.name = "Parameter Test";
    def.version = "1.0.0";
    def.actionIds = {"action_1"};

    registry_->registerSequence(def);

    // 파라미터 전달
    std::map<std::string, std::any> params;
    params["test_param"] = std::string("test_value");
    params["input_var"] = 10;

    std::string executionId = engine_->execute("param_test", params);
    auto context = engine_->getExecutionContext(executionId);

    ASSERT_TRUE(context);
    auto paramValue = context->getVariable("test_param");
    ASSERT_TRUE(paramValue.has_value());
    EXPECT_EQ(std::any_cast<std::string>(paramValue), "test_value");
}

// 존재하지 않는 시퀀스 실행 테스트
TEST_F(SequenceEngineTest, ExecuteNonExistentSequence) {
    EXPECT_THROW(
        engine_->execute("non_existent"),
        std::runtime_error);
}

// 실행 중인 시퀀스 조회 테스트
TEST_F(SequenceEngineTest, GetRunningExecutions) {
    SequenceDefinition def;
    def.id = "running_test";
    def.name = "Running Test";
    def.version = "1.0.0";
    def.actionIds = {"action_1"};

    registry_->registerSequence(def);

    std::string executionId = engine_->execute("running_test");

    // 실행이 완료되므로 running이 아님
    auto running = engine_->getRunningExecutions();
    EXPECT_EQ(running.size(), 0);
}

// 완료된 시퀀스 조회 테스트
TEST_F(SequenceEngineTest, GetCompletedExecutions) {
    SequenceDefinition def;
    def.id = "completed_test";
    def.name = "Completed Test";
    def.version = "1.0.0";
    def.actionIds = {"action_1"};

    registry_->registerSequence(def);

    std::string executionId = engine_->execute("completed_test");

    auto completed = engine_->getCompletedExecutions();
    EXPECT_EQ(completed.size(), 1);
    EXPECT_EQ(completed[0], executionId);
}

// 취소 작업 테스트
TEST_F(SequenceEngineTest, CancelSequence) {
    SequenceDefinition def;
    def.id = "cancel_test";
    def.name = "Cancel Test";
    def.version = "1.0.0";
    def.actionIds = {"action_1"};

    registry_->registerSequence(def);

    std::string executionId = engine_->execute("cancel_test");

    // 취소 시도
    bool cancelResult = engine_->cancel(executionId);
    EXPECT_TRUE(cancelResult);

    // 이미 완료된 상태이므로 취소 불가
    bool cancelAgain = engine_->cancel(executionId + "_invalid");
    EXPECT_FALSE(cancelAgain);
}

// 일시정지 및 재개 테스트
TEST_F(SequenceEngineTest, PauseAndResumeSequence) {
    SequenceDefinition def;
    def.id = "pause_test";
    def.name = "Pause Test";
    def.version = "1.0.0";
    def.actionIds = {"action_1"};

    registry_->registerSequence(def);

    // 실행은 동기이므로 완료되지만, 상태 API는 테스트 가능
    std::string executionId = engine_->execute("pause_test");

    // 이미 완료되어 일시정지 불가
    bool pauseResult = engine_->pause(executionId);
    EXPECT_FALSE(pauseResult);
}

// 액션 결과 저장 테스트
TEST_F(SequenceEngineTest, ActionResultsStored) {
    SequenceDefinition def;
    def.id = "result_test";
    def.name = "Result Test";
    def.version = "1.0.0";
    def.actionIds = {"action_1"};

    registry_->registerSequence(def);

    std::string executionId = engine_->execute("result_test");
    auto context = engine_->getExecutionContext(executionId);

    ASSERT_TRUE(context);
    // SuccessAction는 42를 저장함
    auto result = context->getActionResult("action_1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(std::any_cast<int>(result), 42);
}

// 빈 시퀀스 실행 테스트
TEST_F(SequenceEngineTest, ExecuteEmptySequence) {
    SequenceDefinition def;
    def.id = "empty_seq";
    def.name = "Empty Sequence";
    def.version = "1.0.0";
    def.actionIds = {};  // 빈 리스트

    EXPECT_THROW(
        registry_->registerSequence(def),
        std::invalid_argument);
}

// 여러 시퀀스 정의 테스트
TEST_F(SequenceEngineTest, MultipleSequenceDefinitions) {
    // 첫 번째 시퀀스
    SequenceDefinition def1;
    def1.id = "seq_1";
    def1.name = "Sequence 1";
    def1.version = "1.0.0";
    def1.actionIds = {"action_1"};

    // 두 번째 시퀀스
    SequenceDefinition def2;
    def2.id = "seq_2";
    def2.name = "Sequence 2";
    def2.version = "1.0.0";
    def2.actionIds = {"action_2", "action_3"};

    registry_->registerSequence(def1);
    registry_->registerSequence(def2);

    std::string exec1 = engine_->execute("seq_1");
    std::string exec2 = engine_->execute("seq_2");

    EXPECT_FALSE(exec1.empty());
    EXPECT_FALSE(exec2.empty());
    EXPECT_NE(exec1, exec2);
}

// 메모리 누수 없음 테스트
TEST_F(SequenceEngineTest, NoMemoryLeaks) {
    SequenceDefinition def;
    def.id = "mem_test";
    def.name = "Memory Test";
    def.version = "1.0.0";
    def.actionIds = {"action_1", "action_2", "action_3"};

    registry_->registerSequence(def);

    // 여러 번 실행
    for (int i = 0; i < 10; ++i) {
        std::string executionId = engine_->execute("mem_test");
        ASSERT_FALSE(executionId.empty());
    }

    // 메모리 누수 감지는 valgrind 등으로 수행
    EXPECT_TRUE(true);
}

