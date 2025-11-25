#pragma once

#include "core/action/interfaces/IAction.h"
#include "core/action/dto/ActionStatus.h"
#include "core/action/util/ExecutionContext.h"
#include <string>
#include <atomic>

namespace mxrc::robot::pallet_shuttle {

/**
 * @brief 위치 이동 Action
 *
 * 로봇을 지정된 위치로 이동시킵니다.
 *
 * Feature 016: Pallet Shuttle Control System
 * Phase 5: User Story 1 - 팔렛 픽업 및 배치
 */
class MoveToPositionAction : public mxrc::core::action::IAction {
public:
    /**
     * @brief 생성자
     *
     * @param id Action ID
     * @param target_x 목표 X 좌표
     * @param target_y 목표 Y 좌표
     * @param target_theta 목표 회전각 (라디안)
     */
    MoveToPositionAction(
        const std::string& id,
        double target_x,
        double target_y,
        double target_theta
    );

    ~MoveToPositionAction() override = default;

    // IAction 인터페이스 구현
    std::string getId() const override;
    std::string getType() const override;
    void execute(mxrc::core::action::ExecutionContext& context) override;
    void cancel() override;
    mxrc::core::action::ActionStatus getStatus() const override;
    float getProgress() const override;

private:
    std::string id_;
    double target_x_;
    double target_y_;
    double target_theta_;
    std::atomic<mxrc::core::action::ActionStatus> status_{mxrc::core::action::ActionStatus::PENDING};
    std::atomic<float> progress_{0.0f};
    std::atomic<bool> cancelled_{false};
};

} // namespace mxrc::robot::pallet_shuttle
