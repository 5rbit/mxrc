#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "../TaskDto.h" // Include the common DTO

namespace mxrc {
namespace core {
namespace taskmanager {

class IOperatorInterface {
public:
    virtual ~IOperatorInterface() = default;

    // 사용자 스토리 1, FR-001: 새로운 Task 정의 등록
    virtual std::string defineNewTask(const std::string& taskName, const std::string& taskType, const std::map<std::string, std::string>& defaultParameters) = 0;

    // 사용자 스토리 2, FR-003: 등록된 모든 Task 정의 조회
    virtual std::vector<TaskDto> getAvailableTasks() const = 0;

    // 사용자 스토리 2, FR-004: 특정 Task 정의 상세 정보 조회
    virtual std::unique_ptr<TaskDto> getTaskDetails(const std::string& taskId) const = 0;

    // 사용자 스토리 3, FR-005: Task 실행 요청
    virtual std::string startTaskExecution(const std::string& taskId, const std::map<std::string, std::string>& runtimeParameters) = 0;

    // 사용자 스토리 3, FR-006: 실행 중인 Task의 상태 모니터링
    virtual std::unique_ptr<TaskDto> monitorTaskStatus(const std::string& executionId) const = 0;

    // 사용자 스토리 4, FR-XXX: 실행 중인 Task 취소 요청
    virtual void cancelTask(const std::string& taskId) = 0;

    // 사용자 스토리 4, FR-XXX: 실행 중인 Task 일시 정지 요청
    virtual void pauseTask(const std::string& taskId) = 0;
};

} // namespace taskmanager
} // namespace core
} // namespace mxrc
