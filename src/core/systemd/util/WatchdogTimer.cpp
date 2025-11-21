#include "WatchdogTimer.h"
#include <iostream>

namespace mxrc {
namespace systemd {
namespace util {

WatchdogTimer::WatchdogTimer(
    std::shared_ptr<interfaces::IWatchdogNotifier> notifier,
    std::chrono::milliseconds interval)
    : notifier_(notifier)
    , interval_(interval)
    , running_(false)
    , timerThread_(nullptr) {
}

WatchdogTimer::~WatchdogTimer() {
    stop();
}

bool WatchdogTimer::start() {
    // Prevent double start
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return false;  // Already running
    }

    // Start timer thread
    timerThread_ = std::make_unique<std::thread>(&WatchdogTimer::timerLoop, this);
    return true;
}

bool WatchdogTimer::stop() {
    // Check if running
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return false;  // Not running
    }

    // Wait for thread to terminate
    if (timerThread_ && timerThread_->joinable()) {
        timerThread_->join();
    }
    timerThread_.reset();

    return true;
}

bool WatchdogTimer::isRunning() const {
    return running_.load();
}

void WatchdogTimer::timerLoop() {
    while (running_.load()) {
        // Send watchdog notification
        if (notifier_) {
            notifier_->sendWatchdog();
        }

        // Sleep for interval
        std::this_thread::sleep_for(interval_);
    }
}

}  // namespace util
}  // namespace systemd
}  // namespace mxrc
