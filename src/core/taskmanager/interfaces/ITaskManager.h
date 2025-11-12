#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "TaskDto.h" // Include the common DTO

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

    // New method to update task status
    virtual void updateTaskStatus(const std::string& taskId, TaskStatus status) = 0;
    // New method to update task progress
    virtual void updateTaskProgress(const std::string& taskId, int progress) = 0;
};



