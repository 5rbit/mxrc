#pragma once

#include <string>
#include <variant>
#include <cstdint>
#include <chrono>
#include <memory>
#include <optional>

namespace mxrc::core::event {

// Forward declaration for IEvent
class IEvent;

/**
 * @brief Event priority levels for the priority queue
 *
 * Events are processed in the following order:
 * - CRITICAL: System-critical events (safety, errors, shutdown)
 * - HIGH: Important events (state changes, warnings)
 * - NORMAL: Regular events (telemetry, status updates)
 * - LOW: Low-priority events (debug, verbose logging)
 *
 * Priority levels determine:
 * 1. Processing order (CRITICAL first, LOW last)
 * 2. Drop policy when queue is full (LOW dropped first)
 * 3. Backpressure behavior (CRITICAL never blocked)
 */
enum class EventPriority : uint8_t {
    CRITICAL = 0,  ///< Highest priority (never dropped)
    HIGH     = 1,  ///< High priority
    NORMAL   = 2,  ///< Normal priority
    LOW      = 3   ///< Lowest priority (dropped first)
};

/**
 * @brief Convert EventPriority to string for logging
 */
inline const char* priorityToString(EventPriority priority) {
    switch (priority) {
        case EventPriority::CRITICAL: return "CRITICAL";
        case EventPriority::HIGH:     return "HIGH";
        case EventPriority::NORMAL:   return "NORMAL";
        case EventPriority::LOW:      return "LOW";
        default:                      return "UNKNOWN";
    }
}

/**
 * @brief Prioritized event structure for EventBus priority queue
 *
 * Design Goals (Feature 022 P3):
 * - Compact size: < 128 bytes for cache efficiency
 * - Move semantics: Efficient transfer in lock-free queues
 * - POD-like: Trivially copyable where possible
 * - Type-safe payload: std::variant for flexible data
 *
 * Memory Layout (approximate):
 * - type: 64 bytes (std::string with small string optimization)
 * - priority: 1 byte (enum)
 * - payload: 32 bytes (std::variant<int, double, std::string>)
 * - timestamp_ns: 8 bytes
 * - sequence_num: 8 bytes
 * Total: ~113 bytes (within 128 byte target)
 *
 * Usage Example:
 * @code
 * PrioritizedEvent event{
 *     .type = "temperature.warning",
 *     .priority = EventPriority::HIGH,
 *     .payload = 85.5,  // Temperature value
 *     .timestamp_ns = getCurrentTimeNs(),
 *     .sequence_num = generateSequenceNum()
 * };
 * @endcode
 */
struct PrioritizedEvent {
    /**
     * @brief Event type identifier (max 63 chars + null terminator)
     *
     * Convention: Use dot-separated hierarchical names
     * Examples: "system.startup", "sensor.temperature", "error.timeout"
     */
    std::string type;

    /**
     * @brief Event priority level
     *
     * Determines processing order and drop policy.
     */
    EventPriority priority{EventPriority::NORMAL};

    /**
     * @brief Event payload (flexible data type)
     *
     * Supports:
     * - int: Integer values (error codes, counts, etc.)
     * - double: Floating-point values (sensor readings, metrics)
     * - std::string: String values (messages, identifiers)
     * - std::shared_ptr<IEvent>: Legacy IEvent objects for EventBus integration
     *
     * Use std::get<T>(payload) to access the value.
     */
    std::variant<int, double, std::string, std::shared_ptr<IEvent>> payload;

    /**
     * @brief Event timestamp in nanoseconds (since epoch)
     *
     * Used for:
     * - Event ordering within same priority
     * - Coalescing window calculations
     * - Throttling interval checks
     */
    uint64_t timestamp_ns{0};

    /**
     * @brief Sequence number for FIFO ordering within same priority
     *
     * When two events have the same priority and timestamp,
     * the one with the lower sequence number is processed first.
     */
    uint64_t sequence_num{0};

    /**
     * @brief Time-To-Live in milliseconds (optional)
     *
     * If set, the event will be discarded if (current_time - timestamp_ns) > ttl.
     * Used for time-sensitive events that become irrelevant after a certain period.
     * If nullopt, the event never expires.
     *
     * Feature 019 - US3: TTL expiration policy (FR-009)
     */
    std::optional<std::chrono::milliseconds> ttl;

    /**
     * @brief Coalescing key for event merging (optional)
     *
     * Events with the same coalescing_key can be merged, keeping only the latest.
     * Used to avoid queue buildup of redundant status updates.
     * If nullopt, coalescing is disabled for this event.
     *
     * Feature 019 - US3: Coalescing policy (FR-010)
     */
    std::optional<std::string> coalescing_key;

    /**
     * @brief Check if event has expired based on TTL
     *
     * @return true if TTL is set and event has expired
     */
    bool isExpired() const {
        if (!ttl.has_value()) {
            return false;  // No TTL, never expires
        }

        auto now = std::chrono::system_clock::now();
        auto current_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();

        auto age_ms = (current_ns - timestamp_ns) / 1'000'000;
        return age_ms > static_cast<uint64_t>(ttl->count());
    }

    /**
     * @brief Comparison operator for priority queue ordering
     *
     * Events are ordered by:
     * 1. Priority (CRITICAL < HIGH < NORMAL < LOW)
     * 2. Timestamp (older first)
     * 3. Sequence number (lower first)
     *
     * Note: std::priority_queue is a max-heap, so we use > for min-heap behavior
     */
    bool operator<(const PrioritizedEvent& other) const {
        // Higher priority (lower enum value) comes first
        if (priority != other.priority) {
            return priority > other.priority;  // Reversed for max-heap
        }

        // Within same priority, older timestamp comes first
        if (timestamp_ns != other.timestamp_ns) {
            return timestamp_ns > other.timestamp_ns;  // Reversed for max-heap
        }

        // Within same timestamp, lower sequence number comes first
        return sequence_num > other.sequence_num;  // Reversed for max-heap
    }
};

/**
 * @brief Helper function to create a prioritized event with current timestamp
 *
 * @param type Event type string
 * @param priority Event priority level
 * @param payload Event payload (int, double, or std::string)
 * @param sequence_num Sequence number for ordering
 * @return PrioritizedEvent instance
 */
template<typename T>
inline PrioritizedEvent makePrioritizedEvent(
    const std::string& type,
    EventPriority priority,
    T&& payload,
    uint64_t sequence_num = 0)
{
    auto now = std::chrono::system_clock::now();
    auto timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();

    return PrioritizedEvent{
        .type = type,
        .priority = priority,
        .payload = std::forward<T>(payload),
        .timestamp_ns = static_cast<uint64_t>(timestamp_ns),
        .sequence_num = sequence_num
    };
}

} // namespace mxrc::core::event
