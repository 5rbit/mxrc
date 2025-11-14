// TaskDefinitionRegistry 클래스 테스트.
// 태스크 정의 등록, 생성, 조회 및 예외 처리 기능 검증.
// 다양한 태스크 정의 및 생성 시나리오 테스트.

#include "gtest/gtest.h"
#include "core/taskmanager/TaskDefinitionRegistry.h"
#include "core/taskmanager/interfaces/ITask.h"
#include <memory>

namespace mxrc::core::taskmanager {

// 테스트용 Mock ITask
class MockTask : public ITask {
public:
    MockTask(const std::string& id, const std::string& name)
        : id_(id), name_(name), status_(TaskStatus::PENDING), progress_(0.0f) {}

    void execute() override {}
    void cancel() override {}
    void pause() override {}

    std::string getType() const override { return name_; }
    std::map<std::string, std::string> getParameters() const override { return {}; }

    TaskStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    const std::string& getId() const override { return id_; }
    TaskDto toDto() const override { return TaskDto{id_, name_, status_, progress_}; }

private:
    std::string id_;
    std::string name_;
    TaskStatus status_;
    float progress_;
};

// 태스크 정의 등록 및 생성 기본 테스트
TEST(TaskDefinitionRegistryTest, RegisterAndCreateTask) {
    TaskDefinitionRegistry registry;

    // 태스크 정의 등록
    registry.registerDefinition("TestTask",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id, type);
        });

    // 등록된 정의로 태스크 생성
    std::shared_ptr<ITask> task = registry.createTask("TestTask", "test_id_1", "TestTask", {});

    // 생성된 태스크 확인
    ASSERT_NE(task, nullptr);
    ASSERT_EQ(task->getId(), "test_id_1");
    ASSERT_EQ(task->getType(), "TestTask");
}

// 존재하지 않는 태스크 생성 시도 테스트
TEST(TaskDefinitionRegistryTest, CreateNonExistentTask) {
    TaskDefinitionRegistry registry;

    // 미등록 태스크 생성 시도
    std::shared_ptr<ITask> task = registry.createTask("NonExistentTask", "test_id", "NonExistentTask", {});

    // 태스크 미생성 확인
    ASSERT_EQ(task, nullptr);
}

// 여러 태스크 정의 및 생성 테스트
TEST(TaskDefinitionRegistryTest, RegisterMultipleTasks) {
    TaskDefinitionRegistry registry;

    registry.registerDefinition("TaskA",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id, type);
        });
    registry.registerDefinition("TaskB",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id, type);
        });

    std::shared_ptr<ITask> taskA = registry.createTask("TaskA", "id_A", "TaskA", {});
    std::shared_ptr<ITask> taskB = registry.createTask("TaskB", "id_B", "TaskB", {});

    ASSERT_NE(taskA, nullptr);
    ASSERT_EQ(taskA->getType(), "TaskA");
    ASSERT_NE(taskB, nullptr);
    ASSERT_EQ(taskB->getType(), "TaskB");
}

// 특정 태스크 정의 조회 테스트
TEST(TaskDefinitionRegistryTest, GetDefinition) {
    TaskDefinitionRegistry registry;

    registry.registerDefinition("TaskX",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id, type);
        });

    // 기존 정의 조회
    const auto* definition = registry.getDefinition("TaskX");
    ASSERT_NE(definition, nullptr);
    ASSERT_EQ(definition->typeName, "TaskX");

    // 미존재 정의 조회
    const auto* nonExistent = registry.getDefinition("NonExistent");
    ASSERT_EQ(nonExistent, nullptr);
}

// 기본 매개변수를 포함한 태스크 정의 테스트
TEST(TaskDefinitionRegistryTest, RegisterWithDefaultParams) {
    TaskDefinitionRegistry registry;

    std::map<std::string, std::string> defaultParams = {
        {"param1", "value1"},
        {"param2", "value2"}
    };

    registry.registerDefinition("ParameterizedTask",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id, type);
        },
        defaultParams);

    // 기본 매개변수 저장 확인
    const auto* definition = registry.getDefinition("ParameterizedTask");
    ASSERT_NE(definition, nullptr);
    ASSERT_EQ(definition->defaultParams.at("param1"), "value1");
    ASSERT_EQ(definition->defaultParams.at("param2"), "value2");
}

