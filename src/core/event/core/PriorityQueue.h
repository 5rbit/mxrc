#pragma once

#include "PrioritizedEvent.h"
#include <queue>
#include <mutex>
#include <atomic>
#include <optional>

namespace mxrc::core::event {

/**
 * @brief Priority queue metrics for monitoring
 */
struct PriorityQueueMetrics {
    std::atomic<uint64_t> critical_events_pushed{0};
    std::atomic<uint64_t> high_events_pushed{0};
    std::atomic<uint64_t> normal_events_pushed{0};
    std::atomic<uint64_t> low_events_pushed{0};

    std::atomic<uint64_t> critical_events_dropped{0};
    std::atomic<uint64_t> high_events_dropped{0};
    std::atomic<uint64_t> normal_events_dropped{0};
    std::atomic<uint64_t> low_events_dropped{0};

    std::atomic<uint64_t> events_popped{0};
    std::atomic<size_t> current_size{0};
    std::atomic<size_t> peak_size{0};
};

/**
 * @brief Thread-safe priority queue for PrioritizedEvent
 *
 * Design Goals (Feature 022 P3):
 * - 4-level priority: CRITICAL (never dropped) > HIGH > NORMAL > LOW
 * - Backpressure: Drop lower-priority events when queue is full
 * - Thread-safe: Multiple producers, single consumer (MPSC)
 * - Lock-based: Uses std::mutex for simplicity (non-RT path)
 * - Bounded capacity: 4096 events max (configurable)
 *
 * Drop Policy:
 * - Queue < 80% (3276): Accept all events
 * - Queue 80-90% (3276-3686): Drop LOW priority events
 * - Queue 90-100% (3686-4096): Drop LOW and NORMAL events
 * - Queue 100%: Drop LOW, NORMAL, and HIGH events (CRITICAL never dropped)
 *
 * Usage Example:
 * @code
 * PriorityQueue queue(4096);
 *
 * // Producer thread
 * auto event = makePrioritizedEvent("sensor.temp", EventPriority::HIGH, 25.5);
 * if (!queue.push(std::move(event))) {
 *     // Event was dropped due to backpressure
 * }
 *
 * // Consumer thread
 * if (auto event = queue.pop()) {
 *     processEvent(*event);
 * }
 * @endcode
 */
class PriorityQueue {
public:
    /**
     * @brief Construct priority queue with capacity
     *
     * @param capacity Maximum number of events in queue (default: 4096)
     */
    explicit PriorityQueue(size_t capacity = 4096);

    /**
     * @brief Destructor
     */
    ~PriorityQueue() = default;

    /**
     * @brief Push event to queue (non-blocking)
     *
     * Applies backpressure policy:
     * - CRITICAL: Always accepted (may exceed capacity briefly)
     * - HIGH: Dropped if queue > 90%
     * - NORMAL: Dropped if queue > 90%
     * - LOW: Dropped if queue > 80%
     *
     * @param event Event to push (moved)
     * @return true if accepted, false if dropped
     */
    bool push(PrioritizedEvent&& event);

    /**
     * @brief Pop highest-priority event from queue
     *
     * Returns events in priority order:
     * 1. All CRITICAL events (FIFO within priority)
     * 2. All HIGH events
     * 3. All NORMAL events
     * 4. All LOW events
     *
     * @return Event if available, std::nullopt if queue is empty
     */
    std::optional<PrioritizedEvent> pop();

    /**
     * @brief Get current queue size
     *
     * @return Number of events in queue
     */
    size_t size() const {
        return size_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Check if queue is empty
     *
     * @return true if queue has no events
     */
    bool empty() const {
        return size() == 0;
    }

    /**
     * @brief Get queue capacity
     *
     * @return Maximum number of events
     */
    size_t capacity() const {
        return capacity_;
    }

    /**
     * @brief Get queue metrics
     *
     * @return Metrics structure with event counts
     */
    const PriorityQueueMetrics& metrics() const {
        return metrics_;
    }

    /**
     * @brief Reset metrics (for testing)
     */
    void resetMetrics() {
        metrics_.critical_events_pushed = 0;
        metrics_.high_events_pushed = 0;
        metrics_.normal_events_pushed = 0;
        metrics_.low_events_pushed = 0;
        metrics_.critical_events_dropped = 0;
        metrics_.high_events_dropped = 0;
        metrics_.normal_events_dropped = 0;
        metrics_.low_events_dropped = 0;
        metrics_.events_popped = 0;
        metrics_.current_size = 0;
        metrics_.peak_size = 0;
    }

private:
    /**
     * @brief Check if event should be dropped based on backpressure policy
     *
     * @param priority Event priority
     * @return true if event should be dropped
     */
    bool shouldDrop(EventPriority priority);

    /**
     * @brief Update metrics after push
     *
     * @param priority Event priority
     * @param dropped Whether event was dropped
     */
    void updatePushMetrics(EventPriority priority, bool dropped);

    // Configuration
    const size_t capacity_;                     ///< Maximum queue capacity
    const size_t drop_threshold_80_;            ///< 80% threshold (drop LOW)
    const size_t drop_threshold_90_;            ///< 90% threshold (drop NORMAL)

    // Thread-safe priority queue
    std::priority_queue<PrioritizedEvent> queue_;  ///< Internal priority queue
    mutable std::mutex mutex_;                     ///< Protects queue access
    std::atomic<size_t> size_{0};                  ///< Current queue size

    // Metrics
    PriorityQueueMetrics metrics_;
};

} // namespace mxrc::core::event
