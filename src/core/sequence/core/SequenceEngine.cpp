#include "SequenceEngine.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <algorithm>
#include <ctime>

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

        // 병렬 분기 확인
        const ParallelBranch* parallelBranch = getParallelBranch(itemId);
        if (parallelBranch) {
            if (logger) {
                logger->debug("병렬 분기 감지: {}", itemId);
            }
            // 병렬 분기 실행
            bool parallelSuccess = executeParallel(*parallelBranch, context, executionId);
            if (!parallelSuccess) {
                allSuccess = false;
            }
        } else {
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

void SequenceEngine::registerParallelBranch(const ParallelBranch& branch) {
    auto logger = spdlog::get("mxrc");
    if (logger) {
        logger->info("병렬 분기 등록: id={}, branches.size={}", branch.id, branch.branches.size());
    }
    parallelBranches_[branch.id] = branch;
}

const ConditionalBranch* SequenceEngine::getBranch(const std::string& branchId) const {
    auto it = branches_.find(branchId);
    if (it != branches_.end()) {
        return &it->second;
    }
    return nullptr;
}

const ParallelBranch* SequenceEngine::getParallelBranch(const std::string& branchId) const {
    auto it = parallelBranches_.find(branchId);
    if (it != parallelBranches_.end()) {
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

bool SequenceEngine::executeParallel(
    const ParallelBranch& branch,
    std::shared_ptr<ExecutionContext> context,
    const std::string& executionId) {

    auto logger = spdlog::get("mxrc");

    if (logger) {
        logger->info("병렬 분기 실행: id={}, branches.size={}",
            branch.id, branch.branches.size());
    }

    // 취소 요청 확인
    auto stateIt = executionState_.find(executionId);
    if (stateIt != executionState_.end() && !stateIt->second.first) {
        if (logger) {
            logger->info("병렬 분기 실행 중단됨: {}", executionId);
        }
        return false;
    }

    // 각 분기를 별도 스레드에서 실행
    std::vector<std::thread> threads;
    std::vector<bool> branchResults(branch.branches.size(), false);
    std::vector<std::mutex> result_mutexes(branch.branches.size());

    for (size_t i = 0; i < branch.branches.size(); ++i) {
        threads.emplace_back(
            [this, &branch, context, executionId, i, &branchResults, &result_mutexes]() {
                bool result = executeActionSequence(
                    branch.branches[i], context, executionId);
                {
                    std::lock_guard<std::mutex> lock(result_mutexes[i]);
                    branchResults[i] = result;
                }
            });
    }

    // 모든 스레드 완료 대기
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    if (logger) {
        logger->info("병렬 분기 완료: id={}", branch.id);
    }

    // 모든 분기 성공 확인
    bool allSuccess = true;
    for (bool result : branchResults) {
        if (!result) {
            allSuccess = false;
            break;
        }
    }

    return allSuccess;
}

bool SequenceEngine::executeActionSequence(
    const std::vector<std::string>& actionIds,
    std::shared_ptr<ExecutionContext> context,
    const std::string& executionId) {

    auto logger = spdlog::get("mxrc");

    if (actionIds.empty()) {
        return true;
    }

    bool allSuccess = true;

    for (const auto& actionId : actionIds) {
        // 취소 요청 확인
        auto stateIt = executionState_.find(executionId);
        if (stateIt != executionState_.end() && !stateIt->second.first) {
            if (logger) {
                logger->info("액션 시퀀스 실행 중단됨: {}", executionId);
            }
            return false;
        }

        if (logger) {
            logger->debug("병렬 액션 실행: {}", actionId);
        }

        // 동작 생성
        std::map<std::string, std::string> params;
        auto action = actionFactory_->createAction(actionId, actionId, params);

        if (!action) {
            if (logger) {
                logger->error("병렬 액션 생성 실패: {}", actionId);
            }
            {
                std::lock_guard<std::mutex> lock(monitorMutex_);
                monitor_->logActionExecution(
                    executionId, actionId, ActionStatus::FAILED,
                    "Failed to create action");
            }
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
        {
            std::lock_guard<std::mutex> lock(monitorMutex_);
            monitor_->logActionExecution(
                executionId,
                actionId,
                status,
                actionSuccess ? "" : actionExecutor_->getLastErrorMessage());
        }

        if (!actionSuccess) {
            allSuccess = false;
            if (logger) {
                logger->warn("병렬 액션 실패: {}", actionId);
            }
        } else {
            if (logger) {
                logger->info("병렬 액션 완료: {}", actionId);
            }
        }
    }

    return allSuccess;
}

TemplateInstantiationResult SequenceEngine::instantiateTemplate(
    const std::string& templateId,
    const std::map<std::string, std::any>& parameters,
    const std::string& instanceName) {

    auto logger = spdlog::get("mxrc");

    // 템플릿 조회
    auto templatePtr = registry_->getTemplate(templateId);
    if (!templatePtr) {
        if (logger) {
            logger->error("템플릿을 찾을 수 없음: id={}", templateId);
        }
        return {false, "", "Template not found: " + templateId, {}};
    }

    // 파라미터 검증
    auto [valid, errors] = validateTemplateParameters(templatePtr, parameters);
    if (!valid) {
        if (logger) {
            logger->error("템플릿 파라미터 검증 실패: template={}, errors={}", templateId, errors.size());
        }
        return {false, "", "Parameter validation failed", errors};
    }

    // 인스턴스 ID 생성
    std::string instanceId = "inst_" + std::to_string(std::time(nullptr)) + "_" +
                            std::to_string(++executionCounter_);

    // 인스턴스 이름 (미제공 시 기본값)
    std::string finalInstanceName = instanceName.empty() ?
        templatePtr->name + "_" + instanceId : instanceName;

    // 파라미터 치환하여 액션 ID 생성
    std::vector<std::string> instantiatedActionIds;
    for (const auto& actionId : templatePtr->actionIds) {
        std::string substituted = substituteParameters(actionId, parameters);
        instantiatedActionIds.push_back(substituted);
    }

    // 새 시퀀스 정의 생성
    auto sequenceDef = std::make_shared<SequenceDefinition>();
    sequenceDef->id = instanceId;
    sequenceDef->name = finalInstanceName;
    sequenceDef->version = "1.0.0";
    sequenceDef->description = "Instantiated from template: " + templateId;
    sequenceDef->actionIds = instantiatedActionIds;
    sequenceDef->metadata = templatePtr->metadata;

    // 템플릿 인스턴스 생성 및 저장
    SequenceTemplateInstance instance;
    instance.templateId = templateId;
    instance.instanceId = instanceId;
    instance.instanceName = finalInstanceName;
    instance.parameters = parameters;
    instance.sequenceDefinition = sequenceDef;
    instance.createdAtMs = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;

    try {
        registry_->saveTemplateInstance(instance);
        registry_->registerSequence(*sequenceDef);

        if (logger) {
            logger->info(
                "템플릿 인스턴스화 완료: template={}, instance={}, actions={}",
                templateId, instanceId, instantiatedActionIds.size());
        }

        return {true, instanceId, "", {}};
    } catch (const std::exception& ex) {
        if (logger) {
            logger->error("템플릿 인스턴스화 중 오류: template={}, error={}",
                templateId, ex.what());
        }
        return {false, "", std::string(ex.what()), {}};
    }
}

std::string SequenceEngine::executeTemplate(
    const std::string& templateId,
    const std::map<std::string, std::any>& parameters,
    const std::string& instanceName) {

    // 템플릿 인스턴스화
    auto result = instantiateTemplate(templateId, parameters, instanceName);
    if (!result.success) {
        auto logger = spdlog::get("mxrc");
        if (logger) {
            logger->error("템플릿 실행 실패: template={}, error={}", templateId, result.errorMessage);
        }
        return "";
    }

    // 인스턴스 ID로 시퀀스 실행
    try {
        return execute(result.instanceId, parameters);
    } catch (const std::exception& ex) {
        auto logger = spdlog::get("mxrc");
        if (logger) {
            logger->error("템플릿 시퀀스 실행 실패: instance={}, error={}",
                result.instanceId, ex.what());
        }
        return "";
    }
}

std::pair<bool, std::vector<std::string>> SequenceEngine::validateTemplateParameters(
    const std::shared_ptr<const SequenceTemplate>& templatePtr,
    const std::map<std::string, std::any>& parameters) {

    std::vector<std::string> errors;

    if (!templatePtr) {
        errors.push_back("Template is null");
        return {false, errors};
    }

    // 필수 파라미터 확인
    for (const auto& param : templatePtr->parameters) {
        if (param.required) {
            auto it = parameters.find(param.name);
            if (it == parameters.end()) {
                errors.push_back("Required parameter missing: " + param.name);
            }
        }
    }

    // 알려지지 않은 파라미터 확인 (경고 수준, 에러 아님)
    std::vector<std::string> validParamNames;
    for (const auto& param : templatePtr->parameters) {
        validParamNames.push_back(param.name);
    }

    for (const auto& [paramName, _] : parameters) {
        auto it = std::find(validParamNames.begin(), validParamNames.end(), paramName);
        if (it == validParamNames.end()) {
            // 알려지지 않은 파라미터는 무시 (경고만 발생)
            auto logger = spdlog::get("mxrc");
            if (logger) {
                logger->warn("Unknown template parameter: {}", paramName);
            }
        }
    }

    return {errors.empty(), errors};
}

std::string SequenceEngine::anyToString(const std::any& value) {
    try {
        if (value.type() == typeid(int)) {
            return std::to_string(std::any_cast<int>(value));
        } else if (value.type() == typeid(float)) {
            return std::to_string(std::any_cast<float>(value));
        } else if (value.type() == typeid(double)) {
            return std::to_string(std::any_cast<double>(value));
        } else if (value.type() == typeid(bool)) {
            return std::any_cast<bool>(value) ? "true" : "false";
        } else if (value.type() == typeid(std::string)) {
            return std::any_cast<std::string>(value);
        } else if (value.type() == typeid(const char*)) {
            return std::string(std::any_cast<const char*>(value));
        }
    } catch (...) {
        return "";
    }
    return "";
}

std::string SequenceEngine::substituteParameters(
    const std::string& actionId,
    const std::map<std::string, std::any>& parameters) {

    std::string result = actionId;

    // 파라미터 치환: ${paramName} -> 파라미터 값
    for (const auto& [paramName, paramValue] : parameters) {
        std::string placeholder = "${" + paramName + "}";
        std::string valueStr = anyToString(paramValue);

        // 모든 ${paramName} 발생을 값으로 치환
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), valueStr);
            pos += valueStr.length();
        }
    }

    return result;
}

} // namespace mxrc::core::sequence

