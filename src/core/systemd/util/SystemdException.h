#pragma once

#include <stdexcept>
#include <string>

namespace mxrc {
namespace systemd {
namespace util {

/**
 * @brief systemd 관련 예외 클래스
 *
 * systemd 작업 중 발생하는 오류를 표현하는 예외.
 * std::runtime_error를 상속하여 표준 예외 처리 패턴을 따름.
 *
 * @example
 * if (!SystemdUtil::checkSystemdAvailable()) {
 *     throw SystemdException("systemd is not available on this system");
 * }
 */
class SystemdException : public std::runtime_error {
public:
    /**
     * @brief 생성자
     *
     * @param message 예외 메시지
     */
    explicit SystemdException(const std::string& message)
        : std::runtime_error("SystemdException: " + message) {}

    /**
     * @brief 생성자 (C-string)
     *
     * @param message 예외 메시지
     */
    explicit SystemdException(const char* message)
        : std::runtime_error(std::string("SystemdException: ") + message) {}
};

/**
 * @brief Watchdog 관련 예외 클래스
 *
 * Watchdog 알림 전송 실패 등의 오류를 표현.
 */
class WatchdogException : public SystemdException {
public:
    explicit WatchdogException(const std::string& message)
        : SystemdException("Watchdog: " + message) {}

    explicit WatchdogException(const char* message)
        : SystemdException(std::string("Watchdog: ") + message) {}
};

/**
 * @brief Journald 관련 예외 클래스
 *
 * Journald 로깅 실패 등의 오류를 표현.
 */
class JournaldException : public SystemdException {
public:
    explicit JournaldException(const std::string& message)
        : SystemdException("Journald: " + message) {}

    explicit JournaldException(const char* message)
        : SystemdException(std::string("Journald: ") + message) {}
};

} // namespace util
} // namespace systemd
} // namespace mxrc
