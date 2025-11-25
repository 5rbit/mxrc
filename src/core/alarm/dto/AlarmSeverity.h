#pragma once

#include <cstdint>
#include <string>

namespace mxrc::core::alarm {

/**
 * @brief Alarm 심각도 레벨 (3단계)
 *
 * IEC 62061 (안전 관련 제어 시스템) 표준을 참고한 심각도 분류입니다.
 * 각 레벨에 따라 다른 대응 전략이 적용됩니다.
 *
 * 심각도별 대응:
 * - CRITICAL: 즉시 작업 중단, 안전 상태로 전환
 * - WARNING: 현재 작업 완료 후 대응, 조치 필요
 * - INFO: 로그만 기록, 즉각 조치 불필요
 *
 * @see research.md "3.4 Alarm 시스템 설계" 섹션
 *
 * Feature 016: Pallet Shuttle Control System
 */
enum class AlarmSeverity : uint8_t {
    /**
     * @brief 심각 (Critical)
     *
     * 즉시 조치가 필요한 안전 또는 시스템 위험 상황입니다.
     *
     * 발생 조건 예시:
     * - Emergency stop 버튼 작동
     * - Position limit switch 초과
     * - Motor overcurrent (과전류)
     * - Communication timeout (통신 단절)
     * - Hardware fault (하드웨어 고장)
     *
     * 자동 대응:
     * - Priority::EMERGENCY_STOP 행동 요청 생성
     * - ControlMode를 FAULT로 전환
     * - 모든 작업 즉시 중단
     * - DataStore에 상태 기록
     * - EventBus에 AlarmEvent 발행
     *
     * 복구:
     * - 원인 해결 후 수동 리셋 필요
     * - resetErrors() 호출
     *
     * 응답 시간: < 100ms
     */
    CRITICAL = 0,

    /**
     * @brief 경고 (Warning)
     *
     * 주의가 필요하지만 즉시 위험하지는 않은 상황입니다.
     *
     * 발생 조건 예시:
     * - 배터리 잔량 < 20%
     * - 센서 값 범위 이탈 (허용 오차 내)
     * - 작업 완료 시간 지연
     * - 부품 마모도 임계값 근접
     *
     * 자동 대응:
     * - 현재 작업 완료 후 대응
     * - Priority::SAFETY_ISSUE 행동 요청 생성
     * - 조작자에게 알림
     * - DataStore에 경고 누적
     *
     * 복구:
     * - 자동 복구 가능 (조건 해소 시)
     * - 또는 예방 조치 후 리셋
     *
     * 재발 추적:
     * - 동일 경고가 짧은 시간에 반복되면 CRITICAL로 상향
     */
    WARNING = 1,

    /**
     * @brief 정보 (Info)
     *
     * 정상 동작 중 발생하는 비정상 이벤트나 상태 변화입니다.
     *
     * 발생 조건 예시:
     * - 예방 정비 시기 도래
     * - 센서 캘리브레이션 권장
     * - 작업 통계 임계값 도달
     * - 주기적 점검 결과
     *
     * 자동 대응:
     * - 로그만 기록
     * - 즉각적인 작업 중단 없음
     * - 유지보수 스케줄에 추가 가능
     *
     * 복구:
     * - 자동 소멸 (일정 시간 경과 후)
     * - 또는 정비 완료 후 확인
     */
    INFO = 2
};

/**
 * @brief AlarmSeverity를 문자열로 변환
 *
 * @param severity 심각도 레벨
 * @return std::string 심각도 이름 (예: "CRITICAL")
 */
inline std::string toString(AlarmSeverity severity) {
    switch (severity) {
        case AlarmSeverity::CRITICAL: return "CRITICAL";
        case AlarmSeverity::WARNING:  return "WARNING";
        case AlarmSeverity::INFO:     return "INFO";
        default:                      return "UNKNOWN";
    }
}

/**
 * @brief 문자열을 AlarmSeverity로 변환
 *
 * @param str 심각도 문자열
 * @return AlarmSeverity 대응하는 enum 값, 매칭 실패 시 INFO
 */
inline AlarmSeverity fromString(const std::string& str) {
    if (str == "CRITICAL") return AlarmSeverity::CRITICAL;
    if (str == "WARNING")  return AlarmSeverity::WARNING;
    if (str == "INFO")     return AlarmSeverity::INFO;
    return AlarmSeverity::INFO;  // 기본값
}

/**
 * @brief 심각도가 즉시 조치 필요한지 확인
 *
 * @param severity 심각도 레벨
 * @return true CRITICAL (즉시 조치 필요)
 * @return false WARNING 또는 INFO
 */
inline bool requiresImmediateAction(AlarmSeverity severity) {
    return severity == AlarmSeverity::CRITICAL;
}

} // namespace mxrc::core::alarm
