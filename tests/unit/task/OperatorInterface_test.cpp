#include "gtest/gtest.h"
#include "src/core/task/OperatorInterface.h"
#include "src/core/task/TaskManager.h" // For concrete TaskManager
#include <memory>

using namespace mxrc::core::task;

TEST(OperatorInterfaceTest, ConstructorWithNullTaskManagerThrowsException) {
    ASSERT_THROW({
        OperatorInterface opInterface(nullptr);
    }, std::invalid_argument);
}

TEST(OperatorInterfaceTest, DefineNewTaskSuccessfully) {
    auto taskManager = std::make_shared<TaskManager>();
    OperatorInterface opInterface(taskManager);

    std::map<std::string, std::string> params = {{"setting", "value"}};
    std::string taskId = opInterface.defineNewTask("OpTestTask1", "OpTypeA", params);

    ASSERT_FALSE(taskId.empty());
    auto taskDto = opInterface.getTaskDetails(taskId);
    ASSERT_NE(taskDto, nullptr);
    EXPECT_EQ(taskDto->name, "OpTestTask1");
    EXPECT_EQ(taskDto->type, "OpTypeA");
    EXPECT_EQ(taskDto->parameters.at("setting"), "value");
}

TEST(OperatorInterfaceTest, GetAvailableTasks) {
    auto taskManager = std::make_shared<TaskManager>();
    OperatorInterface opInterface(taskManager);

    opInterface.defineNewTask("OpTaskA", "TypeX", {});
    opInterface.defineNewTask("OpTaskB", "TypeY", {});

    std::vector<TaskDto> tasks = opInterface.getAvailableTasks();
    EXPECT_EQ(tasks.size(), 2);
}

TEST(OperatorInterfaceTest, GetTaskDetailsNotFound) {
    auto taskManager = std::make_shared<TaskManager>();
    OperatorInterface opInterface(taskManager);

    auto taskDto = opInterface.getTaskDetails("non_existent_id");
    EXPECT_EQ(taskDto, nullptr);
}

TEST(OperatorInterfaceTest, StartTaskExecutionAndMonitorStatus) {
    auto taskManager = std::make_shared<TaskManager>();
    OperatorInterface opInterface(taskManager);

    std::string taskId = opInterface.defineNewTask("RunMeTask", "RunType", {});
    std::map<std::string, std::string> runtimeParams = {{"duration", "60"}};

    std::string executionId = opInterface.startTaskExecution(taskId, runtimeParams);
    ASSERT_EQ(executionId, taskId);

    auto statusDto = opInterface.monitorTaskStatus(executionId);
    ASSERT_NE(statusDto, nullptr);
    EXPECT_EQ(statusDto->id, taskId);
    EXPECT_EQ(statusDto->status, taskStatusToString(TaskStatus::RUNNING));
    EXPECT_EQ(statusDto->parameters.at("duration"), "60");
}
