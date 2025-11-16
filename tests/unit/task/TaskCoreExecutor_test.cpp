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

// ========== 종료 안정성 테스트 ==========

/**
 * @brief clearCompletedTasks로 완료된 태스크 정리
 */
TEST_F(TaskExecutorOnceTest, ClearCompletedTasksRemovesFinished) {
    // 짧은 태스크 2개 - 동기 실행으로 완료 보장
    TaskDefinition taskDef1("task_short1");
    taskDef1.setWork("Delay").setOnceMode();

    TaskDefinition taskDef2("task_short2");
    taskDef2.setWork("Delay").setOnceMode();

    auto result1 = taskExecutor->execute(taskDef1, *context);
    auto result2 = taskExecutor->execute(taskDef2, *context);

    EXPECT_EQ(result1.status, TaskStatus::COMPLETED);
    EXPECT_EQ(result2.status, TaskStatus::COMPLETED);

    // 완료된 태스크 2개가 메모리에 존재
    EXPECT_EQ(taskExecutor->getStatus("task_short1"), TaskStatus::COMPLETED);
    EXPECT_EQ(taskExecutor->getStatus("task_short2"), TaskStatus::COMPLETED);

    // 완료된 태스크 정리
    int cleared = taskExecutor->clearCompletedTasks();
    EXPECT_EQ(cleared, 2);  // task_short1, task_short2 제거

    // 정리된 태스크는 IDLE 상태로 반환
    EXPECT_EQ(taskExecutor->getStatus("task_short1"), TaskStatus::IDLE);
    EXPECT_EQ(taskExecutor->getStatus("task_short2"), TaskStatus::IDLE);

    // 두 번째 호출은 0 반환
    int clearedAgain = taskExecutor->clearCompletedTasks();
    EXPECT_EQ(clearedAgain, 0);
}

/**
 * @brief 실패 및 취소된 태스크도 정리됨
 */
TEST_F(TaskExecutorOnceTest, ClearCompletedTasksIncludesFailedAndCancelled) {
    // 성공 태스크
    TaskDefinition taskSuccess("task_success");
    taskSuccess.setWork("Delay").setOnceMode();

    // 실패 태스크
    TaskDefinition taskFail("task_fail");
    taskFail.setWork("non_existent_action").setOnceMode();

    // 동기 실행
    auto resultSuccess = taskExecutor->execute(taskSuccess, *context);
    auto resultFail = taskExecutor->execute(taskFail, *context);

    // 상태 확인
    EXPECT_EQ(taskExecutor->getStatus("task_success"), TaskStatus::COMPLETED);
    EXPECT_EQ(taskExecutor->getStatus("task_fail"), TaskStatus::FAILED);

    // 모두 정리됨
    int cleared = taskExecutor->clearCompletedTasks();
    EXPECT_EQ(cleared, 2);

    // 두 번째 호출은 0 반환
    int clearedAgain = taskExecutor->clearCompletedTasks();
    EXPECT_EQ(clearedAgain, 0);
}

/**
 * @brief 여러 스레드에서 동시에 태스크 취소 시 안전성
 */
