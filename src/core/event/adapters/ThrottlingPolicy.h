#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>
#include <chrono>
#include <mutex>

namespace mxrc::core::event {

/**
 * @brief Throttling policy for event rate limiting
 *
 * Design Goals (Feature 022 P3):
 * - Prevent event flooding by limiting event rate per type
 * - Configurable throttle interval (default: 100ms)
 * - Thread-safe for concurrent access
 * - Minimal overhead for high-frequency events
 *
 * Throttling Logic:
 * - First event of each type is always sent
 * - Subsequent events are throttled if sent within throttle interval
 * - Each event type has independent throttling state
 *
 * Usage Example:
 * @code
 * ThrottlingPolicy policy(100);  // 100ms throttle interval
 *
 * // First event: allowed
 * if (policy.shouldSend("temperature")) {
 *     sendEvent("temperature", 25.0);
 * }
 *
 * // Immediate repeat: throttled
 * if (policy.shouldSend("temperature")) {  // Returns false
 *     sendEvent("temperature", 25.1);  // Not sent
 * }
 *
 * // After 100ms: allowed again
 * std::this_thread::sleep_for(std::chrono::milliseconds(100));
 * if (policy.shouldSend("temperature")) {  // Returns true
 *     sendEvent("temperature", 25.2);  // Sent
 * }
 * @endcode
 */
class ThrottlingPolicy {
public:
    /**
     * @brief Construct throttling policy with interval
     *
     * @param throttle_interval_ms Minimum interval between events of same type (default: 100ms)
     */
    explicit ThrottlingPolicy(uint64_t throttle_interval_ms = 100);

    /**
     * @brief Destructor
     */
    ~ThrottlingPolicy() = default;

    /**
     * @brief Check if event should be sent based on throttling policy
     *
     * This method updates the last sent time for the event type if it should be sent.
     * Thread-safe for concurrent access.
     *
     * @param event_type Event type identifier
     * @return true if event should be sent, false if throttled
     */
    bool shouldSend(const std::string& event_type);

    /**
     * @brief Reset throttling state for all event types
     *
     * Useful for testing or when restarting event system.
     */
    void reset();

    /**
     * @brief Reset throttling state for specific event type
     *
     * @param event_type Event type to reset
     */
    void resetEventType(const std::string& event_type);

    /**
     * @brief Get throttle interval in milliseconds
     *
     * @return Throttle interval
     */
    uint64_t getThrottleInterval() const {
        return throttle_interval_ms_;
    }

private:
    /**
     * @brief Get current time in milliseconds since epoch
     *
     * @return Current time in milliseconds
     */
    uint64_t getCurrentTimeMs() const;

    const uint64_t throttle_interval_ms_;  ///< Throttle interval in milliseconds

    std::unordered_map<std::string, uint64_t> last_sent_time_;  ///< Last sent time per event type
    mutable std::mutex mutex_;  ///< Protects last_sent_time_ map
};

} // namespace mxrc::core::event
