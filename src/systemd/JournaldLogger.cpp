#include "mxrc/systemd/JournaldLogger.hpp"
#include <systemd/sd-journal.h>
#include <syslog.h> // LOG_EMERG, LOG_ALERT, ...

namespace mxrc {
namespace systemd {

int JournaldLogger::toPriority(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:
            return LOG_DEBUG;   // 7
        case LogLevel::DEBUG:
            return LOG_DEBUG;   // 7
        case LogLevel::INFO:
            return LOG_INFO;    // 6
        case LogLevel::WARN:
            return LOG_WARNING; // 4
        case LogLevel::ERROR:
            return LOG_ERR;     // 3
        case LogLevel::CRITICAL:
            return LOG_CRIT;    // 2
        default:
            return LOG_INFO;
    }
}

void JournaldLogger::sendToJournal(
    int priority,
    const std::string& message,
    const LogMetadata& metadata) {

    // journald에 로그 전송
    // sd_journal_send 형식: "KEY=VALUE", ..., NULL

    if (!metadata.trace_id.empty() && !metadata.span_id.empty() && !metadata.component.empty()) {
        // 모든 메타데이터 포함
        sd_journal_send(
            "MESSAGE=%s", message.c_str(),
            "PRIORITY=%d", priority,
            "TRACE_ID=%s", metadata.trace_id.c_str(),
            "SPAN_ID=%s", metadata.span_id.c_str(),
            "COMPONENT=%s", metadata.component.c_str(),
            NULL
        );
    } else if (!metadata.component.empty()) {
        // Component만 포함
        sd_journal_send(
            "MESSAGE=%s", message.c_str(),
            "PRIORITY=%d", priority,
            "COMPONENT=%s", metadata.component.c_str(),
            NULL
        );
    } else {
        // 기본 로그 (메타데이터 없음)
        sd_journal_send(
            "MESSAGE=%s", message.c_str(),
            "PRIORITY=%d", priority,
            NULL
        );
    }

    // 사용자 정의 필드 추가
    // TODO: custom_fields 처리 (필요시)
}

void JournaldLogger::log(LogLevel level, const std::string& message, const LogMetadata& metadata) {
    int priority = toPriority(level);
    sendToJournal(priority, message, metadata);
}

void JournaldLogger::log(LogLevel level, const std::string& message) {
    LogMetadata empty_metadata;
    log(level, message, empty_metadata);
}

void JournaldLogger::trace(const std::string& message, const LogMetadata& metadata) {
    log(LogLevel::TRACE, message, metadata);
}

void JournaldLogger::debug(const std::string& message, const LogMetadata& metadata) {
    log(LogLevel::DEBUG, message, metadata);
}

void JournaldLogger::info(const std::string& message, const LogMetadata& metadata) {
    log(LogLevel::INFO, message, metadata);
}

void JournaldLogger::warn(const std::string& message, const LogMetadata& metadata) {
    log(LogLevel::WARN, message, metadata);
}

void JournaldLogger::error(const std::string& message, const LogMetadata& metadata) {
    log(LogLevel::ERROR, message, metadata);
}

void JournaldLogger::critical(const std::string& message, const LogMetadata& metadata) {
    log(LogLevel::CRITICAL, message, metadata);
}

} // namespace systemd
} // namespace mxrc
