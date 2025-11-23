#pragma once

#include "../interfaces/IWatchdogNotifier.h"
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

namespace mxrc {
namespace systemd {
namespace util {

/**
 * @brief Timer for periodic systemd watchdog notifications
 *
 * WatchdogTimer manages a background thread that sends periodic
 * watchdog notifications via IWatchdogNotifier. The interval should
 * be set to WatchdogSec/2 or WatchdogSec/3 for safety margin.
 *
 * Example:
 *   auto notifier = std::make_shared<SdNotifyWatchdog>();
 *   WatchdogTimer timer(notifier, std::chrono::seconds(10));
 *   timer.start();  // Send watchdog every 10 seconds
 *   // ... application runs ...
 *   timer.stop();   // Clean shutdown
 *
 * Thread Safety: Thread-safe for concurrent start/stop calls
 * Lifecycle: Auto-stops in destructor
 */
class WatchdogTimer {
public:
    /**
     * @brief Construct a new Watchdog Timer
     *
     * @param notifier Shared pointer to IWatchdogNotifier implementation
     * @param interval Interval between watchdog notifications (default: 10s)
     */
    explicit WatchdogTimer(
        std::shared_ptr<interfaces::IWatchdogNotifier> notifier,
        std::chrono::milliseconds interval = std::chrono::milliseconds(10000));

    /**
     * @brief Destroy the Watchdog Timer
     *
     * Automatically stops the timer thread if running
     */
    ~WatchdogTimer();

    // Non-copyable, non-movable
    WatchdogTimer(const WatchdogTimer&) = delete;
    WatchdogTimer& operator=(const WatchdogTimer&) = delete;
    WatchdogTimer(WatchdogTimer&&) = delete;
    WatchdogTimer& operator=(WatchdogTimer&&) = delete;

    /**
     * @brief Start periodic watchdog notifications
     *
     * Spawns a background thread that sends watchdog notifications
     * at the configured interval. Does nothing if already started.
     *
     * @return true if timer was started
     * @return false if timer was already running
     */
    bool start();

    /**
     * @brief Stop periodic watchdog notifications
     *
     * Stops the background thread and waits for it to terminate.
     * Does nothing if not running.
     *
     * @return true if timer was stopped
     * @return false if timer was not running
     */
    bool stop();

    /**
     * @brief Check if timer is currently running
     *
     * @return true if timer thread is active
     */
    bool isRunning() const;

private:
    /**
     * @brief Internal timer loop (runs in background thread)
     *
     * Sends periodic watchdog notifications until stop() is called
     */
    void timerLoop();

    std::shared_ptr<interfaces::IWatchdogNotifier> notifier_;
    std::chrono::milliseconds interval_;
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> timerThread_;
};

}  // namespace util
}  // namespace systemd
}  // namespace mxrc
