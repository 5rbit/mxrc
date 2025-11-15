#include <gtest/gtest.h>
#include "core/sequence/core/SequenceEngine.h"
#include <thread>
#include <chrono>
#include "core/action/core/ActionFactory.h"
#include "core/action/core/ActionExecutor.h"
#include "core/action/impl/DelayAction.h"
#include "core/action/impl/MoveAction.h"
#include "core/action/util/ExecutionContext.h"

using namespace mxrc::core::sequence;
using namespace mxrc::core::action;

/**
 * @brief SequenceEngine 단위 테스트 (순차 실행)
 */
class SequenceEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        factory = std::make_shared<ActionFactory>();
        executor = std::make_shared<ActionExecutor>();
        engine = std::make_unique<SequenceEngine>(factory, executor);
        context = std::make_unique<ExecutionContext>();

        // DelayAction 팩토리 등록
        factory->registerFactory("Delay", [](const std::string& id, const auto& params) {
            long delayMs = 50;  // 테스트용 짧은 딜레이
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

    std::shared_ptr<ActionFactory> factory;
    std::shared_ptr<ActionExecutor> executor;
    std::unique_ptr<SequenceEngine> engine;
    std::unique_ptr<ExecutionContext> context;
};

// 단일 Action으로 구성된 Sequence 실행
TEST_F(SequenceEngineTest, ExecuteSingleActionSequence) {
    SequenceDefinition def("seq1", "Single Action Sequence");
    
    ActionStep step("delay1", "Delay");
    step.addParameter("delay_ms", "50");
    def.addStep(step);

    auto result = engine->execute(def, *context);

    EXPECT_EQ(result.sequenceId, "seq1");
    EXPECT_EQ(result.status, SequenceStatus::COMPLETED);
    EXPECT_EQ(result.completedSteps, 1);
    EXPECT_EQ(result.totalSteps, 1);
    EXPECT_FLOAT_EQ(result.progress, 1.0f);
    EXPECT_TRUE(result.isSuccessful());
}

// 여러 Action으로 구성된 Sequence 순차 실행
TEST_F(SequenceEngineTest, ExecuteMultipleActionsSequentially) {
    SequenceDefinition def("seq2", "Multi Action Sequence");
    
    ActionStep step1("delay1", "Delay");
    step1.addParameter("delay_ms", "30");
    def.addStep(step1);

    ActionStep step2("delay2", "Delay");
    step2.addParameter("delay_ms", "30");
    def.addStep(step2);

    ActionStep step3("delay3", "Delay");
    step3.addParameter("delay_ms", "30");
    def.addStep(step3);

    auto result = engine->execute(def, *context);

    EXPECT_EQ(result.status, SequenceStatus::COMPLETED);
    EXPECT_EQ(result.completedSteps, 3);
    EXPECT_EQ(result.totalSteps, 3);
    EXPECT_TRUE(result.isSuccessful());
}

// Action 실패 시 Sequence 중단
TEST_F(SequenceEngineTest, StopOnActionFailure) {
    SequenceDefinition def("seq3", "Failing Sequence");
    
    ActionStep step1("delay1", "Delay");
    step1.addParameter("delay_ms", "30");
    def.addStep(step1);

    // 존재하지 않는 Action 타입
    ActionStep step2("unknown", "UnknownType");
    def.addStep(step2);

    ActionStep step3("delay3", "Delay");
    step3.addParameter("delay_ms", "30");
    def.addStep(step3);

    auto result = engine->execute(def, *context);

    EXPECT_EQ(result.status, SequenceStatus::FAILED);
    EXPECT_EQ(result.completedSteps, 1);  // 첫 번째만 완료
    EXPECT_EQ(result.totalSteps, 3);
    EXPECT_TRUE(result.isFailed());
    EXPECT_FALSE(result.errorMessage.empty());
}

// Sequence 취소
TEST_F(SequenceEngineTest, CancelSequence) {
    SequenceDefinition def("seq4", "Long Sequence");
    
    // 긴 실행 시간의 Action들
    for (int i = 0; i < 10; ++i) {
        ActionStep step("delay" + std::to_string(i), "Delay");
        step.addParameter("delay_ms", "500");
        def.addStep(step);
    }

    // 별도 스레드에서 Sequence 실행
    std::thread execThread([this, &def]() {
        engine->execute(def, *context);
    });

    // 약간 대기 후 취소
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    engine->cancel("seq4");

    execThread.join();

    // 취소되었는지 확인
    auto status = engine->getStatus("seq4");
    EXPECT_EQ(status, SequenceStatus::CANCELLED);
}

// Sequence 취소 시 현재 실행 중인 Action도 즉시 취소되는지 확인
TEST_F(SequenceEngineTest, CancelSequenceImmediatelyCancelsCurrentAction) {
    SequenceDefinition def("seq_immediate_cancel", "Immediate Cancel Test");

    // 긴 실행 시간의 Action들
    for (int i = 0; i < 5; ++i) {
        ActionStep step("delay" + std::to_string(i), "Delay");
        step.addParameter("delay_ms", "2000");  // 2초 지연
        def.addStep(step);
    }

    auto startTime = std::chrono::steady_clock::now();

    // 별도 스레드에서 Sequence 실행
    std::thread execThread([this, &def]() {
        engine->execute(def, *context);
    });

    // 약간 대기 후 취소 (첫 번째 Action이 실행 중일 때)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    engine->cancel("seq_immediate_cancel");

    execThread.join();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);

    // 취소되었는지 확인
    auto status = engine->getStatus("seq_immediate_cancel");
    EXPECT_EQ(status, SequenceStatus::CANCELLED);

    // 실행 시간이 짧은지 확인 (첫 번째 Action이 즉시 취소되어야 함)
    // 100ms 대기 + Action 취소 시간 + 여유 = 1000ms 이내
    // (만약 개선 없이 첫 Action 완료를 기다린다면 2000ms 이상 걸림)
    EXPECT_LT(elapsed.count(), 1000);
}

