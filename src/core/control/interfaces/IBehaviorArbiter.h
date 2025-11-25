#pragma once

#include <string>
#include <memory>
#include "../dto/Priority.h"
#include "../dto/ControlMode.h"
#include "../dto/BehaviorRequest.h"

namespace mxrc::core::control {

/**
 * @brief 범용 행동 중재자 인터페이스
 *
 * 여러 행동 요청 간의 우선순위를 조율하고, 실행할 행동을 선택하는 의사 결정 컴포넌트입니다.
 * 로봇의 "두뇌" 역할을 하며, 다음 작업을 결정합니다.
 *
 * 핵심 책임:
 * 1. 행동 요청 접수 및 우선순위별 큐잉
 * 2. 현재 상황에서 최적의 행동 선택 (selectNextBehavior)
 * 3. 선점(preemption) 결정 (높은 우선순위 작업 처리)
 * 4. ControlMode 상태 전환 관리
 * 5. TaskExecutor와 통합하여 실제 실행 조율
 *
 * 작동 방식:
 * - 주기적 tick() 호출 (권장: 100ms)
 * - tick마다 우선순위 큐 검사
 * - 새로운 행동이 현재 작업보다 우선순위 높으면 선점
 * - Priority 0-2: 즉시 중단 (cancel/pause)
 * - Priority 3-4: 완료 후 전환
 *
 * 설계 원칙:
 * - **결정론적(Deterministic)**: 동일 입력 → 동일 출력
 * - **예측 가능(Predictable)**: 우선순위 규칙 명확
 * - **확장 가능(Extensible)**: 로봇별 커스터마이징 가능
 * - **테스트 가능(Testable)**: 모든 결정을 검증 가능
 *
 * @see research.md "3.3 BehaviorArbiter 알고리즘" 섹션
 * @see Priority 우선순위 체계
 * @see ControlMode 상태 머신
 *
 * Feature 016: Pallet Shuttle Control System
 */
class IBehaviorArbiter {
public:
    virtual ~IBehaviorArbiter() = default;

    /**
     * @brief 행동 요청 제출
     *
     * 새로운 행동을 우선순위 큐에 추가합니다.
     * 우선순위에 따라 적절한 큐에 삽입됩니다.
     *
     * @param request 행동 요청 (복사됨)
     * @return true 요청 접수 성공
     * @return false 요청 거부 (예: 큐가 가득 참, 유효하지 않은 요청)
     *
     * 스레드 안전성: 이 메서드는 스레드 안전해야 합니다.
     * 예: tbb::concurrent_queue 사용
     *
     * @throws std::invalid_argument request.task가 nullptr인 경우
     */
    virtual bool requestBehavior(const BehaviorRequest& request) = 0;

    /**
     * @brief 주기적 틱 (의사 결정 주기)
     *
     * BehaviorArbiter의 핵심 로직입니다.
     * 매 틱마다 다음을 수행합니다:
     * 1. 우선순위 큐에서 다음 행동 선택
     * 2. 현재 작업과 비교하여 선점 여부 결정
     * 3. 필요시 Task 일시 중지/재개/중단
     * 4. ControlMode 전환
     *
     * 권장 호출 주기: 100ms (10Hz)
     * 최대 실행 시간: < 10ms (틱 주기의 10%)
     *
     * 이 메서드는 RT 스레드에서 호출될 수 있으므로,
     * blocking call을 피해야 합니다.
     *
     * @throws std::runtime_error 심각한 내부 오류 발생 시
     */
    virtual void tick() = 0;

    /**
     * @brief 현재 제어 모드 조회
     *
     * @return ControlMode 현재 로봇의 제어 모드
     */
    virtual ControlMode getCurrentMode() const = 0;

    /**
     * @brief 현재 실행 중인 Task ID 조회
     *
     * @return std::string Task ID, 실행 중인 Task가 없으면 빈 문자열
     */
    virtual std::string getCurrentTaskId() const = 0;

    /**
     * @brief 대기 중인 행동 개수 조회
     *
     * 각 우선순위별 큐의 크기를 합산합니다.
     *
     * @return size_t 대기 중인 행동 총 개수
     */
    virtual size_t getPendingBehaviorCount() const = 0;

    /**
     * @brief ControlMode 전환 요청
     *
     * 로봇의 제어 모드를 변경합니다.
     * 유효하지 않은 전환은 거부됩니다.
     *
     * @param newMode 전환할 모드
     * @return true 전환 성공
     * @return false 전환 실패 (유효하지 않은 전환, 안전 조건 미충족 등)
     *
     * @see ControlMode::isValidTransition()
     *
     * 예시:
     * - STANDBY → AUTO: OK
     * - AUTO → STANDBY: OK
     * - MANUAL → AUTO: NG (STANDBY를 거쳐야 함)
     */
    virtual bool transitionTo(ControlMode newMode) = 0;

    /**
     * @brief 모든 대기 중인 행동 취소
     *
     * 우선순위 큐를 비웁니다.
     * 현재 실행 중인 Task는 영향받지 않습니다.
     *
     * 사용 사례:
     * - Emergency stop 처리
     * - 시스템 셧다운
     * - 작업 큐 초기화
     */
    virtual void clearPendingBehaviors() = 0;

    /**
     * @brief 특정 행동 취소
     *
     * behavior_id로 식별되는 대기 중인 행동을 취소합니다.
     * 이미 실행 중이면 Task를 중단합니다.
     *
     * @param behavior_id 취소할 행동의 ID
     * @return true 취소 성공
     * @return false 해당 ID의 행동을 찾을 수 없음
     */
    virtual bool cancelBehavior(const std::string& behavior_id) = 0;

    /**
     * @brief 일시 정지 (Pause)
     *
     * 현재 실행 중인 Task를 일시 중지합니다.
     * 중단 가능한 지점(pause point)에서 안전하게 중단됩니다.
     *
     * @return true 일시 중지 성공
     * @return false 일시 중지 실패 (실행 중인 Task 없음, pause 불가능 등)
     *
     * @see Task::pause()
     */
    virtual bool pause() = 0;

    /**
     * @brief 재개 (Resume)
     *
     * 일시 중지된 Task를 재개합니다.
     *
     * @return true 재개 성공
     * @return false 재개 실패 (일시 중지된 Task 없음 등)
     *
     * @see Task::resume()
     */
    virtual bool resume() = 0;
};

} // namespace mxrc::core::control
