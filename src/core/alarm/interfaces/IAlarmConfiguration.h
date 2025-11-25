#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include "../dto/AlarmSeverity.h"

namespace mxrc::core::alarm {

/**
 * @brief Alarm 설정 정보
 *
 * alarm-config.yaml에서 로드된 개별 alarm의 설정입니다.
 */
struct AlarmConfig {
    std::string code;            ///< Alarm 코드 (예: "E001")
    std::string name;            ///< Alarm 이름
    AlarmSeverity severity;      ///< 기본 심각도
    std::string description;     ///< 설명
    std::optional<std::string> recommended_action;  ///< 권장 조치

    /**
     * @brief 재발 추적 설정
     *
     * 동일 alarm이 이 시간 내에 threshold회 이상 발생하면
     * 심각도를 자동으로 상향 조정합니다.
     */
    std::chrono::seconds recurrence_window{60};  ///< 재발 추적 시간 창 (기본: 60초)
    uint32_t recurrence_threshold{3};            ///< 재발 임계값 (기본: 3회)

    /**
     * @brief 자동 리셋 가능 여부
     *
     * true: 조건이 해소되면 자동으로 해결
     * false: 수동 리셋 필요
     */
    bool auto_reset{false};
};

/**
 * @brief 범용 Alarm 설정 인터페이스
 *
 * alarm-config.yaml 파일을 파싱하고 alarm 설정을 관리합니다.
 *
 * 설정 파일 구조:
 * ```yaml
 * alarms:
 *   - code: E001
 *     name: Emergency Stop Activated
 *     severity: CRITICAL
 *     description: Emergency stop button pressed
 *     recommended_action: Check safety system
 *     auto_reset: false
 *
 *   - code: W001
 *     name: Battery Low
 *     severity: WARNING
 *     description: Battery level below 20%
 *     recurrence_window: 300  # 5분
 *     recurrence_threshold: 2
 *     auto_reset: true
 * ```
 *
 * @see AlarmConfig Alarm 설정 구조체
 * @see IAlarmManager Alarm 관리자
 *
 * Feature 016: Pallet Shuttle Control System
 */
class IAlarmConfiguration {
public:
    virtual ~IAlarmConfiguration() = default;

    /**
     * @brief 설정 파일 로드
     *
     * YAML 파일을 파싱하여 alarm 설정을 메모리에 로드합니다.
     *
     * @param config_file 설정 파일 경로
     * @return true 로드 성공
     * @return false 로드 실패 (파일 없음, 파싱 에러 등)
     *
     * @throws std::runtime_error YAML 파싱 에러
     */
    virtual bool loadFromFile(const std::string& config_file) = 0;

    /**
     * @brief Alarm 설정 조회
     *
     * @param alarm_code Alarm 코드
     * @return AlarmConfig, 해당 코드가 없으면 std::nullopt
     */
    virtual std::optional<AlarmConfig> getAlarmConfig(
        const std::string& alarm_code) const = 0;

    /**
     * @brief 모든 Alarm 설정 목록
     *
     * @return std::vector 로드된 모든 alarm 설정
     */
    virtual std::vector<AlarmConfig> getAllConfigs() const = 0;

    /**
     * @brief Alarm 코드 존재 여부
     *
     * @param alarm_code 확인할 코드
     * @return true 설정 존재
     * @return false 설정 없음
     */
    virtual bool hasAlarmConfig(const std::string& alarm_code) const = 0;

    /**
     * @brief 심각도 상향 판단
     *
     * 재발 빈도에 따라 심각도를 상향 조정해야 하는지 결정합니다.
     *
     * 로직:
     * - recurrence_count >= recurrence_threshold이면 상향
     * - WARNING → CRITICAL
     * - INFO → WARNING
     * - CRITICAL은 변경 없음 (이미 최고 심각도)
     *
     * @param alarm_code Alarm 코드
     * @param recurrence_count 재발 횟수
     * @return 상향된 심각도, 변경 불필요하면 원래 심각도
     */
    virtual AlarmSeverity shouldEscalateSeverity(
        const std::string& alarm_code,
        uint32_t recurrence_count) const = 0;

    /**
     * @brief 설정 검증
     *
     * 로드된 설정의 유효성을 검증합니다.
     * - 중복된 alarm_code 확인
     * - 필수 필드 누락 확인
     * - 유효하지 않은 severity 값 확인
     *
     * @return true 모든 설정이 유효
     * @return false 유효하지 않은 설정 발견
     */
    virtual bool validate() const = 0;
};

} // namespace mxrc::core::alarm
