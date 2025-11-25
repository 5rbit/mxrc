#include "AlarmManager.h"
#include "../dto/AlarmEvents.h"
#include "core/datastore/DataStore.h"
#include "core/event/interfaces/IEventBus.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <sstream>

namespace mxrc::core::alarm {

AlarmManager::AlarmManager(
    std::shared_ptr<IAlarmConfiguration> config,
    std::shared_ptr<DataStore> data_store,
    std::shared_ptr<event::IEventBus> event_bus)
    : config_(std::move(config)),
      data_store_(std::move(data_store)),
      event_bus_(std::move(event_bus))
{
    spdlog::info("[AlarmManager] Initialized with DataStore: {}, EventBus: {}",
                 data_store_ ? "yes" : "no",
                 event_bus_ ? "yes" : "no");
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

    // 기본 심각도
    AlarmSeverity base_severity = alarm_config->severity;

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

    // 이력에 추가
    alarm_history_.push_back(dto);
    if (alarm_history_.size() > MAX_HISTORY_SIZE) {
        alarm_history_.erase(alarm_history_.begin());
    }

    // DataStore 저장 및 이벤트 발행
    storeToDataStore(dto);

    // 심각도가 상향되었으면 escalate 이벤트, 아니면 일반 raise 이벤트
    if (severity > base_severity && recurrence > 1) {
        publishEscalateEvent(dto, base_severity);
    } else {
        publishEvent(dto);
    }

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

    // 이력에서 최신 limit개 반환 (역순)
    size_t start_idx = alarm_history_.size() > limit ?
                       alarm_history_.size() - limit : 0;

    for (size_t i = alarm_history_.size(); i > start_idx; --i) {
        result.push_back(alarm_history_[i - 1]);
    }

    // 최신 순으로 정렬 (이미 역순으로 추가했으므로 정렬 완료)
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

    // 이력에 해제된 상태로 추가
    auto dto = alarm->toDto();
    alarm_history_.push_back(dto);
    if (alarm_history_.size() > MAX_HISTORY_SIZE) {
        alarm_history_.erase(alarm_history_.begin());
    }

    // DataStore 업데이트
    if (data_store_) {
        try {
            std::string key = "alarm/" + alarm_id;
            // TODO: DataStore doesn't have remove method yet
            // data_store_->remove(key);  // 해결된 alarm 제거

            // 활성 Alarm 카운트 업데이트
            std::string count_key = "alarm/active_count";
            data_store_->set(count_key, static_cast<int>(stats_.active_count), DataType::Alarm);
        } catch (const std::exception& e) {
            spdlog::error("[AlarmManager] Failed to update DataStore on resolve: {}", e.what());
        }
    }

    // 이벤트 발행
    publishClearEvent(alarm_id, dto.alarm_code);

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
    if (!data_store_) {
        return;
    }

    try {
        // "alarm/{alarm_id}" 키로 저장
        std::string key = "alarm/" + alarm.alarm_id;

        // AlarmDto를 map으로 변환
        std::map<std::string, std::any> alarm_data;
        alarm_data["alarm_id"] = alarm.alarm_id;
        alarm_data["alarm_code"] = alarm.alarm_code;
        alarm_data["alarm_name"] = alarm.alarm_name;
        alarm_data["severity"] = static_cast<int>(alarm.severity);
        alarm_data["state"] = static_cast<int>(alarm.state);
        alarm_data["details"] = alarm.details.value_or("");
        alarm_data["source"] = alarm.source;
        alarm_data["timestamp"] = std::chrono::system_clock::to_time_t(alarm.timestamp);
        alarm_data["recurrence_count"] = alarm.recurrence_count;

        // Already handled details above
        if (alarm.acknowledged_by.has_value()) {
            alarm_data["acknowledged_by"] = alarm.acknowledged_by.value();
        }
        if (alarm.acknowledged_time.has_value()) {
            alarm_data["acknowledged_time"] = std::chrono::system_clock::to_time_t(alarm.acknowledged_time.value());
        }
        if (alarm.resolved_time.has_value()) {
            alarm_data["resolved_time"] = std::chrono::system_clock::to_time_t(alarm.resolved_time.value());
        }

        // DataStore에 저장
        data_store_->set(key, alarm_data, DataType::Alarm);

        // 활성 Alarm 카운트 업데이트
        std::string count_key = "alarm/active_count";
        data_store_->set(count_key, static_cast<int>(stats_.active_count), DataType::Alarm);

        spdlog::debug("[AlarmManager] Stored alarm {} to DataStore", alarm.alarm_id);
    } catch (const std::exception& e) {
        spdlog::error("[AlarmManager] Failed to store alarm to DataStore: {}", e.what());
    }
}

void AlarmManager::publishEvent(const AlarmDto& alarm) {
    if (!event_bus_) {
        return;
    }

    try {
        // AlarmRaisedEvent 생성 및 발행
        auto event = std::make_shared<AlarmRaisedEvent>(alarm);
        event_bus_->publish(event);

        spdlog::debug("[AlarmManager] Published AlarmRaisedEvent for {}", alarm.alarm_id);
    } catch (const std::exception& e) {
        spdlog::error("[AlarmManager] Failed to publish alarm event: {}", e.what());
    }
}

void AlarmManager::publishClearEvent(const std::string& alarm_id, const std::string& alarm_type) {
    if (!event_bus_) {
        return;
    }

    try {
        auto event = std::make_shared<AlarmClearedEvent>(alarm_id, alarm_type, "system");
        event_bus_->publish(event);

        spdlog::debug("[AlarmManager] Published AlarmClearedEvent for {}", alarm_id);
    } catch (const std::exception& e) {
        spdlog::error("[AlarmManager] Failed to publish clear event: {}", e.what());
    }
}

void AlarmManager::publishEscalateEvent(const AlarmDto& alarm, AlarmSeverity old_severity) {
    if (!event_bus_) {
        return;
    }

    try {
        auto event = std::make_shared<AlarmEscalatedEvent>(
            alarm.alarm_id, alarm.alarm_code,
            old_severity, alarm.severity,
            alarm.recurrence_count);
        event_bus_->publish(event);

        spdlog::debug("[AlarmManager] Published AlarmEscalatedEvent for {}", alarm.alarm_id);
    } catch (const std::exception& e) {
        spdlog::error("[AlarmManager] Failed to publish escalate event: {}", e.what());
    }
}

} // namespace mxrc::core::alarm
