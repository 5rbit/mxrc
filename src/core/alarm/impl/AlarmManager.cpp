#include "AlarmManager.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mxrc::core::alarm {

AlarmManager::AlarmManager(std::shared_ptr<IAlarmConfiguration> config)
    : config_(std::move(config))
{
    spdlog::info("[AlarmManager] Initialized");
}

std::optional<AlarmDto> AlarmManager::raiseAlarm(
    const std::string& alarm_code,
    const std::string& source,
    const std::optional<std::string>& details) {

    std::lock_guard<std::mutex> lock(mutex_);

    // 설정 조회
    auto alarm_config = config_->getAlarmConfig(alarm_code);
    if (!alarm_config) {
        spdlog::error("[AlarmManager] Unknown alarm code: {}", alarm_code);
        return std::nullopt;
    }

    // 재발 확인
    uint32_t recurrence = checkRecurrence(alarm_code);

    // 심각도 상향 확인
    AlarmSeverity severity = checkEscalation(alarm_code, recurrence);

    // Alarm 생성
    auto alarm = std::make_shared<Alarm>(
        alarm_code,
        alarm_config->name,
        severity,
        source);

    if (details) {
        alarm->setDetails(*details);
    }

    // 재발 횟수 설정
    alarm->setRecurrenceCount(recurrence);

    // 저장
    alarms_[alarm->getId()] = alarm;

    // 통계 업데이트
    stats_.total_raised++;
    stats_.active_count++;
    switch (severity) {
        case AlarmSeverity::CRITICAL:
            stats_.critical_count++;
            break;
        case AlarmSeverity::WARNING:
            stats_.warning_count++;
            break;
        case AlarmSeverity::INFO:
            stats_.info_count++;
            break;
    }

    auto dto = alarm->toDto();

    // DataStore 저장 및 이벤트 발행
    storeToDataStore(dto);
    publishEvent(dto);

    spdlog::warn("[AlarmManager] Raised: {} - {} (severity: {}, recurrence: {})",
        alarm_code, alarm_config->name, toString(severity), recurrence);

    return dto;
}

std::optional<AlarmDto> AlarmManager::getAlarm(const std::string& alarm_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = alarms_.find(alarm_id);
    if (it != alarms_.end()) {
        return it->second->toDto();
    }

    return std::nullopt;
}

std::vector<AlarmDto> AlarmManager::getActiveAlarms() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<AlarmDto> result;
    result.reserve(alarms_.size());

    for (const auto& [id, alarm] : alarms_) {
        if (alarm->isActive()) {
            result.push_back(alarm->toDto());
        }
    }

    // 심각도 순으로 정렬 (CRITICAL → WARNING → INFO)
    std::sort(result.begin(), result.end(),
        [](const AlarmDto& a, const AlarmDto& b) {
            return a.severity < b.severity;
        });

    return result;
}

std::vector<AlarmDto> AlarmManager::getActiveAlarmsBySeverity(
    AlarmSeverity severity) const {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<AlarmDto> result;

    for (const auto& [id, alarm] : alarms_) {
        if (alarm->isActive() && alarm->getSeverity() == severity) {
            result.push_back(alarm->toDto());
        }
    }

    return result;
}

std::vector<AlarmDto> AlarmManager::getAlarmHistory(size_t limit) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<AlarmDto> result;
    result.reserve(std::min(limit, alarms_.size()));

    for (const auto& [id, alarm] : alarms_) {
        result.push_back(alarm->toDto());
        if (result.size() >= limit) {
            break;
        }
    }

    // 최신 순으로 정렬
    std::sort(result.begin(), result.end(),
        [](const AlarmDto& a, const AlarmDto& b) {
            return a.timestamp > b.timestamp;
        });

    return result;
}

bool AlarmManager::acknowledgeAlarm(
    const std::string& alarm_id,
    const std::string& acknowledged_by) {

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = alarms_.find(alarm_id);
    if (it == alarms_.end()) {
        return false;
    }

    it->second->acknowledge(acknowledged_by);

    spdlog::info("[AlarmManager] Acknowledged: {} by {}",
        alarm_id, acknowledged_by);

    return true;
}

bool AlarmManager::resolveAlarm(const std::string& alarm_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = alarms_.find(alarm_id);
    if (it == alarms_.end()) {
        return false;
    }

    auto alarm = it->second;
    if (!alarm->isActive()) {
        return false;  // 이미 해결됨
    }

    alarm->resolve();

    // 통계 업데이트
    stats_.active_count--;
    stats_.resolved_count++;

    switch (alarm->getSeverity()) {
        case AlarmSeverity::CRITICAL:
            stats_.critical_count--;
            break;
        case AlarmSeverity::WARNING:
            stats_.warning_count--;
            break;
        case AlarmSeverity::INFO:
            stats_.info_count--;
            break;
    }

    spdlog::info("[AlarmManager] Resolved: {}", alarm_id);

    return true;
}

size_t AlarmManager::resetAllAlarms() {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = 0;

    for (auto& [id, alarm] : alarms_) {
        if (alarm->isActive()) {
            alarm->resolve();
            count++;
        }
    }

    // 통계 리셋
    stats_.active_count = 0;
    stats_.critical_count = 0;
    stats_.warning_count = 0;
    stats_.info_count = 0;
    stats_.resolved_count += count;

    spdlog::info("[AlarmManager] Reset {} alarms", count);

    return count;
}

bool AlarmManager::hasCriticalAlarm() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_.critical_count > 0;
}

IAlarmManager::AlarmStats AlarmManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

uint32_t AlarmManager::checkRecurrence(const std::string& alarm_code) {
    auto now = std::chrono::system_clock::now();

    // 마지막 발생 시각 확인
    auto it = last_occurrence_.find(alarm_code);
    if (it != last_occurrence_.end()) {
        // 설정된 시간 윈도우 내인지 확인
        auto alarm_config = config_->getAlarmConfig(alarm_code);
        if (alarm_config) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second);

            if (elapsed <= alarm_config->recurrence_window) {
                // 윈도우 내 재발
                recurrence_count_[alarm_code]++;
            } else {
                // 윈도우 밖 - 카운트 리셋
                recurrence_count_[alarm_code] = 1;
            }
        }
    } else {
        // 첫 발생
        recurrence_count_[alarm_code] = 1;
    }

    last_occurrence_[alarm_code] = now;

    return recurrence_count_[alarm_code];
}

AlarmSeverity AlarmManager::checkEscalation(
    const std::string& alarm_code,
    uint32_t recurrence_count) {

    return config_->shouldEscalateSeverity(alarm_code, recurrence_count);
}

void AlarmManager::storeToDataStore(const AlarmDto& alarm) {
    // TODO: DataStore 통합 (Feature 016 후속 작업)
    // DataStore에 "alarm/{alarm_id}" 키로 저장
}

void AlarmManager::publishEvent(const AlarmDto& alarm) {
    // TODO: EventBus 통합 (Feature 016 후속 작업)
    // AlarmEvent 발행
}

} // namespace mxrc::core::alarm
