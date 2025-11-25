#pragma once

#include "core/action/interfaces/IAction.h"
#include "core/action/dto/ActionStatus.h"
#include "core/action/util/ExecutionContext.h"
#include <string>
#include <atomic>

namespace mxrc::robot::pallet_shuttle {

/**
 * @brief 팔렛 배치 Action
 *
 * 그리퍼로 집은 팔렛을 현재 위치에 배치합니다.
 *
 * Feature 016: Pallet Shuttle Control System
 * Phase 5: User Story 1 - 팔렛 픽업 및 배치
 */
class PlacePalletAction : public mxrc::core::action::IAction {
public:
    /**
     * @brief 생성자
     *
     * @param id Action ID
     */
    explicit PlacePalletAction(const std::string& id);

    ~PlacePalletAction() override = default;

    // IAction 인터페이스 구현
    std::string getId() const override;
    std::string getType() const override;
    void execute(mxrc::core::action::ExecutionContext& context) override;
    void cancel() override;
    mxrc::core::action::ActionStatus getStatus() const override;
    float getProgress() const override;

private:
    std::string id_;
    std::atomic<mxrc::core::action::ActionStatus> status_{mxrc::core::action::ActionStatus::PENDING};
    std::atomic<float> progress_{0.0f};
    std::atomic<bool> cancelled_{false};
};

} // namespace mxrc::robot::pallet_shuttle
