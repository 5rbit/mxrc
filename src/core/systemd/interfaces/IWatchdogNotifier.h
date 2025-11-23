#pragma once

#include <string>

namespace mxrc {
namespace systemd {
namespace interfaces {

/**
 * @brief Interface for systemd watchdog notifications
 *
 * This interface defines the contract for sending watchdog notifications
 * to systemd using the sd_notify() API. Implementations must ensure
 * that watchdog notifications are sent with minimal overhead (<10μs).
 *
 * Watchdog notifications keep systemd aware that the process is alive
 * and healthy. If notifications stop for WatchdogSec duration, systemd
 * will restart the service according to Restart policy.
 *
 * Thread Safety: Implementations must be thread-safe for concurrent calls.
 */
class IWatchdogNotifier {
public:
    virtual ~IWatchdogNotifier() = default;

    /**
     * @brief Send watchdog keep-alive notification to systemd
     *
     * Sends "WATCHDOG=1" notification via sd_notify(). This should be
     * called periodically at intervals less than WatchdogSec/2 to
     * prevent systemd from timing out.
     *
     * Performance: Must complete in <10μs to avoid RT jitter
     *
     * @return true if notification was sent successfully
     * @return false if systemd is not available or notification failed
     */
    virtual bool sendWatchdog() = 0;

    /**
     * @brief Send service ready notification to systemd
     *
     * Sends "READY=1" notification via sd_notify(). This signals to
     * systemd that the service has completed initialization and is
     * ready to serve. Should be called once during startup.
     *
     * Only effective when Type=notify is set in service file.
     *
     * @return true if notification was sent successfully
     * @return false if systemd is not available or notification failed
     */
    virtual bool sendReady() = 0;

    /**
     * @brief Send custom status message to systemd
     *
     * Sends "STATUS=<message>" notification via sd_notify(). The status
     * message is visible in systemctl status output. Use for human-readable
     * status updates (e.g., "Processing batch 5/10").
     *
     * @param status Human-readable status message
     * @return true if notification was sent successfully
     * @return false if systemd is not available or notification failed
     */
    virtual bool sendStatus(const std::string& status) = 0;
};

}  // namespace interfaces
}  // namespace systemd
}  // namespace mxrc
