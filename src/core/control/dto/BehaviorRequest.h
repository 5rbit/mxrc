#pragma once

#include <string>
#include <memory>
#include <chrono>
#include "Priority.h"
#include "../../task/interfaces/ITask.h"

namespace mxrc::core::control {

/**
 * @brief 행동 요청 구조체
 *
 * BehaviorArbiter에 제출되는 행동 요청을 표현합니다.
 * 각 요청은 우선순위와 함께 실행할 Task를 포함합니다.
 *
 * 생명주기:
 * 1. 생성: 외부 시스템 또는 사용자가 행동 요청
 * 2. 큐잉: BehaviorPriorityQueue에 우선순위별로 저장
 * 3. 선택: selectNextBehavior()에 의해 선택됨
 * 4. 실행: TaskExecutor에 전달되어 실행
 * 5. 완료: Task 완료 후 소멸
 *
 * @see IBehaviorArbiter::requestBehavior()
 * @see Priority 우선순위 레벨
 *
 * Feature 016: Pallet Shuttle Control System
 */
struct BehaviorRequest {
    /**
     * @brief 행동 고유 ID
     *
     * 형식: "{behavior_type}_{timestamp}"
     * 예시: "pallet_transport_1234567890"
     *
     * 추적 및 디버깅에 사용됩니다.
     */
    std::string behavior_id;

    /**
     * @brief 우선순위
     *
     * 이 행동의 실행 우선순위입니다.
     * BehaviorArbiter는 이 값에 따라 실행 순서를 결정합니다.
     *
     * @see Priority enum
     */
    Priority priority;

    /**
     * @brief 실행할 Task
     *
     * 이 행동을 수행하기 위한 Task 객체입니다.
     * nullptr일 수 없으며, TaskExecutor에 전달됩니다.
     *
     * Task 타입 예시:
     * - PalletTransportTask
     * - SafetyCheckTask
     * - RecoveryTask
     */
    std::shared_ptr<mxrc::core::task::ITask> task;

    /**
     * @brief 요청 생성 시각
     *
     * 성능 측정 및 타임아웃 관리에 사용됩니다.
     * 예: "요청부터 실행까지 지연 시간"
     */
    std::chrono::steady_clock::time_point timestamp;

    /**
     * @brief 요청자 ID (선택적)
     *
     * 이 행동을 요청한 주체의 식별자입니다.
     * 추적 및 감사(audit)에 사용됩니다.
     *
     * 예시:
     * - "user:operator1"
     * - "system:alarm_manager"
     * - "scheduler:periodic_check"
     */
    std::string requester_id;

    /**
     * @brief 취소 가능 여부
     *
     * true: 이 행동은 다른 높은 우선순위 행동에 의해 중단될 수 있음
     * false: 완료될 때까지 중단 불가능 (safety-critical)
     *
     * 기본값: true
     */
    bool cancellable{true};

    /**
     * @brief 타임아웃 (선택적)
     *
     * 이 시간 내에 완료되지 않으면 자동 취소됩니다.
     * 0ms는 무한대를 의미합니다.
     *
     * 기본값: 0ms (타임아웃 없음)
     */
    std::chrono::milliseconds timeout{0};

    /**
     * @brief 기본 생성자
     */
    BehaviorRequest() = default;

    /**
     * @brief 편의 생성자
     *
     * @param id 행동 ID
     * @param prio 우선순위
     * @param t 실행할 Task
     * @param req 요청자 ID (선택)
     */
    BehaviorRequest(
        std::string id,
        Priority prio,
        std::shared_ptr<mxrc::core::task::ITask> t,
        std::string req = "")
        : behavior_id(std::move(id))
        , priority(prio)
        , task(std::move(t))
        , timestamp(std::chrono::steady_clock::now())
        , requester_id(std::move(req))
    {
    }

    /**
     * @brief 요청 경과 시간 계산
     *
     * @return 요청 생성부터 현재까지의 시간 (ms)
     */
    [[nodiscard]] int64_t getElapsedMs() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now - timestamp).count();
    }

    /**
     * @brief 타임아웃 확인
     *
     * @return true 타임아웃 발생
     * @return false 타임아웃 없거나 아직 유효
     */
    [[nodiscard]] bool isTimedOut() const {
        if (timeout.count() == 0) {
            return false;  // 타임아웃 설정 안됨
        }
        return getElapsedMs() >= timeout.count();
    }
};

} // namespace mxrc::core::control
