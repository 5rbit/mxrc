#include "ActionExecutor.h"
#include "core/action/util/Logger.h"
#include "interfaces/IEventBus.h"
#include "dto/ActionEvents.h"
#include <thread>

namespace mxrc::core::action {

ActionExecutor::ActionExecutor(std::shared_ptr<mxrc::core::event::IEventBus> eventBus)
    : eventBus_(eventBus) {
    // EventBus는 optional이므로 nullptr 허용
}

template<typename EventType>
void ActionExecutor::publishEvent(std::shared_ptr<EventType> event) {
    if (eventBus_ && event) {
        eventBus_->publish(event);
    }
}

ActionExecutor::~ActionExecutor() {
    auto logger = Logger::get();
    logger->info("[ActionExecutor] Destructor called, cleaning up running actions");

    std::vector<std::string> runningIds;

    // 실행 중인 모든 액션 ID 수집
    {
        std::lock_guard<std::mutex> lock(actionsMutex_);
        for (const auto& [id, state] : runningActions_) {
            runningIds.push_back(id);
            // 타임아웃 모니터링 스레드 중지 요청
            if (state.timeoutThread) {
                const_cast<ExecutionState&>(state).shouldStopMonitoring = true;
            }
        }
    }

    // 모든 실행 중인 액션 취소
    for (const auto& id : runningIds) {
        logger->debug("[ActionExecutor] Cancelling action {} during cleanup", id);
        cancel(id);
    }

    // 모든 타임아웃 스레드 종료 대기
    // NOTE: RAII 패턴으로 안전하게 처리 - 스레드를 먼저 수집한 후 락 없이 join
    std::vector<std::unique_ptr<std::thread>> threadsToJoin;
    {
        std::lock_guard<std::mutex> lock(actionsMutex_);
        for (auto& [id, state] : runningActions_) {
            if (state.timeoutThread && state.timeoutThread->joinable()) {
                logger->debug("[ActionExecutor] Collecting timeout thread for action {}", id);
                threadsToJoin.push_back(std::move(state.timeoutThread));
            }
        }
        runningActions_.clear();
    }

    // 락을 해제한 상태에서 스레드 종료 대기
    for (auto& thread : threadsToJoin) {
        if (thread && thread->joinable()) {
            logger->debug("[ActionExecutor] Joining timeout thread");
            thread->join();
        }
    }

    logger->info("[ActionExecutor] Destructor completed");
}

ExecutionResult ActionExecutor::execute(
    std::shared_ptr<IAction> action,
    ExecutionContext& context,
    std::chrono::milliseconds timeout) {

    auto logger = Logger::get();
    logger->info("[ActionExecutor] SYNC START - Action: {} (type: {}, timeout: {}ms)",
                 action->getId(), action->getType(), timeout.count());

    // 내부적으로 executeAsync() 사용
    std::string actionId = executeAsync(action, context, timeout);

    // 완료 대기
    waitForCompletion(actionId);

    // 결과 반환
    ExecutionResult result = getResult(actionId);

    // 맵에서 제거 (정리)
    {
        std::lock_guard<std::mutex> lock(actionsMutex_);
        auto it = runningActions_.find(actionId);
        if (it != runningActions_.end()) {
            // 타임아웃 모니터링 스레드 중지 및 정리
            if (it->second.timeoutThread) {
                it->second.shouldStopMonitoring = true;
                if (it->second.timeoutThread->joinable()) {
                    // 뮤텍스를 해제하고 join (데드락 방지)
                    actionsMutex_.unlock();
                    it->second.timeoutThread->join();
                    actionsMutex_.lock();
                }
            }
            runningActions_.erase(actionId);
            logger->debug("[ActionExecutor] Action {} removed from runningActions_ map", actionId);
        }
    }

    logger->debug("[ActionExecutor] SYNC END - Action {} execution time: {}ms, status: {}",
                 actionId, result.executionTime.count(), static_cast<int>(result.status));

    return result;
}

std::string ActionExecutor::executeAsync(
    std::shared_ptr<IAction> action,
    ExecutionContext& context,
    std::chrono::milliseconds timeout) {

    auto logger = Logger::get();
    std::string actionId = action->getId();
    auto startTime = std::chrono::steady_clock::now();

    logger->info("[ActionExecutor] ASYNC START - Action: {} (type: {}, timeout: {}ms)",
                 actionId, action->getType(), timeout.count());

    // 이벤트 발행: ACTION_STARTED
    publishEvent(std::make_shared<mxrc::core::event::ActionStartedEvent>(
        actionId, action->getType()));

    // 비동기로 액션 실행
    auto future = std::async(std::launch::async, [action, &context, actionId, logger, weak_self = weak_from_this(), startTime]() {
        auto self = weak_self.lock();
        if (!self) {
            logger->warn("[ActionExecutor] ASYNC ABORT - ActionExecutor expired before action {} could run.", actionId);
            return;
        }

        try {
            action->execute(context);
            logger->info("[ActionExecutor] ASYNC COMPLETE - Action {} finished successfully", actionId);

            // 이벤트 발행: ACTION_COMPLETED
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime);
            self->publishEvent(std::make_shared<mxrc::core::event::ActionCompletedEvent>(
                actionId, action->getType(), elapsed.count()));

        } catch (const std::exception& e) {
            logger->error("[ActionExecutor] ASYNC FAILED - Action {} threw exception: {}", actionId, e.what());

            // 상태에 오류 정보 저장
            {
                std::lock_guard<std::mutex> lock(self->actionsMutex_);
                auto it = self->runningActions_.find(actionId);
                if (it != self->runningActions_.end()) {
                    it->second.exceptionOccurred = true;
                    it->second.exceptionMessage = e.what();
                }
            }

            // 이벤트 발행: ACTION_FAILED
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime);
            self->publishEvent(std::make_shared<mxrc::core::event::ActionFailedEvent>(
                actionId, action->getType(), e.what(), elapsed.count()));
        }
    });

    // ExecutionState 생성 및 저장
    {
        std::lock_guard<std::mutex> lock(actionsMutex_);
        runningActions_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(actionId),
            std::forward_as_tuple(action, std::move(future), startTime, timeout)
        );
        logger->debug("[ActionExecutor] Action {} registered in runningActions_ map", actionId);
    }

    // 타임아웃이 설정된 경우 백그라운드 모니터링 시작
    if (timeout.count() > 0) {
        // 타임아웃 모니터링 스레드를 관리 가능하게 생성
        std::lock_guard<std::mutex> lock(actionsMutex_);
        auto it = runningActions_.find(actionId);
        if (it != runningActions_.end()) {
            it->second.timeoutThread = std::make_unique<std::thread>(
                [weak_self = weak_from_this(), actionId, timeout, startTime, logger]() {
                    auto self = weak_self.lock();
                    if (!self) {
                        logger->warn("[ActionExecutor] TIMEOUT ABORT - ActionExecutor expired for action {}.", actionId);
                        return;
                    }

                    while (true) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));

                        bool shouldTimeout = false;
                        {
                            std::lock_guard<std::mutex> lock(self->actionsMutex_);
                            auto it = self->runningActions_.find(actionId);
                            if (it == self->runningActions_.end() || it->second.shouldStopMonitoring) {
                                return; // 액션이 완료되었거나 모니터링 중지 요청
                            }

                            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - startTime);

                            if (elapsed > timeout && !it->second.cancelRequested) {
                                shouldTimeout = true;
                                it->second.cancelRequested = true;
                            }
                        }

                        if (shouldTimeout) {
                            logger->warn("[ActionExecutor] TIMEOUT - Action {} exceeded timeout of {}ms, cancelling",
                                        actionId, timeout.count());

                            // 이벤트 발행: ACTION_TIMEOUT
                            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - startTime);
                            self->publishEvent(std::make_shared<mxrc::core::event::ActionTimeoutEvent>(
                                actionId, "", timeout.count(), elapsed.count()));

                            self->cancel(actionId);
                            return;
                        }
                    }
                }
            );
        }
    }

    return actionId;
}

