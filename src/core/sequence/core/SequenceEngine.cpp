#include "SequenceEngine.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace mxrc::core::sequence {

int SequenceEngine::executionCounter_ = 0;

SequenceEngine::SequenceEngine(
    std::shared_ptr<SequenceRegistry> registry,
    std::shared_ptr<IActionFactory> actionFactory)
    : registry_(registry),
      actionFactory_(actionFactory),
      actionExecutor_(std::make_shared<ActionExecutor>()),
      conditionEvaluator_(std::make_shared<ConditionEvaluator>()),
      monitor_(std::make_shared<ExecutionMonitor>()) {

    auto logger = spdlog::get("mxrc");
    if (logger) {
        logger->info("SequenceEngine 초기화됨");
    }
}

std::string SequenceEngine::execute(
    const std::string& sequenceId,
    const std::map<std::string, std::any>& parameters) {

    auto logger = spdlog::get("mxrc");

    // 시퀀스 정의 조회
    auto definition = registry_->getSequence(sequenceId);
    if (!definition) {
        if (logger) {
            logger->error("시퀀스를 찾을 수 없음: {}", sequenceId);
        }
        throw std::runtime_error("Sequence not found: " + sequenceId);
    }

    // 실행 ID 생성
    std::string executionId = generateExecutionId();
    if (logger) {
        logger->info("시퀀스 실행 시작: id={}, sequence={}", executionId, sequenceId);
    }

    // 실행 컨텍스트 생성
    auto context = std::make_shared<ExecutionContext>();
    context->setExecutionId(executionId);

    // 파라미터를 컨텍스트에 설정
    for (const auto& param : parameters) {
        context->setVariable(param.first, param.second);
    }

    // 실행 추적 저장
    executions_[executionId] = context;
    executionState_[executionId] = {true, false};  // running, !paused

    // 모니터링 시작
    monitor_->startExecution(
        executionId,
        sequenceId,
        definition->actionIds.size());

    // 순차 실행
    bool success = executeSequentially(definition, context, executionId);

    // 실행 종료
    SequenceStatus finalStatus = success ? SequenceStatus::COMPLETED : SequenceStatus::FAILED;
    monitor_->endExecution(executionId, finalStatus);
    executionState_[executionId].first = false;  // running = false

    if (logger) {
        logger->info(
            "시퀀스 실행 완료: id={}, status={}, success={}",
            executionId, toString(finalStatus), success);
    }

    return executionId;
}

bool SequenceEngine::pause(const std::string& executionId) {
    auto it = executionState_.find(executionId);
    if (it != executionState_.end() && it->second.first) {
        it->second.second = true;  // paused = true

        auto logger = spdlog::get("mxrc");
        if (logger) {
            logger->info("시퀀스 일시정지: {}", executionId);
        }
        return true;
    }
    return false;
}

bool SequenceEngine::resume(const std::string& executionId) {
    auto it = executionState_.find(executionId);
    if (it != executionState_.end() && it->second.first && it->second.second) {
        it->second.second = false;  // paused = false

        auto logger = spdlog::get("mxrc");
        if (logger) {
            logger->info("시퀀스 재개: {}", executionId);
        }
        return true;
    }
    return false;
}

bool SequenceEngine::cancel(const std::string& executionId) {
    auto it = executionState_.find(executionId);
    if (it != executionState_.end()) {
        it->second.first = false;  // running = false

        auto logger = spdlog::get("mxrc");
        if (logger) {
            logger->info("시퀀스 취소: {}", executionId);
        }

        // 모니터링 업데이트
        monitor_->endExecution(executionId, SequenceStatus::CANCELLED);
        return true;
    }
    return false;
}

SequenceExecutionResult SequenceEngine::getStatus(const std::string& executionId) const {
    return monitor_->getExecutionStatus(executionId);
}

std::vector<std::string> SequenceEngine::getRunningExecutions() const {
    return monitor_->getRunningExecutions();
}

std::vector<std::string> SequenceEngine::getCompletedExecutions() const {
    return monitor_->getCompletedExecutions();
}

std::shared_ptr<ExecutionContext> SequenceEngine::getExecutionContext(
    const std::string& executionId) const {

    auto it = executions_.find(executionId);
    if (it != executions_.end()) {
        return it->second;
    }
    return nullptr;
}

std::string SequenceEngine::generateExecutionId() {
    return "exec_" + std::to_string(std::time(nullptr)) + "_" +
           std::to_string(++executionCounter_);
}

