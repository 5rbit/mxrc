#include "core/task/core/TaskExecutor.h"
#include "core/sequence/dto/SequenceDefinition.h"
#include <chrono>
#include <sstream>

namespace mxrc::core::task {

using namespace mxrc::core::action;

TaskExecutor::TaskExecutor(
    std::shared_ptr<ActionFactory> actionFactory,
    std::shared_ptr<ActionExecutor> actionExecutor,
    std::shared_ptr<mxrc::core::sequence::SequenceEngine> sequenceEngine
) : actionFactory_(actionFactory),
    actionExecutor_(actionExecutor),
    sequenceEngine_(sequenceEngine) {
    Logger::get()->info("TaskExecutor initialized");
}

TaskExecution TaskExecutor::execute(
    const TaskDefinition& definition,
    ExecutionContext& context
) {
    Logger::get()->info(
        "Executing task: {} (name: {}, mode: {}, workType: {})",
        definition.id, definition.name,
        taskExecutionModeToString(definition.executionMode),
        definition.workType == TaskWorkType::ACTION ? "ACTION" : "SEQUENCE"
    );

    auto& state = getOrCreateState(definition.id);
    state.status = TaskStatus::RUNNING;
    state.cancelRequested = false;
    state.pauseRequested = false;

    TaskExecution result;

    if (definition.workType == TaskWorkType::ACTION) {
        result = executeAction(definition, context, state);
    } else {
        result = executeSequence(definition, context, state);
    }

    Logger::get()->info(
        "Task {} finished with status: {}",
        definition.id, taskStatusToString(result.status)
    );

    return result;
}

TaskExecution TaskExecutor::executeAction(
    const TaskDefinition& definition,
    ExecutionContext& context,
    TaskState& state
) {
    TaskExecution result;
    result.taskId = definition.id;
    result.executionId = definition.id + "_exec_" + 
                        std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    result.startTime = std::chrono::system_clock::now();
    result.status = TaskStatus::RUNNING;

    try {
        // Action 파라미터 준비
        std::map<std::string, std::string> params;
        params["id"] = definition.workId;

        // Action 생성 및 실행
        auto action = actionFactory_->createAction(definition.workId, params);
        auto actionResult = actionExecutor_->execute(action, context);

        // 결과 변환
        if (actionResult.isSuccessful()) {
            result.status = TaskStatus::COMPLETED;
            result.progress = 1.0f;
            state.status = TaskStatus::COMPLETED;
            state.progress = 1.0f;
        } else {
            result.status = TaskStatus::FAILED;
            result.errorMessage = actionResult.errorMessage;
            state.status = TaskStatus::FAILED;
        }

        result.executionTime = actionResult.executionTime;

    } catch (const std::exception& e) {
        Logger::get()->error("Task {} action execution failed: {}", definition.id, e.what());
        result.status = TaskStatus::FAILED;
        result.errorMessage = e.what();
        state.status = TaskStatus::FAILED;
    }

    result.endTime = std::chrono::system_clock::now();
    return result;
}

TaskExecution TaskExecutor::executeSequence(
    const TaskDefinition& definition,
    ExecutionContext& context,
    TaskState& state
) {
    TaskExecution result;
    result.taskId = definition.id;
    result.executionId = definition.id + "_exec_" +
                        std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    result.startTime = std::chrono::system_clock::now();
    result.status = TaskStatus::RUNNING;

    try {
        // Sequence 정의 생성 (간단한 구현)
        mxrc::core::sequence::SequenceDefinition seqDef(definition.workId, definition.workId);
        
        // Sequence 실행
        auto seqResult = sequenceEngine_->execute(seqDef, context);

        // 결과 변환
        if (seqResult.isSuccessful()) {
            result.status = TaskStatus::COMPLETED;
            result.progress = 1.0f;
            state.status = TaskStatus::COMPLETED;
            state.progress = 1.0f;
        } else {
            result.status = TaskStatus::FAILED;
            result.errorMessage = seqResult.errorMessage;
            state.status = TaskStatus::FAILED;
        }

        result.executionTime = seqResult.executionTime;

    } catch (const std::exception& e) {
        Logger::get()->error("Task {} sequence execution failed: {}", definition.id, e.what());
        result.status = TaskStatus::FAILED;
        result.errorMessage = e.what();
        state.status = TaskStatus::FAILED;
    }

    result.endTime = std::chrono::system_clock::now();
    return result;
}

void TaskExecutor::cancel(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = states_.find(taskId);
    if (it != states_.end()) {
        it->second.cancelRequested = true;
        Logger::get()->info("Cancel requested for task: {}", taskId);
    }
}

void TaskExecutor::pause(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = states_.find(taskId);
    if (it != states_.end()) {
        it->second.pauseRequested = true;
        it->second.status = TaskStatus::PAUSED;
        Logger::get()->info("Pause requested for task: {}", taskId);
    }
}

void TaskExecutor::resume(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = states_.find(taskId);
    if (it != states_.end()) {
        it->second.pauseRequested = false;
        it->second.status = TaskStatus::RUNNING;
        Logger::get()->info("Resume requested for task: {}", taskId);
    }
}

TaskStatus TaskExecutor::getStatus(const std::string& taskId) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = states_.find(taskId);
    if (it != states_.end()) {
        return it->second.status;
    }
    return TaskStatus::IDLE;
}

float TaskExecutor::getProgress(const std::string& taskId) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = states_.find(taskId);
    if (it != states_.end()) {
        return it->second.progress.load();
    }
    return 0.0f;
}

TaskExecutor::TaskState& TaskExecutor::getOrCreateState(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return states_[taskId];
}

} // namespace mxrc::core::task
