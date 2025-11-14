#pragma once

#include "RetryPolicy.h"
#include <string>
#include <functional>
#include <chrono>

namespace mxrc::core::sequence {

/**
 * @brief 재시도 로직 관리자
 *
 * 동작 실패 시 재시도 정책에 따라 자동 재시도를 수행합니다.
 */
class RetryHandler {
public:
    /**
     * @brief 재시도 결과 콜백 함수 타입
     * @param success 성공 여부
     * @param retryCount 수행된 재시도 횟수
     * @param errorMessage 에러 메시지 (실패 시)
     */
    using RetryCallback = std::function<void(bool success, int retryCount, const std::string& errorMessage)>;

    RetryHandler() = default;
    ~RetryHandler() = default;

    // Copy/Move 삭제
    RetryHandler(const RetryHandler&) = delete;
    RetryHandler& operator=(const RetryHandler&) = delete;
    RetryHandler(RetryHandler&&) = default;
    RetryHandler& operator=(RetryHandler&&) = default;

    /**
     * @brief 동작 실행 및 재시도 수행
     * @param action 실행할 동작
     * @param policy 재시도 정책
     * @param callback 재시도 결과 콜백
     * @return 최종 성공 여부
     */
    bool executeWithRetry(
        const std::function<bool()>& action,
        const RetryPolicy& policy,
        RetryCallback callback = nullptr);

    /**
     * @brief 현재 재시도 횟수 조회
     * @return 재시도 횟수
     */
    int getCurrentRetryCount() const { return currentRetryCount_; }

    /**
     * @brief 마지막 에러 메시지 조회
     * @return 에러 메시지
     */
    const std::string& getLastError() const { return lastErrorMessage_; }

    /**
     * @brief 재시도 통계 초기화
     */
    void reset() {
        currentRetryCount_ = 0;
        lastErrorMessage_.clear();
    }

private:
    int currentRetryCount_ = 0;           // 현재 재시도 횟수
    std::string lastErrorMessage_;         // 마지막 에러 메시지

    /**
     * @brief 주어진 시간(밀리초) 동안 대기
     * @param delayMs 대기 시간
     */
    void sleep(int delayMs);
};

} // namespace mxrc::core::sequence

