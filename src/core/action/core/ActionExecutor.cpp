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

    try {
        logger->info("Executing action: {} (type: {})", action->getId(), action->getType());

        // Action 실행
        action->execute(context);

        // 타임아웃 체크 (실행 완료 후)
        if (timeout.count() > 0 && checkTimeout(action, startTime, timeout)) {
            result.status = ActionStatus::TIMEOUT;
            result.errorMessage = "Action exceeded timeout";
            logger->warn("Action {} timed out", action->getId());
        } else {
            result.status = action->getStatus();
            result.progress = action->getProgress();

            if (result.status == ActionStatus::COMPLETED) {
                logger->info("Action {} completed successfully", action->getId());
            }
        }

    } catch (const std::exception& e) {
        result.status = ActionStatus::FAILED;
        result.errorMessage = e.what();
        logger->error("Action {} failed: {}", action->getId(), e.what());
    }

    // 실행 시간 계산
    auto endTime = std::chrono::steady_clock::now();
    result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    return result;
}

void ActionExecutor::cancel(std::shared_ptr<IAction> action) {
    auto logger = Logger::get();
    logger->info("Cancelling action: {}", action->getId());
    action->cancel();
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
