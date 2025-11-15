#include "ActionExecutor.h"
#include "core/action/util/Logger.h"
#include <thread>

namespace mxrc::core::action {

ExecutionResult ActionExecutor::execute(
    std::shared_ptr<IAction> action,
    ExecutionContext& context,
    std::chrono::milliseconds timeout) {

    auto logger = Logger::get();
    auto startTime = std::chrono::steady_clock::now();

    ExecutionResult result(action->getId(), ActionStatus::PENDING);

    // 실행 시작 로그 (상세)
    logger->info("[ActionExecutor] START - Action: {} (type: {}, timeout: {}ms)",
                 action->getId(), action->getType(), timeout.count());
    logger->debug("[ActionExecutor] Action {} initial status: {}",
                  action->getId(), static_cast<int>(action->getStatus()));

    try {
        // 타임아웃이 설정된 경우 비동기 실행
        if (timeout.count() > 0) {
            // 비동기로 액션 실행
            auto future = std::async(std::launch::async, [&action, &context]() {
                action->execute(context);
            });

            // 타임아웃 동안 대기하며 주기적으로 체크
            auto waitResult = future.wait_for(timeout);

            if (waitResult == std::future_status::timeout) {
                // 타임아웃 발생 - 액션 취소 시도
                logger->warn("[ActionExecutor] TIMEOUT - Action {} exceeded timeout of {}ms, cancelling",
                            action->getId(), timeout.count());
                action->cancel();

                // 취소 후 액션 스레드가 완전히 종료될 때까지 대기
                future.wait();

                result.status = ActionStatus::TIMEOUT;
                result.errorMessage = "Action exceeded timeout";
            } else {
                // 정상 완료
                future.get();  // 예외 발생 시 여기서 던져짐
                result.status = action->getStatus();
                result.progress = action->getProgress();

                if (result.status == ActionStatus::COMPLETED) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - startTime);
                    logger->info("[ActionExecutor] SUCCESS - Action {} completed in {}ms (progress: {:.1f}%)",
                                action->getId(), elapsed.count(), result.progress * 100.0f);
                } else {
                    logger->warn("[ActionExecutor] Action {} finished with status: {}",
                                action->getId(), static_cast<int>(result.status));
                }
            }
        } else {
            // 타임아웃이 없는 경우 동기 실행
            action->execute(context);

            result.status = action->getStatus();
            result.progress = action->getProgress();

            if (result.status == ActionStatus::COMPLETED) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime);
                logger->info("[ActionExecutor] SUCCESS - Action {} completed in {}ms (progress: {:.1f}%)",
                            action->getId(), elapsed.count(), result.progress * 100.0f);
            } else {
                logger->warn("[ActionExecutor] Action {} finished with status: {}",
                            action->getId(), static_cast<int>(result.status));
            }
        }

    } catch (const std::exception& e) {
        result.status = ActionStatus::FAILED;
        result.errorMessage = e.what();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);
        logger->error("[ActionExecutor] FAILED - Action {} failed after {}ms: {}",
                     action->getId(), elapsed.count(), e.what());
        logger->debug("[ActionExecutor] Error context - Action type: {}, Status before error: {}",
                     action->getType(), static_cast<int>(action->getStatus()));
    }

    // 실행 시간 계산
    auto endTime = std::chrono::steady_clock::now();
    result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    logger->debug("[ActionExecutor] END - Action {} total execution time: {}ms",
                 action->getId(), result.executionTime.count());

    return result;
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
