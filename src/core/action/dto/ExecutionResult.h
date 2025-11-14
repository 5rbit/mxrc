#ifndef MXRC_CORE_ACTION_EXECUTION_RESULT_H
#define MXRC_CORE_ACTION_EXECUTION_RESULT_H

#include "ActionStatus.h"
#include <string>
#include <any>
#include <chrono>

namespace mxrc::core::action {

/**
 * @brief Action 실행 결과
 *
 * Action 실행의 결과와 메타데이터를 포함합니다.
 */
struct ExecutionResult {
    std::string actionId;                               // Action ID
    ActionStatus status;                                // 최종 상태
    float progress;                                      // 진행률 (0.0 ~ 1.0)
    std::string errorMessage;                           // 에러 메시지 (실패 시)
    std::chrono::milliseconds executionTime{0};         // 실행 시간 (밀리초)
    int retryCount{0};                                  // 재시도 횟수
    std::any result;                                     // 실행 결과 데이터 (타입 자유)

    /**
     * @brief 기본 생성자
     */
    ExecutionResult() : status(ActionStatus::PENDING), progress(0.0f) {}

    /**
     * @brief 생성자
     *
     * @param actionId Action ID
     * @param status Action 상태
     */
    ExecutionResult(const std::string& actionId, ActionStatus status)
        : actionId(actionId), status(status), progress(0.0f) {}

    /**
     * @brief 성공 여부 확인
     *
     * @return Action이 성공적으로 완료되었으면 true
     */
    bool isSuccessful() const {
        return status == ActionStatus::COMPLETED;
    }

    /**
     * @brief 실패 여부 확인
     *
     * @return Action이 실패했으면 true
     */
    bool isFailed() const {
        return status == ActionStatus::FAILED ||
               status == ActionStatus::TIMEOUT;
    }

    /**
     * @brief 취소 여부 확인
     *
     * @return Action이 취소되었으면 true
     */
    bool isCancelled() const {
        return status == ActionStatus::CANCELLED;
    }
};

} // namespace mxrc::core::action

#endif // MXRC_CORE_ACTION_EXECUTION_RESULT_H
