#include "core/task/core/TaskExecutor.h"
#include "core/sequence/dto/SequenceDefinition.h"
#include "interfaces/IEventBus.h"
#include "dto/TaskEvents.h"
#include <chrono>
#include <sstream>
#include <thread>
#include <atomic>

namespace mxrc::core::task {

using namespace mxrc::core::action;

TaskExecutor::TaskExecutor(
    std::shared_ptr<ActionFactory> actionFactory,
    std::shared_ptr<ActionExecutor> actionExecutor,
    std::shared_ptr<mxrc::core::sequence::SequenceEngine> sequenceEngine,
    std::shared_ptr<mxrc::core::event::IEventBus> eventBus
) : actionFactory_(actionFactory),
    actionExecutor_(actionExecutor),
    sequenceEngine_(sequenceEngine),
    eventBus_(eventBus) {
    // EventBus는 optional이므로 nullptr 허용
    Logger::get()->info("TaskExecutor initialized");
}

template<typename EventType>
void TaskExecutor::publishEvent(std::shared_ptr<EventType> event) {
    if (eventBus_ && event) {
        eventBus_->publish(event);
    }
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
    state.startTime = std::chrono::steady_clock::now();

    Logger::get()->debug("[TaskExecutor] Task {} state transition: {} -> RUNNING",
                        definition.id, taskStatusToString(prevStatus));

    // 이벤트 발행: TASK_STARTED
    publishEvent(std::make_shared<mxrc::core::event::TaskStartedEvent>(
        definition.id,
        definition.name,
        taskExecutionModeToString(definition.executionMode),
        definition.workType == TaskWorkType::ACTION ? "ACTION" : "SEQUENCE"
    ));

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

    // 이벤트 발행: Task 완료 상태
    if (result.status == TaskStatus::COMPLETED) {
        publishEvent(std::make_shared<mxrc::core::event::TaskCompletedEvent>(
            definition.id,
            definition.name,
            elapsed.count(),
            result.progress * 100.0  // progressPercent
        ));
    } else if (result.status == TaskStatus::FAILED) {
        publishEvent(std::make_shared<mxrc::core::event::TaskFailedEvent>(
            definition.id,
            definition.name,
            result.errorMessage,
            elapsed.count(),
            result.progress * 100.0  // progressPercent
        ));
    } else if (result.status == TaskStatus::CANCELLED) {
        publishEvent(std::make_shared<mxrc::core::event::TaskCancelledEvent>(
            definition.id,
            definition.name,
            elapsed.count(),
            result.progress * 100.0  // progressPercent
        ));
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

        // Action 생성
        auto action = actionFactory_->createAction(definition.workId, params);

        // 비동기 실행 시작
        std::string actionId = actionExecutor_->executeAsync(action, context);

        // 완료 또는 취소 대기
        while (actionExecutor_->isRunning(actionId)) {
            // 취소 요청 확인
            if (state.cancelRequested) {
                Logger::get()->info("[TaskExecutor] Task {} cancelling action {}",
                                   definition.id, actionId);
                actionExecutor_->cancel(actionId);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // 결과 수집
        auto actionResult = actionExecutor_->getResult(actionId);

        // 결과 변환
        if (actionResult.isCancelled() || (actionResult.status == mxrc::core::action::ActionStatus::TIMEOUT && state.cancelRequested)) {
            result.status = TaskStatus::CANCELLED;
            state.status = TaskStatus::CANCELLED;
            Logger::get()->info("[TaskExecutor] Task {} was cancelled", definition.id);
        } else if (actionResult.isSuccessful()) {
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
        // Sequence 정의 생성
        mxrc::core::sequence::SequenceDefinition seqDef(definition.workId, definition.workId);

        // 비동기 실행을 위해 별도 스레드에서 실행
        std::atomic<bool> sequenceCompleted{false};
        mxrc::core::sequence::SequenceResult seqResult;

        std::thread seqThread([&]() {
            seqResult = sequenceEngine_->execute(seqDef, context);
            sequenceCompleted = true;
        });

        // 완료 또는 취소 대기
        while (!sequenceCompleted) {
            // 취소 요청 확인
            if (state.cancelRequested) {
                Logger::get()->info("[TaskExecutor] Task {} cancelling sequence {}",
                                   definition.id, definition.workId);
                sequenceEngine_->cancel(definition.workId);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // 스레드 종료 대기
        if (seqThread.joinable()) {
            seqThread.join();
        }

        // 결과 변환
        if (seqResult.status == mxrc::core::sequence::SequenceStatus::CANCELLED) {
            result.status = TaskStatus::CANCELLED;
            state.status = TaskStatus::CANCELLED;
            Logger::get()->info("[TaskExecutor] Task {} was cancelled", definition.id);
        } else if (seqResult.isSuccessful()) {
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

int TaskExecutor::clearCompletedTasks() {
    auto logger = Logger::get();
    std::lock_guard<std::mutex> lock(stateMutex_);

    int count = 0;
    for (auto it = states_.begin(); it != states_.end(); ) {
        if (it->second.status == TaskStatus::COMPLETED ||
            it->second.status == TaskStatus::FAILED ||
            it->second.status == TaskStatus::CANCELLED) {
            logger->debug("[TaskExecutor] Clearing completed task: {} (status: {})",
                         it->first, taskStatusToString(it->second.status));
            it = states_.erase(it);
            count++;
        } else {
            ++it;
        }
    }

    if (count > 0) {
        logger->info("[TaskExecutor] Cleared {} completed tasks", count);
    }
    return count;
}

} // namespace mxrc::core::task
