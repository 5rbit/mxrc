#include "gtest/gtest.h"
#include "../../../src/core/taskmanager/operator_interface/OperatorInterface.h" // Corrected include
#include "../../../src/core/taskmanager/TaskManager.h" // Corrected include
#include "../../../src/core/taskmanager/TaskDefinitionRegistry.h" // Include for registry
#include "../../../src/core/taskmanager/TaskExecutor.h" // Include for executor
#include "../../../src/core/taskmanager/interfaces/ITask.h" // For MockTask
#include <memory>
#include <chrono>
#include <thread>

namespace mxrc::core::taskmanager {

// Mock ITask for testing OperatorInterface interactions
class MockTaskForOperator : public ITask {
public:
    MockTaskForOperator(const std::string& id, const std::string& type, const std::map<std::string, std::string>& params)
        : id_(id), name_("MockOperatorTask"), type_(type), parameters_(params),
          status_(TaskStatus::PENDING), progress_(0.0f) {}

    void execute() override {
        status_ = TaskStatus::RUNNING;
        // Simulate some work
        for (int i = 0; i <= 10; ++i) {
            if (status_ == TaskStatus::CANCELLED || status_ == TaskStatus::PAUSED) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            progress_ = static_cast<float>(i) / 10.0f;
        }
        status_ = TaskStatus::COMPLETED;
    }

    void cancel() override {
        if (status_ == TaskStatus::RUNNING || status_ == TaskStatus::PENDING || status_ == TaskStatus::PAUSED) {
            status_ = TaskStatus::CANCELLED;
        }
    }

    void pause() override {
        if (status_ == TaskStatus::RUNNING) {
            status_ = TaskStatus::PAUSED;
        }
    }

    TaskStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    const std::string& getId() const override { return id_; }
    std::string getType() const override { return type_; }
    std::map<std::string, std::string> getParameters() const override { return parameters_; }
    TaskDto toDto() const override {
        return TaskDto{id_, name_, type_, status_, progress_, parameters_};
    }

private:
    std::string id_;
    std::string name_;
    std::string type_;
    std::map<std::string, std::string> parameters_;
    TaskStatus status_;
    float progress_;
};

// OperatorInterface 생성 시 nullptr TaskManager 주입 시 예외 처리를 테스트합니다.
TEST(OperatorInterfaceTest, ConstructorWithNullTaskManagerThrowsException) {
    ASSERT_THROW({
        OperatorInterface opInterface(nullptr);
    }, std::invalid_argument);
}

// OperatorInterface를 통한 새로운 Task 정의 등록 기능을 성공적으로 테스트합니다.
TEST(OperatorInterfaceTest, DefineNewTaskSuccessfully) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    registry->registerDefinition("OpTypeA", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });

    std::map<std::string, std::string> params = {{"setting", "value"}};
    std::string taskId = opInterface.defineNewTask("OpTestTask1", "OpTypeA", params);

    ASSERT_FALSE(taskId.empty());
    auto taskDto = opInterface.getTaskDetails(taskId);
    ASSERT_NE(taskDto, nullptr);
    EXPECT_EQ(taskDto->name, "OpTestTask1");
    EXPECT_EQ(taskDto->type, "OpTypeA");
    EXPECT_EQ(taskDto->parameters.at("setting"), "value");
}

// OperatorInterface를 통한 사용 가능한 Task 목록 조회 기능을 테스트합니다.
TEST(OperatorInterfaceTest, GetAvailableTasks) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    registry->registerDefinition("TypeX", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });
    registry->registerDefinition("TypeY", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });

    opInterface.defineNewTask("OpTaskA", "TypeX", {});
    opInterface.defineNewTask("OpTaskB", "TypeY", {});

    std::vector<TaskDto> tasks = opInterface.getAvailableTasks();
    EXPECT_EQ(tasks.size(), 2);
}

// OperatorInterface를 통해 존재하지 않는 ID로 Task 상세 정보 조회 시를 테스트합니다.
TEST(OperatorInterfaceTest, GetTaskDetailsNotFound) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    auto taskDto = opInterface.getTaskDetails("non_existent_id");
    EXPECT_EQ(taskDto, nullptr);
}

// OperatorInterface를 통한 Task 실행 요청 및 상태 모니터링 기능을 테스트합니다.
TEST(OperatorInterfaceTest, StartTaskExecutionAndMonitorStatus) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    registry->registerDefinition("RunType", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });

    std::string taskId = opInterface.defineNewTask("RunMeTask", "RunType", {});
    std::map<std::string, std::string> runtimeParams = {{"duration", "60"}};

    std::string executionId = opInterface.startTaskExecution(taskId, runtimeParams);
    ASSERT_EQ(executionId, taskId);

    // Give some time for the task to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto statusDto = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(statusDto, nullptr);
    EXPECT_EQ(statusDto->id, taskId);
    EXPECT_EQ(statusDto->status, taskStatusToString(TaskStatus::COMPLETED)); // Should be completed by now
    
    auto finalParams = statusDto->parameters;
    EXPECT_EQ(finalParams["duration"], "60");
}

TEST(OperatorInterfaceTest, CancelTaskSuccessfully) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    registry->registerDefinition("CancellableType", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });

    std::string taskId = opInterface.defineNewTask("CancellableTask", "CancellableType", {});
    std::string executionId = opInterface.startTaskExecution(taskId, {});

    // Give some time for the task to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    opInterface.cancelTask(executionId);

    // Give some time for the task to cancel
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto statusDto = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(statusDto, nullptr);
    EXPECT_EQ(statusDto->status, taskStatusToString(TaskStatus::CANCELLED));
}

