#include <iostream>
#include <memory>
#include <thread> // For std::this_thread::sleep_for
#include "core/taskmanager/TaskManager.h"
#include "core/taskmanager/operator_interface/OperatorInterface.h"
#include "core/taskmanager/TaskManagerInit.h"
#include "core/taskmanager/TaskDefinitionRegistry.h"
#include "core/taskmanager/TaskExecutor.h"

using namespace mxrc::core::taskmanager; // Add this line

int main() {
    std::cout << "MXRC Task Management Module 예제" << std::endl;

    mxrc::core::taskmanager::initializeTaskManagerModule();

    // Create instances of TaskDefinitionRegistry and TaskExecutor
    auto registry = std::make_shared<mxrc::core::taskmanager::TaskDefinitionRegistry>();
    auto executor = std::make_shared<mxrc::core::taskmanager::TaskExecutor>();

    // TaskManager 인스턴스 생성 및 registry, executor 주입
    auto taskManager = std::make_shared<mxrc::core::taskmanager::TaskManager>(registry, executor);

    // OperatorInterface 인스턴스 생성 및 TaskManager 주입
    mxrc::core::taskmanager::OperatorInterface opInterface(taskManager);

    // --- 케이스 1: 여러 Task 정의 및 목록 출력 ---
    std::cout << "\n--- 케이스 1: 여러 Task 정의 및 목록 출력 ---" << std::endl;
    std::map<std::string, std::string> driveParams = {{"speed", "1.0"}, {"distance", "10.0"}};
    std::string driveTaskId = opInterface.defineNewTask("DriveForward", "DriveToPosition", driveParams);
    std::cout << "정의된 Task: DriveForward (ID: " << driveTaskId << ")" << std::endl;

    std::map<std::string, std::string> liftParams = {{"height", "0.5"}, {"payload", "pallet"}};
    std::string liftTaskId = opInterface.defineNewTask("LiftPallet", "LiftPallet", liftParams);
    std::cout << "정의된 Task: LiftPallet (ID: " << liftTaskId << ")" << std::endl;

    std::map<std::string, std::string> inspectParams = {{"area", "zoneA"}, {"camera", "front"}};
    std::string inspectTaskId = opInterface.defineNewTask("InspectArea", "Inspection", inspectParams);
    std::cout << "정의된 Task: InspectArea (ID: " << inspectTaskId << ")" << std::endl;

    // DummyTask 정의 및 팩토리 사용 시연
    std::map<std::string, std::string> dummyParams = {{"message", "Hello from DummyTask!"}};
    std::string dummyTaskId = opInterface.defineNewTask("MyDummyTask", "DummyTask", dummyParams);
    std::cout << "정의된 Task: MyDummyTask (ID: " << dummyTaskId << ")" << std::endl;


    std::cout << "\n사용 가능한 Task 목록:" << std::endl;
    for (const auto& taskDto : opInterface.getAvailableTasks()) {
        std::cout << "- " << taskDto.name << " (ID: " << taskDto.id << ", 유형: " << taskDto.type << ", 상태: " << taskDto.status << ")" << std::endl;
    }

    // --- 케이스 2: 특정 Task의 상세 정보 조회 ---
    std::cout << "\n--- 케이스 2: 특정 Task의 상세 정보 조회 ---" << std::endl;
    auto taskDetails = opInterface.getTaskDetails(driveTaskId);
    if (taskDetails) {
        std::cout << taskDetails->name << "에 대한 상세 정보:" << std::endl;
        std::cout << "  상태: " << taskDetails->status << std::endl;
        std::cout << "  매개변수: ";
        for (const auto& param : taskDetails->parameters) {
            std::cout << param.first << ": " << param.second << ", ";
        }
        std::cout << std::endl;
    }

    // --- 케이스 3: Task 실행 시작 및 상태 모니터링 ---
    std::cout << "\n--- 케이스 3: Task 실행 시작 및 상태 모니터링 ---" << std::endl;
    std::map<std::string, std::string> runtimeDriveParams = {{"speed", "1.5"}, {"obstacle_avoidance", "true"}};
    std::string driveExecutionId = opInterface.startTaskExecution(driveTaskId, runtimeDriveParams);
    std::cout << "DriveForward 실행 시작. 실행 ID: " << driveExecutionId << std::endl;

    auto driveStatus = opInterface.monitorTaskStatus(driveExecutionId);
    if (driveStatus) {
        std::cout << "  DriveForward 상태: " << driveStatus->status << ", 진행률: " << driveStatus->progress << std::endl;
    }

    // --- 케이스 4: Task 완료 시뮬레이션 및 상태/진행률 업데이트 ---
    std::cout << "\n--- 케이스 4: Task 완료 시뮬레이션 및 상태/진행률 업데이트 ---" << std::endl;
    // Task status and progress are now managed internally by the Task itself.
    // The OperatorInterface monitors the status, but does not directly update it.
    // Simulate waiting for task completion
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait for DriveForwardTask to complete

    driveStatus = opInterface.monitorTaskStatus(driveExecutionId);
    if (driveStatus) {
        std::cout << "  DriveForward 상태 (완료 후): " << driveStatus->status << ", 진행률: " << driveStatus->progress << std::endl;
    }

    // --- 케이스 5: 중복된 이름으로 Task 정의 시도 (예외 발생 예상) ---
    std::cout << "\n--- 케이스 5: 중복된 이름으로 Task 정의 시도 ---" << std::endl;
    try {
        opInterface.defineNewTask("DriveForward", "AnotherType", {{"param", "value"}});
        std::cout << "  오류: 중복된 Task 이름이 허용되었습니다." << std::endl;
    } catch (const std::runtime_error& e) {
        std::cout << "  예상된 예외 발생: " << e.what() << std::endl;
    }

    // --- 케이스 6: 존재하지 않는 Task의 상세 정보 조회 시도 (nullptr 반환 예상) ---
    std::cout << "\n--- 케이스 6: 존재하지 않는 Task의 상세 정보 조회 시도 ---" << std::endl;
    auto nonExistentTask = opInterface.getTaskDetails("non-existent-id-123");
    if (!nonExistentTask) {
        std::cout << "  존재하지 않는 Task 상세 정보 요청을 성공적으로 처리했습니다 (nullptr 반환)." << std::endl;
    } else {
        std::cout << "  오류: 존재하지 않는 Task 상세 정보가 유효한 객체를 반환했습니다." << std::endl;
    }

    // --- 케이스 7: 존재하지 않는 Task 실행 요청 시도 (예외 발생 예상) ---
    std::cout << "\n--- 케이스 7: 존재하지 않는 Task 실행 요청 시도 ---" << std::endl;
    try {
        opInterface.startTaskExecution("non-existent-id-456", {});
        std::cout << "  오류: 존재하지 않는 Task 실행이 허용되었습니다." << std::endl;
    } catch (const std::runtime_error& e) {
        std::cout << "  예상된 예외 발생: " << e.what() << std::endl;
    }

    // --- 케이스 8: Task 실패 시뮬레이션 ---
    std::cout << "\n--- 케이스 8: Task 실패 시뮬레이션 ---" << std::endl;
    std::string failingTaskId = opInterface.defineNewTask("FailingTask", "FailureType", {});
    std::string failingExecutionId = opInterface.startTaskExecution(failingTaskId, {});
    // Simulate waiting for task failure
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait for FailureTypeTask to fail

    auto failingStatus = opInterface.monitorTaskStatus(failingExecutionId);
    if (failingStatus) {
        std::cout << "  FailingTask 상태 (실패 후): " << failingStatus->status << std::endl;
    }

    return 0;
}
