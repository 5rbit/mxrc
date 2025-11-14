// TaskManager 클래스 테스트.
// TaskDefinitionRegistry, TaskExecutor와 협력하여 태스크 생명주기 관리.
// 커맨드 패턴(Start, Cancel, Pause)을 사용한 태스크 제어 및 시나리오 검증.

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

// TaskManager 테스트용 Mock ITask
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

// 생성자 초기화 테스트
TEST(TaskManagerTest, ConstructorInitialization) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    
    ASSERT_NO_THROW(TaskManager taskManager(registry, executor));
}

// Start 커맨드를 통한 태스크 실행 성공 테스트
TEST(TaskManagerTest, ExecuteStartTaskCommandSuccessfully) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    std::string taskName = "MyTestTask";
    std::string taskType = "TestTask";
    std::map<std::string, std::string> params{{"key1", "value1"}};

    // 1. 레지스트리에 정의 등록
    registry->registerDefinition(taskType,
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& p) {
            return std::make_shared<MockTaskForManager>(id, type, p);
        });

    // 2. TaskManager에 태스크 등록 후 ID 획득
    std::string taskId = taskManager.registerTaskDefinition(taskName, taskType, params);
    ASSERT_FALSE(taskId.empty());

    // 3. 획득한 ID로 커맨드 생성 및 실행
    auto startCommand = std::make_shared<StartTaskCommand>(taskManager, taskId, params);
    ASSERT_NO_THROW(taskManager.executeCommand(startCommand));

    // 태스크 실행 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 실행기에서 태스크 완료 상태 확인
    std::shared_ptr<ITask> executedTask = executor->getTask(taskId);
    ASSERT_NE(executedTask, nullptr);
    ASSERT_EQ(executedTask->getStatus(), TaskStatus::COMPLETED);
    ASSERT_EQ(executedTask->getProgress(), 1.0f);

    auto taskParams = executedTask->getParameters();
    ASSERT_EQ(taskParams["key1"], "value1");
}

// 존재하지 않는 태스크에 대한 Start 커맨드 실행 테스트
TEST(TaskManagerTest, ExecuteStartTaskCommandForNonExistentTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    std::string nonExistentTaskId = "NonExistentTask";
    std::map<std::string, std::string> params;

    auto startCommand = std::make_shared<StartTaskCommand>(taskManager, nonExistentTaskId, params);

    // 존재하지 않는 태스크 시작 (예외 미발생, 내부 처리)
    ASSERT_NO_THROW(taskManager.executeCommand(startCommand));

    // 태스크 미생성 확인
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::shared_ptr<ITask> task = executor->getTask(nonExistentTaskId);
    ASSERT_EQ(task, nullptr);
}

// Cancel 커맨드를 통한 태스크 취소 테스트
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

    // 태스크 시작 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); 

    // 실행 중 상태 확인
    std::shared_ptr<ITask> runningTask = executor->getTask(taskId);
    ASSERT_NE(runningTask, nullptr);
    ASSERT_EQ(runningTask->getStatus(), TaskStatus::RUNNING);

    auto cancelCommand = std::make_shared<CancelTaskCommand>(taskManager, taskId);
    ASSERT_NO_THROW(taskManager.executeCommand(cancelCommand));

    // 태스크 취소 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); 

    // 취소 상태 확인
    std::shared_ptr<ITask> cancelledTask = executor->getTask(taskId);
    ASSERT_NE(cancelledTask, nullptr);
    ASSERT_EQ(cancelledTask->getStatus(), TaskStatus::CANCELLED);
}

// Pause 커맨드를 통한 태스크 일시정지 테스트
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

    // 태스크 시작 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); 

    // 실행 중 상태 확인
    std::shared_ptr<ITask> runningTask = executor->getTask(taskId);
    ASSERT_NE(runningTask, nullptr);
    ASSERT_EQ(runningTask->getStatus(), TaskStatus::RUNNING);

    auto pauseCommand = std::make_shared<PauseTaskCommand>(taskManager, taskId);
    ASSERT_NO_THROW(taskManager.executeCommand(pauseCommand));

    // 태스크 일시정지 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); 

    // 일시정지 상태 확인
    std::shared_ptr<ITask> pausedTask = executor->getTask(taskId);
    ASSERT_NE(pausedTask, nullptr);
    ASSERT_EQ(pausedTask->getStatus(), TaskStatus::PAUSED);
}

