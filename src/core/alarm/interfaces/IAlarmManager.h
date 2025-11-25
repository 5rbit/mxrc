#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "../dto/AlarmDto.h"
#include "../dto/AlarmSeverity.h"

namespace mxrc::core::alarm {

/**
 * @brief 범용 Alarm 관리자 인터페이스
 *
 * 로봇 시스템의 모든 alarm을 중앙에서 관리합니다.
 *
 * 핵심 책임:
 * 1. Alarm 생성, 조회, 리셋
 * 2. 심각도 기반 우선순위 결정
 * 3. 재발 빈도 추적 및 심각도 상향
 * 4. DataStore 통합 (alarm 이력 저장)
 * 5. EventBus 통합 (alarm 이벤트 발행)
 *
 * 설계 원칙:
 * - **중앙 집중화**: 모든 alarm을 한 곳에서 관리
 * - **확장 가능**: 새로운 alarm 타입 추가 용이
 * - **추적 가능**: 모든 alarm 이력 보관
 * - **실시간 대응**: Critical alarm < 100ms 처리
 *
 * 통합 컴포넌트:
 * - IAlarmConfiguration: 설정 파일 파싱
 * - DataStore: Alarm 이력 저장
 * - EventBus: Alarm 이벤트 발행
 * - BehaviorArbiter: Alarm에 따른 행동 결정
 *
 * @see IAlarmConfiguration Alarm 설정
 * @see AlarmDto Alarm 데이터
 * @see research.md "3.4 Alarm 시스템 설계" 섹션
 *
 * Feature 016: Pallet Shuttle Control System
 */
class IAlarmManager {
public:
    virtual ~IAlarmManager() = default;

    /**
     * @brief Alarm 생성
     *
     * 새로운 alarm을 시스템에 등록합니다.
     * 설정 파일에 정의된 alarm_code를 기반으로 AlarmDto를 생성합니다.
     *
     * 자동 처리:
     * - DataStore에 저장 (키: "alarm/{alarm_id}")
     * - EventBus에 AlarmEvent 발행
     * - 재발 여부 확인 (동일 코드의 최근 alarm 검색)
     * - 필요 시 심각도 자동 상향
     *
     * @param alarm_code 설정에 정의된 alarm 코드 (예: "E001")
     * @param source Alarm 발생 소스 (예: "pallet_shuttle.x_axis")
     * @param details 상세 메시지 (선택적)
     * @return 생성된 AlarmDto, 생성 실패 시 std::nullopt
     *
     * @throws std::invalid_argument alarm_code가 설정에 없는 경우
     */
    virtual std::optional<AlarmDto> raiseAlarm(
        const std::string& alarm_code,
        const std::string& source,
        const std::optional<std::string>& details = std::nullopt) = 0;

    /**
     * @brief Alarm 조회 (ID로)
     *
     * @param alarm_id Alarm 고유 ID
     * @return AlarmDto, 해당 ID의 alarm이 없으면 std::nullopt
     */
    virtual std::optional<AlarmDto> getAlarm(const std::string& alarm_id) const = 0;

    /**
     * @brief 활성 Alarm 목록 조회
     *
     * 현재 ACTIVE 상태인 모든 alarm을 반환합니다.
     * 심각도 순으로 정렬됩니다 (CRITICAL → WARNING → INFO).
     *
     * @return std::vector 활성 alarm 목록
     */
    virtual std::vector<AlarmDto> getActiveAlarms() const = 0;

    /**
     * @brief 특정 심각도의 활성 Alarm 목록 조회
     *
     * @param severity 조회할 심각도
     * @return std::vector 해당 심각도의 활성 alarm 목록
     */
    virtual std::vector<AlarmDto> getActiveAlarmsBySeverity(
        AlarmSeverity severity) const = 0;

    /**
     * @brief Alarm 이력 조회
     *
     * 과거에 발생한 모든 alarm을 조회합니다.
     * 디버깅 및 분석에 사용됩니다.
     *
     * @param limit 최대 조회 개수 (0 = 무제한)
     * @return std::vector Alarm 이력 (최신 순)
     */
    virtual std::vector<AlarmDto> getAlarmHistory(size_t limit = 100) const = 0;

    /**
     * @brief Alarm 확인 (Acknowledge)
     *
     * Alarm을 "확인됨" 상태로 전환합니다.
     * 조작자가 alarm을 인지했음을 표시합니다.
     *
     * @param alarm_id Alarm ID
     * @param acknowledged_by 확인자 ID
     * @return true 확인 성공
     * @return false 확인 실패 (alarm이 없거나 이미 확인됨)
     */
    virtual bool acknowledgeAlarm(
        const std::string& alarm_id,
        const std::string& acknowledged_by) = 0;

    /**
     * @brief Alarm 해결 (Resolve)
     *
     * Alarm을 "해결됨" 상태로 전환합니다.
     * 원인이 제거되었음을 표시합니다.
     *
     * @param alarm_id Alarm ID
     * @return true 해결 성공
     * @return false 해결 실패 (alarm이 없거나 이미 해결됨)
     */
    virtual bool resolveAlarm(const std::string& alarm_id) = 0;

    /**
     * @brief 모든 Alarm 리셋
     *
     * 모든 활성 alarm을 해결 상태로 전환합니다.
     * Emergency stop 해제 후 사용됩니다.
     *
     * @return size_t 리셋된 alarm 개수
     */
    virtual size_t resetAllAlarms() = 0;

    /**
     * @brief Critical Alarm 존재 여부
     *
     * 현재 CRITICAL 심각도의 활성 alarm이 있는지 확인합니다.
     * BehaviorArbiter가 FAULT 모드 진입 여부를 결정하는 데 사용합니다.
     *
     * @return true Critical alarm이 하나 이상 존재
     * @return false Critical alarm 없음
     */
    virtual bool hasCriticalAlarm() const = 0;

    /**
     * @brief Alarm 통계 조회
     *
     * @return 총 발생 횟수, 활성 개수, 해결 개수 등의 통계
     */
    struct AlarmStats {
        size_t total_raised{0};      ///< 총 발생 횟수
        size_t active_count{0};      ///< 현재 활성 개수
        size_t resolved_count{0};    ///< 해결된 개수
        size_t critical_count{0};    ///< Critical 개수
        size_t warning_count{0};     ///< Warning 개수
        size_t info_count{0};        ///< Info 개수
    };

    /**
     * @brief Alarm 통계 조회
     *
     * @return AlarmStats 통계 정보
     */
    virtual AlarmStats getStatistics() const = 0;
};

} // namespace mxrc::core::alarm
