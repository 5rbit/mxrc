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

// Forward declaration for EventBus (optional dependency)
namespace mxrc::core::event {
    class IEventBus;
    class IEvent;
}

namespace mxrc::core::action {

/**
 * @brief Action 실행기
 *
 * 개별 Action의 실행, 타임아웃 관리, 결과 수집을 담당합니다.
 * 비동기 실행 및 실시간 타임아웃/취소를 지원합니다.
 *
 * EventBus 통합: ActionExecutor는 선택적으로 EventBus를 받아
 * 모든 주요 상태 전환 시 이벤트를 발행합니다.
 */
class ActionExecutor : public std::enable_shared_from_this<ActionExecutor> {
public:
    /**
     * @brief ActionExecutor 생성자
     *
     * @param eventBus 이벤트 버스 (nullptr이면 이벤트 발행하지 않음)
     */
    explicit ActionExecutor(std::shared_ptr<mxrc::core::event::IEventBus> eventBus = nullptr);
    ~ActionExecutor();

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
     * @brief Action 비동기 실행 (새로운 Stateful API)
     *
     * @param action 실행할 Action
     * @param context 실행 컨텍스트
     * @param timeout 타임아웃 (0 = 무제한)
     * @return actionId 실행 중인 액션의 고유 ID
     */
    std::string executeAsync(
        std::shared_ptr<IAction> action,
        ExecutionContext& context,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    /**
     * @brief actionId로 액션 취소 (새로운 Stateful API)
     *
     * @param actionId 취소할 액션의 ID
     */
    void cancel(const std::string& actionId);

    /**
     * @brief Action 취소 (기존 API - 하위 호환성)
     *
     * @param action 취소할 Action
     */
    void cancel(std::shared_ptr<IAction> action);

    /**
     * @brief 액션이 실행 중인지 확인
     *
     * @param actionId 확인할 액션의 ID
     * @return 실행 중이면 true
     */
    bool isRunning(const std::string& actionId) const;

    /**
     * @brief 액션의 실행 결과 조회
     *
     * @param actionId 조회할 액션의 ID
     * @return 실행 결과
     */
    ExecutionResult getResult(const std::string& actionId);

    /**
     * @brief 액션 완료 대기
     *
     * @param actionId 대기할 액션의 ID
     */
    void waitForCompletion(const std::string& actionId);

    /**
     * @brief 완료된 액션 상태 정리
     *
     * 완료, 실패, 취소된 액션의 상태를 메모리에서 제거합니다.
     * @return 정리된 액션 개수
     */
    int clearCompletedActions();

private:
    // EventBus (optional)
    std::shared_ptr<mxrc::core::event::IEventBus> eventBus_;

    /**
     * @brief 이벤트 발행 헬퍼 (non-blocking)
     *
     * @tparam EventType 이벤트 타입
     * @param event 발행할 이벤트
     */
    template<typename EventType>
    void publishEvent(std::shared_ptr<EventType> event);

    /**
     * @brief 실행 중인 액션의 상태
     */
    struct ExecutionState {
        std::shared_ptr<IAction> action;
        std::future<void> future;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::milliseconds timeout;
        std::atomic<bool> cancelRequested{false};
        std::atomic<bool> shouldStopMonitoring{false};  // 타임아웃 모니터링 중지 플래그
        std::unique_ptr<std::thread> timeoutThread;      // 타임아웃 모니터링 스레드
        ExecutionResult result;
        bool exceptionOccurred{false};  // 예외 발생 여부
        std::string exceptionMessage;    // 예외 메시지

        ExecutionState() = default;
        ExecutionState(std::shared_ptr<IAction> act,
                      std::future<void> fut,
                      std::chrono::steady_clock::time_point start,
                      std::chrono::milliseconds to)
            : action(std::move(act))
            , future(std::move(fut))
            , startTime(start)
            , timeout(to)
            , cancelRequested(false)
            , shouldStopMonitoring(false)
            , exceptionOccurred(false) {}
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
