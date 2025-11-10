#include "TaskManager.h"
#include <algorithm>

// 새로운 Task 정의를 등록합니다. Task 이름이 중복되면 예외를 발생시킵니다.
std::string TaskManager::registerTaskDefinition(const std::string& taskName, const std::string& taskType, const std::map<std::string, std::string>& defaultParameters) {
    // Task 이름 중복 확인
    auto it = std::find_if(tasks_.begin(), tasks_.end(), [&](const auto& pair) {
        return pair.second->getName() == taskName;
    });

    if (it != tasks_.end()) {
        throw std::runtime_error("Task with this name already exists.");
    }

    // 새로운 Task 객체 생성 및 등록
    auto task = std::make_unique<Task>(taskName, taskType, defaultParameters);
    std::string taskId = task->getId();
    tasks_[taskId] = std::move(task);
    return taskId;
}

// 등록된 모든 Task 정의 목록을 반환합니다.
std::vector<TaskDto> TaskManager::getAllTaskDefinitions() const {
    std::vector<TaskDto> dtos;
    for (const auto& pair : tasks_) {
        const auto& task = pair.second;
        dtos.push_back({
            task->getId(),
            task->getName(),
            task->getType(),
            task->getParameters(),
            taskStatusToString(task->getStatus()),
            task->getProgress(),
            task->getCreatedAt(),
            task->getUpdatedAt()
        });
    }
    return dtos;
}

// 특정 Task ID를 사용하여 Task 정의의 상세 정보를 조회합니다.
std::unique_ptr<TaskDto> TaskManager::getTaskDefinitionById(const std::string& taskId) const {
    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) {
        const auto& task = it->second;
        return std::make_unique<TaskDto>(
            task->getId(),
            task->getName(),
            task->getType(),
            task->getParameters(),
            taskStatusToString(task->getStatus()),
            task->getProgress(),
            task->getCreatedAt(),
            task->getUpdatedAt()
        );
    }
    return nullptr; // Task를 찾을 수 없는 경우 nullptr 반환
}

// 특정 Task의 실행을 요청합니다. Task가 없으면 예외를 발생시킵니다.
std::string TaskManager::requestTaskExecution(const std::string& taskId, const std::map<std::string, std::string>& runtimeParameters) {
    auto it = tasks_.find(taskId);
    if (it == tasks_.end()) {
        throw std::runtime_error("Task definition not found.");
    }
    // 단순화를 위해 기존 Task 정의의 상태를 업데이트합니다.
    // 실제 시스템에서는 새로운 Task 실행 인스턴스를 생성할 것입니다.
    it->second->setStatus(TaskStatus::RUNNING);
    it->second->setParameters(runtimeParameters); // 실행 파라미터 업데이트
    return taskId; // 단순화를 위해 taskId를 실행 ID로 반환
}

// 실행 중인 Task의 현재 상태를 모니터링합니다.
std::unique_ptr<TaskDto> TaskManager::getTaskExecutionStatus(const std::string& executionId) const {
    // 단순화를 위해 실행 ID는 Task ID와 동일하다고 가정합니다.
    return getTaskDefinitionById(executionId);
}

// 특정 Task의 상태를 업데이트합니다. Task가 없으면 예외를 발생시킵니다.
void TaskManager::updateTaskStatus(const std::string& taskId, TaskStatus status) {
    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) {
        it->second->setStatus(status);
    } else {
        throw std::runtime_error("Task not found for status update.");
    }
}

// 특정 Task의 진행률을 업데이트합니다. Task가 없으면 예외를 발생시킵니다.
void TaskManager::updateTaskProgress(const std::string& taskId, int progress) {
    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) {
        it->second->setProgress(progress);
    } else {
        throw std::runtime_error("Task not found for progress update.");
    }
}
