#include "gtest/gtest.h"
#include "../../../src/core/taskmanager/TaskManager.h" // Corrected include
#include "../../../src/core/taskmanager/Task.h" // Corrected include
#include <map>
#include <stdexcept>

// TaskManager의 Task 정의 등록 기능을 성공적으로 테스트합니다.
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

// TaskManager의 중복된 이름의 Task 정의 등록 시 예외 처리를 테스트합니다.
TEST(TaskManagerTest, RegisterTaskDefinitionDuplicateName) {
    TaskManager taskManager;
    std::map<std::string, std::string> params = {{"key1", "value1"}};
    taskManager.registerTaskDefinition("DuplicateTask", "TypeA", params);

    ASSERT_THROW({
        taskManager.registerTaskDefinition("DuplicateTask", "TypeB", params);
    }, std::runtime_error);
}

// TaskManager에 등록된 Task 정의가 없을 때 모든 Task 정의 조회 기능을 테스트합니다.
TEST(TaskManagerTest, GetAllTaskDefinitionsEmpty) {
    TaskManager taskManager;
    std::vector<TaskDto> tasks = taskManager.getAllTaskDefinitions();
    EXPECT_TRUE(tasks.empty());
}

// TaskManager에 등록된 Task 정의가 있을 때 모든 Task 정의 조회 기능을 테스트합니다.
TEST(TaskManagerTest, GetAllTaskDefinitionsNonEmpty) {
    TaskManager taskManager;
    taskManager.registerTaskDefinition("TaskA", "TypeA", {});
    taskManager.registerTaskDefinition("TaskB", "TypeB", {});

    std::vector<TaskDto> tasks = taskManager.getAllTaskDefinitions();
    EXPECT_EQ(tasks.size(), 2);
}

// TaskManager에서 존재하지 않는 ID로 Task 정의를 조회할 때를 테스트합니다.
TEST(TaskManagerTest, GetTaskDefinitionByIdNotFound) {
    TaskManager taskManager;
    auto taskDto = taskManager.getTaskDefinitionById("non_existent_id");
    EXPECT_EQ(taskDto, nullptr);
}

// TaskManager에서 존재하지 않는 ID로 Task 실행을 요청할 때 예외 처리를 테스트합니다.
TEST(TaskManagerTest, RequestTaskExecutionNotFound) {
    TaskManager taskManager;
    ASSERT_THROW({
        taskManager.requestTaskExecution("non_existent_id", {});
    }, std::runtime_error);
}

// TaskManager의 Task 실행 요청 기능을 성공적으로 테스트합니다.
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

// TaskManager의 Task 실행 상태 모니터링 기능을 테스트합니다.
TEST(TaskManagerTest, GetTaskExecutionStatus) {
    TaskManager taskManager;
    std::string taskId = taskManager.registerTaskDefinition("StatusTask", "TypeY", {});
    taskManager.requestTaskExecution(taskId, {});

    auto statusDto = taskManager.getTaskExecutionStatus(taskId);
    ASSERT_NE(statusDto, nullptr);
    EXPECT_EQ(statusDto->id, taskId);
    EXPECT_EQ(statusDto->status, taskStatusToString(TaskStatus::RUNNING));
}