// 액션별 타임아웃 설정 테스트
TEST_F(SequenceEngineTest, PerActionTimeout) {
    SequenceDefinition def("seq_per_action_timeout", "Per-Action Timeout Test");

    // 첫 번째 액션: 타임아웃 없음 (정상 완료)
    ActionStep step1("delay1", "Delay");
    step1.addParameter("delay_ms", "50");
    def.addStep(step1);

    // 두 번째 액션: 타임아웃 설정 (100ms로 5초 대기 액션 실행 -> 타임아웃)
    ActionStep step2("delay2", "Delay");
    step2.addParameter("delay_ms", "5000");
    step2.setTimeout(std::chrono::milliseconds(100));
    def.addStep(step2);

    auto result = engine->execute(def, *context);

    // 두 번째 스텝에서 타임아웃으로 실패
    EXPECT_EQ(result.status, SequenceStatus::FAILED);
    EXPECT_EQ(result.completedSteps, 1);  // 첫 번째만 완료
    EXPECT_EQ(result.totalSteps, 2);
    EXPECT_FALSE(result.isSuccessful());
}

// 빈 Sequence 실행
TEST_F(SequenceEngineTest, ExecuteEmptySequence) {
    SequenceDefinition def("seq5", "Empty Sequence");
    // 스텝 없음

    auto result = engine->execute(def, *context);

    EXPECT_EQ(result.status, SequenceStatus::COMPLETED);
    EXPECT_EQ(result.completedSteps, 0);
    EXPECT_EQ(result.totalSteps, 0);
    EXPECT_TRUE(result.isSuccessful());
}

// MoveAction이 포함된 Sequence
TEST_F(SequenceEngineTest, ExecuteSequenceWithMoveAction) {
    SequenceDefinition def("seq6", "Move Sequence");
    
    ActionStep step1("move1", "Move");
    step1.addParameter("x", "10.5");
    step1.addParameter("y", "20.3");
    step1.addParameter("z", "5.0");
    def.addStep(step1);

    auto result = engine->execute(def, *context);

    EXPECT_EQ(result.status, SequenceStatus::COMPLETED);
    EXPECT_EQ(result.completedSteps, 1);

    // Context에 위치 정보가 저장되었는지 확인
    auto posX = context->getVariable("last_position_x");
    ASSERT_TRUE(posX.has_value());
    EXPECT_DOUBLE_EQ(std::any_cast<double>(posX.value()), 10.5);
}

// 진행률 확인
TEST_F(SequenceEngineTest, CheckProgress) {
    SequenceDefinition def("seq7", "Progress Sequence");
    
    for (int i = 0; i < 5; ++i) {
        ActionStep step("delay" + std::to_string(i), "Delay");
        step.addParameter("delay_ms", "100");
        def.addStep(step);
    }

    // 별도 스레드에서 실행
    std::thread execThread([this, &def]() {
        engine->execute(def, *context);
    });

    // 진행 중 진행률 확인
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    float progress = engine->getProgress("seq7");
    
    // 최소한 한 개 이상 완료되었어야 함
    EXPECT_GT(progress, 0.0f);
    EXPECT_LE(progress, 1.0f);

    execThread.join();
}
