#ifndef MXRC_CORE_ACTION_ACTION_EXECUTOR_H
#define MXRC_CORE_ACTION_ACTION_EXECUTOR_H

#include "core/action/interfaces/IAction.h"
#include "core/action/dto/ExecutionResult.h"
#include "core/action/util/ExecutionContext.h"
#include <memory>
#include <chrono>
#include <future>
#include <atomic>
#include <map>
#include <mutex>
#include <string>

namespace mxrc::core::action {

/**
 * @brief Action 실행기
 *
 * 개별 Action의 실행, 타임아웃 관리, 결과 수집을 담당합니다.
 * 비동기 실행 및 실시간 타임아웃/취소를 지원합니다.
 */
class ActionExecutor {
public:
    ActionExecutor() = default;
    ~ActionExecutor() = default;

    /**
     * @brief Action 실행 (비동기, 타임아웃 및 취소 지원)
     *
     * @param action 실행할 Action
     * @param context 실행 컨텍스트
     * @param timeout 타임아웃 (0 = 무제한)
     * @return 실행 결과
     */
    ExecutionResult execute(
        std::shared_ptr<IAction> action,
        ExecutionContext& context,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    /**
     * @brief Action 취소
     *
     * @param action 취소할 Action
     */
    void cancel(std::shared_ptr<IAction> action);

private:
    /**
     * @brief 실행 중인 액션의 상태
     */
    struct ExecutionState {
        std::shared_ptr<IAction> action;
        std::future<void> future;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::milliseconds timeout;
        std::atomic<bool> cancelRequested{false};
        ExecutionResult result;

        ExecutionState() = default;
        ExecutionState(std::shared_ptr<IAction> act,
                      std::future<void> fut,
                      std::chrono::steady_clock::time_point start,
                      std::chrono::milliseconds to)
            : action(std::move(act))
            , future(std::move(fut))
            , startTime(start)
            , timeout(to)
            , cancelRequested(false) {}
    };

    // 실행 중인 액션 관리
    std::map<std::string, ExecutionState> runningActions_;
    mutable std::mutex actionsMutex_;

    /**
     * @brief 타임아웃 체크
     *
     * @param action Action
     * @param startTime 시작 시간
     * @param timeout 타임아웃
     * @return 타임아웃 발생 시 true
     */
    bool checkTimeout(
        std::shared_ptr<IAction> action,
        std::chrono::steady_clock::time_point startTime,
        std::chrono::milliseconds timeout);
};

} // namespace mxrc::core::action

#endif // MXRC_CORE_ACTION_ACTION_EXECUTOR_H
