#ifndef MXRC_CORE_ACTION_MOVE_ACTION_H
#define MXRC_CORE_ACTION_MOVE_ACTION_H

#include "core/action/interfaces/IAction.h"
#include "core/action/dto/ActionStatus.h"
#include "core/action/util/ExecutionContext.h"
#include <string>
#include <atomic>

namespace mxrc::core::action {

/**
 * @brief 이동 Action
 *
 * 로봇을 지정된 위치로 이동시키는 Action입니다.
 * 현재는 시뮬레이션 목적으로 간단히 구현되었습니다.
 */
class MoveAction : public IAction {
public:
    /**
     * @brief 생성자
     *
     * @param id Action ID
     * @param targetX 목표 X 좌표
     * @param targetY 목표 Y 좌표
     * @param targetZ 목표 Z 좌표
     */
    MoveAction(const std::string& id, double targetX, double targetY, double targetZ);

    ~MoveAction() override = default;

    // IAction 인터페이스 구현
    std::string getId() const override;
    std::string getType() const override;
    void execute(ExecutionContext& context) override;
    void cancel() override;
    ActionStatus getStatus() const override;
    float getProgress() const override;

private:
    std::string id_;
    double targetX_;
    double targetY_;
    double targetZ_;
    std::atomic<ActionStatus> status_{ActionStatus::PENDING};
    std::atomic<float> progress_{0.0f};
    std::atomic<bool> cancelled_{false};
};

} // namespace mxrc::core::action

#endif // MXRC_CORE_ACTION_MOVE_ACTION_H
