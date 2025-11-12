#include "gtest/gtest.h"
#include "../../../src/core/taskmanager/factory/TaskFactory.h" // Corrected include
#include "../../../src/core/taskmanager/tasks/DummyTask.h" // Corrected include
#include "../../../src/core/taskmanager/interfaces/ITask.h" // Corrected include
#include <memory>
#include <map>
#include <string>

// Test fixture for TaskFactory
class TaskFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure the factory is clean before each test
        // This is a bit tricky with a singleton. For proper isolation,
        // one might need to reset the internal state or use dependency injection.
        // For this example, we'll assume tests don't interfere too much.
        // In a real scenario, consider a test-specific factory instance or a reset mechanism.
    }
};

// Test case for singleton instance
TEST_F(TaskFactoryTest, IsSingleton) {
    TaskFactory& instance1 = TaskFactory::getInstance();
    TaskFactory& instance2 = TaskFactory::getInstance();
    ASSERT_EQ(&instance1, &instance2);
}

// Test case for registering a task type
TEST_F(TaskFactoryTest, RegisterTaskType) {
    TaskFactory& factory = TaskFactory::getInstance();
    
    // Register DummyTask
    factory.registerTaskType("DummyTask", [](const std::map<std::string, std::string>& params) {
        return std::make_unique<DummyTask>(params);
    });

    // Try to create it
    std::map<std::string, std::string> params = {{"key", "value"}};
    std::unique_ptr<ITask> task = factory.createTask("DummyTask", params);
    ASSERT_NE(task, nullptr);
    ASSERT_EQ(task->getType(), "DummyTask");
    ASSERT_EQ(task->getParameters().at("key"), "value");
}

// Test case for creating an unknown task type
TEST_F(TaskFactoryTest, CreateUnknownTaskTypeThrowsException) {
    TaskFactory& factory = TaskFactory::getInstance();
    std::map<std::string, std::string> params;
    ASSERT_THROW(factory.createTask("UnknownTask", params), std::runtime_error);
}

// Test case for creating multiple tasks of the same type
TEST_F(TaskFactoryTest, CreateMultipleTasksOfSameType) {
    TaskFactory& factory = TaskFactory::getInstance();
    
    // Ensure DummyTask is registered (might be from previous test, but good to be explicit)
    factory.registerTaskType("DummyTask", [](const std::map<std::string, std::string>& params) {
        return std::make_unique<DummyTask>(params);
    });

    std::map<std::string, std::string> params1 = {{"id", "1"}};
    std::unique_ptr<ITask> task1 = factory.createTask("DummyTask", params1);
    ASSERT_NE(task1, nullptr);
    ASSERT_EQ(task1->getParameters().at("id"), "1");

    std::map<std::string, std::string> params2 = {{"id", "2"}};
    std::unique_ptr<ITask> task2 = factory.createTask("DummyTask", params2);
    ASSERT_NE(task2, nullptr);
    ASSERT_EQ(task2->getParameters().at("id"), "2");

    // Ensure they are different instances
    ASSERT_NE(task1.get(), task2.get());
}