bool SequenceEngine::executeSequentially(
    const std::shared_ptr<const SequenceDefinition>& definition,
    std::shared_ptr<ExecutionContext> context,
    const std::string& executionId) {

    auto logger = spdlog::get("mxrc");

    if (!definition || definition->actionIds.empty()) {
        if (logger) {
            logger->error("시퀀스 정의가 유효하지 않음");
        }
        return false;
    }

    int totalActions = definition->actionIds.size();
    bool allSuccess = true;

    for (size_t i = 0; i < definition->actionIds.size(); ++i) {
        // 취소 요청 확인
        auto stateIt = executionState_.find(executionId);
        if (stateIt != executionState_.end() && !stateIt->second.first) {
            if (logger) {
                logger->info("시퀀스 실행 중단됨: {}", executionId);
            }
            return false;
        }

        const std::string& itemId = definition->actionIds[i];

        if (logger) {
            logger->debug("항목 실행: {} ({}/{})", itemId, i + 1, totalActions);
        }

        // 조건부 분기 확인
        const ConditionalBranch* branch = getBranch(itemId);
        if (branch) {
            if (logger) {
                logger->debug("조건부 분기 감지: {}", itemId);
            }
            // 분기 실행
            bool branchSuccess = executeBranch(*branch, context, executionId);
            if (!branchSuccess) {
                allSuccess = false;
            }
        } else {
            // 일반 동작 실행
            if (logger) {
                logger->debug("동작 실행: {} ({}/{})", itemId, i + 1, totalActions);
            }

            // 동작 생성
            std::map<std::string, std::string> params;  // 기본 빈 파라미터
            auto action = actionFactory_->createAction(itemId, itemId, params);

            if (!action) {
                if (logger) {
                    logger->error("동작 생성 실패: {}", itemId);
                }
                monitor_->logActionExecution(
                    executionId, itemId, ActionStatus::FAILED,
                    "Failed to create action");
                allSuccess = false;
                continue;
            }

            // 동작 실행
            bool actionSuccess = actionExecutor_->execute(
                action,
                *context,
                0,  // no timeout
                RetryPolicy::noRetry());  // no retry

            // 로깅
            ActionStatus status = actionSuccess ? ActionStatus::COMPLETED : ActionStatus::FAILED;
            monitor_->logActionExecution(
                executionId,
                itemId,
                status,
                actionSuccess ? "" : actionExecutor_->getLastErrorMessage());

            if (!actionSuccess) {
                allSuccess = false;
                if (logger) {
                    logger->warn("동작 실패: {}", itemId);
                }
            } else {
                if (logger) {
                    logger->info("동작 완료: {}", itemId);
                }
            }
        }

        // 진행률 업데이트
        float progress = static_cast<float>(i + 1) / totalActions;
        monitor_->updateProgress(executionId, progress);
    }

    return allSuccess;
}

void SequenceEngine::registerBranch(const ConditionalBranch& branch) {
    auto logger = spdlog::get("mxrc");
    if (logger) {
        logger->info("조건부 분기 등록: id={}, condition={}", branch.id, branch.condition);
    }
    branches_[branch.id] = branch;
}

const ConditionalBranch* SequenceEngine::getBranch(const std::string& branchId) const {
    auto it = branches_.find(branchId);
    if (it != branches_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool SequenceEngine::executeBranch(
    const ConditionalBranch& branch,
    std::shared_ptr<ExecutionContext> context,
    const std::string& executionId) {

    auto logger = spdlog::get("mxrc");

    // 조건 평가
    bool conditionResult = false;
    try {
        conditionResult = conditionEvaluator_->evaluate(branch.condition, *context);
        if (logger) {
            logger->debug("조건 평가 결과: branch={}, condition={}, result={}",
                branch.id, branch.condition, conditionResult);
        }
    } catch (const std::exception& ex) {
        if (logger) {
            logger->error("조건 평가 실패: branch={}, error={}", branch.id, ex.what());
        }
        return false;
    }

    // 조건 결과에 따라 적절한 동작 실행
    const auto& actionsToExecute = conditionResult ? branch.thenActions : branch.elseActions;

    if (logger) {
        logger->info("분기 실행: id={}, path={}, actions={}",
            branch.id, conditionResult ? "THEN" : "ELSE", actionsToExecute.size());
    }

    bool allSuccess = true;

    for (const auto& actionId : actionsToExecute) {
        // 취소 요청 확인
        auto stateIt = executionState_.find(executionId);
        if (stateIt != executionState_.end() && !stateIt->second.first) {
            if (logger) {
                logger->info("분기 실행 중단됨: {}", executionId);
            }
            return false;
        }

        if (logger) {
            logger->debug("분기 내 동작 실행: branch={}, action={}", branch.id, actionId);
        }

        // 동작 생성
        std::map<std::string, std::string> params;
        auto action = actionFactory_->createAction(actionId, actionId, params);

        if (!action) {
            if (logger) {
                logger->error("동작 생성 실패: {}", actionId);
            }
            monitor_->logActionExecution(
                executionId, actionId, ActionStatus::FAILED,
                "Failed to create action");
            allSuccess = false;
            continue;
        }

        // 동작 실행
        bool actionSuccess = actionExecutor_->execute(
            action,
            *context,
            0,  // no timeout
            RetryPolicy::noRetry());

        // 로깅
        ActionStatus status = actionSuccess ? ActionStatus::COMPLETED : ActionStatus::FAILED;
        monitor_->logActionExecution(
            executionId,
            actionId,
            status,
            actionSuccess ? "" : actionExecutor_->getLastErrorMessage());

        if (!actionSuccess) {
            allSuccess = false;
            if (logger) {
                logger->warn("동작 실패: {}", actionId);
            }
        } else {
            if (logger) {
                logger->info("동작 완료: {}", actionId);
            }
        }
    }

    return allSuccess;
}

} // namespace mxrc::core::sequence

