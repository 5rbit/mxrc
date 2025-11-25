#pragma once

#include <cstdint>
#include <string>

namespace mxrc::core::control {

/**
 * @brief 행동 우선순위 레벨 (5단계)
 *
 * BehaviorArbiter의 의사 결정에 사용되는 우선순위 체계입니다.
 * 높은 숫자일수록 낮은 우선순위를 의미합니다 (0이 최우선).
 *
 * 우선순위 정책:
 * - Priority 0-2 (EMERGENCY_STOP, SAFETY_ISSUE, URGENT_TASK): 현재 작업을 선점(preempt) 가능
 * - Priority 3-4 (NORMAL_TASK, MAINTENANCE): 현재 작업 완료 후 처리
 *
 * Starvation 방지:
 * - 낮은 우선순위 작업은 높은 우선순위 작업이 완료된 후 반드시 처리됨
 * - 동일 우선순위 내에서는 FIFO 순서 보장
 *
 * @see research.md "3.1.2 우선순위 체계" 섹션
 *
 * Feature 016: Pallet Shuttle Control System
 */
enum class Priority : uint8_t {
    /**
     * @brief 비상 정지 (최우선)
     *
     * 사용 사례:
     * - Emergency stop 버튼 감지
     * - Critical safety violation
     * - Alarm severity CRITICAL 발생
     *
     * 동작:
     * - 현재 작업 즉시 중단 (Task.cancel())
     * - ControlMode를 FAULT로 전환
     * - 모든 액추에이터 정지
     *
     * 응답 시간: < 100ms
     */
    EMERGENCY_STOP = 0,

    /**
     * @brief 안전 이슈
     *
     * 사용 사례:
     * - Position limit switch 감지
     * - Overload 경고
     * - Alarm severity WARNING 발생
     *
     * 동작:
     * - 현재 작업 중단 (필요 시 pause/resume 지원)
     * - 안전 점검 Sequence 실행
     * - 문제 해결 후 재개 가능
     */
    SAFETY_ISSUE = 1,

    /**
     * @brief 긴급 작업
     *
     * 사용 사례:
     * - 우선 처리 필요한 팔렛 운반
     * - 배터리 부족 시 충전소 이동
     * - Time-critical operation
     *
     * 동작:
     * - 현재 NORMAL_TASK를 일시 중지 (Task.pause())
     * - URGENT_TASK 완료 후 NORMAL_TASK 재개 (Task.resume())
     */
    URGENT_TASK = 2,

    /**
     * @brief 일반 작업 (기본값)
     *
     * 사용 사례:
     * - 표준 팔렛 운반 작업
     * - 정규 생산 작업
     * - 사용자 요청 작업
     *
     * 동작:
     * - 선점 불가능 (완료될 때까지 실행)
     * - TaskQueue의 작업 순서대로 처리
     */
    NORMAL_TASK = 3,

    /**
     * @brief 유지보수 및 점검
     *
     * 사용 사례:
     * - 주기적 안전 점검 (SafetyCheckSequence)
     * - 센서 캘리브레이션
     * - 예방 정비
     *
     * 동작:
     * - 유휴 시간(idle)에만 실행
     * - 다른 작업이 들어오면 중단 가능
     * - 시스템 부하가 낮을 때 백그라운드 실행
     */
    MAINTENANCE = 4
};

/**
 * @brief Priority를 문자열로 변환
 *
 * @param priority 우선순위 레벨
 * @return std::string 우선순위 이름 (예: "EMERGENCY_STOP")
 */
inline std::string toString(Priority priority) {
    switch (priority) {
        case Priority::EMERGENCY_STOP: return "EMERGENCY_STOP";
        case Priority::SAFETY_ISSUE:   return "SAFETY_ISSUE";
        case Priority::URGENT_TASK:    return "URGENT_TASK";
        case Priority::NORMAL_TASK:    return "NORMAL_TASK";
        case Priority::MAINTENANCE:    return "MAINTENANCE";
        default:                       return "UNKNOWN";
    }
}

/**
 * @brief 우선순위가 현재 작업을 선점 가능한지 확인
 *
 * @param priority 확인할 우선순위
 * @return true Priority 0-2 (선점 가능)
 * @return false Priority 3-4 (선점 불가능)
 */
inline bool canPreempt(Priority priority) {
    return priority <= Priority::URGENT_TASK;
}

} // namespace mxrc::core::control
