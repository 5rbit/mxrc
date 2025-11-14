// 이 파일은 TaskManager 클래스의 핵심 기능을 테스트합니다.
// TaskManager는 TaskDefinitionRegistry, TaskExecutor와 협력하여
// 태스크의 전체 생명주기를 관리하는 중앙 컨트롤러 역할을 합니다.
// 이 테스트 모음은 커맨드 패턴(Start, Cancel, Pause)을 사용하여
// 태스크를 제어하는 기능과 다양한 시나리오에서의 동작을 검증합니다.

#include "gtest/gtest.h"
#include "core/taskmanager/TaskManager.h"
#include "core/taskmanager/TaskDefinitionRegistry.h"
#include "core/taskmanager/TaskExecutor.h"
#include "core/taskmanager/commands/StartTaskCommand.h"
#include "core/taskmanager/commands/CancelTaskCommand.h"
#include "core/taskmanager/commands/PauseTaskCommand.h"
#include "core/taskmanager/interfaces/ITask.h" 
#include <map>
#include <stdexcept>
#include <chrono>
#include <thread>

namespace mxrc::core::taskmanager {

// TaskManager 상호작용 테스트를 위한 Mock ITask
class MockTaskForManager : public ITask {
public:
    MockTaskForManager(const std::string& id, const std::string& type,
                      const std::map<std::string, std::string>& params)
        : id_(id), name_("MockTask"), type_(type), parameters_(params),
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

    std::string getType() const override { return type_; }
    std::map<std::string, std::string> getParameters() const override { return parameters_; }
    TaskStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    const std::string& getId() const override { return id_; }
    TaskDto toDto() const override { return TaskDto{id_, name_, type_, status_, progress_, parameters_}; }

private:
    std::string id_;
    std::string name_;
    std::string type_;
    std::map<std::string, std::string> parameters_;
    TaskStatus status_;
    float progress_;
};

TEST(TaskManagerTest, ConstructorInitialization) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    
    ASSERT_NO_THROW(TaskManager taskManager(registry, executor));
}

TEST(TaskManagerTest, ExecuteStartTaskCommandSuccessfully) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    std::string taskName = "MyTestTask";
    std::string taskType = "TestTask";
    std::map<std::string, std::string> params{{"key1", "value1"}};

    // 1. 레지스트리에 직접 정의를 등록합니다.
    registry->registerDefinition(taskType,
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& p) {
            return std::make_shared<MockTaskForManager>(id, type, p);
        });

    // 2. TaskManager에 태스크를 등록하여 유효한 taskId를 얻습니다.
    std::string taskId = taskManager.registerTaskDefinition(taskName, taskType, params);
    ASSERT_FALSE(taskId.empty());

    // 3. 얻은 taskId를 사용하여 명령을 생성하고 실행합니다.
    auto startCommand = std::make_shared<StartTaskCommand>(taskManager, taskId, params);
    ASSERT_NO_THROW(taskManager.executeCommand(startCommand));

    // 태스크가 실행될 시간을 줍니다.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 태스크가 실행기에 있고 완료되었는지 확인합니다.
    std::shared_ptr<ITask> executedTask = executor->getTask(taskId);
    ASSERT_NE(executedTask, nullptr);
    ASSERT_EQ(executedTask->getStatus(), TaskStatus::COMPLETED);
    ASSERT_EQ(executedTask->getProgress(), 1.0f);

    auto taskParams = executedTask->getParameters();
    ASSERT_EQ(taskParams["key1"], "value1");
}

TEST(TaskManagerTest, ExecuteStartTaskCommandForNonExistentTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    std::string nonExistentTaskId = "NonExistentTask";
    std::map<std::string, std::string> params;

    auto startCommand = std::make_shared<StartTaskCommand>(taskManager, nonExistentTaskId, params);

    // 테스트: 존재하지 않는 Task 타입으로 시작 시도 시 예외 발생하지 않음 (에러 처리됨)
    ASSERT_NO_THROW(taskManager.executeCommand(startCommand));

    // 테스트: Task가 생성되지 않음
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::shared_ptr<ITask> task = executor->getTask(nonExistentTaskId);
    ASSERT_EQ(task, nullptr);
}

