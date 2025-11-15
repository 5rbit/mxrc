// Task 모듈 TaskExecutor 클래스 테스트.
// Phase 3B-1: 단일 실행 모드 (ONCE) 테스트

#include "gtest/gtest.h"
#include "core/task/core/TaskExecutor.h"
#include "core/task/dto/TaskDefinition.h"
#include "core/action/core/ActionFactory.h"
#include "core/action/core/ActionExecutor.h"
#include "core/action/util/ExecutionContext.h"
#include "core/sequence/core/SequenceEngine.h"
#include "core/sequence/core/SequenceRegistry.h"
#include "core/sequence/dto/SequenceDefinition.h"
#include "core/action/impl/DelayAction.h"
#include <memory>

namespace mxrc::core::task {

using namespace mxrc::core::action;
using namespace mxrc::core::sequence;

/**
 * @brief TaskExecutor 단일 실행 모드 테스트
 */
class TaskExecutorOnceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Action 관련 컴포넌트 초기화
        actionFactory = std::make_shared<ActionFactory>();
        actionExecutor = std::make_shared<ActionExecutor>();

        // Delay Action 팩토리 등록
        actionFactory->registerFactory("Delay", [](const std::string& id, const auto& params) {
            long delayMs = 50;  // 기본값
            auto it = params.find("duration");
            if (it != params.end()) {
                delayMs = std::stol(it->second);
            }
            return std::make_shared<DelayAction>(id, delayMs);
        });

        // Sequence 관련 컴포넌트 초기화
        sequenceRegistry = std::make_shared<SequenceRegistry>();
        sequenceEngine = std::make_shared<SequenceEngine>(
            actionFactory,
            actionExecutor
        );

        // TaskExecutor 초기화
        taskExecutor = std::make_shared<TaskExecutor>(
            actionFactory,
            actionExecutor,
            sequenceEngine
        );

        // ExecutionContext 생성
        context = std::make_shared<ExecutionContext>();
    }

    void TearDown() override {
        context.reset();
        taskExecutor.reset();
        sequenceEngine.reset();
        sequenceRegistry.reset();
        actionExecutor.reset();
        actionFactory.reset();
    }

    std::shared_ptr<ActionFactory> actionFactory;
    std::shared_ptr<ActionExecutor> actionExecutor;
    std::shared_ptr<SequenceRegistry> sequenceRegistry;
    std::shared_ptr<SequenceEngine> sequenceEngine;
    std::shared_ptr<TaskExecutor> taskExecutor;
    std::shared_ptr<ExecutionContext> context;
};

/**
 * @brief 단일 Action 기반 Task 실행 테스트
 */
TEST_F(TaskExecutorOnceTest, ExecuteSingleActionTask) {
    // Action 기반 Task 정의
    TaskDefinition taskDef("task1", "Single Action Task");
    taskDef.setWork("Delay")  // Action 타입을 지정
           .setOnceMode();

    // Task 실행
    auto result = taskExecutor->execute(taskDef, *context);

    // 결과 검증
    EXPECT_EQ(result.taskId, "task1");
    EXPECT_TRUE(result.isSuccessful());
    EXPECT_EQ(result.status, TaskStatus::COMPLETED);
    EXPECT_FLOAT_EQ(result.progress, 1.0f);
}

/**
 * @brief Sequence 기반 Task 실행 테스트
 */
TEST_F(TaskExecutorOnceTest, ExecuteSequenceBasedTask) {
    // Sequence 정의 및 등록
    SequenceDefinition seqDef("seq1", "Test Sequence");
    seqDef.addStep(ActionStep("step1", "Delay").addParameter("duration", "50"));
    seqDef.addStep(ActionStep("step2", "Delay").addParameter("duration", "50"));
    sequenceRegistry->registerDefinition(seqDef);

    // Sequence 기반 Task 정의
    TaskDefinition taskDef("task2", "Sequence Task");
    taskDef.setWorkSequence("seq1")
           .setOnceMode();

    // Task 실행
    auto result = taskExecutor->execute(taskDef, *context);

    // 결과 검증
    EXPECT_EQ(result.taskId, "task2");
    EXPECT_TRUE(result.isSuccessful());
    EXPECT_EQ(result.status, TaskStatus::COMPLETED);
}

