#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "../interfaces/IAlarmManager.h"
#include "../interfaces/IAlarmConfiguration.h"
#include "Alarm.h"

namespace mxrc::core::alarm {

/**
 * @brief Alarm 관리자 구현 클래스
 *
 * 시스템의 모든 alarm을 중앙에서 관리합니다.
 * Thread-safe한 구현으로 여러 컴포넌트에서 동시에 접근 가능합니다.
 *
 * Feature 016: Pallet Shuttle Control System
 */
class AlarmManager : public IAlarmManager {
public:
    /**
     * @brief 생성자
     *
     * @param config Alarm 설정 인터페이스
     */
    explicit AlarmManager(std::shared_ptr<IAlarmConfiguration> config);

    ~AlarmManager() override = default;

    // IAlarmManager 인터페이스 구현
    std::optional<AlarmDto> raiseAlarm(
        const std::string& alarm_code,
        const std::string& source,
        const std::optional<std::string>& details = std::nullopt) override;

    std::optional<AlarmDto> getAlarm(const std::string& alarm_id) const override;

    std::vector<AlarmDto> getActiveAlarms() const override;

    std::vector<AlarmDto> getActiveAlarmsBySeverity(
        AlarmSeverity severity) const override;

    std::vector<AlarmDto> getAlarmHistory(size_t limit = 100) const override;

    bool acknowledgeAlarm(
        const std::string& alarm_id,
        const std::string& acknowledged_by) override;

    bool resolveAlarm(const std::string& alarm_id) override;

    size_t resetAllAlarms() override;

    bool hasCriticalAlarm() const override;

    AlarmStats getStatistics() const override;

private:
    /**
     * @brief 재발 여부 확인
     *
     * @param alarm_code Alarm 코드
     * @return 재발 횟수
     */
    uint32_t checkRecurrence(const std::string& alarm_code);

    /**
     * @brief 심각도 상향 필요 여부 확인
     *
     * @param alarm_code Alarm 코드
     * @param recurrence_count 재발 횟수
     * @return 상향된 심각도
     */
    AlarmSeverity checkEscalation(
        const std::string& alarm_code,
        uint32_t recurrence_count);

    /**
     * @brief DataStore에 alarm 저장
     */
    void storeToDataStore(const AlarmDto& alarm);

    /**
     * @brief EventBus에 alarm 이벤트 발행
     */
    void publishEvent(const AlarmDto& alarm);

    // Alarm 설정
    std::shared_ptr<IAlarmConfiguration> config_;

    // alarm_id -> Alarm 매핑
    std::unordered_map<std::string, std::shared_ptr<Alarm>> alarms_;

    // alarm_code -> 최근 발생 시각 (재발 추적용)
    std::unordered_map<std::string, std::chrono::system_clock::time_point> last_occurrence_;

    // alarm_code -> 재발 횟수 (시간 윈도우 내)
    std::unordered_map<std::string, uint32_t> recurrence_count_;

    // 통계
    AlarmStats stats_;

    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace mxrc::core::alarm
