#pragma once

#include "core/sequence/interfaces/IAction.h"
#include "core/sequence/core/ExecutionContext.h"
#include "core/sequence/core/RetryPolicy.h"
#include "core/sequence/dto/ActionStatus.h"
#include <memory>
#include <chrono>

namespace mxrc::core::sequence {

/**
 * @brief 개별 동작 실행 관리자
 *
 * 단일 동작의 실행, 타임아웃, 재시도 등을 관리합니다.
 */
class ActionExecutor {
public:
    ActionExecutor() = default;
    ~ActionExecutor() = default;

    // Copy/Move
    ActionExecutor(const ActionExecutor&) = delete;
    ActionExecutor& operator=(const ActionExecutor&) = delete;
    ActionExecutor(ActionExecutor&&) = default;
    ActionExecutor& operator=(ActionExecutor&&) = default;

    /**
     * @brief 동작 실행 (타임아웃 및 재시도 지원)
     * @param action 실행할 동작
     * @param context 실행 컨텍스트
     * @param timeoutMs 타임아웃 (밀리초, 0 = 무제한)
     * @param retryPolicy 재시도 정책
     * @return 실행 성공 여부
     */
    bool execute(
        std::shared_ptr<IAction> action,
        ExecutionContext& context,
        int timeoutMs = 0,
        const RetryPolicy& retryPolicy = RetryPolicy::noRetry());

    /**
     * @brief 실행 중인 동작 취소
     * @param actionId 동작 ID
     */
    void cancel(const std::string& actionId);

    /**
     * @brief 마지막 실행 시간 조회 (밀리초)
     * @return 실행 시간
     */
    long long getLastExecutionTimeMs() const { return lastExecutionTimeMs_; }

    /**
     * @brief 마지막 동작 상태 조회
     * @return 동작 상태
     */
    ActionStatus getLastStatus() const { return lastStatus_; }

    /**
     * @brief 마지막 에러 메시지 조회
     * @return 에러 메시지
     */
    const std::string& getLastErrorMessage() const { return lastErrorMessage_; }

private:
    long long lastExecutionTimeMs_ = 0;   // 마지막 실행 시간
    ActionStatus lastStatus_ = ActionStatus::PENDING;  // 마지막 상태
    std::string lastErrorMessage_;        // 마지막 에러 메시지

    /**
     * @brief 동작 싱글 실행
     * @param action 실행할 동작
     * @param context 실행 컨텍스트
     * @param timeoutMs 타임아웃
     * @return 성공 여부
     */
    bool executeOnce(
        std::shared_ptr<IAction> action,
        ExecutionContext& context,
        int timeoutMs);

    /**
     * @brief 타임아웃 검사
     * @param startTime 시작 시간
     * @param timeoutMs 타임아웃 (0 = 무제한)
     * @return 타임아웃 여부
     */
    bool isTimedOut(
        std::chrono::steady_clock::time_point startTime,
        int timeoutMs) const;
};

} // namespace mxrc::core::sequence

