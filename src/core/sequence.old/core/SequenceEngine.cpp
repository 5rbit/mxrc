#include "core/sequence/core/SequenceEngine.h"
#include <thread>
#include <chrono>

namespace mxrc::core::sequence {

using namespace mxrc::core::action;

SequenceEngine::SequenceEngine(
    std::shared_ptr<ActionFactory> factory,
    std::shared_ptr<ActionExecutor> executor
) : factory_(factory), executor_(executor) {
    Logger::get()->info("SequenceEngine initialized");
}

SequenceResult SequenceEngine::execute(
    const SequenceDefinition& definition,
    ExecutionContext& context
) {
    Logger::get()->info("Executing sequence: {} (name: {})", definition.id, definition.name);

    auto& state = getOrCreateState(definition.id);
    state.status = SequenceStatus::RUNNING;
    state.totalSteps = definition.steps.size();
    state.completedSteps = 0;
    state.cancelRequested = false;
    state.pauseRequested = false;

    auto startTime = std::chrono::steady_clock::now();

    // 순차 실행
    SequenceResult result = executeSequential(definition, context, state);

    auto endTime = std::chrono::steady_clock::now();
    result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime
    );

    Logger::get()->info(
        "Sequence {} finished with status: {} ({}/{} steps completed)",
        definition.id,
        sequenceStatusToString(result.status),
        result.completedSteps,
        result.totalSteps
    );

    return result;
}

SequenceResult SequenceEngine::executeSequential(
    const SequenceDefinition& definition,
    ExecutionContext& context,
    SequenceState& state
) {
    SequenceResult result;
    result.sequenceId = definition.id;
    result.status = SequenceStatus::RUNNING;
    result.totalSteps = definition.steps.size();
    result.completedSteps = 0;

    for (size_t i = 0; i < definition.steps.size(); ++i) {
        // 취소 확인
        if (state.cancelRequested) {
            Logger::get()->info("Sequence {} cancelled at step {}", definition.id, i);
            result.status = SequenceStatus::CANCELLED;
            state.status = SequenceStatus::CANCELLED;
            return result;
        }

        // 일시정지 확인
        while (state.pauseRequested && !state.cancelRequested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        const auto& step = definition.steps[i];
        
        Logger::get()->info(
            "Sequence {}: Executing step {}/{} - {} (type: {})",
            definition.id, i + 1, definition.steps.size(),
            step.actionId, step.actionType
        );

        try {
            // Action 파라미터 준비
            std::map<std::string, std::string> params = step.parameters;
            params["id"] = step.actionId;

            // Action 생성
            auto action = factory_->createAction(step.actionType, params);
            
            // Action 실행
            auto actionResult = executor_->execute(action, context);

            // Action 결과 저장
            context.setActionResult(step.actionId, actionResult);

            // Action 실패 시
            if (actionResult.isFailed()) {
                Logger::get()->error(
                    "Sequence {}: Step {} ({}) failed: {}",
                    definition.id, i + 1, step.actionId, actionResult.errorMessage
                );
                result.status = SequenceStatus::FAILED;
                result.errorMessage = "Step " + std::to_string(i + 1) + 
                                     " (" + step.actionId + ") failed: " + actionResult.errorMessage;
                state.status = SequenceStatus::FAILED;
                return result;
            }

            // 완료 스텝 업데이트
            result.completedSteps++;
            state.completedSteps++;
            updateProgress(state, result.completedSteps, result.totalSteps);

        } catch (const std::exception& e) {
            Logger::get()->error(
                "Sequence {}: Exception at step {} ({}): {}",
                definition.id, i + 1, step.actionId, e.what()
            );
            result.status = SequenceStatus::FAILED;
            result.errorMessage = "Step " + std::to_string(i + 1) + 
                                 " (" + step.actionId + ") exception: " + e.what();
            state.status = SequenceStatus::FAILED;
            return result;
        }
    }

    // 모든 스텝 완료
    result.status = SequenceStatus::COMPLETED;
    result.progress = 1.0f;
    state.status = SequenceStatus::COMPLETED;
    state.progress = 1.0f;

    return result;
}

void SequenceEngine::cancel(const std::string& sequenceId) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = states_.find(sequenceId);
    if (it != states_.end()) {
        it->second.cancelRequested = true;
        Logger::get()->info("Cancel requested for sequence: {}", sequenceId);
    }
}

void SequenceEngine::pause(const std::string& sequenceId) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = states_.find(sequenceId);
    if (it != states_.end()) {
        it->second.pauseRequested = true;
        it->second.status = SequenceStatus::PAUSED;
        Logger::get()->info("Pause requested for sequence: {}", sequenceId);
    }
}

void SequenceEngine::resume(const std::string& sequenceId) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = states_.find(sequenceId);
    if (it != states_.end()) {
        it->second.pauseRequested = false;
        it->second.status = SequenceStatus::RUNNING;
        Logger::get()->info("Resume requested for sequence: {}", sequenceId);
    }
}

SequenceStatus SequenceEngine::getStatus(const std::string& sequenceId) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = states_.find(sequenceId);
    if (it != states_.end()) {
        return it->second.status;
    }
    return SequenceStatus::PENDING;
}

float SequenceEngine::getProgress(const std::string& sequenceId) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = states_.find(sequenceId);
    if (it != states_.end()) {
        return it->second.progress.load();
    }
    return 0.0f;
}

SequenceEngine::SequenceState& SequenceEngine::getOrCreateState(const std::string& sequenceId) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return states_[sequenceId];
}

void SequenceEngine::updateProgress(SequenceState& state, int completedSteps, int totalSteps) {
    if (totalSteps > 0) {
        float progress = static_cast<float>(completedSteps) / static_cast<float>(totalSteps);
        state.progress = progress;
    }
}

} // namespace mxrc::core::sequence
