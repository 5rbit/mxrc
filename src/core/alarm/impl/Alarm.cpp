#include "Alarm.h"
#include <spdlog/spdlog.h>

namespace mxrc::core::alarm {

Alarm::Alarm(
    std::string code,
    std::string name,
    AlarmSeverity severity,
    std::string source)
    : dto_(std::move(code), std::move(name), severity, std::move(source))
{
    spdlog::info("[Alarm] Created: {} - {} (severity: {})",
        dto_.alarm_code, dto_.alarm_name, toString(severity));
}

void Alarm::setDetails(const std::string& details) {
    dto_.details = details;
}

void Alarm::acknowledge(const std::string& acknowledged_by) {
    if (dto_.state == AlarmState::ACTIVE) {
        dto_.state = AlarmState::ACKNOWLEDGED;
        dto_.acknowledged_time = std::chrono::system_clock::now();
        dto_.acknowledged_by = acknowledged_by;

        spdlog::info("[Alarm] Acknowledged: {} by {}",
            dto_.alarm_id, acknowledged_by);
    }
}

void Alarm::resolve() {
    if (dto_.state != AlarmState::RESOLVED) {
        dto_.state = AlarmState::RESOLVED;
        dto_.resolved_time = std::chrono::system_clock::now();

        spdlog::info("[Alarm] Resolved: {} (elapsed: {}ms)",
            dto_.alarm_id, getElapsedMs());
    }
}

void Alarm::recordRecurrence() {
    dto_.recurrence_count++;
    dto_.last_recurrence = std::chrono::system_clock::now();

    spdlog::warn("[Alarm] Recurrence recorded: {} (count: {})",
        dto_.alarm_code, dto_.recurrence_count);
}

void Alarm::setRecurrenceCount(uint32_t count) {
    dto_.recurrence_count = count;
}

void Alarm::escalateSeverity(AlarmSeverity new_severity) {
    if (new_severity > dto_.severity) {
        auto old_severity = dto_.severity;
        dto_.severity = new_severity;

        spdlog::warn("[Alarm] Severity escalated: {} - {} -> {}",
            dto_.alarm_code,
            toString(old_severity),
            toString(new_severity));
    }
}

} // namespace mxrc::core::alarm