void ActionExecutor::cancel(const std::string& actionId) {
    auto logger = Logger::get();
    std::shared_ptr<IAction> actionToCancel;

    // 뮤텍스 안전성: 락 보유 중 액션 포인터 추출
    {
        std::lock_guard<std::mutex> lock(actionsMutex_);
        auto it = runningActions_.find(actionId);
        if (it != runningActions_.end()) {
            it->second.cancelRequested = true;
            actionToCancel = it->second.action;
            logger->info("[ActionExecutor] CANCEL - Requesting cancellation for action: {} (type: {})",
                        actionId, actionToCancel->getType());
        } else {
            logger->warn("[ActionExecutor] CANCEL - Action {} not found in running actions", actionId);
            return;
        }
    }

    // 데드락 방지: 중첩 락을 피하기 위해 뮤텍스 외부에서 cancel 호출
    if (actionToCancel) {
        actionToCancel->cancel();
        logger->info("[ActionExecutor] Action {} cancel request processed", actionId);

        // 이벤트 발행: ACTION_CANCELLED
        publishEvent(std::make_shared<mxrc::core::event::ActionCancelledEvent>(
            actionId, actionToCancel->getType(), 0));
    }
}

bool ActionExecutor::isRunning(const std::string& actionId) const {
    std::lock_guard<std::mutex> lock(actionsMutex_);
    auto it = runningActions_.find(actionId);
    if (it == runningActions_.end()) {
        return false;
    }

    // future의 상태 확인
    return it->second.future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready;
}