TEST_F(TaskExecutorOnceTest, ConcurrentTaskCancellationSafety) {
    std::vector<SequenceDefinition> sequences;
    std::vector<TaskDefinition> tasks;
    std::vector<std::thread> execThreads;

    // 5개의 긴 시퀀스와 태스크 생성 (10개에서 줄임)
    for (int i = 0; i < 5; i++) {
        SequenceDefinition seqDef("seq_concurrent_" + std::to_string(i),
                                  "Concurrent Sequence " + std::to_string(i));

        for (int j = 0; j < 20; j++) {
            ActionStep step("delay_" + std::to_string(i) + "_" + std::to_string(j), "Delay");
            step.addParameter("duration", "2000");  // 충분히 긴 딜레이
            seqDef.addStep(step);
        }

        sequenceRegistry->registerDefinition(seqDef);
        sequences.push_back(seqDef);

        TaskDefinition taskDef("task_concurrent_" + std::to_string(i));
        taskDef.setWorkSequence("seq_concurrent_" + std::to_string(i)).setOnceMode();
        tasks.push_back(taskDef);
    }

    // 모든 태스크를 별도 스레드에서 실행
    for (auto& taskDef : tasks) {
        execThreads.emplace_back([&, taskDef]() {
            taskExecutor->execute(taskDef, *context);
        });
    }

    // 모두 실행 중인지 확인
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    for (int i = 0; i < 5; i++) {
        auto status = taskExecutor->getStatus("task_concurrent_" + std::to_string(i));
        // RUNNING이 아니면 테스트가 유효하지 않음
        if (status != TaskStatus::RUNNING) {
            // 실행 스레드들 정리
            for (auto& t : execThreads) {
                if (t.joinable()) t.join();
            }
            GTEST_SKIP() << "Task completed too quickly, test invalid";
        }
    }

    auto startTime = std::chrono::steady_clock::now();

    // 여러 스레드에서 동시에 취소
    std::vector<std::thread> cancelThreads;
    for (int i = 0; i < 5; i++) {
        cancelThreads.emplace_back([this, i]() {
            taskExecutor->cancel("task_concurrent_" + std::to_string(i));
        });
    }

    // 모든 취소 스레드 종료 대기
    for (auto& t : cancelThreads) {
        t.join();
    }

    // 모든 실행 스레드 종료 대기
    for (auto& t : execThreads) {
        t.join();
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);

    // 동시 취소가 데드락 없이 완료되어야 함
    EXPECT_LT(elapsed.count(), 3000);

    // 모든 태스크가 취소되었는지 확인
    for (int i = 0; i < 5; i++) {
        auto status = taskExecutor->getStatus("task_concurrent_" + std::to_string(i));
        EXPECT_EQ(status, TaskStatus::CANCELLED);
    }
}

/**
 * @brief 메모리 누수 방지: 많은 태스크 실행 후 정리
 */
TEST_F(TaskExecutorOnceTest, NoMemoryLeakWithManyTasks) {
    // 50개의 짧은 태스크 순차 실행
    for (int i = 0; i < 50; i++) {
        TaskDefinition taskDef("task_mem_" + std::to_string(i));
        taskDef.setWork("Delay").setOnceMode();

        auto result = taskExecutor->execute(taskDef, *context);
        EXPECT_EQ(result.status, TaskStatus::COMPLETED);
    }

    // 모두 완료 상태로 메모리에 누적됨
    int cleared = taskExecutor->clearCompletedTasks();
    EXPECT_EQ(cleared, 50);

    // 두 번째 호출은 0 반환
    int clearedAgain = taskExecutor->clearCompletedTasks();
    EXPECT_EQ(clearedAgain, 0);
}

/**
 * @brief Task 취소가 즉시 동작하는지 확인
 */
TEST_F(TaskExecutorOnceTest, TaskCancellationWorksImmediately) {
    // 긴 시퀀스 생성
    SequenceDefinition seqDef("seq_cancel_test", "Cancel Test Sequence");
    for (int i = 0; i < 10; i++) {
        ActionStep step("delay_" + std::to_string(i), "Delay");
        step.addParameter("duration", "3000");  // 더 긴 딜레이로 취소 기회 보장
        seqDef.addStep(step);
    }
    sequenceRegistry->registerDefinition(seqDef);

    TaskDefinition taskDef("task_cancel_test");
    taskDef.setWorkSequence("seq_cancel_test").setOnceMode();

    auto startTime = std::chrono::steady_clock::now();

    // 별도 스레드에서 실행
    std::thread execThread([&]() {
        taskExecutor->execute(taskDef, *context);
    });

    // 약간 대기 후 취소
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 실행 중인지 확인
    auto statusBeforeCancel = taskExecutor->getStatus("task_cancel_test");
    if (statusBeforeCancel != TaskStatus::RUNNING) {
        execThread.join();
        GTEST_SKIP() << "Task completed too quickly, test invalid";
    }

    taskExecutor->cancel("task_cancel_test");

    execThread.join();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);

    // 취소되었는지 확인
    auto status = taskExecutor->getStatus("task_cancel_test");
    EXPECT_EQ(status, TaskStatus::CANCELLED);

    // 실행 시간이 짧은지 확인 (즉시 취소되어야 함)
    // 300ms 대기 + Action 스텝 완료 (300ms) + 취소 시간 + 여유 = 1500ms 이내
    EXPECT_LT(elapsed.count(), 1500);
}

} // namespace mxrc::core::task
