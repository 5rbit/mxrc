#include "ActionExecutor.h"
#include "RetryHandler.h"
#include <spdlog/spdlog.h>

namespace mxrc::core::sequence {

bool ActionExecutor::execute(
    std::shared_ptr<IAction> action,
    ExecutionContext& context,
    int timeoutMs,
    const RetryPolicy& retryPolicy) {

    if (!action) {
        lastErrorMessage_ = "Action is null";
        lastStatus_ = ActionStatus::FAILED;
        return false;
    }

    auto logger = spdlog::get("mxrc");

    auto startTime = std::chrono::steady_clock::now();

    logger->debug(
        "동작 실행 시작: id={}, timeout={}ms",
        action->getId(), timeoutMs);

    // 재시도 로직 수행
    RetryHandler retryHandler;
    bool success = retryHandler.executeWithRetry(
        [this, action, &context, timeoutMs, startTime]() {
            return executeOnce(action, context, timeoutMs);
        },
        retryPolicy,
        nullptr);

    // 실행 시간 기록
    auto endTime = std::chrono::steady_clock::now();
    lastExecutionTimeMs_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();

    // 상태 업데이트
    if (success) {
        lastStatus_ = ActionStatus::COMPLETED;
        lastErrorMessage_.clear();
        logger->info(
            "동작 완료: id={}, time={}ms",
            action->getId(), lastExecutionTimeMs_);
    } else {
        lastStatus_ = ActionStatus::FAILED;
        lastErrorMessage_ = retryHandler.getLastError();
        logger->error(
            "동작 실패: id={}, error={}",
            action->getId(), lastErrorMessage_);
    }

    return success;
}

void ActionExecutor::cancel(const std::string& actionId) {
    auto logger = spdlog::get("mxrc");
    if (logger) {
        logger->info("동작 취소 요청: id={}", actionId);
    }
    lastStatus_ = ActionStatus::CANCELLED;
}

bool ActionExecutor::executeOnce(
    std::shared_ptr<IAction> action,
    ExecutionContext& context,
    int timeoutMs) {

    auto logger = spdlog::get("mxrc");

    auto startTime = std::chrono::steady_clock::now();

    try {
        // 타임아웃 체크
        if (isTimedOut(startTime, timeoutMs)) {
            lastStatus_ = ActionStatus::TIMEOUT;
            lastErrorMessage_ = "Timeout before execution";
            logger->warn("동작 타임아웃: id={}", action->getId());
            return false;
        }

        // 동작 실행
        action->execute(context);

        // 완료 후 타임아웃 재확인
        if (isTimedOut(startTime, timeoutMs)) {
            lastStatus_ = ActionStatus::TIMEOUT;
            lastErrorMessage_ = "Timeout during execution";
            action->cancel();
            logger->warn("동작 타임아웃 (실행 중): id={}", action->getId());
            return false;
        }

        // 동작 상태 확인
        ActionStatus status = action->getStatus();
        lastStatus_ = status;

        if (status == ActionStatus::COMPLETED) {
            logger->debug("동작 완료: id={}", action->getId());
            return true;
        } else if (status == ActionStatus::FAILED) {
            lastErrorMessage_ = "Action reported failure";
            logger->warn("동작 실패: id={}", action->getId());
            return false;
        } else {
            lastErrorMessage_ = "Unexpected status: " + std::string(toString(status));
            logger->error("예상치 못한 상태: id={}, status={}",
                         action->getId(), toString(status));
            return false;
        }

    } catch (const std::exception& e) {
        lastStatus_ = ActionStatus::FAILED;
        lastErrorMessage_ = std::string("Exception: ") + e.what();
        logger->error("동작 예외: id={}, error={}", action->getId(), lastErrorMessage_);
        return false;
    } catch (...) {
        lastStatus_ = ActionStatus::FAILED;
        lastErrorMessage_ = "Unknown exception";
        logger->error("동작 알 수 없는 예외: id={}", action->getId());
        return false;
    }
}

bool ActionExecutor::isTimedOut(
    std::chrono::steady_clock::time_point startTime,
    int timeoutMs) const {

    if (timeoutMs <= 0) {
        return false;  // 타임아웃 없음
    }

    auto currentTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        currentTime - startTime).count();

    return elapsedMs >= timeoutMs;
}

} // namespace mxrc::core::sequence