ExecutionResult ActionExecutor::getResult(const std::string& actionId) {
    std::lock_guard<std::mutex> lock(actionsMutex_);
    auto it = runningActions_.find(actionId);
    if (it == runningActions_.end()) {
        ExecutionResult result(actionId, ActionStatus::FAILED);
        result.errorMessage = "Action not found";
        return result;
    }

    // future가 완료되었는지 확인
    if (it->second.future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        // 완료됨 - 결과 수집
        auto& state = it->second;

        // 예외가 발생한 경우 FAILED 상태 반환
        if (state.exceptionOccurred) {
            ExecutionResult result(actionId, ActionStatus::FAILED);
            result.errorMessage = state.exceptionMessage;
            result.progress = state.action->getProgress();

            auto endTime = std::chrono::steady_clock::now();
            result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                endTime - state.startTime);

            return result;
        }

        ExecutionResult result(actionId, state.action->getStatus());
        result.progress = state.action->getProgress();

        auto endTime = std::chrono::steady_clock::now();
        result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - state.startTime);

        // 타임아웃으로 인한 취소인 경우 상태를 TIMEOUT으로 변경
        if (state.cancelRequested && result.status == ActionStatus::CANCELLED) {
            result.status = ActionStatus::TIMEOUT;
            result.errorMessage = "Action exceeded timeout";
        }

        return result;
    } else {
        // 아직 실행 중
        ExecutionResult result(actionId, ActionStatus::RUNNING);
        result.progress = it->second.action->getProgress();
        return result;
    }
}

void ActionExecutor::waitForCompletion(const std::string& actionId) {
    auto logger = Logger::get();
    std::future<void>* futurePtr = nullptr;

    // 뮤텍스 안전성: 락 보유 중 future 포인터 획득
    // runningActions_ 항목이 명시적으로 제거될 때까지 존재하므로 future는 유효함
    {
        std::lock_guard<std::mutex> lock(actionsMutex_);
        auto it = runningActions_.find(actionId);
        if (it == runningActions_.end()) {
            logger->warn("[ActionExecutor] WAIT - Action {} not found", actionId);
            return;
        }
        // future는 move-only이므로 포인터로 접근
        futurePtr = &(it->second.future);
    }

    // 데드락 방지: 다른 작업을 허용하기 위해 뮤텍스 외부에서 대기 수행
    if (futurePtr) {
        logger->debug("[ActionExecutor] WAIT - Waiting for action {} to complete", actionId);
        futurePtr->wait();
        logger->info("[ActionExecutor] WAIT - Action {} completed", actionId);
    }
}

void ActionExecutor::cancel(std::shared_ptr<IAction> action) {
    auto logger = Logger::get();
    logger->info("[ActionExecutor] CANCEL - Requesting cancellation for action: {} (type: {})",
                action->getId(), action->getType());
    logger->debug("[ActionExecutor] Action {} status before cancel: {}",
                 action->getId(), static_cast<int>(action->getStatus()));

    action->cancel();

    logger->info("[ActionExecutor] Action {} cancel request processed, new status: {}",
                action->getId(), static_cast<int>(action->getStatus()));
}

bool ActionExecutor::checkTimeout(
    std::shared_ptr<IAction> action,
    std::chrono::steady_clock::time_point startTime,
    std::chrono::milliseconds timeout) {

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);

    return elapsed > timeout;
}

int ActionExecutor::clearCompletedActions() {
    auto logger = Logger::get();

    // Phase 1: 정리할 액션 ID와 타임아웃 스레드 수집 (뮤텍스 안)
    std::vector<std::string> idsToRemove;
    std::vector<std::unique_ptr<std::thread>> threadsToJoin;

    {
        std::lock_guard<std::mutex> lock(actionsMutex_);

        for (auto it = runningActions_.begin(); it != runningActions_.end(); ++it) {
            // future가 완료되었는지 확인
            if (it->second.future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                auto status = it->second.action->getStatus();
                if (status == ActionStatus::COMPLETED ||
                    status == ActionStatus::FAILED ||
                    status == ActionStatus::CANCELLED ||
                    status == ActionStatus::TIMEOUT) {

                    idsToRemove.push_back(it->first);

                    // 타임아웃 스레드가 있으면 중지 요청하고 수집
                    if (it->second.timeoutThread) {
                        it->second.shouldStopMonitoring = true;
                        if (it->second.timeoutThread->joinable()) {
                            threadsToJoin.push_back(std::move(it->second.timeoutThread));
                        }
                    }
                }
            }
        }
    }

    // Phase 2: 타임아웃 스레드 종료 대기 (뮤텍스 밖 - 데드락 방지)
    for (auto& thread : threadsToJoin) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }

    // Phase 3: 맵에서 제거 (뮤텍스 안)
    int count = 0;
    {
        std::lock_guard<std::mutex> lock(actionsMutex_);

        for (const auto& id : idsToRemove) {
            auto it = runningActions_.find(id);
            if (it != runningActions_.end()) {
                logger->debug("[ActionExecutor] Clearing completed action: {} (status: {})",
                             id, static_cast<int>(it->second.action->getStatus()));
                runningActions_.erase(it);
                count++;
            }
        }
    }

    if (count > 0) {
        logger->info("[ActionExecutor] Cleared {} completed actions", count);
    }
    return count;
}

} // namespace mxrc::core::action
