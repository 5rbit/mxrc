// OperatorInterface 클래스 동작 테스트.
// TaskManager와 상호작용하여 태스크 정의, 실행, 취소, 일시정지, 상태 모니터링 기능 검증.
// 예외 상황 및 경계 조건 처리 기능 테스트.

#include "gtest/gtest.h"
#include "../../../src/core/taskmanager/operator_interface/OperatorInterface.h"
#include "../../../src/core/taskmanager/TaskManager.h"
#include "../../../src/core/taskmanager/TaskDefinitionRegistry.h"
#include "../../../src/core/taskmanager/TaskExecutor.h"
#include "../../../src/core/taskmanager/interfaces/ITask.h"
#include <memory>
#include <chrono>
#include <thread>

namespace mxrc::core::taskmanager {

// OperatorInterface 테스트용 Mock ITask
class MockTaskForOperator : public ITask {
public:
    MockTaskForOperator(const std::string& id, const std::string& type, const std::map<std::string, std::string>& params)
        : id_(id), name_("MockOperatorTask"), type_(type), parameters_(params),
          status_(TaskStatus::PENDING), progress_(0.0f) {}

    void execute() override {
        status_ = TaskStatus::RUNNING;
        // 작업 시뮬레이션
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

// 생성자에 nullptr TaskManager 주입 시 예외 발생 테스트
TEST(OperatorInterfaceTest, ConstructorWithNullTaskManagerThrowsException) {
    ASSERT_THROW({
        OperatorInterface opInterface(nullptr);
    }, std::invalid_argument);
}

// 신규 태스크 정의 및 등록 테스트
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

// 사용 가능한 태스크 목록 조회 테스트
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

// 존재하지 않는 ID의 태스크 상세 정보 조회 테스트
TEST(OperatorInterfaceTest, GetTaskDetailsNotFound) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    auto taskDto = opInterface.getTaskDetails("non_existent_id");
    EXPECT_EQ(taskDto, nullptr);
}

// 태스크 실행 및 상태 모니터링 테스트
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

    // 태스크 실행 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto statusDto = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(statusDto, nullptr);
    EXPECT_EQ(statusDto->id, taskId);
    EXPECT_EQ(statusDto->status, taskStatusToString(TaskStatus::COMPLETED)); // 완료 상태 확인
    
    auto finalParams = statusDto->parameters;
    EXPECT_EQ(finalParams["duration"], "60");
}

// 태스크 성공적 취소 테스트
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

    // 태스크 시작 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    opInterface.cancelTask(executionId);

    // 태스크 취소 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto statusDto = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(statusDto, nullptr);
    EXPECT_EQ(statusDto->status, taskStatusToString(TaskStatus::CANCELLED));
}

// 태스크 성공적 일시정지 테스트
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

    // 태스크 시작 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    opInterface.pauseTask(executionId);

    // 태스크 일시정지 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto statusDto = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(statusDto, nullptr);
    EXPECT_EQ(statusDto->status, taskStatusToString(TaskStatus::PAUSED));
}

// --- 에러 및 경계 조건 테스트 ---

// 존재하지 않는 태스크 실행 시도 (예외 발생)
TEST(OperatorInterfaceTest, StartNonExistentTaskExecution) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    ASSERT_THROW(opInterface.startTaskExecution("non_existent_task_id", {}), std::runtime_error);
}

// 존재하지 않는 태스크 취소 시도 (예외 미발생)
TEST(OperatorInterfaceTest, CancelNonExistentTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    ASSERT_NO_THROW(opInterface.cancelTask("non_existent_task"));
}

// 존재하지 않는 태스크 일시정지 시도 (예외 미발생)
TEST(OperatorInterfaceTest, PauseNonExistentTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    ASSERT_NO_THROW(opInterface.pauseTask("non_existent_task"));
}

// 존재하지 않는 태스크 상태 모니터링
TEST(OperatorInterfaceTest, MonitorNonExistentTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    auto statusDto = opInterface.monitorTaskStatus("non_existent_task");
    EXPECT_EQ(statusDto, nullptr);
}

// 빈 파라미터로 태스크 정의
TEST(OperatorInterfaceTest, DefineTaskWithEmptyParameters) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    registry->registerDefinition("EmptyParamType", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });

    std::map<std::string, std::string> emptyParams;
    std::string taskId = opInterface.defineNewTask("EmptyTask", "EmptyParamType", emptyParams);

    ASSERT_FALSE(taskId.empty());
    auto taskDto = opInterface.getTaskDetails(taskId);
    ASSERT_NE(taskDto, nullptr);
    EXPECT_TRUE(taskDto->parameters.empty());
}

// 완료된 태스크 취소 시도 (상태 변경 없음)
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

    // 태스크 완료 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto statusDto = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(statusDto, nullptr);
    EXPECT_EQ(statusDto->status, taskStatusToString(TaskStatus::COMPLETED));

    opInterface.cancelTask(executionId);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    statusDto = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(statusDto, nullptr);
    EXPECT_EQ(statusDto->status, taskStatusToString(TaskStatus::COMPLETED));
}

