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
        "[TaskExecutor] START - Task: {} (name: '{}', mode: {}, workType: {}, work: '{}')",
        definition.id, definition.name,
        taskExecutionModeToString(definition.executionMode),
        definition.workType == TaskWorkType::ACTION ? "ACTION" : "SEQUENCE",
        definition.workId
    );

    auto& state = getOrCreateState(definition.id);
    auto prevStatus = state.status;
    state.status = TaskStatus::RUNNING;
    state.cancelRequested = false;
    state.pauseRequested = false;

    Logger::get()->debug("[TaskExecutor] Task {} state transition: {} -> RUNNING",
                        definition.id, taskStatusToString(prevStatus));

    TaskExecution result;
    auto startTime = std::chrono::steady_clock::now();

    if (definition.workType == TaskWorkType::ACTION) {
        Logger::get()->debug("[TaskExecutor] Task {} executing ACTION: {}",
                            definition.id, definition.workId);
        result = executeAction(definition, context, state);
    } else {
        Logger::get()->debug("[TaskExecutor] Task {} executing SEQUENCE: {}",
                            definition.id, definition.workId);
        result = executeSequence(definition, context, state);
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);

    Logger::get()->info(
        "[TaskExecutor] {} - Task {} finished in {}ms (status: {}, progress: {:.1f}%)",
        result.isSuccessful() ? "SUCCESS" : "FAILED",
        definition.id, elapsed.count(),
        taskStatusToString(result.status), result.progress * 100.0f
    );

    if (result.isFailed()) {
        Logger::get()->error("[TaskExecutor] Task {} error: {}", definition.id, result.errorMessage);
    }

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
        auto& taskState = it->second;
        auto prevStatus = taskState.status;
        taskState.cancelRequested = true;
        Logger::get()->info("[TaskExecutor] CANCEL - Task: {} (current status: {})",
                           taskId, taskStatusToString(prevStatus));
        Logger::get()->debug("[TaskExecutor] Task {} cancel flag set, waiting for task to acknowledge",
                            taskId);
    } else {
        Logger::get()->warn("[TaskExecutor] CANCEL - Task not found: {}", taskId);
    }
}

void TaskExecutor::pause(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = states_.find(taskId);
    if (it != states_.end()) {
        auto& taskState = it->second;
        auto prevStatus = taskState.status;
        taskState.pauseRequested = true;
        taskState.status = TaskStatus::PAUSED;
        Logger::get()->info("[TaskExecutor] PAUSE - Task: {} (state transition: {} -> PAUSED)",
                           taskId, taskStatusToString(prevStatus));
    } else {
        Logger::get()->warn("[TaskExecutor] PAUSE - Task not found: {}", taskId);
    }
}

void TaskExecutor::resume(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = states_.find(taskId);
    if (it != states_.end()) {
        auto& taskState = it->second;
        auto prevStatus = taskState.status;
        taskState.pauseRequested = false;
        taskState.status = TaskStatus::RUNNING;
        Logger::get()->info("[TaskExecutor] RESUME - Task: {} (state transition: {} -> RUNNING)",
                           taskId, taskStatusToString(prevStatus));
    } else {
        Logger::get()->warn("[TaskExecutor] RESUME - Task not found: {}", taskId);
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
