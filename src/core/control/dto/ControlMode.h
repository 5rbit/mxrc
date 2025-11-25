#pragma once

#include <cstdint>
#include <string>

namespace mxrc::core::control {

/**
 * @brief 로봇 제어 모드 (9단계 상태 머신)
 *
 * IEC 61131-3 PLC 표준 및 산업용 로봇 제어 패턴을 따르는 상태 머신입니다.
 * 각 모드 간 전환은 엄격하게 검증되어야 합니다.
 *
 * 상태 전환 규칙:
 * - BOOT → INIT → STANDBY (시작 시퀀스)
 * - STANDBY ↔ MANUAL ↔ READY ↔ AUTO (정상 운영)
 * - 모든 모드 → FAULT (에러 발생 시)
 * - FAULT → STANDBY (에러 리셋 후)
 *
 * @see research.md "3.2 ControlMode 상태 머신" 섹션
 *
 * Feature 016: Pallet Shuttle Control System
 */
enum class ControlMode : uint8_t {
    /**
     * @brief 부팅 중
     *
     * 진입 조건:
     * - 시스템 전원 ON
     * - 프로세스 시작
     *
     * 허용 동작:
     * - 하드웨어 자가 진단
     * - 통신 연결 확인
     * - 설정 파일 로드
     *
     * 다음 상태:
     * - INIT (부팅 성공)
     * - FAULT (부팅 실패)
     *
     * 평균 체류 시간: < 5초
     */
    BOOT = 0,

    /**
     * @brief 초기화 중
     *
     * 진입 조건:
     * - BOOT 완료
     *
     * 허용 동작:
     * - 액추에이터 홈 위치 복귀
     * - 센서 캘리브레이션
     * - 기준 좌표계 설정
     * - 초기 상태 확인
     *
     * 다음 상태:
     * - STANDBY (초기화 성공)
     * - FAULT (초기화 실패)
     *
     * 평균 체류 시간: 10-30초
     */
    INIT = 1,

    /**
     * @brief 대기 (준비 완료)
     *
     * 진입 조건:
     * - INIT 완료
     * - FAULT에서 복구
     * - 작업 완료 후 자동 복귀
     *
     * 허용 동작:
     * - 작업 수신 대기
     * - 상태 모니터링
     * - 유지보수 작업 수행 가능
     *
     * 다음 상태:
     * - MANUAL (수동 모드 전환)
     * - AUTO (자동 작업 시작)
     * - MAINT (유지보수 모드)
     * - FAULT (에러 발생)
     *
     * 에너지 소비: 최소 (대기 전력만 사용)
     */
    STANDBY = 2,

    /**
     * @brief 수동 제어 모드
     *
     * 진입 조건:
     * - 조작자 수동 모드 요청
     * - 테스트/디버깅 목적
     *
     * 허용 동작:
     * - 조이스틱/버튼으로 직접 제어
     * - 단일 액추에이터 개별 조작
     * - 안전 한계 내에서 자유 이동
     *
     * 다음 상태:
     * - STANDBY (수동 모드 종료)
     * - FAULT (안전 위반)
     *
     * 안전 제약: 속도 제한 50%
     */
    MANUAL = 3,

    /**
     * @brief 자동 모드 준비 완료
     *
     * 진입 조건:
     * - STANDBY에서 자동 모드 활성화
     * - 모든 안전 조건 만족
     *
     * 허용 동작:
     * - Task 수신 및 큐잉
     * - 사전 점검(pre-check) 실행
     * - Task 시작 대기
     *
     * 다음 상태:
     * - AUTO (Task 시작)
     * - STANDBY (자동 모드 비활성화)
     * - FAULT (에러 발생)
     */
    READY = 4,

    /**
     * @brief 자동 작업 실행 중
     *
     * 진입 조건:
     * - READY에서 Task 시작
     *
     * 허용 동작:
     * - Task/Sequence/Action 자동 실행
     * - BehaviorArbiter 의사 결정 수행
     * - 우선순위 기반 작업 전환
     *
     * 다음 상태:
     * - READY (Task 완료, 다음 Task 대기)
     * - STANDBY (모든 작업 완료)
     * - FAULT (에러 발생)
     * - CHARGING (배터리 부족 시)
     *
     * 정상 운영 모드: 대부분의 시간을 이 모드에서 보냄
     */
    AUTO = 5,

