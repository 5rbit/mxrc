#ifndef MXRC_CORE_SEQUENCE_RETRY_POLICY_H
#define MXRC_CORE_SEQUENCE_RETRY_POLICY_H

#include <chrono>
#include <cmath>

namespace mxrc::core::sequence {

/**
 * @brief 재시도 정책
 *
 * Action 또는 Sequence 실패 시 재시도 방법을 정의
 */
struct RetryPolicy {
    int maxRetries{0};                              // 최대 재시도 횟수 (0 = 재시도 안함)
    std::chrono::milliseconds retryDelay{0};        // 재시도 간 대기 시간
    bool exponentialBackoff{false};                 // 지수 백오프 사용 여부
    double backoffMultiplier{2.0};                  // 백오프 배수 (exponentialBackoff가 true일 때)

    RetryPolicy() = default;

    RetryPolicy(int retries, std::chrono::milliseconds delay = std::chrono::milliseconds(0))
        : maxRetries(retries), retryDelay(delay) {}

    /**
     * @brief 최대 재시도 횟수 설정
     */
    RetryPolicy& setMaxRetries(int retries) {
        maxRetries = retries;
        return *this;
    }

    /**
     * @brief 재시도 대기 시간 설정
     */
    RetryPolicy& setRetryDelay(std::chrono::milliseconds delay) {
        retryDelay = delay;
        return *this;
    }

    /**
     * @brief 지수 백오프 설정
     */
    RetryPolicy& setExponentialBackoff(bool enabled, double multiplier = 2.0) {
        exponentialBackoff = enabled;
        backoffMultiplier = multiplier;
        return *this;
    }

    /**
     * @brief N번째 재시도의 대기 시간 계산
     */
    std::chrono::milliseconds calculateDelay(int retryCount) const {
        if (!exponentialBackoff) {
            return retryDelay;
        }

        // 지수 백오프: delay * (multiplier ^ retryCount)
        double multipliedDelay = retryDelay.count() * std::pow(backoffMultiplier, retryCount);
        return std::chrono::milliseconds(static_cast<long>(multipliedDelay));
    }
};

} // namespace mxrc::core::sequence

#endif // MXRC_CORE_SEQUENCE_RETRY_POLICY_H
