#include "gtest/gtest.h"
#include "core/taskmanager/TaskManager.h"
#include "core/taskmanager/TaskDefinitionRegistry.h"
#include "core/taskmanager/TaskExecutor.h"
#include "core/taskmanager/tasks/SequenceTaskAdapter.h"
#include "core/taskmanager/TaskManagerInit.h" // For initializeTaskManagerModule and getters
#include "core/sequence/core/SequenceEngine.h"
#include "core/sequence/core/SequenceRegistry.h"
#include "core/sequence/impl/DummyActionFactory.h"
#include "core/sequence/dto/SequenceDto.h"
#include "core/sequence/dto/ActionStatus.h"
#include "core/sequence/core/ConditionalBranch.h"
#include "core/sequence/core/ParallelBranch.h"
#include "MockActions.h" // Assuming MockActions are available for sequence testing
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <chrono>
#include <thread>

using namespace mxrc::core::taskmanager;
using namespace mxrc::core::sequence;
using namespace mxrc::core::sequence::testing; // For MockActionFactory and MockAction

class SequenceTaskManagerIntegrationTest : public ::testing::Test {
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
        // Initialize the TaskManager module, which also initializes SequenceEngine
        initializeTaskManagerModule();

        // Retrieve the globally managed instances
        taskManager_ = getGlobalTaskManager();
        sequenceEngine_ = getGlobalSequenceEngine();
        sequenceRegistry_ = std::static_pointer_cast<SequenceRegistry>(sequenceEngine_->getRegistry()); // Assuming getRegistry() exists and returns shared_ptr<SequenceRegistry>
        actionFactory_ = std::static_pointer_cast<IActionFactory>(sequenceEngine_->getActionFactory()); // Assuming getActionFactory() exists and returns shared_ptr<IActionFactory>

        // Register a mock action with the sequence engine's action factory
        // This is crucial for sequences to have actions to execute
        if (auto mockFactory = std::dynamic_pointer_cast<MockActionFactory>(actionFactory_)) {
            mockFactory->registerActionType("MockAction", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
                return std::make_shared<MockAction>(id, type, params);
            });
        }
    }

    void TearDown() override {
        // Clean up global instances if necessary, or rely on shared_ptr destruction
        // For integration tests, it might be better to reset the state explicitly
        // For now, we'll let shared_ptr handle it.
    }

    std::shared_ptr<TaskManager> taskManager_;
    std::shared_ptr<SequenceEngine> sequenceEngine_;
    std::shared_ptr<SequenceRegistry> sequenceRegistry_;
    std::shared_ptr<IActionFactory> actionFactory_;
};

/**
 * @brief Test case: Register and execute a simple sequence as a TaskManager task.
 */
TEST_F(SequenceTaskManagerIntegrationTest, ExecuteSimpleSequenceAsTask) {
    // 1. Define a simple sequence
    SequenceDefinition simpleSeqDef;
    simpleSeqDef.id = "simple_sequence";
    simpleSeqDef.name = "Simple Sequence";
    simpleSeqDef.version = "1.0.0";
    simpleSeqDef.actionIds = {"action_1", "action_2"};

    // 2. Register the sequence with the SequenceRegistry
    ASSERT_NO_THROW(sequenceRegistry_->registerSequence(simpleSeqDef));

    // 3. Register the SequenceTaskAdapter definition with TaskManager
    // This is already done in TaskManagerInit.cpp, but we need to ensure it's available.
    // We'll request a task execution using the "SequenceTask" type.

    // 4. Request execution of the sequence as a task
    std::map<std::string, std::string> taskParams;
    taskParams["sequenceId"] = "simple_sequence";
    taskParams["name"] = "MySimpleSequenceTask";
    taskParams["param_for_action_1"] = "value_1"; // Example parameter

    std::string taskExecutionId = taskManager_->requestTaskExecution("SequenceTask", taskParams);
    ASSERT_FALSE(taskExecutionId.empty());

    // 5. Monitor task status
    std::unique_ptr<TaskDto> taskStatus;
    // Give some time for the sequence to execute (it's synchronous in SequenceEngine for now)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    taskStatus = taskManager_->getTaskExecutionStatus(taskExecutionId);
    ASSERT_TRUE(taskStatus != nullptr);
    EXPECT_EQ(taskStatus->id, taskExecutionId);
    EXPECT_EQ(taskStatus->name, "MySimpleSequenceTask");
    EXPECT_EQ(taskStatus->type, "SequenceTask");
    EXPECT_EQ(taskStatus->status, "COMPLETED"); // Simple sequence should complete quickly
    EXPECT_FLOAT_EQ(taskStatus->progress, 100.0f);

    // Verify sequence engine status directly
    SequenceExecutionResult seqResult = sequenceEngine_->getStatus(taskExecutionId);
    EXPECT_EQ(seqResult.status, SequenceStatus::COMPLETED);
    EXPECT_FLOAT_EQ(seqResult.progress, 1.0f);
    EXPECT_EQ(seqResult.actionResults.size(), 2);
    EXPECT_EQ(seqResult.actionResults[0].actionId, "action_1");
    EXPECT_EQ(seqResult.actionResults[1].actionId, "action_2");
}

