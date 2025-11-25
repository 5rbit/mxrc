#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include "../interfaces/IAlarmConfiguration.h"

namespace mxrc::core::alarm {

/**
 * @brief Alarm 설정 구현 클래스
 *
 * alarm-config.yaml 파일을 파싱하고 alarm 설정을 메모리에 저장합니다.
 * yaml-cpp 라이브러리를 사용하여 YAML 파싱을 수행합니다.
 *
 * Feature 016: Pallet Shuttle Control System
 */
class AlarmConfiguration : public IAlarmConfiguration {
public:
    AlarmConfiguration() = default;
    ~AlarmConfiguration() override = default;

    // IAlarmConfiguration 인터페이스 구현
    bool loadFromFile(const std::string& config_file) override;
    std::optional<AlarmConfig> getAlarmConfig(const std::string& alarm_code) const override;
    std::vector<AlarmConfig> getAllConfigs() const override;
    bool hasAlarmConfig(const std::string& alarm_code) const override;
    AlarmSeverity shouldEscalateSeverity(
        const std::string& alarm_code,
        uint32_t recurrence_count) const override;
    bool validate() const override;

    /**
     * @brief 테스트용: Alarm 설정 직접 추가
     */
    void addConfig(const AlarmConfig& config);

private:
    /**
     * @brief YAML 노드에서 AlarmConfig 파싱
     */
    AlarmConfig parseAlarmConfig(const void* node) const;

    /**
     * @brief 심각도 문자열을 enum으로 변환
     */
    AlarmSeverity parseSeverity(const std::string& severity_str) const;

    // alarm_code -> AlarmConfig 매핑
    std::unordered_map<std::string, AlarmConfig> configs_;

    // 로드된 설정 파일 경로
    std::string config_file_path_;
};

} // namespace mxrc::core::alarm