// --- 에러 및 경계 조건 테스트 ---

// 존재하지 않는 태스크 취소 시도
TEST(TaskManagerTest, CancelNonExistentTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    auto cancelCommand = std::make_shared<CancelTaskCommand>(taskManager, "non_existent_task");
    ASSERT_NO_THROW(taskManager.executeCommand(cancelCommand));
}

// 존재하지 않는 태스크 일시정지 시도
TEST(TaskManagerTest, PauseNonExistentTask) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    auto pauseCommand = std::make_shared<PauseTaskCommand>(taskManager, "non_existent_task");
    ASSERT_NO_THROW(taskManager.executeCommand(pauseCommand));
}

// 기본 파라미터를 포함한 태스크 정의 등록 테스트
TEST(TaskManagerTest, RegisterTaskDefinitionWithDefaultParams) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    std::string taskType = "TaskWithDefaults";
    std::map<std::string, std::string> defaultParams{{"speed", "1.0"}, {"distance", "10.0"}};

    // 기본 파라미터와 함께 정의 등록
    ASSERT_NO_THROW(
        registry->registerDefinition(taskType,
            [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& p) {
                return std::make_shared<MockTaskForManager>(id, type, p);
            }, defaultParams)
    );

    // 등록된 정의 확인
    const auto* definition = registry->getDefinition(taskType);
    ASSERT_NE(definition, nullptr);
    ASSERT_EQ(definition->defaultParams.size(), 2);
    ASSERT_EQ(definition->defaultParams.at("speed"), "1.0");
    ASSERT_EQ(definition->defaultParams.at("distance"), "10.0");
}

// 모든 태스크 정의 조회 테스트
TEST(TaskManagerTest, GetAllTaskDefinitions) {
    auto registry = std::make_shared<TaskDefinitionRegistry>();
    auto executor = std::make_shared<TaskExecutor>();
    TaskManager taskManager(registry, executor);

    // 여러 태스크 정의 등록
    registry->registerDefinition("Task1",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& p) {
            return std::make_shared<MockTaskForManager>(id, type, p);
        });

    registry->registerDefinition("Task2",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& p) {
            return std::make_shared<MockTaskForManager>(id, type, p);
        });

    // 모든 정의 조회
    auto definitions = registry->getAllDefinitions();
    ASSERT_EQ(definitions.size(), 2);
}

// 파라미터를 사용한 태스크 실행 테스트
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

    // 파라미터를 사용한 태스크 생성 및 실행 확인
    std::shared_ptr<ITask> task = executor->getTask(taskId);
    ASSERT_NE(task, nullptr);
    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);

    auto taskParams = task->getParameters();
    ASSERT_EQ(taskParams.size(), 2);
    ASSERT_EQ(taskParams["param1"], "value1");
    ASSERT_EQ(taskParams["param2"], "value2");
}

// 완료된 태스크 취소 시도 테스트
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

    // 태스크 완료 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); 

    // 완료 상태 확인
    std::shared_ptr<ITask> task = executor->getTask(taskId);
    ASSERT_NE(task, nullptr);
    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);

    // 완료된 태스크 취소 시도 (상태 변경 없음)
    auto cancelCommand = std::make_shared<CancelTaskCommand>(taskManager, taskId);
    taskManager.executeCommand(cancelCommand);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 완료 상태 유지 확인
    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);
}

// 완료된 태스크 일시정지 시도 테스트
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

    // 태스크 완료 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); 

    // 완료 상태 확인
    std::shared_ptr<ITask> task = executor->getTask(taskId);
    ASSERT_NE(task, nullptr);
    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);

    // 완료된 태스크 일시정지 시도 (상태 변경 없음)
    auto pauseCommand = std::make_shared<PauseTaskCommand>(taskManager, taskId);
    taskManager.executeCommand(pauseCommand);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 완료 상태 유지 확인
    ASSERT_EQ(task->getStatus(), TaskStatus::COMPLETED);
}

} // namespace mxrc::core::taskmanager
