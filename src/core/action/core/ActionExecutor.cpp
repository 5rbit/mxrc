#include "ActionExecutor.h"
#include "core/action/util/Logger.h"
#include <thread>

namespace mxrc::core::action {

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

    // 비동기로 액션 실행
    auto future = std::async(std::launch::async, [action, &context, actionId, logger]() {
        try {
            action->execute(context);
            logger->info("[ActionExecutor] ASYNC COMPLETE - Action {} finished successfully", actionId);
        } catch (const std::exception& e) {
            logger->error("[ActionExecutor] ASYNC FAILED - Action {} threw exception: {}", actionId, e.what());
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
        std::thread([this, actionId, timeout, startTime, logger]() {
            while (true) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));

                bool shouldTimeout = false;
                {
                    std::lock_guard<std::mutex> lock(actionsMutex_);
                    auto it = runningActions_.find(actionId);
                    if (it == runningActions_.end()) {
                        // 액션이 이미 완료되어 제거됨
                        return;
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
                    cancel(actionId);
                    return;
                }
            }
        }).detach();
    }

    return actionId;
}

void ActionExecutor::cancel(const std::string& actionId) {
    auto logger = Logger::get();
    std::shared_ptr<IAction> actionToCancel;

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

    // 락을 푼 상태에서 cancel 호출 (데드락 방지)
    if (actionToCancel) {
        actionToCancel->cancel();
        logger->info("[ActionExecutor] Action {} cancel request processed", actionId);
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
        ExecutionResult result(actionId, state.action->getStatus());
        result.progress = state.action->getProgress();

        auto endTime = std::chrono::steady_clock::now();
        result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - state.startTime);

        if (state.cancelRequested && result.status != ActionStatus::CANCELLED) {
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

    // 락을 푼 상태에서 대기
    if (futurePtr) {
        logger->debug("[ActionExecutor] WAIT - Waiting for action {} to complete", actionId);
        futurePtr->wait();
        logger->info("[ActionExecutor] WAIT - Action {} completed", actionId);

        // 완료 후 맵에서 제거 (자동 정리)
        {
            std::lock_guard<std::mutex> lock(actionsMutex_);
            runningActions_.erase(actionId);
            logger->debug("[ActionExecutor] Action {} removed from runningActions_ map", actionId);
        }
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

} // namespace mxrc::core::action
