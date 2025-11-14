#ifndef MXRC_CORE_SEQUENCE_RETRY_HANDLER_H
#define MXRC_CORE_SEQUENCE_RETRY_HANDLER_H

#include "core/sequence/dto/RetryPolicy.h"
#include "core/action/util/Logger.h"
#include <thread>
#include <chrono>

namespace mxrc::core::sequence {

/**
 * @brief 재시도 핸들러
 *
 * Action 실패 시 재시도 로직을 처리합니다.
 */
class RetryHandler {
public:
    RetryHandler() = default;
    ~RetryHandler() = default;

    /**
     * @brief 재시도 대기
     *
     * @param policy 재시도 정책
     * @param currentRetry 현재 재시도 횟수 (0부터 시작)
     */
    void waitBeforeRetry(const RetryPolicy& policy, int currentRetry) const;

    /**
     * @brief 재시도 가능 여부 확인
     *
     * @param policy 재시도 정책
     * @param currentRetry 현재 재시도 횟수
     * @return 재시도 가능하면 true
     */
    bool canRetry(const RetryPolicy& policy, int currentRetry) const;
};

} // namespace mxrc::core::sequence

#endif // MXRC_CORE_SEQUENCE_RETRY_HANDLER_H