/**
 * @brief Test case: Execute a sequence with parameters passed from TaskManager.
 */
TEST_F(SequenceTaskManagerIntegrationTest, ParameterPassingToSequenceTask) {
    // 1. Define a sequence that uses parameters
    SequenceDefinition paramSeqDef;
    paramSeqDef.id = "param_sequence";
    paramSeqDef.name = "Parameter Sequence";
    paramSeqDef.version = "1.0.0";
    paramSeqDef.actionIds = {"action_with_param"};

    ASSERT_NO_THROW(sequenceRegistry_->registerSequence(paramSeqDef));

    // 2. Request execution with parameters
    std::map<std::string, std::string> taskParams;
    taskParams["sequenceId"] = "param_sequence";
    taskParams["name"] = "ParamTestTask";
    taskParams["my_custom_param"] = "hello_world";
    taskParams["another_value"] = "123";

    std::string taskExecutionId = taskManager_->requestTaskExecution("SequenceTask", taskParams);
    ASSERT_FALSE(taskExecutionId.empty());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 3. Verify parameters in SequenceEngine's ExecutionContext
    auto context = sequenceEngine_->getExecutionContext(taskExecutionId);
    ASSERT_TRUE(context != nullptr);

    auto customParam = context->getVariable("my_custom_param");
    ASSERT_TRUE(customParam.has_value());
    EXPECT_EQ(std::any_cast<std::string>(customParam), "hello_world");

    auto anotherValue = context->getVariable("another_value");
    ASSERT_TRUE(anotherValue.has_value());
    EXPECT_EQ(std::any_cast<std::string>(anotherValue), "123"); // Currently all params are strings
}

/**
 * @brief Test case: Cancel a running sequence task.
 */
TEST_F(SequenceTaskManagerIntegrationTest, CancelSequenceTask) {
    // 1. Define a sequence with a long-running action (MockAction with delay)
    SequenceDefinition longRunningSeqDef;
    longRunningSeqDef.id = "long_running_sequence";
    longRunningSeqDef.name = "Long Running Sequence";
    longRunningSeqDef.version = "1.0.0";
    longRunningSeqDef.actionIds = {"long_action"};

    ASSERT_NO_THROW(sequenceRegistry_->registerSequence(longRunningSeqDef));

    // Register a mock action that can be cancelled
    if (auto mockFactory = std::dynamic_pointer_cast<MockActionFactory>(actionFactory_)) {
        mockFactory->registerActionType("LongMockAction", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockAction>(id, type, params, 5000); // 5 second delay
        });
    }

    // 2. Request execution
    std::map<std::string, std::string> taskParams;
    taskParams["sequenceId"] = "long_running_sequence";
    taskParams["name"] = "LongRunningTask";
    taskParams["action_type_long_action"] = "LongMockAction"; // Tell MockActionFactory to create a long action

    std::string taskExecutionId = taskManager_->requestTaskExecution("SequenceTask", taskParams);
    ASSERT_FALSE(taskExecutionId.empty());

    // 3. Wait a short period, then cancel
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Let it start
    taskManager_->executeCommand(std::make_shared<CancelTaskCommand>(*taskManager_, taskExecutionId));

    // 4. Monitor status - should be CANCELLED
    std::unique_ptr<TaskDto> taskStatus;
    // Give some time for cancellation to propagate
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    taskStatus = taskManager_->getTaskExecutionStatus(taskExecutionId);
    ASSERT_TRUE(taskStatus != nullptr);
    EXPECT_EQ(taskStatus->status, "CANCELLED");

    SequenceExecutionResult seqResult = sequenceEngine_->getStatus(taskExecutionId);
    EXPECT_EQ(seqResult.status, SequenceStatus::CANCELLED);
}

