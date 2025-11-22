#include "CoalescingPolicy.h"

namespace mxrc::core::event {

CoalescingPolicy::CoalescingPolicy(uint64_t coalesce_window_ms)
    : coalesce_window_ms_(coalesce_window_ms)
{
}

std::optional<PrioritizedEvent> CoalescingPolicy::coalesce(PrioritizedEvent&& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t now = getCurrentTimeMs();
    std::optional<PrioritizedEvent> result;

    // Save event type before potential move
    std::string event_type = event.type;

    // Check if there's a pending event of the same type
    auto it = pending_events_.find(event_type);
    if (it != pending_events_.end()) {
        // Check if window has expired
        uint64_t time_since_pending = now - it->second.timestamp_ms;

        if (time_since_pending >= coalesce_window_ms_) {
            // Window expired: flush previous event and store new one
            result = std::move(it->second.event);
            it->second.event = std::move(event);
            it->second.timestamp_ms = now;
        } else {
            // Within window: merge by replacing with newer event
            it->second.event = std::move(event);
            // Keep original timestamp to maintain window
        }
    } else {
        // First event of this type: store it
        pending_events_[event_type] = PendingEvent{std::move(event), now};
    }

    return result;
}

std::vector<PrioritizedEvent> CoalescingPolicy::flush() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<PrioritizedEvent> result;
    result.reserve(pending_events_.size());

    for (auto& [event_type, pending] : pending_events_) {
        result.push_back(std::move(pending.event));
    }

    pending_events_.clear();
    return result;
}

std::optional<PrioritizedEvent> CoalescingPolicy::flushEventType(const std::string& event_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = pending_events_.find(event_type);
    if (it != pending_events_.end()) {
        auto result = std::move(it->second.event);
        pending_events_.erase(it);
        return result;
    }

    return std::nullopt;
}

size_t CoalescingPolicy::getPendingCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_events_.size();
}

uint64_t CoalescingPolicy::getCurrentTimeMs() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace mxrc::core::event
