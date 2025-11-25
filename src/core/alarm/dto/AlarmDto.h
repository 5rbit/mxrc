#pragma once

#include <string>
#include <chrono>
#include <optional>
#include "AlarmSeverity.h"

namespace mxrc::core::alarm {

/**
 * @brief Alarm 상태
 *
 * Alarm의 생명주기를 나타냅니다.
 */
enum class AlarmState : uint8_t {
    ACTIVE = 0,      ///< 활성 (발생 중)
    ACKNOWLEDGED = 1, ///< 확인됨 (조작자가 인지)
    RESOLVED = 2     ///< 해결됨 (원인 제거 완료)
};

/**
 * @brief Alarm 데이터 전송 객체 (DTO)
 *
 * Alarm 시스템에서 사용되는 alarm 정보를 표현합니다.
 * 이 구조체는 DataStore, EventBus, 로그 시스템 간에 전달됩니다.
 *
 * 생명주기:
 * 1. 생성: AlarmManager가 이상 감지 시 생성
 * 2. 활성화: AlarmState::ACTIVE
 * 3. 확인: 조작자가 인지 → AlarmState::ACKNOWLEDGED
 * 4. 해결: 원인 제거 → AlarmState::RESOLVED
 * 5. 보관: DataStore에 이력 저장
 *
 * @see AlarmSeverity 심각도 레벨
 * @see IAlarmManager Alarm 관리자
 *
 * Feature 016: Pallet Shuttle Control System
 */
struct AlarmDto {
    /**
     * @brief Alarm 고유 ID
     *
     * 형식: "{alarm_code}_{timestamp}"
     * 예시: "E001_1234567890"
     *
     * alarm_code는 설정 파일(alarm-config.yaml)에 정의됨
     */
    std::string alarm_id;

    /**
     * @brief Alarm 코드
     *
     * 예시:
     * - "E001": Emergency stop activated
     * - "E002": Position limit exceeded
     * - "W001": Battery low
     * - "I001": Maintenance due
     *
     * 코드 규칙:
     * - E: Error (CRITICAL)
     * - W: Warning (WARNING)
     * - I: Info (INFO)
     * - 숫자 3자리: 일련번호
     */
    std::string alarm_code;

    /**
     * @brief Alarm 이름
     *
     * 사람이 읽을 수 있는 alarm 설명입니다.
     * 예: "Emergency Stop Activated"
     */
    std::string alarm_name;

    /**
     * @brief 심각도
     *
     * 이 alarm의 심각도 레벨입니다.
     * BehaviorArbiter는 이 값에 따라 대응 전략을 결정합니다.
     */
    AlarmSeverity severity;

    /**
     * @brief 현재 상태
     *
     * Alarm의 처리 상태입니다.
     */
    AlarmState state;

    /**
     * @brief Alarm 발생 시각
     */
    std::chrono::system_clock::time_point timestamp;

    /**
     * @brief 상세 메시지 (선택적)
     *
     * Alarm에 대한 추가 정보입니다.
     * 예: "X-axis position: 10500mm, limit: 10000mm"
     */
    std::optional<std::string> details;

    /**
     * @brief 소스 (발생 위치)
     *
     * Alarm을 발생시킨 컴포넌트 또는 센서입니다.
     * 예: "pallet_shuttle.x_axis", "safety_controller", "battery_monitor"
     */
    std::string source;

    /**
     * @brief 재발 횟수
     *
     * 짧은 시간에 동일 alarm이 반복 발생한 횟수입니다.
     * 재발이 빈번하면 심각도 상향 조정에 사용됩니다.
     *
     * @see AlarmManager::shouldEscalateSeverity()
     */
    uint32_t recurrence_count{0};

    /**
     * @brief 마지막 재발 시각 (선택적)
     *
     * 가장 최근에 이 alarm이 재발한 시각입니다.
     */
    std::optional<std::chrono::system_clock::time_point> last_recurrence;

    /**
     * @brief 확인 시각 (선택적)
     *
     * 조작자가 이 alarm을 확인한 시각입니다.
     * state == ACKNOWLEDGED 또는 RESOLVED일 때만 유효합니다.
     */
    std::optional<std::chrono::system_clock::time_point> acknowledged_time;

    /**
     * @brief 확인자 ID (선택적)
     *
     * Alarm을 확인한 조작자의 ID입니다.
     * 예: "operator1", "system", "auto_reset"
     */
    std::optional<std::string> acknowledged_by;

    /**
     * @brief 해결 시각 (선택적)
     *
     * Alarm이 해결된 시각입니다.
     * state == RESOLVED일 때만 유효합니다.
     */
    std::optional<std::chrono::system_clock::time_point> resolved_time;

    /**
     * @brief 기본 생성자
     */
    AlarmDto() = default;

    /**
     * @brief 편의 생성자
     *
     * @param code Alarm 코드
     * @param name Alarm 이름
     * @param sev 심각도
     * @param src 소스
     */
    AlarmDto(
        std::string code,
        std::string name,
        AlarmSeverity sev,
        std::string src)
        : alarm_code(std::move(code))
        , alarm_name(std::move(name))
        , severity(sev)
        , state(AlarmState::ACTIVE)
        , timestamp(std::chrono::system_clock::now())
        , source(std::move(src))
    {
        // alarm_id 생성: code_timestamp
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count();
        alarm_id = alarm_code + "_" + std::to_string(ms);
    }

    /**
     * @brief Alarm 경과 시간 계산
     *
     * @return 발생 시각부터 현재까지의 시간 (ms)
     */
    [[nodiscard]] int64_t getElapsedMs() const {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now - timestamp).count();
    }

    /**
     * @brief 활성 상태인지 확인
     *
     * @return true ACTIVE 상태
     * @return false ACKNOWLEDGED 또는 RESOLVED 상태
     */
    [[nodiscard]] bool isActive() const {
        return state == AlarmState::ACTIVE;
    }

    /**
     * @brief 해결되었는지 확인
     *
     * @return true RESOLVED 상태
     * @return false ACTIVE 또는 ACKNOWLEDGED 상태
     */
    [[nodiscard]] bool isResolved() const {
        return state == AlarmState::RESOLVED;
    }
};

/**
 * @brief AlarmState를 문자열로 변환
 */
inline std::string toString(AlarmState state) {
    switch (state) {
        case AlarmState::ACTIVE:       return "ACTIVE";
        case AlarmState::ACKNOWLEDGED: return "ACKNOWLEDGED";
        case AlarmState::RESOLVED:     return "RESOLVED";
        default:                       return "UNKNOWN";
    }
}

} // namespace mxrc::core::alarm