TEST(TaskManagerTest, ExecuteCancelTaskCommand) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    std::string taskName = "CancellableTask";
    std::string taskType = "CancellableTaskType";
    std::map<std::string, std::string> params;

    registry->registerDefinition(taskType,
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& p) {
            return std::make_shared<MockTaskForManager>(id, type, p);
        });

    std::string taskId = taskManager.registerTaskDefinition(taskName, taskType, params);

    auto startCommand = std::make_shared<StartTaskCommand>(taskManager, taskId, params);
    taskManager.executeCommand(startCommand);

    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 시작되도록 잠시 기다립니다.

    // 테스트: Task가 실행 중임을 확인
    std::shared_ptr<ITask> runningTask = executor->getTask(taskId);
    ASSERT_NE(runningTask, nullptr);
    ASSERT_EQ(runningTask->getStatus(), TaskStatus::RUNNING);

    auto cancelCommand = std::make_shared<CancelTaskCommand>(taskManager, taskId);
    ASSERT_NO_THROW(taskManager.executeCommand(cancelCommand));

    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 취소되도록 잠시 기다립니다.

    // 테스트: Task가 취소됨
    std::shared_ptr<ITask> cancelledTask = executor->getTask(taskId);
    ASSERT_NE(cancelledTask, nullptr);
    ASSERT_EQ(cancelledTask->getStatus(), TaskStatus::CANCELLED);
}

TEST(TaskManagerTest, ExecutePauseTaskCommand) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    std::string taskName = "PausableTask";
    std::string taskType = "PausableTaskType";
    std::map<std::string, std::string> params;

    registry->registerDefinition(taskType,
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& p) {
            return std::make_shared<MockTaskForManager>(id, type, p);
        });

    std::string taskId = taskManager.registerTaskDefinition(taskName, taskType, params);

    auto startCommand = std::make_shared<StartTaskCommand>(taskManager, taskId, params);
    taskManager.executeCommand(startCommand);

    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 시작되도록 잠시 기다립니다.

    // 테스트: Task가 실행 중임을 확인
    std::shared_ptr<ITask> runningTask = executor->getTask(taskId);
    ASSERT_NE(runningTask, nullptr);
    ASSERT_EQ(runningTask->getStatus(), TaskStatus::RUNNING);

    auto pauseCommand = std::make_shared<PauseTaskCommand>(taskManager, taskId);
    ASSERT_NO_THROW(taskManager.executeCommand(pauseCommand));

    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 일시정지되도록 잠시 기다립니다.

    // 테스트: Task가 일시정지됨
    std::shared_ptr<ITask> pausedTask = executor->getTask(taskId);
    ASSERT_NE(pausedTask, nullptr);
    ASSERT_EQ(pausedTask->getStatus(), TaskStatus::PAUSED);
}

// ========== 에러 케이스 테스트 ==========

TEST(TaskManagerTest, CancelNonExistentTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    // 테스트: 존재하지 않는 Task 취소 시도
    auto cancelCommand = std::make_shared<CancelTaskCommand>(taskManager, "non_existent_task");
    ASSERT_NO_THROW(taskManager.executeCommand(cancelCommand));
}

TEST(TaskManagerTest, PauseNonExistentTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    // 테스트: 존재하지 않는 Task 일시정지 시도
    auto pauseCommand = std::make_shared<PauseTaskCommand>(taskManager, "non_existent_task");
    ASSERT_NO_THROW(taskManager.executeCommand(pauseCommand));
}

TEST(TaskManagerTest, RegisterTaskDefinitionWithDefaultParams) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    std::string taskType = "TaskWithDefaults";
    std::map<std::string, std::string> defaultParams{{"speed", "1.0"}, {"distance", "10.0"}};

    // 테스트: 기본 파라미터와 함께 Task 정의 등록
    ASSERT_NO_THROW(
        registry->registerDefinition(taskType,
            [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& p) {
                return std::make_shared<MockTaskForManager>(id, type, p);
            }, defaultParams)
    );

    // 테스트: 등록된 정의 조회
    const auto* definition = registry->getDefinition(taskType);
    ASSERT_NE(definition, nullptr);
    ASSERT_EQ(definition->defaultParams.size(), 2);
    ASSERT_EQ(definition->defaultParams.at("speed"), "1.0");
    ASSERT_EQ(definition->defaultParams.at("distance"), "10.0");
}

