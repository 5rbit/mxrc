#include "SdNotifyWatchdog.h"
#include <systemd/sd-daemon.h>
#include <cstdlib>

namespace mxrc {
namespace systemd {
namespace impl {

SdNotifyWatchdog::SdNotifyWatchdog() {
    // Lightweight constructor - no systemd operations
}

bool SdNotifyWatchdog::sendWatchdog() {
    return sendNotification("WATCHDOG=1");
}

bool SdNotifyWatchdog::sendReady() {
    return sendNotification("READY=1");
}

bool SdNotifyWatchdog::sendStatus(const std::string& status) {
    std::string message = "STATUS=" + status;
    return sendNotification(message.c_str());
}

bool SdNotifyWatchdog::isSystemdAvailable() const {
    // Check if NOTIFY_SOCKET environment variable is set
    const char* notifySocket = std::getenv("NOTIFY_SOCKET");
    return (notifySocket != nullptr && notifySocket[0] != '\0');
}

bool SdNotifyWatchdog::sendNotification(const char* state) const {
    // If not running under systemd, silently succeed
    if (!isSystemdAvailable()) {
        return true;
    }

    // Send notification using sd_notify()
    // unset_environment = 0 (don't unset NOTIFY_SOCKET)
    int result = sd_notify(0, state);

    // sd_notify returns:
    //   > 0 if notification was sent
    //   = 0 if not sent (systemd not available)
    //   < 0 on error
    return (result >= 0);
}

}  // namespace impl
}  // namespace systemd
}  // namespace mxrc