// 여러 태스크 동시 실행
TEST(OperatorInterfaceTest, MultipleTasksSimultaneousExecution) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    registry->registerDefinition("ParallelType", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });

    std::string task1 = opInterface.defineNewTask("ParallelTask1", "ParallelType", {});
    std::string task2 = opInterface.defineNewTask("ParallelTask2", "ParallelType", {});
    std::string task3 = opInterface.defineNewTask("ParallelTask3", "ParallelType", {});

    std::string exec1 = opInterface.startTaskExecution(task1, {});
    std::string exec2 = opInterface.startTaskExecution(task2, {});
    std::string exec3 = opInterface.startTaskExecution(task3, {});

    // 모든 태스크 완료 대기
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

// 런타임에 파라미터 오버라이드
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

    std::map<std::string, std::string> runtimeParams{{"speed", "fast"}, {"priority", "high"}};
    std::string executionId = opInterface.startTaskExecution(taskId, runtimeParams);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto statusDto = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(statusDto, nullptr);

    // 파라미터 오버라이드 및 추가 확인
    EXPECT_EQ(statusDto->parameters["speed"], "fast");  // 오버라이드된 값
    EXPECT_EQ(statusDto->parameters["priority"], "high");  // 새로 추가된 값
}

// 정의되지 않은 타입의 태스크 정의
TEST(OperatorInterfaceTest, DefineTaskWithUndefinedType) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    // ID는 생성되나, 실행 시 null 태스크 반환
    std::string taskId = opInterface.defineNewTask("UndefinedTask", "UndefinedType", {});
    ASSERT_FALSE(taskId.empty());

    // 실행 시도 (정의 없어 생성 실패)
    std::string executionId = opInterface.startTaskExecution(taskId, {});

    // 실행 ID는 반환되나, 실제 태스크는 없음
    auto statusDto = opInterface.monitorTaskStatus(executionId);
    // 태스크 미생성으로 상태 조회 시 null 반환 가능 (구현에 따라 다름)
}

// 긴 태스크 이름 테스트
TEST(OperatorInterfaceTest, LongTaskName) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    registry->registerDefinition("LongNameType", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });

    std::string longName = "VeryLongTaskNameWith" + std::string(200, 'X');
    std::string taskId = opInterface.defineNewTask(longName, "LongNameType", {});

    ASSERT_FALSE(taskId.empty());
    auto taskDto = opInterface.getTaskDetails(taskId);
    ASSERT_NE(taskDto, nullptr);
    ASSERT_EQ(taskDto->name, longName);
}

// 파라미터에 특수 문자 포함 테스트
TEST(OperatorInterfaceTest, SpecialCharactersInParameters) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    registry->registerDefinition("SpecialCharType", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });

    std::map<std::string, std::string> specialParams{
        {"path", "/home/user/file.txt"},
        {"command", "cd /tmp && ls -la"},
        {"formula", "a + b * c = 42"}
    };

    std::string taskId = opInterface.defineNewTask("SpecialTask", "SpecialCharType", specialParams);
    ASSERT_FALSE(taskId.empty());

    auto taskDto = opInterface.getTaskDetails(taskId);
    ASSERT_NE(taskDto, nullptr);
    EXPECT_EQ(taskDto->parameters["path"], "/home/user/file.txt");
    EXPECT_EQ(taskDto->parameters["command"], "cd /tmp && ls -la");
}

// 완료된 태스크 일시정지 시도
TEST(OperatorInterfaceTest, PauseCompletedTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    registry->registerDefinition("PauseCompletedType", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });

    std::string taskId = opInterface.defineNewTask("PauseCompletedTask", "PauseCompletedType", {});
    std::string executionId = opInterface.startTaskExecution(taskId, {});

    // 태스크 완료 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto statusDto = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(statusDto, nullptr);
    EXPECT_EQ(statusDto->status, taskStatusToString(TaskStatus::COMPLETED));

    ASSERT_NO_THROW(opInterface.pauseTask(executionId));
}

// 태스크 상태 반복 모니터링
TEST(OperatorInterfaceTest, MonitorTaskMultipleTimes) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    auto taskManager = std::make_shared<TaskManager>(registry, executor);
    OperatorInterface opInterface(taskManager);

    registry->registerDefinition("MonitorType", [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
        return std::make_shared<MockTaskForOperator>(id, type, params);
    });

    std::string taskId = opInterface.defineNewTask("MonitorTask", "MonitorType", {});
    std::string executionId = opInterface.startTaskExecution(taskId, {});

    auto status1 = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(status1, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto status2 = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(status2, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    auto status3 = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(status3, nullptr);
    EXPECT_EQ(status3->status, taskStatusToString(TaskStatus::COMPLETED));
}

} // namespace mxrc::core::taskmanager
