#pragma once

#include <cstddef>

namespace mxrc::core::event {

/**
 * @brief Backpressure policy for event queue management
 *
 * Feature 019 - US3: EventBus Priority & Policies
 *
 * When the event queue approaches capacity, the backpressure policy determines
 * which events should be dropped to prevent queue overflow and maintain system
 * responsiveness.
 *
 * Design Goals:
 * - Protect CRITICAL events from being dropped
 * - Prevent queue overflow and memory exhaustion
 * - Maintain fair processing for high-priority events
 * - Provide clear feedback when events are dropped
 */
enum class BackpressurePolicy {
    /**
     * @brief Drop oldest events when queue is full
     *
     * This policy drops the oldest events in the queue based on priority:
     * - At 80% capacity: Drop oldest LOW priority events
     * - At 90% capacity: Drop oldest NORMAL and LOW events
     * - At 100% capacity: Drop oldest HIGH, NORMAL, and LOW events
     *
     * CRITICAL events are never dropped.
     *
     * Use case: Real-time systems where fresher data is more valuable
     */
    DROP_OLDEST,

    /**
     * @brief Drop newest events when queue is full
     *
     * This policy rejects new incoming events based on priority:
     * - At 80% capacity: Reject new LOW priority events
     * - At 90% capacity: Reject new NORMAL and LOW events
     * - At 100% capacity: Reject new HIGH, NORMAL, and LOW events
     *
     * CRITICAL events are never dropped.
     *
     * Use case: Guaranteed processing order, event history matters
     * This is the DEFAULT policy implemented in PriorityQueue.
     */
    DROP_NEWEST,

    /**
     * @brief Block producer until space is available
     *
     * This policy blocks the push() operation until:
     * - Queue has available space, OR
     * - Timeout expires
     *
     * Use case: Systems that cannot tolerate event loss
     * Note: NOT RECOMMENDED for RT processes (blocking violates RT constraints)
     */
    BLOCK
};

/**
 * @brief Backpressure thresholds configuration
 *
 * Defines at what queue fill levels backpressure policies are applied.
 */
struct BackpressureThresholds {
    /**
     * @brief Threshold for dropping LOW priority events (default: 80%)
     *
     * When queue size exceeds this percentage, LOW priority events
     * are subject to the backpressure policy.
     */
    double low_priority_threshold = 0.80;

    /**
     * @brief Threshold for dropping NORMAL priority events (default: 90%)
     *
     * When queue size exceeds this percentage, NORMAL and LOW priority
     * events are subject to the backpressure policy.
     */
    double normal_priority_threshold = 0.90;

    /**
     * @brief Threshold for dropping HIGH priority events (default: 100%)
     *
     * When queue size reaches capacity, HIGH, NORMAL, and LOW priority
     * events are subject to the backpressure policy.
     *
     * CRITICAL events are NEVER dropped regardless of queue size.
     */
    double high_priority_threshold = 1.00;

    /**
     * @brief Validate thresholds
     *
     * Ensures:
     * - All thresholds are in range [0.0, 1.0]
     * - low < normal < high
     *
     * @return true if valid, false otherwise
     */
    bool isValid() const {
        return (low_priority_threshold >= 0.0 && low_priority_threshold <= 1.0) &&
               (normal_priority_threshold >= 0.0 && normal_priority_threshold <= 1.0) &&
               (high_priority_threshold >= 0.0 && high_priority_threshold <= 1.0) &&
               (low_priority_threshold < normal_priority_threshold) &&
               (normal_priority_threshold <= high_priority_threshold);
    }
};

/**
 * @brief Get default backpressure thresholds
 *
 * Returns the recommended default values:
 * - LOW: 80% (aggressive dropping)
 * - NORMAL: 90% (moderate protection)
 * - HIGH: 100% (maximum protection, only drop at full capacity)
 *
 * @return Default threshold configuration
 */
inline BackpressureThresholds getDefaultThresholds() {
    return BackpressureThresholds{
        .low_priority_threshold = 0.80,
        .normal_priority_threshold = 0.90,
        .high_priority_threshold = 1.00
    };
}

/**
 * @brief Convert BackpressurePolicy to string for logging
 *
 * @param policy Policy enum value
 * @return String representation
 */
inline const char* toString(BackpressurePolicy policy) {
    switch (policy) {
        case BackpressurePolicy::DROP_OLDEST: return "DROP_OLDEST";
        case BackpressurePolicy::DROP_NEWEST: return "DROP_NEWEST";
        case BackpressurePolicy::BLOCK:       return "BLOCK";
        default:                               return "UNKNOWN";
    }
}

} // namespace mxrc::core::event