// 모든 태스크 정의 조회 테스트
TEST(TaskDefinitionRegistryTest, GetAllDefinitions) {
    TaskDefinitionRegistry registry;

    registry.registerDefinition("Task1",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id, type);
        });

    registry.registerDefinition("Task2",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id, type);
        });

    registry.registerDefinition("Task3",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id, type);
        });

    // 모든 정의 조회
    auto allDefinitions = registry.getAllDefinitions();

    // 3개 정의 확인
    ASSERT_EQ(allDefinitions.size(), 3);

    // 태스크 유형 확인
    std::vector<std::string> types;
    for (const auto& def : allDefinitions) {
        types.push_back(def.type);
    }

    ASSERT_TRUE(std::find(types.begin(), types.end(), "Task1") != types.end());
    ASSERT_TRUE(std::find(types.begin(), types.end(), "Task2") != types.end());
    ASSERT_TRUE(std::find(types.begin(), types.end(), "Task3") != types.end());
}

// 매개변수를 사용한 태스크 생성 테스트
TEST(TaskDefinitionRegistryTest, TaskCreationWithParameters) {
    TaskDefinitionRegistry registry;

    registry.registerDefinition("ParamTask",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            // 팩토리에 매개변수 전달 확인
            EXPECT_EQ(params.at("velocity"), "100");
            EXPECT_EQ(params.at("direction"), "forward");
            return std::make_shared<MockTask>(id, type);
        });

    std::map<std::string, std::string> params = {
        {"velocity", "100"},
        {"direction", "forward"}
    };

    auto task = registry.createTask("ParamTask", "param_task_1", "ParamTask", params);

    ASSERT_NE(task, nullptr);
    ASSERT_EQ(task->getId(), "param_task_1");
}

// 특수 문자가 포함된 이름의 태스크 정의 테스트
TEST(TaskDefinitionRegistryTest, RegisterDefinitionWithSpecialCharacters) {
    TaskDefinitionRegistry registry;

    // 특수 문자가 포함된 태스크 이름
    registry.registerDefinition("Task-With-Dash",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id, type);
        });

    registry.registerDefinition("Task_With_Underscore",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id, type);
        });

    auto task1 = registry.createTask("Task-With-Dash", "id1", "Task-With-Dash", {});
    auto task2 = registry.createTask("Task_With_Underscore", "id2", "Task_With_Underscore", {});

    ASSERT_NE(task1, nullptr);
    ASSERT_NE(task2, nullptr);
}

// 동일한 이름으로 태스크 재정의 시 덮어쓰기 테스트
TEST(TaskDefinitionRegistryTest, ReRegisterDefinitionOverwrites) {
    TaskDefinitionRegistry registry;

    // 첫 번째 등록
    registry.registerDefinition("OverwriteTask",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id, type);
        });

    // 다른 팩토리로 재등록
    registry.registerDefinition("OverwriteTask",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id + "_new", type);
        });

    auto task = registry.createTask("OverwriteTask", "test_id", "OverwriteTask", {});

    // 새 팩토리 사용 확인
    ASSERT_NE(task, nullptr);
    ASSERT_EQ(task->getId(), "test_id_new");
}

// 많은 수의 매개변수를 사용한 태스크 생성 테스트
TEST(TaskDefinitionRegistryTest, LargeParameterMap) {
    TaskDefinitionRegistry registry;

    registry.registerDefinition("LargeParamTask",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            EXPECT_EQ(params.size(), 100);
            return std::make_shared<MockTask>(id, type);
        });

    // 대규모 파라미터 맵 생성
    std::map<std::string, std::string> largeParams;
    for (int i = 0; i < 100; ++i) {
        largeParams["param_" + std::to_string(i)] = "value_" + std::to_string(i);
    }

    auto task = registry.createTask("LargeParamTask", "large_id", "LargeParamTask", largeParams);

    ASSERT_NE(task, nullptr);
}

} // namespace mxrc::core::taskmanager