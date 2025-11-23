#include "PriorityQueue.h"
#include <algorithm>

namespace mxrc::core::event {

PriorityQueue::PriorityQueue(size_t capacity)
    : capacity_(capacity),
      drop_threshold_80_(static_cast<size_t>(capacity * 0.8)),
      drop_threshold_90_(static_cast<size_t>(capacity * 0.9))
{
}

bool PriorityQueue::push(PrioritizedEvent&& event) {
    EventPriority priority = event.priority;

    // Check if event should be dropped based on backpressure policy
    if (shouldDrop(priority)) {
        updatePushMetrics(priority, true);
        return false;  // Event dropped
    }

    // Lock and push event
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(event));
    }

    // Update metrics
    size_t new_size = size_.fetch_add(1, std::memory_order_relaxed) + 1;
    metrics_.current_size.store(new_size, std::memory_order_relaxed);

    // Update peak size
    size_t current_peak = metrics_.peak_size.load(std::memory_order_relaxed);
    while (new_size > current_peak) {
        if (metrics_.peak_size.compare_exchange_weak(
                current_peak, new_size,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            break;
        }
    }

    updatePushMetrics(priority, false);
    return true;  // Event accepted
}

std::optional<PrioritizedEvent> PriorityQueue::pop() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (queue_.empty()) {
        return std::nullopt;
    }

    // Extract top event (highest priority)
    PrioritizedEvent event = std::move(const_cast<PrioritizedEvent&>(queue_.top()));
    queue_.pop();

    // Update metrics
    size_t new_size = size_.fetch_sub(1, std::memory_order_relaxed) - 1;
    metrics_.current_size.store(new_size, std::memory_order_relaxed);
    metrics_.events_popped.fetch_add(1, std::memory_order_relaxed);

    return event;
}

bool PriorityQueue::shouldDrop(EventPriority priority) {
    size_t current_size = size_.load(std::memory_order_relaxed);

    // CRITICAL events are NEVER dropped
    if (priority == EventPriority::CRITICAL) {
        return false;
    }

    // Queue < 80%: Accept all events
    if (current_size < drop_threshold_80_) {
        return false;
    }

    // Queue 80-90%: Drop LOW priority events
    if (current_size < drop_threshold_90_) {
        return priority == EventPriority::LOW;
    }

    // Queue 90-100%: Drop LOW and NORMAL events
    if (current_size < capacity_) {
        return priority == EventPriority::LOW || priority == EventPriority::NORMAL;
    }

    // Queue 100%: Drop LOW, NORMAL, and HIGH events (CRITICAL still accepted)
    return priority == EventPriority::LOW ||
           priority == EventPriority::NORMAL ||
           priority == EventPriority::HIGH;
}

void PriorityQueue::updatePushMetrics(EventPriority priority, bool dropped) {
    if (dropped) {
        switch (priority) {
            case EventPriority::CRITICAL:
                metrics_.critical_events_dropped.fetch_add(1, std::memory_order_relaxed);
                break;
            case EventPriority::HIGH:
                metrics_.high_events_dropped.fetch_add(1, std::memory_order_relaxed);
                break;
            case EventPriority::NORMAL:
                metrics_.normal_events_dropped.fetch_add(1, std::memory_order_relaxed);
                break;
            case EventPriority::LOW:
                metrics_.low_events_dropped.fetch_add(1, std::memory_order_relaxed);
                break;
        }
    } else {
        switch (priority) {
            case EventPriority::CRITICAL:
                metrics_.critical_events_pushed.fetch_add(1, std::memory_order_relaxed);
                break;
            case EventPriority::HIGH:
                metrics_.high_events_pushed.fetch_add(1, std::memory_order_relaxed);
                break;
            case EventPriority::NORMAL:
                metrics_.normal_events_pushed.fetch_add(1, std::memory_order_relaxed);
                break;
            case EventPriority::LOW:
                metrics_.low_events_pushed.fetch_add(1, std::memory_order_relaxed);
                break;
        }
    }
}

} // namespace mxrc::core::event
