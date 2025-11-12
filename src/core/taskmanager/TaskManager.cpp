#include "TaskManager.h" // Updated include
#include "commands/StartTaskCommand.h" // Updated include
#include <algorithm>
#include <iostream> // For logging in executeCommand

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
    // Command Pattern을 사용하여 Task 실행 요청을 캡슐화합니다.
    // StartTaskCommand를 생성하고 executeCommand를 통해 실행합니다.
    auto command = std::make_unique<StartTaskCommand>(*this, taskId, runtimeParameters);
    std::string executionId = taskId; // For simplicity, executionId is same as taskId for now
    executeCommand(std::move(command));
    return executionId;
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

// ICommand 객체를 실행합니다.
void TaskManager::executeCommand(std::unique_ptr<ICommand> command) {
    if (command) {
        command->execute();
    } else {
        std::cerr << "Error: Attempted to execute a null command." << std::endl;
    }
}

// 실행 가능한 Task를 활성 목록에 추가합니다.
void TaskManager::addExecutableTask(const std::string& executionId, std::unique_ptr<ITask> executableTask) { // Corrected type
    if (executableTask) {
        activeExecutableTasks_[executionId] = std::move(executableTask);
    } else {
        std::cerr << "Error: Attempted to add a null executable task." << std::endl;
    }
}

// 실행 가능한 Task를 가져옵니다.
ITask* TaskManager::getExecutableTask(const std::string& executionId) { // Corrected type
    auto it = activeExecutableTasks_.find(executionId);
    if (it != activeExecutableTasks_.end()) {
        return it->second.get();
    }
    return nullptr; // Task를 찾을 수 없는 경우 nullptr 반환
}
