#include "gtest/gtest.h"
#include "src/core/task/TaskManager.h"
#include "src/core/task/Task.h"
#include <map>
#include <stdexcept>

using namespace mxrc::core::task;

TEST(TaskManagerTest, RegisterTaskDefinitionSuccessfully) {
    TaskManager taskManager;
    std::map<std::string, std::string> params = {{"key1", "value1"}};
    std::string taskId = taskManager.registerTaskDefinition("TestTask1", "TypeA", params);

    ASSERT_FALSE(taskId.empty());
    auto taskDto = taskManager.getTaskDefinitionById(taskId);
    ASSERT_NE(taskDto, nullptr);
    EXPECT_EQ(taskDto->name, "TestTask1");
    EXPECT_EQ(taskDto->type, "TypeA");
    EXPECT_EQ(taskDto->parameters.at("key1"), "value1");
    EXPECT_EQ(taskDto->status, taskStatusToString(TaskStatus::PENDING));
}

TEST(TaskManagerTest, RegisterTaskDefinitionDuplicateName) {
    TaskManager taskManager;
    std::map<std::string, std::string> params = {{"key1", "value1"}};
    taskManager.registerTaskDefinition("DuplicateTask", "TypeA", params);

    ASSERT_THROW({
        taskManager.registerTaskDefinition("DuplicateTask", "TypeB", params);
    }, std::runtime_error);
}

TEST(TaskManagerTest, GetAllTaskDefinitionsEmpty) {
    TaskManager taskManager;
    std::vector<TaskDto> tasks = taskManager.getAllTaskDefinitions();
    EXPECT_TRUE(tasks.empty());
}

TEST(TaskManagerTest, GetAllTaskDefinitionsNonEmpty) {
    TaskManager taskManager;
    taskManager.registerTaskDefinition("TaskA", "TypeA", {});
    taskManager.registerTaskDefinition("TaskB", "TypeB", {});

    std::vector<TaskDto> tasks = taskManager.getAllTaskDefinitions();
    EXPECT_EQ(tasks.size(), 2);
}

TEST(TaskManagerTest, GetTaskDefinitionByIdNotFound) {
    TaskManager taskManager;
    auto taskDto = taskManager.getTaskDefinitionById("non_existent_id");
    EXPECT_EQ(taskDto, nullptr);
}

TEST(TaskManagerTest, RequestTaskExecutionNotFound) {
    TaskManager taskManager;
    ASSERT_THROW({
        taskManager.requestTaskExecution("non_existent_id", {});
    }, std::runtime_error);
}

TEST(TaskManagerTest, RequestTaskExecutionSuccessfully) {
    TaskManager taskManager;
    std::string taskId = taskManager.registerTaskDefinition("ExecutableTask", "TypeX", {});
    std::map<std::string, std::string> runtimeParams = {{"speed", "10"}};
    
    std::string executionId = taskManager.requestTaskExecution(taskId, runtimeParams);
    ASSERT_EQ(executionId, taskId);

    auto taskDto = taskManager.getTaskDefinitionById(taskId);
    ASSERT_NE(taskDto, nullptr);
    EXPECT_EQ(taskDto->status, taskStatusToString(TaskStatus::RUNNING));
    EXPECT_EQ(taskDto->parameters.at("speed"), "10");
}

TEST(TaskManagerTest, GetTaskExecutionStatus) {
    TaskManager taskManager;
    std::string taskId = taskManager.registerTaskDefinition("StatusTask", "TypeY", {});
    taskManager.requestTaskExecution(taskId, {});

    auto statusDto = taskManager.getTaskExecutionStatus(taskId);
    ASSERT_NE(statusDto, nullptr);
    EXPECT_EQ(statusDto->id, taskId);
    EXPECT_EQ(statusDto->status, taskStatusToString(TaskStatus::RUNNING));
}
