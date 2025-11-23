#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional> // For std::function
#include "TaskDto.h" // Assuming TaskDto is defined in a common header

// Forward declaration for polymorphic Task interface
class ITask;

// Interface for managing Task types and instances
class ITaskManager {
public:
    virtual ~ITaskManager() = default;

    // FR-001: 새로운 Task 유형을 정의하고 등록하는 메커니즘 제공
    // Task 유형을 등록하고, 해당 유형의 Task 인스턴스를 생성할 팩토리 함수를 연결합니다.
    virtual void registerTaskType(const std::string& taskType, std::function<std::unique_ptr<ITask>(const std::string& taskId, const std::string& taskName, const std::map<std::string, std::string>& parameters)> taskFactory) = 0;

    // FR-002: 등록된 Task 유형에 관계없이 Task 인스턴스를 생성하고 관리
    // 등록된 Task 유형을 기반으로 새로운 Task 인스턴스를 생성합니다.
    virtual std::string createTaskInstance(const std::string& taskType, const std::string& taskName, const std::map<std::string, std::string>& initialParameters) = 0;

    // FR-004: Task의 실행 상태를 일관된 인터페이스를 통해 모니터링
    virtual std::unique_ptr<TaskDto> getTaskStatus(const std::string& taskId) const = 0;

    // FR-003: Task 유형에 따라 다른 실행 로직을 동적으로 호출 (다형성)
    // Task 인스턴스의 실행을 시작합니다.
    virtual void startTaskExecution(const std::string& taskId, const std::map<std::string, std::string>& runtimeParameters) = 0;

    // Task 상태 업데이트 (내부용)
    virtual void updateTaskStatus(const std::string& taskId, TaskStatus status) = 0;
    virtual void updateTaskProgress(const std::string& taskId, int progress) = 0;
 
    // FR-007: 모든 Task 생명주기 이벤트 로깅
    // (이것은 API가 아닌 구현 세부 사항에 가깝지만, 인터페이스에 로깅 메커니즘을 노출할 수도 있음)
    // FR-008: Task 상태 및 진행률에 대한 메트릭 제공
    // (이것은 API가 아닌 구현 세부 사항에 가깝지만, 인터페이스에 메트릭 수집 메커니즘을 노출할 수도 있음)
};