/**
 * @brief Test case: Pause and Resume a sequence task.
 */
TEST_F(SequenceTaskManagerIntegrationTest, PauseResumeSequenceTask) {
    // 1. Define a sequence with a long-running action
    SequenceDefinition pausableSeqDef;
    pausableSeqDef.id = "pausable_sequence";
    pausableSeqDef.name = "Pausable Sequence";
    pausableSeqDef.version = "1.0.0";
    pausableSeqDef.actionIds = {"pausable_action_1", "pausable_action_2"};

    ASSERT_NO_THROW(sequenceRegistry_->registerSequence(pausableSeqDef));

    if (auto mockFactory = std::dynamic_pointer_cast<MockActionFactory>(actionFactory_)) {
        mockFactory->registerActionType("PausableMockAction", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockAction>(id, type, params, 200); // 200ms delay per action
        });
    }

    // 2. Request execution
    std::map<std::string, std::string> taskParams;
    taskParams["sequenceId"] = "pausable_sequence";
    taskParams["name"] = "PausableTask";
    taskParams["action_type_pausable_action_1"] = "PausableMockAction";
    taskParams["action_type_pausable_action_2"] = "PausableMockAction";

    std::string taskExecutionId = taskManager_->requestTaskExecution("SequenceTask", taskParams);
    ASSERT_FALSE(taskExecutionId.empty());

    // 3. Wait a short period, then pause
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Let it start
    taskManager_->executeCommand(std::make_shared<PauseTaskCommand>(*taskManager_, taskExecutionId));

    // 4. Monitor status - should be PAUSED
    std::unique_ptr<TaskDto> taskStatus;
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Give time to pause

    taskStatus = taskManager_->getTaskExecutionStatus(taskExecutionId);
    ASSERT_TRUE(taskStatus != nullptr);
    EXPECT_EQ(taskStatus->status, "PAUSED");

    SequenceExecutionResult seqResult = sequenceEngine_->getStatus(taskExecutionId);
    EXPECT_EQ(seqResult.status, SequenceStatus::PAUSED);

    // 5. Resume the task
    taskManager_->executeCommand(std::make_shared<StartTaskCommand>(*taskManager_, taskExecutionId, taskParams)); // StartCommand can also resume

    // 6. Monitor status - should eventually be COMPLETED
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Allow it to complete

    taskStatus = taskManager_->getTaskExecutionStatus(taskExecutionId);
    ASSERT_TRUE(taskStatus != nullptr);
    EXPECT_EQ(taskStatus->status, "COMPLETED");

    seqResult = sequenceEngine_->getStatus(taskExecutionId);
    EXPECT_EQ(seqResult.status, SequenceStatus::COMPLETED);
}

/**
 * @brief Test case: Error handling for invalid sequence ID.
 */
TEST_F(SequenceTaskManagerIntegrationTest, InvalidSequenceIdErrorHandling) {
    // Request execution of a non-existent sequence
    std::map<std::string, std::string> taskParams;
    taskParams["sequenceId"] = "non_existent_sequence";
    taskParams["name"] = "InvalidSequenceTask";

    // Expect an exception when requesting execution
    EXPECT_THROW(taskManager_->requestTaskExecution("SequenceTask", taskParams), std::runtime_error);
}

/**
 * @brief Test case: Error handling for missing sequenceId parameter.
 */
TEST_F(SequenceTaskManagerIntegrationTest, MissingSequenceIdParameterErrorHandling) {
    // Request execution without providing sequenceId parameter
    std::map<std::string, std::string> taskParams;
    taskParams["name"] = "MissingSequenceIdTask";

    // Expect an exception during task creation (in TaskManagerInit's factory lambda)
    EXPECT_THROW(taskManager_->requestTaskExecution("SequenceTask", taskParams), std::runtime_error);
}

