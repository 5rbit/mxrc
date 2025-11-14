#include "ExecutionMonitor.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mxrc::core::sequence {

void ExecutionMonitor::startExecution(
    const std::string& executionId,
    const std::string& sequenceId,
    int totalActions) {

    auto logger = spdlog::get("mxrc");

    ExecutionTracker tracker;
    tracker.result.executionId = executionId;
    tracker.result.sequenceId = sequenceId;
    tracker.result.status = SequenceStatus::RUNNING;
    tracker.result.progress = 0.0f;
    tracker.result.totalExecutionTimeMs = 0;
    tracker.startTime = std::chrono::steady_clock::now();

    executions_[executionId] = tracker;

    logger->info(
        "시퀀스 실행 시작: id={}, sequence={}, actions={}",
        executionId, sequenceId, totalActions);
}

void ExecutionMonitor::logActionExecution(
    const std::string& executionId,
    const std::string& actionId,
    ActionStatus status,
    const std::string& errorMessage) {

    auto logger = spdlog::get("mxrc");

    auto it = executions_.find(executionId);
    if (it == executions_.end()) {
        logger->warn("실행 ID를 찾을 수 없음: {}", executionId);
        return;
    }

    ExecutionTracker& tracker = it->second;

    // 로그 항목 생성
    ExecutionLogEntry entry;
    entry.executionId = executionId;
    entry.actionId = actionId;
    entry.actionStatus = status;
    entry.timestampMs = getCurrentTimeMs();
    entry.progress = tracker.result.progress;
    entry.errorMessage = errorMessage;

    tracker.logs.push_back(entry);

    // 결과 업데이트
    ActionExecutionResult actionResult;
    actionResult.actionId = actionId;
    actionResult.status = status;
    actionResult.progress = entry.progress;
    actionResult.errorMessage = errorMessage;
    actionResult.executionTimeMs = 0;
    actionResult.retryCount = 0;

    tracker.result.actionResults.push_back(actionResult);

    logger->debug(
        "동작 로그: execution={}, action={}, status={}",
        executionId, actionId, toString(status));
}

void ExecutionMonitor::updateProgress(
    const std::string& executionId,
    float progress) {

    auto it = executions_.find(executionId);
    if (it != executions_.end()) {
        it->second.result.progress = std::clamp(progress, 0.0f, 1.0f);
    }
}

void ExecutionMonitor::endExecution(
    const std::string& executionId,
    SequenceStatus finalStatus) {

    auto logger = spdlog::get("mxrc");

    auto it = executions_.find(executionId);
    if (it == executions_.end()) {
        logger->warn("실행 ID를 찾을 수 없음: {}", executionId);
        return;
    }

    ExecutionTracker& tracker = it->second;
    tracker.result.status = finalStatus;

    // 실행 시간 계산
    auto endTime = std::chrono::steady_clock::now();
    tracker.result.totalExecutionTimeMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - tracker.startTime).count();

    logger->info(
        "시퀀스 실행 종료: id={}, status={}, time={}ms",
        executionId, toString(finalStatus), tracker.result.totalExecutionTimeMs);
}

SequenceExecutionResult ExecutionMonitor::getExecutionStatus(
    const std::string& executionId) const {

    auto it = executions_.find(executionId);
    if (it != executions_.end()) {
        return it->second.result;
    }

    // 기본값 반환
    SequenceExecutionResult result;
    result.executionId = executionId;
    result.status = SequenceStatus::PENDING;
    result.progress = 0.0f;
    return result;
}

std::vector<ExecutionMonitor::ExecutionLogEntry> ExecutionMonitor::getExecutionLogs(
    const std::string& executionId) const {

    auto it = executions_.find(executionId);
    if (it != executions_.end()) {
        return it->second.logs;
    }

    return std::vector<ExecutionLogEntry>();
}

ActionExecutionResult ExecutionMonitor::getActionResult(
    const std::string& executionId,
    const std::string& actionId) const {

    auto it = executions_.find(executionId);
    if (it != executions_.end()) {
        const auto& actionResults = it->second.result.actionResults;
        auto resultIt = std::find_if(
            actionResults.begin(), actionResults.end(),
            [&actionId](const ActionExecutionResult& r) {
                return r.actionId == actionId;
            });

        if (resultIt != actionResults.end()) {
            return *resultIt;
        }
    }

    // 기본값 반환
    ActionExecutionResult result;
    result.actionId = actionId;
    result.status = ActionStatus::PENDING;
    return result;
}

std::vector<std::string> ExecutionMonitor::getCompletedExecutions() const {
    std::vector<std::string> completed;

    for (const auto& pair : executions_) {
        if (pair.second.result.status == SequenceStatus::COMPLETED ||
            pair.second.result.status == SequenceStatus::FAILED ||
            pair.second.result.status == SequenceStatus::CANCELLED) {
            completed.push_back(pair.first);
        }
    }

    return completed;
}

std::vector<std::string> ExecutionMonitor::getRunningExecutions() const {
    std::vector<std::string> running;

    for (const auto& pair : executions_) {
        if (pair.second.result.status == SequenceStatus::RUNNING ||
            pair.second.result.status == SequenceStatus::PAUSED) {
            running.push_back(pair.first);
        }
    }

    return running;
}

void ExecutionMonitor::clear() {
    executions_.clear();
    auto logger = spdlog::get("mxrc");
    if (logger) {
        logger->info("실행 모니터 초기화됨");
    }
}

bool ExecutionMonitor::removeExecution(const std::string& executionId) {
    auto it = executions_.find(executionId);
    if (it != executions_.end()) {
        executions_.erase(it);
        return true;
    }
    return false;
}

size_t ExecutionMonitor::getExecutionCount() const {
    return executions_.size();
}

long long ExecutionMonitor::getCurrentTimeMs() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace mxrc::core::sequence

