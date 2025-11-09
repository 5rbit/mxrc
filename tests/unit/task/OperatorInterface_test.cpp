#include "gtest/gtest.h"
#include "OperatorInterface.h"
#include "TaskManager.h" // For concrete TaskManager
#include <memory>

// OperatorInterface 생성 시 nullptr TaskManager 주입 시 예외 처리를 테스트합니다.
TEST(OperatorInterfaceTest, ConstructorWithNullTaskManagerThrowsException) {
    ASSERT_THROW({
        OperatorInterface opInterface(nullptr);
    }, std::invalid_argument);
}

// OperatorInterface를 통한 새로운 Task 정의 등록 기능을 성공적으로 테스트합니다.
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

// OperatorInterface를 통한 사용 가능한 Task 목록 조회 기능을 테스트합니다.
TEST(OperatorInterfaceTest, GetAvailableTasks) {
    auto taskManager = std::make_shared<TaskManager>();
    OperatorInterface opInterface(taskManager);

    opInterface.defineNewTask("OpTaskA", "TypeX", {});
    opInterface.defineNewTask("OpTaskB", "TypeY", {});

    std::vector<TaskDto> tasks = opInterface.getAvailableTasks();
    EXPECT_EQ(tasks.size(), 2);
}

// OperatorInterface를 통해 존재하지 않는 ID로 Task 상세 정보 조회 시를 테스트합니다.
TEST(OperatorInterfaceTest, GetTaskDetailsNotFound) {
    auto taskManager = std::make_shared<TaskManager>();
    OperatorInterface opInterface(taskManager);

    auto taskDto = opInterface.getTaskDetails("non_existent_id");
    EXPECT_EQ(taskDto, nullptr);
}

// OperatorInterface를 통한 Task 실행 요청 및 상태 모니터링 기능을 테스트합니다.
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
