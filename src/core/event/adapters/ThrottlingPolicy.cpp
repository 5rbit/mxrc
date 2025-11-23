#include "ThrottlingPolicy.h"

namespace mxrc::core::event {

ThrottlingPolicy::ThrottlingPolicy(uint64_t throttle_interval_ms)
    : throttle_interval_ms_(throttle_interval_ms)
{
}

bool ThrottlingPolicy::shouldSend(const std::string& event_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t now = getCurrentTimeMs();

    // Check if this event type has been sent before
    auto it = last_sent_time_.find(event_type);
    if (it == last_sent_time_.end()) {
        // First time: always send
        last_sent_time_[event_type] = now;
        return true;
    }

    // Check if enough time has passed since last send
    uint64_t time_since_last = now - it->second;
    if (time_since_last >= throttle_interval_ms_) {
        // Enough time has passed: send and update timestamp
        it->second = now;
        return true;
    }

    // Throttled: too soon since last send
    return false;
}

void ThrottlingPolicy::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_sent_time_.clear();
}

void ThrottlingPolicy::resetEventType(const std::string& event_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    last_sent_time_.erase(event_type);
}

uint64_t ThrottlingPolicy::getCurrentTimeMs() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace mxrc::core::event
