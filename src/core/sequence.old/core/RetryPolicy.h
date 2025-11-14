#pragma once

#include <chrono>
#include <cmath>

namespace mxrc::core::sequence {

/**
 * @brief 동작 재시도 정책 설정
 *
 * 동작 실패 시 자동 재시도를 위한 정책을 정의합니다.
 */
struct RetryPolicy {
    /**
     * @brief 최대 재시도 횟수 (0 = 재시도 없음)
     */
    int maxRetries = 0;

    /**
     * @brief 첫 번째 재시도 대기 시간 (밀리초)
     */
    int initialDelayMs = 100;

    /**
     * @brief 최대 대기 시간 (밀리초)
     */
    int maxDelayMs = 10000;

    /**
     * @brief 지수 백오프 배수 (1 = 고정 간격, 2 = 2배씩 증가)
     */
    double backoffMultiplier = 2.0;

    /**
     * @brief 정책 유효성 검증
     * @return 유효하면 true
     */
    bool isValid() const {
        return maxRetries >= 0 &&
               initialDelayMs > 0 &&
               maxDelayMs >= initialDelayMs &&
               backoffMultiplier >= 1.0;
    }

    /**
     * @brief 주어진 재시도 횟수에 대한 대기 시간 계산
     * @param retryCount 재시도 횟수 (0부터 시작)
     * @return 대기 시간 (밀리초)
     */
    int getDelayForRetry(int retryCount) const {
        if (retryCount < 0 || retryCount > maxRetries) {
            return 0;
        }

        // 지수 백오프 계산
        int delay = static_cast<int>(initialDelayMs *
                    std::pow(backoffMultiplier, retryCount));

        // 최대 지연 제한
        if (delay > maxDelayMs) {
            delay = maxDelayMs;
        }

        return delay;
    }

    /**
     * @brief 기본값 (재시도 없음)
     */
    static RetryPolicy noRetry() {
        return RetryPolicy{0, 100, 10000, 2.0};
    }

    /**
     * @brief 기본 재시도 정책 (최대 3회, 지수 백오프)
     */
    static RetryPolicy defaultPolicy() {
        return RetryPolicy{3, 100, 5000, 2.0};
    }

    /**
     * @brief 공격적 재시도 정책 (최대 5회, 빠른 간격)
     */
    static RetryPolicy aggressive() {
        return RetryPolicy{5, 50, 2000, 1.5};
    }

    /**
     * @brief 보수적 재시도 정책 (최대 2회, 긴 간격)
     */
    static RetryPolicy conservative() {
        return RetryPolicy{2, 500, 10000, 1.0};
    }
};

} // namespace mxrc::core::sequence

