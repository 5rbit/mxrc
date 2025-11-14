#include "RetryHandler.h"
#include <thread>
#include <spdlog/spdlog.h>

namespace mxrc::core::sequence {

bool RetryHandler::executeWithRetry(
    const std::function<bool()>& action,
    const RetryPolicy& policy,
    RetryCallback callback) {

    if (!policy.isValid()) {
        lastErrorMessage_ = "Invalid retry policy";
        if (callback) {
            callback(false, 0, lastErrorMessage_);
        }
        return false;
    }

    auto logger = spdlog::get("mxrc");

    reset();

    // 첫 시도
    if (action()) {
        logger->debug("동작 성공 (재시도 없음)");
        if (callback) {
            callback(true, 0, "");
        }
        return true;
    }

    logger->info("동작 실패, 재시도 시작: maxRetries={}", policy.maxRetries);

    // 재시도 수행
    for (int attempt = 1; attempt <= policy.maxRetries; ++attempt) {
        int delayMs = policy.getDelayForRetry(attempt - 1);

        logger->info(
            "재시도 {}회 ({}/{}), {}ms 대기",
            attempt, attempt, policy.maxRetries, delayMs);

        // 대기
        sleep(delayMs);

        // 재시도 실행
        currentRetryCount_ = attempt;
        if (action()) {
            logger->info("동작 성공 (재시도 {}회 후)", attempt);
            if (callback) {
                callback(true, attempt, "");
            }
            return true;
        }

        logger->warn("재시도 {}회 실패", attempt);
    }

    // 모든 재시도 실패
    lastErrorMessage_ = "All " + std::to_string(policy.maxRetries) + " retries failed";
    logger->error("동작 최종 실패: {}", lastErrorMessage_);

    if (callback) {
        callback(false, policy.maxRetries, lastErrorMessage_);
    }

    return false;
}

void RetryHandler::sleep(int delayMs) {
    if (delayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
}

} // namespace mxrc::core::sequence

