// 이 파일은 TaskDefinitionRegistry 클래스를 테스트합니다.
// TaskDefinitionRegistry는 다양한 유형의 태스크(Task) 정의를 등록하고,
// 등록된 정의를 기반으로 태스크 인스턴스를 생성하는 역할을 합니다.
// 이 테스트 모음은 태스크 정의의 등록, 생성, 조회 및
// 존재하지 않는 태스크에 대한 처리와 같은 핵심 기능이 올바르게 작동하는지 검증합니다.

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

TEST(TaskDefinitionRegistryTest, RegisterAndCreateTask) {
    TaskDefinitionRegistry registry;

    // 태스크 정의 등록
    registry.registerDefinition("TestTask",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id, type);
        });

    // 등록된 정의를 사용하여 태스크 생성
    std::shared_ptr<ITask> task = registry.createTask("TestTask", "test_id_1", "TestTask", {});

    // 생성된 태스크 확인
    ASSERT_NE(task, nullptr);
    ASSERT_EQ(task->getId(), "test_id_1");
    ASSERT_EQ(task->getType(), "TestTask");
}

TEST(TaskDefinitionRegistryTest, CreateNonExistentTask) {
    TaskDefinitionRegistry registry;

    // 등록되지 않은 태스크 생성을 시도
    std::shared_ptr<ITask> task = registry.createTask("NonExistentTask", "test_id", "NonExistentTask", {});

    // 태스크가 생성되지 않았는지 확인
    ASSERT_EQ(task, nullptr);
}

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

TEST(TaskDefinitionRegistryTest, GetDefinition) {
    TaskDefinitionRegistry registry;

    registry.registerDefinition("TaskX",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id, type);
        });

    // 기존 정의 가져오기
    const auto* definition = registry.getDefinition("TaskX");
    ASSERT_NE(definition, nullptr);
    ASSERT_EQ(definition->typeName, "TaskX");

    // 존재하지 않는 정의 가져오기
    const auto* nonExistent = registry.getDefinition("NonExistent");
    ASSERT_EQ(nonExistent, nullptr);
}

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

    // 정의에 기본 매개변수가 저장되었는지 확인
    const auto* definition = registry.getDefinition("ParameterizedTask");
    ASSERT_NE(definition, nullptr);
    ASSERT_EQ(definition->defaultParams.at("param1"), "value1");
    ASSERT_EQ(definition->defaultParams.at("param2"), "value2");
}

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

    // 모든 정의 가져오기
    auto allDefinitions = registry.getAllDefinitions();

    // 3개의 태스크 정의가 있어야 함
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

TEST(TaskDefinitionRegistryTest, TaskCreationWithParameters) {
    TaskDefinitionRegistry registry;

    registry.registerDefinition("ParamTask",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            // 매개변수가 팩토리에 전달되었는지 확인
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

TEST(TaskDefinitionRegistryTest, RegisterDefinitionWithSpecialCharacters) {
    TaskDefinitionRegistry registry;

    // Task names with special characters
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

TEST(TaskDefinitionRegistryTest, ReRegisterDefinitionOverwrites) {
    TaskDefinitionRegistry registry;

    // First registration
    registry.registerDefinition("OverwriteTask",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id, type);
        });

    // Re-register with different factory
    registry.registerDefinition("OverwriteTask",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            return std::make_shared<MockTask>(id + "_new", type);
        });

    auto task = registry.createTask("OverwriteTask", "test_id", "OverwriteTask", {});

    // Should use the new factory
    ASSERT_NE(task, nullptr);
    ASSERT_EQ(task->getId(), "test_id_new");
}

TEST(TaskDefinitionRegistryTest, LargeParameterMap) {
    TaskDefinitionRegistry registry;

    registry.registerDefinition("LargeParamTask",
        [](const std::string& id, const std::string& type, const std::map<std::string, std::string>& params) {
            EXPECT_EQ(params.size(), 100);
            return std::make_shared<MockTask>(id, type);
        });

    // Create a large parameter map
    std::map<std::string, std::string> largeParams;
    for (int i = 0; i < 100; ++i) {
        largeParams["param_" + std::to_string(i)] = "value_" + std::to_string(i);
    }

    auto task = registry.createTask("LargeParamTask", "large_id", "LargeParamTask", largeParams);

    ASSERT_NE(task, nullptr);
}

} // namespace mxrc::core::taskmanager