    /**
     * @brief 고장/에러 상태
     *
     * 진입 조건:
     * - Critical Alarm 발생
     * - Safety violation
     * - Emergency stop 신호
     * - 하드웨어 고장
     *
     * 허용 동작:
     * - 모든 작업 즉시 중단
     * - 안전 상태로 전환 (브레이크 작동)
     * - 에러 로그 기록
     * - 복구 가능성 진단
     *
     * 다음 상태:
     * - STANDBY (에러 해결 및 리셋 후)
     *
     * 주의: FAULT 상태에서는 resetErrors() 호출 후에만 복구 가능
     */
    FAULT = 6,

    /**
     * @brief 유지보수 모드
     *
     * 진입 조건:
     * - 정기 점검 시간
     * - 조작자 유지보수 요청
     * - 이상 징후 감지
     *
     * 허용 동작:
     * - 주기적 안전 점검 (SafetyCheckSequence)
     * - 센서 데이터 수집 및 분석
     * - 부품 마모도 진단
     * - 예방 정비 알림 생성
     *
     * 다음 상태:
     * - STANDBY (유지보수 완료)
     * - FAULT (정비 필요 발견)
     */
    MAINT = 7,

    /**
     * @brief 충전 중
     *
     * 진입 조건:
     * - 배터리 잔량 < 20%
     * - 충전소 도착
     *
     * 허용 동작:
     * - 충전 진행
     * - 충전 상태 모니터링
     * - 긴급 작업 대기 (충전 중단 가능)
     *
     * 다음 상태:
     * - STANDBY (충전 완료)
     * - AUTO (긴급 작업 발생 시 중단 후 복귀)
     *
     * 노트: 팔렛 셔틀이 배터리 구동인 경우에만 사용
     */
    CHARGING = 8
};

/**
 * @brief ControlMode를 문자열로 변환
 *
 * @param mode 제어 모드
 * @return std::string 모드 이름 (예: "AUTO")
 */
inline std::string toString(ControlMode mode) {
    switch (mode) {
        case ControlMode::BOOT:     return "BOOT";
        case ControlMode::INIT:     return "INIT";
        case ControlMode::STANDBY:  return "STANDBY";
        case ControlMode::MANUAL:   return "MANUAL";
        case ControlMode::READY:    return "READY";
        case ControlMode::AUTO:     return "AUTO";
        case ControlMode::FAULT:    return "FAULT";
        case ControlMode::MAINT:    return "MAINT";
        case ControlMode::CHARGING: return "CHARGING";
        default:                    return "UNKNOWN";
    }
}

/**
 * @brief 상태 전환 유효성 검증
 *
 * @param from 현재 모드
 * @param to 전환할 모드
 * @return true 허용된 전환
 * @return false 금지된 전환
 *
 * 허용된 전환 예시:
 * - STANDBY → AUTO (OK)
 * - AUTO → FAULT (OK, 언제든지 가능)
 * - MANUAL → AUTO (NG, STANDBY를 거쳐야 함)
 */
inline bool isValidTransition(ControlMode from, ControlMode to) {
    // FAULT는 어느 모드에서든 진입 가능 (에러 발생 시)
    if (to == ControlMode::FAULT) {
        return true;
    }

    // 각 모드별 허용된 전환
    switch (from) {
        case ControlMode::BOOT:
            return (to == ControlMode::INIT);

        case ControlMode::INIT:
            return (to == ControlMode::STANDBY);

        case ControlMode::STANDBY:
            return (to == ControlMode::MANUAL ||
                    to == ControlMode::READY ||
                    to == ControlMode::AUTO ||
                    to == ControlMode::MAINT);

        case ControlMode::MANUAL:
            return (to == ControlMode::STANDBY);

        case ControlMode::READY:
            return (to == ControlMode::AUTO ||
                    to == ControlMode::STANDBY);

        case ControlMode::AUTO:
            return (to == ControlMode::READY ||
                    to == ControlMode::STANDBY ||
                    to == ControlMode::CHARGING);

        case ControlMode::FAULT:
            return (to == ControlMode::STANDBY);  // 리셋 후 복구

        case ControlMode::MAINT:
            return (to == ControlMode::STANDBY);

        case ControlMode::CHARGING:
            return (to == ControlMode::STANDBY ||
                    to == ControlMode::AUTO);  // 긴급 작업 시 복귀

        default:
            return false;
    }
}

} // namespace mxrc::core::control
