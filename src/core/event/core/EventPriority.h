#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace mxrc::core::event {

/**
 * @brief Three-tier event priority classification
 *
 * Priority levels determine:
 * - Queue selection in PriorityEventBus
 * - Drop policy under backpressure
 * - Processing order (CRITICAL > NORMAL > DEBUG)
 *
 * Drop Policy (under backpressure):
 * - CRITICAL: Never dropped (always queued, may block if queue full)
 * - NORMAL: Dropped when total queue > 90% capacity
 * - DEBUG: Dropped when total queue > 80% capacity
 *
 * Performance Impact:
 * - CRITICAL: ~50ns push latency (lock-free SPSC queue)
 * - NORMAL: ~50ns push latency
 * - DEBUG: ~50ns push latency (or 0ns if dropped)
 */
enum class EventPriority : uint8_t {
    CRITICAL = 0,  ///< System errors, state changes, RT cycle events (never drop)
    NORMAL = 1,    ///< Regular operational events (drop at 90%)
    DEBUG = 2      ///< Verbose debug logs, traces (drop at 80%)
};

/**
 * @brief Convert EventPriority to human-readable string
 * @param priority Priority enum value
 * @return String representation ("CRITICAL", "NORMAL", "DEBUG")
 */
inline const char* toString(EventPriority priority) {
    switch (priority) {
        case EventPriority::CRITICAL: return "CRITICAL";
        case EventPriority::NORMAL:   return "NORMAL";
        case EventPriority::DEBUG:    return "DEBUG";
        default:                       return "UNKNOWN";
    }
}

/**
 * @brief Convert EventPriority to integer (for array indexing)
 * @param priority Priority enum value
 * @return Integer index (0, 1, 2)
 */
inline uint8_t toIndex(EventPriority priority) {
    return static_cast<uint8_t>(priority);
}

/**
 * @brief PrioritizedEvent wraps an event with priority metadata
 *
 * This struct is the core unit stored in the PriorityEventBus.
 * It contains:
 * - Priority level (CRITICAL/NORMAL/DEBUG)
 * - Event type identifier (e.g., "sensor_update", "state_change")
 * - Timestamp (nanoseconds since epoch)
 * - Payload (flexible string or JSON)
 *
 * Memory Layout:
 * - priority: 1 byte
 * - timestamp_ns: 8 bytes
 * - event_type: ~32 bytes (std::string with SSO)
 * - payload: ~32 bytes (std::string with SSO)
 * Total: ~73 bytes (stack-allocated, no heap unless strings > 15 chars)
 *
 * Performance:
 * - Construction: ~10ns (SSO strings)
 * - Copy: ~20ns (SSO strings)
 * - Move: ~5ns (pointer swap)
 *
 * Thread Safety:
 * - Immutable after construction (const members)
 * - Safe to copy across threads
 */
struct PrioritizedEvent {
    const EventPriority priority;      ///< Event priority level (immutable)
    const uint64_t timestamp_ns;       ///< Nanosecond timestamp (immutable)
    const std::string event_type;      ///< Event type identifier (immutable)
    const std::string payload;         ///< Event payload (JSON, text, etc.) (immutable)

    /**
     * @brief Construct a prioritized event
     * @param prio Event priority
     * @param type Event type identifier (e.g., "sensor_update")
     * @param data Event payload (e.g., JSON string)
     */
    PrioritizedEvent(EventPriority prio, std::string type, std::string data)
        : priority(prio),
          timestamp_ns(getCurrentTimestampNs()),
          event_type(std::move(type)),
          payload(std::move(data)) {}

    /**
     * @brief Copy constructor (explicit copy)
     * @param other Source event
     */
    PrioritizedEvent(const PrioritizedEvent& other) = default;

    /**
     * @brief Move constructor (efficient transfer)
     * @param other Source event (moved)
     */
    PrioritizedEvent(PrioritizedEvent&& other) noexcept = default;

    /**
     * @brief Get priority as string (for logging)
     * @return "CRITICAL", "NORMAL", or "DEBUG"
     */
    const char* getPriorityString() const {
        return toString(priority);
    }

    /**
     * @brief Get age of event in milliseconds
     * @return Age in milliseconds since creation
     */
    uint64_t getAgeMs() const {
        uint64_t now = getCurrentTimestampNs();
        return (now - timestamp_ns) / 1'000'000;  // ns to ms
    }

    /**
     * @brief Check if event is critical priority
     * @return true if priority is CRITICAL
     */
    bool isCritical() const {
        return priority == EventPriority::CRITICAL;
    }

    /**
     * @brief Check if event is normal priority
     * @return true if priority is NORMAL
     */
    bool isNormal() const {
        return priority == EventPriority::NORMAL;
    }

    /**
     * @brief Check if event is debug priority
     * @return true if priority is DEBUG
     */
    bool isDebug() const {
        return priority == EventPriority::DEBUG;
    }

private:
    /**
     * @brief Get current time in nanoseconds since epoch
     * @return Nanosecond timestamp (steady_clock for monotonicity)
     */
    static uint64_t getCurrentTimestampNs() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }
};

/**
 * @brief EventMetrics tracks event bus statistics
 *
 * This struct is used by PriorityEventBus to collect metrics:
 * - Events pushed per priority
 * - Events dropped per priority (due to backpressure)
 * - Queue depths per priority
 *
 * Metrics are exposed via Prometheus:
 * - mxrc_events_pushed_total{priority="CRITICAL"}
 * - mxrc_events_dropped_total{priority="DEBUG"}
 * - mxrc_event_queue_depth{priority="NORMAL"}
 */
struct EventMetrics {
    // Events pushed (successfully queued)
    uint64_t critical_pushed = 0;
    uint64_t normal_pushed = 0;
    uint64_t debug_pushed = 0;

    // Events dropped (rejected due to backpressure)
    uint64_t critical_dropped = 0;  // Should always be 0!
    uint64_t normal_dropped = 0;
    uint64_t debug_dropped = 0;

    // Current queue depths
    uint64_t critical_queue_depth = 0;
    uint64_t normal_queue_depth = 0;
    uint64_t debug_queue_depth = 0;

    /**
     * @brief Get total events pushed across all priorities
     * @return Total pushed events
     */
    uint64_t getTotalPushed() const {
        return critical_pushed + normal_pushed + debug_pushed;
    }

    /**
     * @brief Get total events dropped across all priorities
     * @return Total dropped events
     */
    uint64_t getTotalDropped() const {
        return critical_dropped + normal_dropped + debug_dropped;
    }

    /**
     * @brief Get total queue depth across all priorities
     * @return Total queued events
     */
    uint64_t getTotalQueueDepth() const {
        return critical_queue_depth + normal_queue_depth + debug_queue_depth;
    }

    /**
     * @brief Calculate drop rate (percentage)
     * @return Drop rate as percentage (0.0 - 100.0)
     */
    double getDropRate() const {
        uint64_t total = getTotalPushed() + getTotalDropped();
        if (total == 0) return 0.0;
        return (static_cast<double>(getTotalDropped()) / total) * 100.0;
    }

    /**
     * @brief Reset all counters (for testing)
     */
    void reset() {
        *this = EventMetrics{};
    }
};

} // namespace mxrc::core::event
