#pragma once

#include <memory>
#include "../../task/interfaces/ITask.h"

namespace mxrc::core::control {

/**
 * @brief 범용 로봇 컨트롤러 인터페이스
 *
 * 모든 로봇 제어기가 구현해야 하는 최상위 인터페이스입니다.
 * MXRC의 "어떤 로봇도 제어할 수 있는 범용 컨트롤러" 철학을 구현합니다.
 *
 * 주요 책임:
 * - 로봇 생명주기 관리 (start, stop)
 * - Task 제출 및 실행 관리
 * - 하위 컴포넌트(BehaviorArbiter, TaskQueue) 조율
 *
 * 구현 예시:
 * - PalletShuttleController: 팔렛 셔틀 로봇 제어
 * - AGVController: 자율 주행 차량 제어
 * - RoboticArmController: 로봇 팔 제어
 *
 * @see IBehaviorArbiter 행동 의사 결정 인터페이스
 * @see ITaskQueue 작업 큐 관리 인터페이스
 *
 * Feature 016: Pallet Shuttle Control System
 */
class IRobotController {
public:
    virtual ~IRobotController() = default;

    /**
     * @brief 로봇 컨트롤러 시작
     *
     * 로봇의 초기화를 수행하고 작업 수신 대기 상태로 전환합니다.
     * 이 메서드는 멱등성(idempotent)을 가져야 하며, 여러 번 호출해도 안전해야 합니다.
     *
     * @throws std::runtime_error 초기화 실패 시
     */
    virtual void start() = 0;

    /**
     * @brief 로봇 컨트롤러 정지
     *
     * 진행 중인 작업을 안전하게 중단하고 리소스를 정리합니다.
     * 현재 실행 중인 Task는 완료되거나 일시 중지될 수 있습니다.
     *
     * @throws std::runtime_error 정지 실패 시
     */
    virtual void stop() = 0;

    /**
     * @brief Task 제출
     *
     * 새로운 Task를 작업 큐에 추가합니다.
     * Task는 우선순위에 따라 BehaviorArbiter에 의해 스케줄링됩니다.
     *
     * @param task 실행할 Task 객체 (nullptr 불가)
     * @throws std::invalid_argument task가 nullptr인 경우
     * @throws std::runtime_error 큐가 가득 찬 경우
     *
     * 스레드 안전성: 이 메서드는 스레드 안전해야 합니다.
     */
    virtual void submitTask(std::shared_ptr<mxrc::core::task::ITask> task) = 0;

    /**
     * @brief 로봇 상태 조회
     *
     * @return true 로봇이 동작 중(running) 상태
     * @return false 로봇이 정지(stopped) 상태
     */
    virtual bool isRunning() const = 0;

    /**
     * @brief 긴급 정지 (Emergency Stop)
     *
     * 모든 작업을 즉시 중단하고 로봇을 안전 상태로 전환합니다.
     * 이 메서드는 safety-critical하며, 최대한 빠르게 실행되어야 합니다.
     *
     * 목표 응답 시간: < 100ms
     *
     * @throws std::runtime_error 긴급 정지 실패 시 (하드웨어 문제 등)
     */
    virtual void emergencyStop() = 0;

    /**
     * @brief 에러 리셋
     *
     * Fault 상태에서 복구하여 정상 동작을 재개할 수 있도록 합니다.
     * Alarm이 발생한 경우 해당 Alarm을 확인하고 리셋해야 합니다.
     *
     * @return true 리셋 성공
     * @return false 리셋 실패 (해결되지 않은 문제가 남아있음)
     */
    virtual bool resetErrors() = 0;
};

} // namespace mxrc::core::control
