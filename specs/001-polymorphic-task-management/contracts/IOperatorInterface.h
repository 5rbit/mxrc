#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "TaskDto.h" // Assuming TaskDto is defined in a common header

// Interface for external operators to interact with Task management
class IOperatorInterface {
public:
    virtual ~IOperatorInterface() = default;

    // FR-001: 새로운 Task 유형을 정의하고 등록하는 메커니즘 제공
    // 새로운 Task 유형을 시스템에 등록합니다. (TaskManager의 registerTaskType을 추상화)
    virtual void defineNewTaskType(const std::string& taskType, const std::string& description, const std::map<std::string, std::string>& requiredParametersSchema) = 0;

    // FR-002: 등록된 Task 유형에 관계없이 Task 인스턴스를 생성하고 관리
    // 특정 유형의 Task 인스턴스를 생성합니다.
    virtual std::string createAndRegisterTask(const std::string& taskType, const std::string& taskName, const std::map<std::string, std::string>& initialParameters) = 0;

    // FR-004: Task의 실행 상태를 일관된 인터페이스를 통해 모니터링
    virtual std::unique_ptr<TaskDto> getTaskDetails(const std::string& taskId) const = 0;

    // FR-003: Task 유형에 따라 다른 실행 로직을 동적으로 호출 (다형성)
    // Task 인스턴스의 실행을 시작합니다.
    virtual void startTask(const std::string& taskId, const std::map<std::string, std::string>& runtimeParameters) = 0;

    // 등록된 모든 Task 유형 목록 조회
    virtual std::vector<std::string> getAvailableTaskTypes() const = 0;

    // 등록된 모든 Task 인스턴스 목록 조회
    virtual std::vector<TaskDto> getAllTaskInstances() const = 0;
};
