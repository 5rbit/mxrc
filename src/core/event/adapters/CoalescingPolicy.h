#pragma once

#include "core/event/core/PrioritizedEvent.h"
#include <string>
#include <unordered_map>
#include <optional>
#include <cstdint>
#include <chrono>
#include <mutex>

namespace mxrc::core::event {

/**
 * @brief Coalescing policy for event deduplication
 *
 * Design Goals (Feature 022 P3):
 * - Prevent duplicate events by merging identical events within time window
 * - Configurable coalescing window (default: 100ms)
 * - Thread-safe for concurrent access
 * - Reduces event bus load during event floods
 *
 * Coalescing Logic:
 * - Events of same type within window are merged into one
 * - Latest event data replaces previous data
 * - Event is flushed when window expires
 * - Each event type has independent coalescing window
 *
 * Usage Example:
 * @code
 * CoalescingPolicy policy(100);  // 100ms coalescing window
 *
 * // First event: stored for coalescing
 * auto result = policy.coalesce(PrioritizedEvent{"temp", 25.0, NORMAL});
 * // result is nullopt (still in window)
 *
 * // Same type within 100ms: merged
 * result = policy.coalesce(PrioritizedEvent{"temp", 25.5, NORMAL});
 * // result is nullopt (merged with previous)
 *
 * // After 100ms: previous event flushed
 * std::this_thread::sleep_for(std::chrono::milliseconds(100));
 * result = policy.coalesce(PrioritizedEvent{"temp", 26.0, NORMAL});
 * // result contains previous event with data 25.5
 * @endcode
 */
class CoalescingPolicy {
public:
    /**
     * @brief Construct coalescing policy with window size
     *
     * @param coalesce_window_ms Time window for coalescing same events (default: 100ms)
     */
    explicit CoalescingPolicy(uint64_t coalesce_window_ms = 100);

    /**
     * @brief Destructor
     */
    ~CoalescingPolicy() = default;

    /**
     * @brief Coalesce event with pending events
     *
     * This method checks if there's a pending event of the same type within
     * the coalescing window. If so, it merges them. If the window has expired,
     * it returns the previous event and stores the new one.
     *
     * Thread-safe for concurrent access.
     *
     * @param event Event to coalesce
     * @return Previous event if window expired, nullopt if merged or first event
     */
    std::optional<PrioritizedEvent> coalesce(PrioritizedEvent&& event);

    /**
     * @brief Flush all pending events
     *
     * Returns all pending events regardless of window expiration.
     * Useful for system shutdown or forced flush.
     *
     * @return Vector of pending events
     */
    std::vector<PrioritizedEvent> flush();

    /**
     * @brief Flush specific event type
     *
     * @param event_type Event type to flush
     * @return Pending event if exists, nullopt otherwise
     */
    std::optional<PrioritizedEvent> flushEventType(const std::string& event_type);

    /**
     * @brief Get coalescing window in milliseconds
     *
     * @return Coalescing window size
     */
    uint64_t getCoalesceWindow() const {
        return coalesce_window_ms_;
    }

    /**
     * @brief Get number of pending events
     *
     * @return Count of events waiting in coalescing window
     */
    size_t getPendingCount() const;

private:
    /**
     * @brief Get current time in milliseconds since epoch
     *
     * @return Current time in milliseconds
     */
    uint64_t getCurrentTimeMs() const;

    /**
     * @brief Pending event with timestamp
     */
    struct PendingEvent {
        PrioritizedEvent event;
        uint64_t timestamp_ms;
    };

    const uint64_t coalesce_window_ms_;  ///< Coalescing window in milliseconds

    std::unordered_map<std::string, PendingEvent> pending_events_;  ///< Pending events per type
    mutable std::mutex mutex_;  ///< Protects pending_events_ map
};

} // namespace mxrc::core::event
