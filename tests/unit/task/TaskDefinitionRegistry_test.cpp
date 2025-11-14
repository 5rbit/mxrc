#include "gtest/gtest.h"
#include "core/taskmanager/TaskDefinitionRegistry.h"
#include "core/taskmanager/interfaces/ITask.h"
#include <memory>

namespace mxrc::core::taskmanager {

// Mock ITask for testing
class MockTask : public ITask {
public:
    MockTask(const std::string& id, const std::string& name) : id_(id), name_(name), status_(TaskStatus::PENDING), progress_(0.0f) {}
    void execute() override {}
    void cancel() override {}
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
    
    // Register a task definition
    registry.registerDefinition("TestTask", []() {
        return std::make_shared<MockTask>("mock_id_1", "TestTask");
    });

    // Create a task using the registered definition
    std::shared_ptr<ITask> task = registry.createTask("TestTask");

    // Verify the created task
    ASSERT_NE(task, nullptr);
    ASSERT_EQ(task->getId(), "mock_id_1");
    ASSERT_EQ(task->toDto().name, "TestTask");
}

TEST(TaskDefinitionRegistryTest, CreateNonExistentTask) {
    TaskDefinitionRegistry registry;
    
    // Attempt to create a task that is not registered
    std::shared_ptr<ITask> task = registry.createTask("NonExistentTask");

    // Verify that no task was created
    ASSERT_EQ(task, nullptr);
}

TEST(TaskDefinitionRegistryTest, RegisterMultipleTasks) {
    TaskDefinitionRegistry registry;

    registry.registerDefinition("TaskA", []() {
        return std::make_shared<MockTask>("id_A", "TaskA");
    });
    registry.registerDefinition("TaskB", []() {
        return std::make_shared<MockTask>("id_B", "TaskB");
    });

    std::shared_ptr<ITask> taskA = registry.createTask("TaskA");
    std::shared_ptr<ITask> taskB = registry.createTask("TaskB");

    ASSERT_NE(taskA, nullptr);
    ASSERT_EQ(taskA->toDto().name, "TaskA");
    ASSERT_NE(taskB, nullptr);
    ASSERT_EQ(taskB->toDto().name, "TaskB");
}

} // namespace mxrc::core::taskmanager