TEST(OperatorInterfaceTest, PauseTaskSuccessfully) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    registry->registerDefinition("PausableType", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });

    std::string taskId = opInterface.defineNewTask("PausableTask", "PausableType", {});
    std::string executionId = opInterface.startTaskExecution(taskId, {});

    // Give some time for the task to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    opInterface.pauseTask(executionId);

    // Give some time for the task to pause
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto statusDto = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(statusDto, nullptr);
    EXPECT_EQ(statusDto->status, taskStatusToString(TaskStatus::PAUSED));
}

// ========== 에러 케이스 테스트 ==========

TEST(OperatorInterfaceTest, StartNonExistentTaskExecution) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    // 테스트: 존재하지 않는 Task로 실행 시도 시 예외 발생
    ASSERT_THROW(opInterface.startTaskExecution("non_existent_task_id", {}), std::runtime_error);
}

TEST(OperatorInterfaceTest, CancelNonExistentTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    // 테스트: 존재하지 않는 Task 취소 시도 - 예외 발생하지 않음
    ASSERT_NO_THROW(opInterface.cancelTask("non_existent_task"));
}

TEST(OperatorInterfaceTest, PauseNonExistentTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    // 테스트: 존재하지 않는 Task 일시정지 시도 - 예외 발생하지 않음
    ASSERT_NO_THROW(opInterface.pauseTask("non_existent_task"));
}

TEST(OperatorInterfaceTest, MonitorNonExistentTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    // 테스트: 존재하지 않는 Task 상태 모니터링
    auto statusDto = opInterface.monitorTaskStatus("non_existent_task");
    EXPECT_EQ(statusDto, nullptr);
}

TEST(OperatorInterfaceTest, DefineTaskWithEmptyParameters) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    registry->registerDefinition("EmptyParamType", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });

    // 테스트: 빈 파라미터로 Task 정의
    std::map<std::string, std::string> emptyParams;
    std::string taskId = opInterface.defineNewTask("EmptyTask", "EmptyParamType", emptyParams);

    ASSERT_FALSE(taskId.empty());
    auto taskDto = opInterface.getTaskDetails(taskId);
    ASSERT_NE(taskDto, nullptr);
    EXPECT_TRUE(taskDto->parameters.empty());
}

TEST(OperatorInterfaceTest, CancelCompletedTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    registry->registerDefinition("QuickType", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });

    std::string taskId = opInterface.defineNewTask("QuickTask", "QuickType", {});
    std::string executionId = opInterface.startTaskExecution(taskId, {});

    // Task 완료 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto statusDto = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(statusDto, nullptr);
    EXPECT_EQ(statusDto->status, taskStatusToString(TaskStatus::COMPLETED));

    // 테스트: 완료된 Task 취소 시도 - 상태 변경 없음
    opInterface.cancelTask(executionId);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    statusDto = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(statusDto, nullptr);
    EXPECT_EQ(statusDto->status, taskStatusToString(TaskStatus::COMPLETED));
}

TEST(OperatorInterfaceTest, MultipleTasksSimultaneousExecution) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    registry->registerDefinition("ParallelType", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });

    // 테스트: 여러 Task 동시 실행
    std::string task1 = opInterface.defineNewTask("ParallelTask1", "ParallelType", {});
    std::string task2 = opInterface.defineNewTask("ParallelTask2", "ParallelType", {});
    std::string task3 = opInterface.defineNewTask("ParallelTask3", "ParallelType", {});

    std::string exec1 = opInterface.startTaskExecution(task1, {});
    std::string exec2 = opInterface.startTaskExecution(task2, {});
    std::string exec3 = opInterface.startTaskExecution(task3, {});

    // 모든 Task 완료 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto status1 = opInterface.monitorTaskStatus(exec1);
    auto status2 = opInterface.monitorTaskStatus(exec2);
    auto status3 = opInterface.monitorTaskStatus(exec3);

    ASSERT_NE(status1, nullptr);
    ASSERT_NE(status2, nullptr);
    ASSERT_NE(status3, nullptr);

    EXPECT_EQ(status1->status, taskStatusToString(TaskStatus::COMPLETED));
    EXPECT_EQ(status2->status, taskStatusToString(TaskStatus::COMPLETED));
    EXPECT_EQ(status3->status, taskStatusToString(TaskStatus::COMPLETED));
}

TEST(OperatorInterfaceTest, TaskParameterOverrideAtRuntime) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    registry->registerDefinition("OverrideType", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });

    std::map<std::string, std::string> defaultParams{{"speed", "slow"}, {"mode", "auto"}};
    std::string taskId = opInterface.defineNewTask("OverrideTask", "OverrideType", defaultParams);

    // 테스트: 런타임 파라미터로 기본값 오버라이드
    std::map<std::string, std::string> runtimeParams{{"speed", "fast"}, {"priority", "high"}};
    std::string executionId = opInterface.startTaskExecution(taskId, runtimeParams);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto statusDto = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(statusDto, nullptr);

    // 런타임 파라미터가 기본값을 오버라이드했는지 확인
    EXPECT_EQ(statusDto->parameters["speed"], "fast");  // 오버라이드됨
    EXPECT_EQ(statusDto->parameters["priority"], "high");  // 새로 추가됨
}

} // namespace mxrc::core::taskmanager
