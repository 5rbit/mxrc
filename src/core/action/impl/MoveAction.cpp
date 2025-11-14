#include "MoveAction.h"
#include "core/action/util/Logger.h"
#include <thread>
#include <cmath>

namespace mxrc::core::action {

MoveAction::MoveAction(const std::string& id, double targetX, double targetY, double targetZ)
    : id_(id), targetX_(targetX), targetY_(targetY), targetZ_(targetZ) {}

std::string MoveAction::getId() const {
    return id_;
}

std::string MoveAction::getType() const {
    return "Move";
}

void MoveAction::execute(ExecutionContext& context) {
    auto logger = Logger::get();
    logger->info("MoveAction {} moving to ({}, {}, {})",
                 id_, targetX_, targetY_, targetZ_);

    status_ = ActionStatus::RUNNING;
    progress_ = 0.0f;
    cancelled_ = false;

    // 이동 시뮬레이션 (실제로는 로봇 제어 코드가 들어갈 위치)
    // 거리에 비례한 시간 동안 대기
    double distance = std::sqrt(
        targetX_ * targetX_ +
        targetY_ * targetY_ +
        targetZ_ * targetZ_);

    // 단위 거리당 100ms 가정
    int steps = static_cast<int>(distance * 10);
    if (steps < 1) steps = 1;

    for (int i = 0; i < steps && !cancelled_; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        progress_ = static_cast<float>(i + 1) / steps;
    }

    if (cancelled_) {
        status_ = ActionStatus::CANCELLED;
        logger->info("MoveAction {} was cancelled", id_);
    } else {
        status_ = ActionStatus::COMPLETED;
        progress_ = 1.0f;
        logger->info("MoveAction {} completed", id_);

        // 최종 위치를 컨텍스트에 저장
        context.setVariable("last_position_x", targetX_);
        context.setVariable("last_position_y", targetY_);
        context.setVariable("last_position_z", targetZ_);
    }
}

void MoveAction::cancel() {
    cancelled_ = true;
}

ActionStatus MoveAction::getStatus() const {
    return status_;
}

float MoveAction::getProgress() const {
    return progress_;
}

} // namespace mxrc::core::action
