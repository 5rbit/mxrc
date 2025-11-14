#include "core/sequence/core/SequenceEngine.h"
#include <thread>
#include <chrono>

namespace mxrc::core::sequence {

using namespace mxrc::core::action;

SequenceEngine::SequenceEngine(
    std::shared_ptr<ActionFactory> factory,
    std::shared_ptr<ActionExecutor> executor
) : factory_(factory),
    executor_(executor),
    conditionEvaluator_(std::make_unique<ConditionEvaluator>()),
    retryHandler_(std::make_unique<RetryHandler>()) {
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

    std::set<std::string> executedActions;  // 실행된 Action ID 추적

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

            // 재시도 로직
            int retryCount = 0;
            ExecutionResult actionResult;
            bool actionSucceeded = false;

            while (!actionSucceeded) {
                // Action 생성
                auto action = factory_->createAction(step.actionType, params);

                // Action 실행
                actionResult = executor_->execute(action, context);

                if (actionResult.isSuccessful()) {
                    actionSucceeded = true;
                } else {
                    // 재시도 정책 확인
                    if (definition.retryPolicy.has_value() &&
                        retryHandler_->canRetry(definition.retryPolicy.value(), retryCount)) {

                        Logger::get()->warn(
                            "Sequence {}: Step {} ({}) failed, retrying ({}/{}): {}",
                            definition.id, i + 1, step.actionId,
                            retryCount + 1, definition.retryPolicy->maxRetries,
                            actionResult.errorMessage
                        );

                        retryHandler_->waitBeforeRetry(definition.retryPolicy.value(), retryCount);
                        retryCount++;
                    } else {
                        // 재시도 불가능 또는 정책 없음
                        break;
                    }
                }
            }

            // Action 결과 저장
            context.setActionResult(step.actionId, actionResult);

            // Action 실패 시 (재시도 후에도)
            if (actionResult.isFailed()) {
                Logger::get()->error(
                    "Sequence {}: Step {} ({}) failed after {} retries: {}",
                    definition.id, i + 1, step.actionId, retryCount, actionResult.errorMessage
                );
                result.status = SequenceStatus::FAILED;
                result.errorMessage = "Step " + std::to_string(i + 1) +
                                     " (" + step.actionId + ") failed after " +
                                     std::to_string(retryCount) + " retries: " + actionResult.errorMessage;
                state.status = SequenceStatus::FAILED;
                return result;
            }

            // 완료 스텝 업데이트
            result.completedSteps++;
            state.completedSteps++;
            executedActions.insert(step.actionId);
            updateProgress(state, result.completedSteps, result.totalSteps);

            // 조건부 분기 처리
            int branchSteps = handleConditionalBranch(step.actionId, definition, context, executedActions, state);
            result.completedSteps += branchSteps;
            state.completedSteps += branchSteps;
            if (branchSteps > 0) {
                updateProgress(state, result.completedSteps, result.totalSteps);
            }

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


int SequenceEngine::handleConditionalBranch(
    const std::string& actionId,
    const SequenceDefinition& definition,
    ExecutionContext& context,
    std::set<std::string>& executedActions,
    SequenceState& state
) {
    // 조건부 분기가 정의되어 있는지 확인
    auto branchIt = definition.conditionalBranches.find(actionId);
    if (branchIt == definition.conditionalBranches.end()) {
        return 0;  // 조건부 분기 없음
    }

    const auto& branch = branchIt->second;
    
    // 조건 평가
    bool conditionResult = false;
    try {
        conditionResult = conditionEvaluator_->evaluate(branch.condition, context);
        Logger::get()->info(
            "Sequence {}: Condition '{}' evaluated to {}",
            definition.id, branch.condition, conditionResult ? "true" : "false"
        );
    } catch (const std::exception& e) {
        Logger::get()->error(
            "Sequence {}: Failed to evaluate condition '{}': {}",
            definition.id, branch.condition, e.what()
        );
        return 0;
    }

    // 조건 결과에 따라 실행할 Action 목록 선택
    const auto& actionsToExecute = conditionResult ? branch.trueActions : branch.falseActions;

    int additionalSteps = 0;

    // 분기 Action들 실행
    for (const auto& branchActionId : actionsToExecute) {
        // 이미 실행된 Action은 스킵
        if (executedActions.find(branchActionId) != executedActions.end()) {
            continue;
        }

        // Action 정의 찾기 (steps에서)
        ActionStep const* stepPtr = nullptr;
        for (const auto& step : definition.steps) {
            if (step.actionId == branchActionId) {
                stepPtr = &step;
                break;
            }
        }

        if (!stepPtr) {
            Logger::get()->warn(
                "Sequence {}: Branch action '{}' not found in steps",
                definition.id, branchActionId
            );
            continue;
        }

        Logger::get()->info(
            "Sequence {}: Executing branch action {} (type: {})",
            definition.id, stepPtr->actionId, stepPtr->actionType
        );

        try {
            // Action 파라미터 준비
            std::map<std::string, std::string> params = stepPtr->parameters;
            params["id"] = stepPtr->actionId;

            // Action 생성
            auto action = factory_->createAction(stepPtr->actionType, params);
            
            // Action 실행
            auto actionResult = executor_->execute(action, context);

            // Action 결과 저장
            context.setActionResult(stepPtr->actionId, actionResult);

            // 실행된 Action 기록
            executedActions.insert(stepPtr->actionId);
            additionalSteps++;

            // Action 실패 시
            if (actionResult.isFailed()) {
                Logger::get()->error(
                    "Sequence {}: Branch action {} ({}) failed: {}",
                    definition.id, stepPtr->actionId, stepPtr->actionType, actionResult.errorMessage
                );
                // 분기 Action 실패는 전체 Sequence 실패로 이어지지 않음 (선택적)
                // 필요시 여기서 예외를 throw할 수 있음
            }

        } catch (const std::exception& e) {
            Logger::get()->error(
                "Sequence {}: Exception executing branch action {}: {}",
                definition.id, branchActionId, e.what()
            );
        }
    }

    return additionalSteps;
}

} // namespace mxrc::core::sequence