/**
 * @brief Task 실행 중 상태 조회 테스트
 */
TEST_F(TaskExecutorOnceTest, GetStatusDuringExecution) {
    TaskDefinition taskDef("task3");
    taskDef.setWork("Delay")
           .setOnceMode();

    // Task 실행 전 상태
    EXPECT_EQ(taskExecutor->getStatus("task3"), TaskStatus::IDLE);

    // Task 실행 (비동기로 실행되면 RUNNING 상태를 확인할 수 있음)
    auto result = taskExecutor->execute(taskDef, *context);

    // Task 실행 후 상태
    auto status = taskExecutor->getStatus("task3");
    EXPECT_TRUE(status == TaskStatus::COMPLETED || status == TaskStatus::RUNNING);
}

/**
 * @brief Task 진행률 조회 테스트
 */
TEST_F(TaskExecutorOnceTest, GetProgressAfterExecution) {
    TaskDefinition taskDef("task4");
    taskDef.setWork("Delay")
           .setOnceMode();

    // Task 실행
    auto result = taskExecutor->execute(taskDef, *context);

    // 진행률 확인
    float progress = taskExecutor->getProgress("task4");
    EXPECT_FLOAT_EQ(progress, 1.0f);
}

/**
 * @brief Task 취소 테스트
 */
TEST_F(TaskExecutorOnceTest, CancelTask) {
    TaskDefinition taskDef("task5");
    taskDef.setWork("Delay")
           .setOnceMode();

    // Task 실행 전 취소 요청 (효과 없음)
    taskExecutor->cancel("task5");

    // Task 실행
    auto result = taskExecutor->execute(taskDef, *context);

    // 취소 후 상태 확인 (이미 완료되었을 수 있음)
    auto status = taskExecutor->getStatus("task5");
    EXPECT_TRUE(
        status == TaskStatus::COMPLETED ||
        status == TaskStatus::CANCELLED
    );
}

/**
 * @brief Task 일시정지 테스트
 */
TEST_F(TaskExecutorOnceTest, PauseTask) {
    TaskDefinition taskDef("task6");
    taskDef.setWork("Delay")
           .setOnceMode();

    // Task 실행
    taskExecutor->execute(taskDef, *context);

    // 일시정지 요청
    taskExecutor->pause("task6");

    // 상태 확인 (이미 완료되었을 수 있음)
    auto status = taskExecutor->getStatus("task6");
    EXPECT_TRUE(
        status == TaskStatus::PAUSED ||
        status == TaskStatus::COMPLETED
    );
}

/**
 * @brief Task 재개 테스트
 */
TEST_F(TaskExecutorOnceTest, ResumeTask) {
    TaskDefinition taskDef("task7");
    taskDef.setWork("Delay")
           .setOnceMode();

    // Task 실행
    taskExecutor->execute(taskDef, *context);

    // 일시정지 후 재개
    taskExecutor->pause("task7");
    taskExecutor->resume("task7");

    // 상태 확인
    auto status = taskExecutor->getStatus("task7");
    EXPECT_TRUE(
        status == TaskStatus::RUNNING ||
        status == TaskStatus::COMPLETED
    );
}

/**
 * @brief 여러 Task 순차 실행 테스트
 */
TEST_F(TaskExecutorOnceTest, ExecuteMultipleTasksSequentially) {
    // Task 1
    TaskDefinition task1("task1");
    task1.setWork("Delay").setOnceMode();
    auto result1 = taskExecutor->execute(task1, *context);
    EXPECT_TRUE(result1.isSuccessful());

    // Task 2
    TaskDefinition task2("task2");
    task2.setWork("Delay").setOnceMode();
    auto result2 = taskExecutor->execute(task2, *context);
    EXPECT_TRUE(result2.isSuccessful());

    // Task 3
    TaskDefinition task3("task3");
    task3.setWork("Delay").setOnceMode();
    auto result3 = taskExecutor->execute(task3, *context);
    EXPECT_TRUE(result3.isSuccessful());

    // 모든 Task 완료 확인
    EXPECT_EQ(taskExecutor->getStatus("task1"), TaskStatus::COMPLETED);
    EXPECT_EQ(taskExecutor->getStatus("task2"), TaskStatus::COMPLETED);
    EXPECT_EQ(taskExecutor->getStatus("task3"), TaskStatus::COMPLETED);
}

