#pragma once

#include <memory>
#include <string>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include "../interfaces/IBehaviorArbiter.h"
#include "../interfaces/IAlarmManager.h"
#include "BehaviorPriorityQueue.h"
#include "../../task/interfaces/ITask.h"

namespace mxrc::core::control {

/**
 * @brief Behavior Arbitration 구현 클래스
 *
 * 100ms 주기로 tick()이 호출되며, 우선순위 기반으로 다음 실행할 behavior를 선택합니다.
 *
 * 주요 기능:
 * - 우선순위 기반 행동 선택
 * - Preemption (선점) 처리
 * - ControlMode 상태 전이
 * - Task pause/resume/cancel 관리
 *
 * Thread-safety:
 * - requestBehavior(): 여러 스레드에서 호출 가능 (lock-free queue)
 * - tick(): 단일 스레드에서 호출 (BehaviorArbiter 실행 스레드)
 * - 기타 조회 메서드: mutex로 보호
 *
 * @see research.md "3.3 BehaviorArbiter Algorithm" 섹션
 *
 * Feature 016: Pallet Shuttle Control System
 */
class BehaviorArbiter : public IBehaviorArbiter {
public:
    /**
     * @brief 생성자
     *
     * @param alarm_manager Alarm 관리자 (Critical Alarm 체크용)
     */
    explicit BehaviorArbiter(std::shared_ptr<alarm::IAlarmManager> alarm_manager);

    ~BehaviorArbiter() override = default;

    // IBehaviorArbiter 인터페이스 구현
    bool requestBehavior(const BehaviorRequest& request) override;

    void tick() override;

    ControlMode getCurrentMode() const override;

    std::string getCurrentTaskId() const override;

    size_t getPendingBehaviorCount() const override;

    bool transitionTo(ControlMode newMode) override;

    void clearPendingBehaviors() override;

    bool cancelBehavior(const std::string& behavior_id) override;

    bool pause() override;

    bool resume() override;

private:
    /**
     * @brief 다음 실행할 behavior 선택
     *
     * 우선순위 큐에서 가장 높은 우선순위의 behavior를 가져옵니다.
     *
     * @return 다음 behavior, 없으면 std::nullopt
     */
    std::optional<BehaviorRequest> selectNextBehavior();

    /**
     * @brief 선점(preemption) 필요 여부 확인
     *
     * 새로운 behavior가 현재 실행 중인 behavior보다 우선순위가 높은지 확인합니다.
     *
     * @param new_behavior 새로운 behavior
     * @return true 선점 필요
     * @return false 선점 불필요
     */
    bool shouldPreempt(const BehaviorRequest& new_behavior);

    /**
     * @brief 선점(preemption) 처리
     *
     * 현재 실행 중인 behavior를 pause 또는 cancel하고 새로운 behavior를 실행합니다.
     *
     * @param new_behavior 새로운 behavior
     */
    void handlePreemption(const BehaviorRequest& new_behavior);

    /**
     * @brief 현재 실행 중인 task 중단
     *
     * @param method "pause" 또는 "cancel"
     */
    void stopCurrentTask(const std::string& method);

    /**
     * @brief 새로운 task 시작
     *
     * @param behavior 실행할 behavior
     */
    void startTask(const BehaviorRequest& behavior);

    /**
     * @brief ControlMode 전이 유효성 검증
     *
     * @param from 현재 모드
     * @param to 전환할 모드
     * @return true 전이 가능
     * @return false 전이 불가
     */
    bool isValidModeTransition(ControlMode from, ControlMode to);

    /**
     * @brief Critical Alarm 발생 시 FAULT 모드 전환
     */
    void checkCriticalAlarms();

    /**
     * @brief Timeout된 behavior 제거
     */
    void removeTimedOutBehaviors();

    // Alarm 관리자 (Critical Alarm 확인용)
    std::shared_ptr<alarm::IAlarmManager> alarm_manager_;

    // 현재 ControlMode
    std::atomic<ControlMode> current_mode_{ControlMode::STANDBY};

    // Priority Queue (lock-free)
    BehaviorPriorityQueue pending_behaviors_;

    // 현재 실행 중인 behavior
    std::optional<BehaviorRequest> current_behavior_;

    // 일시 중지된 behaviors (behavior_id → BehaviorRequest)
    std::unordered_map<std::string, BehaviorRequest> suspended_behaviors_;

    // 통계
    struct Statistics {
        uint64_t total_requests{0};
        uint64_t preemptions{0};
        uint64_t mode_transitions{0};
        uint64_t timeouts{0};
    } stats_;

    // Thread safety (조회 메서드용)
    mutable std::mutex mutex_;

    // 일시 정지 상태
    std::atomic<bool> paused_{false};
};

} // namespace mxrc::core::control
