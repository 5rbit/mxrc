#include "DelayAction.h"
#include "core/action/util/Logger.h"
#include <thread>

namespace mxrc::core::action {

DelayAction::DelayAction(const std::string& id, long delayMs)
    : id_(id), delay_(delayMs) {}

std::string DelayAction::getId() const {
    return id_;
}

std::string DelayAction::getType() const {
    return "Delay";
}

void DelayAction::execute(ExecutionContext& context) {
    auto logger = Logger::get();
    logger->debug("DelayAction {} starting delay of {}ms", id_, delay_.count());

    status_ = ActionStatus::RUNNING;
    progress_ = 0.0f;
    cancelled_ = false;

    // 짧은 간격으로 나누어 대기 (취소 확인 및 진행률 업데이트를 위해)
    const int steps = 10;
    auto stepDelay = delay_ / steps;

    for (int i = 0; i < steps && !cancelled_; ++i) {
        std::this_thread::sleep_for(stepDelay);
        progress_ = static_cast<float>(i + 1) / steps;
    }

    if (cancelled_) {
        status_ = ActionStatus::CANCELLED;
        logger->info("DelayAction {} was cancelled", id_);
    } else {
        status_ = ActionStatus::COMPLETED;
        progress_ = 1.0f;
        logger->debug("DelayAction {} completed", id_);
    }

    // 결과를 컨텍스트에 저장
    context.setActionResult(id_, delay_.count());
}

void DelayAction::cancel() {
    cancelled_ = true;
}

ActionStatus DelayAction::getStatus() const {
    return status_;
}

float DelayAction::getProgress() const {
    return progress_;
}

} // namespace mxrc::core::action
