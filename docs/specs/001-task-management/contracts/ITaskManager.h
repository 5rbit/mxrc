#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

// Forward declaration for Task entity (or a DTO)
struct TaskDto;

class ITaskManager {
public:
    virtual ~ITaskManager() = default;

    // FR-001, FR-002: Task 정의 등록 및 고유 식별자 할당
    virtual std::string registerTaskDefinition(const std::string& taskName, const std::string& taskType, const std::map<std::string, std::string>& defaultParameters) = 0;

    // FR-003: 등록된 모든 Task 목록 조회
    virtual std::vector<TaskDto> getAllTaskDefinitions() const = 0;

    // FR-004: 특정 Task의 상세 정보를 ID를 통해 조회
    virtual std::unique_ptr<TaskDto> getTaskDefinitionById(const std::string& taskId) const = 0;

    // FR-005: 등록된 Task의 실행 요청
    virtual std::string requestTaskExecution(const std::string& taskId, const std::map<std::string, std::string>& runtimeParameters) = 0;

    // FR-006: 실행 중인 Task의 현재 상태 모니터링
    virtual std::unique_ptr<TaskDto> getTaskExecutionStatus(const std::string& executionId) const = 0;

    // FR-007: Task 실행 중 발생하는 오류 처리 및 알림 (내부적으로 처리되므로 명시적인 API는 아닐 수 있음)
    // FR-008: Task 실행의 동시성 관리 (내부적으로 처리되므로 명시적인 API는 아닐 수 있음)
};

// Simple DTO for Task information
struct TaskDto {
    std::string id;
    std::string name;
    std::string type;
    std::map<std::string, std::string> parameters;
    std::string status; // PENDING, RUNNING, COMPLETED, FAILED, CANCELLED
    int progress; // 0-100
    std::string created_at;
    std::string updated_at;
};