TEST(TaskManagerTest, GetAllTaskDefinitions) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    // 테스트: 여러 Task 정의 등록
    registry->registerDefinition("Task1",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& p) {
            return std::make_shared<MockTaskForManager>(id, type, p);
        });

    registry->registerDefinition("Task2",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& p) {
            return std::make_shared<MockTaskForManager>(id, type, p);
        });

    // 테스트: 모든 정의 조회
    auto definitions = registry->getAllDefinitions();
    ASSERT_EQ(definitions.size(), 2);
}

TEST(TaskManagerTest, TaskExecutionWithParameters) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    std::string taskName = "ParameterizedTask";
    std::string taskType = "ParameterizedTaskType";
    std::map<std::string, std::string> params{{"param1", "value1"}, {"param2", "value2"}};

    registry->registerDefinition(taskType,
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& p) {
            return std::make_shared<MockTaskForManager>(id, type, p);
        });

    std::string taskId = taskManager.registerTaskDefinition(taskName, taskType, params);

    auto startCommand = std::make_shared<StartTaskCommand>(taskManager, taskId, params);
    taskManager.executeCommand(startCommand);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 테스트: Task가 파라미터와 함께 올바르게 생성되고 실행됨
    std::shared_ptr<ITask> task = executor->getTask(taskId);
    ASSERT_NE(task, nullptr);
    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);

    auto taskParams = task->getParameters();
    ASSERT_EQ(taskParams.size(), 2);
    ASSERT_EQ(taskParams["param1"], "value1");
    ASSERT_EQ(taskParams["param2"], "value2");
}

TEST(TaskManagerTest, CancelCompletedTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    std::string taskName = "QuickTask";
    std::string taskType = "QuickTaskType";
    std::map<std::string, std::string> params;

    registry->registerDefinition(taskType,
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& p) {
            return std::make_shared<MockTaskForManager>(id, type, p);
        });

    std::string taskId = taskManager.registerTaskDefinition(taskName, taskType, params);

    auto startCommand = std::make_shared<StartTaskCommand>(taskManager, taskId, params);
    taskManager.executeCommand(startCommand);

    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Task 완료 대기

    // 테스트: Task 완료 확인
    std::shared_ptr<ITask> task = executor->getTask(taskId);
    ASSERT_NE(task, nullptr);
    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);

    // 테스트: 이미 완료된 Task 취소 시도 (상태 변경 없음)
    auto cancelCommand = std::make_shared<CancelTaskCommand>(taskManager, taskId);
    taskManager.executeCommand(cancelCommand);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 완료된 Task는 취소할 수 없으므로 COMPLETED 상태 유지
    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);
}

TEST(TaskManagerTest, PauseCompletedTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    std::string taskName = "QuickTask2";
    std::string taskType = "QuickTaskType2";
    std::map<std::string, std::string> params;

    registry->registerDefinition(taskType,
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& p) {
            return std::make_shared<MockTaskForManager>(id, type, p);
        });

    std::string taskId = taskManager.registerTaskDefinition(taskName, taskType, params);

    auto startCommand = std::make_shared<StartTaskCommand>(taskManager, taskId, params);
    taskManager.executeCommand(startCommand);

    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Task 완료 대기

    // 테스트: Task 완료 확인
    std::shared_ptr<ITask> task = executor->getTask(taskId);
    ASSERT_NE(task, nullptr);
    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);

    // 테스트: 이미 완료된 Task 일시정지 시도 (상태 변경 없음)
    auto pauseCommand = std::make_shared<PauseTaskCommand>(taskManager, taskId);
    taskManager.executeCommand(pauseCommand);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 완료된 Task는 일시정지할 수 없으므로 COMPLETED 상태 유지
    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);
}

} // namespace mxrc::core::taskmanager
