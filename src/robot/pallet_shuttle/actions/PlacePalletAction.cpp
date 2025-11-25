#include "PlacePalletAction.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <thread>
#include <chrono>

namespace mxrc::robot::pallet_shuttle {

using namespace mxrc::core::action;

PlacePalletAction::PlacePalletAction(const std::string& id) : id_(id) {
    spdlog::debug("[PlacePalletAction] Created: {}", id_);
}

std::string PlacePalletAction::getId() const {
    return id_;
}

std::string PlacePalletAction::getType() const {
    return "PlacePallet";
}

void PlacePalletAction::execute(ExecutionContext& context) {
    if (status_ != ActionStatus::PENDING) {
        throw std::runtime_error("Action already executed: " + id_);
    }

    // 팔렛을 들고 있는지 확인
    if (!context.has("picked_pallet_id") || context.get("picked_pallet_id").empty()) {
        throw std::runtime_error("No pallet held");
    }

    std::string pallet_id = context.get("picked_pallet_id");
    spdlog::info("[PlacePalletAction] Executing: {} (pallet: {})", id_, pallet_id);

    status_ = ActionStatus::RUNNING;
    progress_ = 0.0f;

    // Mock 배치 시뮬레이션
    // 1. 그리퍼 하강
    progress_ = 0.3f;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (cancelled_) {
        status_ = ActionStatus::CANCELLED;
        return;
    }

    // 2. 그리퍼 열기
    progress_ = 0.6f;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (cancelled_) {
        status_ = ActionStatus::CANCELLED;
        return;
    }

    // 3. 그리퍼 상승
    progress_ = 0.9f;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Context 업데이트
    context.set("picked_pallet_id", "");  // 팔렛 놓음
    context.set("gripper_closed", "false");

    // 배치 위치 기록
    if (context.has("current_x")) {
        context.set("last_place_x", context.get("current_x"));
    }
    if (context.has("current_y")) {
        context.set("last_place_y", context.get("current_y"));
    }

    progress_ = 1.0f;
    status_ = ActionStatus::COMPLETED;

    spdlog::info("[PlacePalletAction] Completed: {} (pallet: {})", id_, pallet_id);
}

void PlacePalletAction::cancel() {
    cancelled_ = true;
    spdlog::info("[PlacePalletAction] Cancel requested: {}", id_);
}

ActionStatus PlacePalletAction::getStatus() const {
    return status_;
}

float PlacePalletAction::getProgress() const {
    return progress_;
}

} // namespace mxrc::robot::pallet_shuttle