/**
 * @brief 존재하지 않는 Action으로 Task 실행 시 실패 테스트
 */
TEST_F(TaskExecutorOnceTest, ExecuteTaskWithNonExistentAction) {
    TaskDefinition taskDef("task_fail");
    taskDef.setWork("non_existent_action")
           .setOnceMode();

    // Task 실행 (예외 발생 또는 실패 상태)
    auto result = taskExecutor->execute(taskDef, *context);

    // 실패 상태 확인
    EXPECT_TRUE(result.isFailed());
    EXPECT_EQ(result.status, TaskStatus::FAILED);
    EXPECT_FALSE(result.errorMessage.empty());
}

/**
 * @brief 존재하지 않는(빈) Sequence로 Task 실행 테스트
 *
 * 등록되지 않은 Sequence는 빈 Sequence로 처리되어 성공합니다.
 */
TEST_F(TaskExecutorOnceTest, ExecuteTaskWithNonExistentSequence) {
    TaskDefinition taskDef("task_seq_empty");
    taskDef.setWorkSequence("non_existent_sequence")
           .setOnceMode();

    // Task 실행
    auto result = taskExecutor->execute(taskDef, *context);

    // 빈 Sequence는 성공으로 처리됨
    EXPECT_TRUE(result.isSuccessful());
    EXPECT_EQ(result.status, TaskStatus::COMPLETED);
}

/**
 * @brief Task 실행 시간 측정 테스트
 */
TEST_F(TaskExecutorOnceTest, MeasureExecutionTime) {
    TaskDefinition taskDef("task_time");
    taskDef.setWork("Delay")
           .setOnceMode();

    // Task 실행
    auto result = taskExecutor->execute(taskDef, *context);

    // 실행 시간 확인 (100ms 정도 소요)
    EXPECT_GT(result.executionTime.count(), 0);
    EXPECT_TRUE(result.isSuccessful());
}

/**
 * @brief ExecutionContext에 변수 전달 테스트
 */
TEST_F(TaskExecutorOnceTest, PassVariablesToContext) {
    // Context에 변수 설정
    context->setVariable("input_value", 42);

    TaskDefinition taskDef("task_context");
    taskDef.setWork("Delay")
           .setOnceMode();

    // Task 실행
    auto result = taskExecutor->execute(taskDef, *context);

    EXPECT_TRUE(result.isSuccessful());

    // Context에서 변수 확인
    auto value = context->getVariable("input_value");
    EXPECT_TRUE(value.has_value());
}

/**
 * @brief Task 정의에서 timeout 설정 테스트
 */
TEST_F(TaskExecutorOnceTest, TaskWithTimeout) {
    TaskDefinition taskDef("task_timeout");
    taskDef.setWork("Delay")
           .setOnceMode()
           .setTimeout(std::chrono::milliseconds(200));

    // Task 실행 (timeout 내에 완료되어야 함)
    auto result = taskExecutor->execute(taskDef, *context);

    EXPECT_TRUE(result.isSuccessful());
    EXPECT_LT(result.executionTime, std::chrono::milliseconds(200));
}

/**
 * @brief Task 정의에서 description 설정 테스트
 */
TEST_F(TaskExecutorOnceTest, TaskWithDescription) {
    TaskDefinition taskDef("task_desc");
    taskDef.setWork("Delay")
           .setOnceMode()
           .setDescription("This is a test task with description");

    // Task 실행
    auto result = taskExecutor->execute(taskDef, *context);

    EXPECT_TRUE(result.isSuccessful());
    EXPECT_EQ(taskDef.description, "This is a test task with description");
}

} // namespace mxrc::core::task
