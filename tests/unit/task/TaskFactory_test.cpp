#include "gtest/gtest.h"
#include "core/task/task_mission_management/TaskFactory.h"
#include "core/task/task_mission_management/AbstractTask.h"
#include "core/task/task_mission_management/TaskContext.h"
#include "core/task/task_mission_management/DriveToPositionTask.h"

using namespace mxrc::task_mission;

// Define a dummy task for testing registration
class DummyTask : public AbstractTask {
public:
    DummyTask(const std::string& id) : taskId_(id) {}

    bool initialize(TaskContext& context) override { return true; }
    bool execute(TaskContext& context) override { return true; }
    void terminate(TaskContext& context) override {}
    std::string getTaskId() const override { return taskId_; }

private:
    std::string taskId_;
};

// Test fixture for TaskFactory
class TaskFactoryTest : public ::testing::Test {
protected:
    TaskFactory& factory = TaskFactory::getInstance();

    void SetUp() override {
        // Ensure a clean state for each test
        // Note: TaskFactory is a singleton, so we can't easily reset it.
        // For this test, we'll rely on unique task IDs or careful registration.
    }
};

TEST_F(TaskFactoryTest, RegisterAndCreateTask) {
    // Register a dummy task
    bool registered = factory.registerTask("DummyTask", []() {
        return std::make_unique<DummyTask>("DummyTask");
    });
    ASSERT_TRUE(registered);

    // Create the dummy task
    std::unique_ptr<AbstractTask> task = factory.createTask("DummyTask");
    ASSERT_NE(task, nullptr);
    ASSERT_EQ(task->getTaskId(), "DummyTask");
}

TEST_F(TaskFactoryTest, CreateNonExistentTask) {
    std::unique_ptr<AbstractTask> task = factory.createTask("NonExistentTask");
    ASSERT_EQ(task, nullptr);
}

TEST_F(TaskFactoryTest, RegisterExistingTaskFails) {
    // Register a dummy task first
    factory.registerTask("AnotherDummyTask", []() {
        return std::make_unique<DummyTask>("AnotherDummyTask");
    });

    // Try to register again with the same ID
    bool registered = factory.registerTask("AnotherDummyTask", []() {
        return std::make_unique<DummyTask>("AnotherDummyTask");
    });
    ASSERT_FALSE(registered);
}

TEST_F(TaskFactoryTest, DriveToPositionTaskLifecycle) {
    // DriveToPositionTask is statically registered, so we can directly create it
    std::unique_ptr<AbstractTask> driveTask = factory.createTask("DriveToPosition");
    ASSERT_NE(driveTask, nullptr);
    ASSERT_EQ(driveTask->getTaskId(), "DriveToPosition");
    ASSERT_EQ(driveTask->getState(), TaskState::PENDING);

    TaskContext context;
    context.set<double>("target_x", 10.0);
    context.set<double>("target_y", 20.0);
    context.set<double>("speed", 1.5);

    // Initialize the task
    bool initialized = driveTask->initialize(context);
    ASSERT_TRUE(initialized);
    ASSERT_EQ(driveTask->getState(), TaskState::PENDING); // State remains PENDING after successful init

    // Execute the task
    bool executed = driveTask->execute(context);
    ASSERT_TRUE(executed);
    ASSERT_EQ(driveTask->getState(), TaskState::COMPLETED);
    ASSERT_TRUE(context.get<bool>("drive_successful"));

    // Terminate the task
    driveTask->terminate(context);
    ASSERT_EQ(driveTask->getState(), TaskState::COMPLETED); // State should remain COMPLETED after successful termination
}

TEST_F(TaskFactoryTest, DriveToPositionTaskInitializationFailure) {
    std::unique_ptr<AbstractTask> driveTask = factory.createTask("DriveToPosition");
    ASSERT_NE(driveTask, nullptr);

    TaskContext context; // Missing required parameters

    // Initialize the task (should fail due to missing parameters)
    bool initialized = driveTask->initialize(context);
    ASSERT_FALSE(initialized);
    ASSERT_EQ(driveTask->getState(), TaskState::FAILED);
}