#include "core/sequence/core/RetryHandler.h"

namespace mxrc::core::sequence {

using namespace mxrc::core::action;

void RetryHandler::waitBeforeRetry(const RetryPolicy& policy, int currentRetry) const {
    auto delay = policy.calculateDelay(currentRetry);
    
    if (delay.count() > 0) {
        Logger::get()->info("Waiting {}ms before retry {}", delay.count(), currentRetry + 1);
        std::this_thread::sleep_for(delay);
    }
}

bool RetryHandler::canRetry(const RetryPolicy& policy, int currentRetry) const {
    return currentRetry < policy.maxRetries;
}

} // namespace mxrc::core::sequence
∫