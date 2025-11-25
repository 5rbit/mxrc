#include "MoveToPositionAction.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <cmath>
#include <thread>
#include <chrono>

namespace mxrc::robot::pallet_shuttle {

using namespace mxrc::core::action;

MoveToPositionAction::MoveToPositionAction(
    const std::string& id,
    double target_x,
    double target_y,
    double target_theta
) : id_(id),
    target_x_(target_x),
    target_y_(target_y),
    target_theta_(target_theta) {
    spdlog::debug("[MoveToPositionAction] Created: {} -> ({}, {}, {})",
        id_, target_x_, target_y_, target_theta_);
}

std::string MoveToPositionAction::getId() const {
    return id_;
}

std::string MoveToPositionAction::getType() const {
    return "MoveToPosition";
}

void MoveToPositionAction::execute(ExecutionContext& context) {
    if (status_ != ActionStatus::PENDING) {
        throw std::runtime_error("Action already executed: " + id_);
    }

    spdlog::info("[MoveToPositionAction] Executing: {} -> ({}, {}, {})",
        id_, target_x_, target_y_, target_theta_);

    status_ = ActionStatus::RUNNING;
    progress_ = 0.0f;

    // 현재 위치 가져오기 (없으면 원점으로 가정)
    double current_x = 0.0;
    double current_y = 0.0;
    double current_theta = 0.0;

    if (context.has("current_x")) {
        current_x = std::stod(context.get("current_x"));
    }
    if (context.has("current_y")) {
        current_y = std::stod(context.get("current_y"));
    }
    if (context.has("current_theta")) {
        current_theta = std::stod(context.get("current_theta"));
    }

    // 거리 계산
    double dx = target_x_ - current_x;
    double dy = target_y_ - current_y;
    double distance = std::sqrt(dx * dx + dy * dy);

    spdlog::debug("[MoveToPositionAction] Distance: {:.2f}m", distance);

    // Mock 이동 시뮬레이션 (실제로는 Fieldbus Driver 호출)
    const int steps = 10;
    for (int i = 0; i <= steps && !cancelled_; ++i) {
        progress_ = static_cast<float>(i) / steps;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (cancelled_) {
        status_ = ActionStatus::CANCELLED;
        spdlog::warn("[MoveToPositionAction] Cancelled: {}", id_);
        return;
    }

    // 목표 위치로 도착
    context.set("current_x", std::to_string(target_x_));
    context.set("current_y", std::to_string(target_y_));
    context.set("current_theta", std::to_string(target_theta_));

    progress_ = 1.0f;
    status_ = ActionStatus::COMPLETED;

    spdlog::info("[MoveToPositionAction] Completed: {}", id_);
}

void MoveToPositionAction::cancel() {
    cancelled_ = true;
    spdlog::info("[MoveToPositionAction] Cancel requested: {}", id_);
}

ActionStatus MoveToPositionAction::getStatus() const {
    return status_;
}

float MoveToPositionAction::getProgress() const {
    return progress_;
}

} // namespace mxrc::robot::pallet_shuttle
