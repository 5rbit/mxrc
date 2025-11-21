#pragma once

#include "../interfaces/IWatchdogNotifier.h"

namespace mxrc {
namespace systemd {
namespace impl {

/**
 * @brief systemd sd_notify() API wrapper for watchdog notifications
 *
 * This class implements IWatchdogNotifier using the libsystemd sd_notify()
 * function. It provides a lightweight wrapper (<10μs overhead) for sending
 * watchdog notifications to systemd.
 *
 * The implementation checks for NOTIFY_SOCKET environment variable to
 * determine if running under systemd. If not running as a systemd service,
 * notifications silently succeed without error.
 *
 * Thread Safety: Thread-safe for concurrent calls to all methods
 *
 * Requirements:
 * - libsystemd-dev installed
 * - Service file must have Type=notify
 * - NOTIFY_SOCKET environment variable set by systemd
 */
class SdNotifyWatchdog : public interfaces::IWatchdogNotifier {
public:
    /**
     * @brief Construct a new SdNotifyWatchdog
     *
     * Constructor is lightweight and does not perform any systemd operations.
     */
    SdNotifyWatchdog();

    /**
     * @brief Destroy the SdNotifyWatchdog
     */
    ~SdNotifyWatchdog() override = default;

    /**
     * @brief Send watchdog keep-alive notification
     *
     * Calls sd_notify(0, "WATCHDOG=1") to signal process health.
     *
     * Performance: Completes in <10μs on typical systems
     *
     * @return true if notification sent or systemd not available
     * @return false only on critical sd_notify() failure
     */
    bool sendWatchdog() override;

    /**
     * @brief Send service ready notification
     *
     * Calls sd_notify(0, "READY=1") to signal initialization complete.
     *
     * @return true if notification sent or systemd not available
     * @return false only on critical sd_notify() failure
     */
    bool sendReady() override;

    /**
     * @brief Send custom status message
     *
     * Calls sd_notify(0, "STATUS=<message>") to update status text.
     *
     * @param status Human-readable status message
     * @return true if notification sent or systemd not available
     * @return false only on critical sd_notify() failure
     */
    bool sendStatus(const std::string& status) override;

private:
    /**
     * @brief Check if running under systemd
     *
     * Checks for NOTIFY_SOCKET environment variable.
     *
     * @return true if NOTIFY_SOCKET is set
     */
    bool isSystemdAvailable() const;

    /**
     * @brief Send notification to systemd
     *
     * Internal helper that calls sd_notify() with given state string.
     *
     * @param state State string (e.g., "WATCHDOG=1", "READY=1")
     * @return true if notification sent successfully
     */
    bool sendNotification(const char* state) const;
};

}  // namespace impl
}  // namespace systemd
}  // namespace mxrc
