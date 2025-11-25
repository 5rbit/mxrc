#include "PickPalletAction.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <thread>
#include <chrono>

namespace mxrc::robot::pallet_shuttle {

using namespace mxrc::core::action;

PickPalletAction::PickPalletAction(
    const std::string& id,
    const std::string& pallet_id
) : id_(id), pallet_id_(pallet_id) {
    if (pallet_id_.empty()) {
        throw std::invalid_argument("Pallet ID cannot be empty");
    }
    spdlog::debug("[PickPalletAction] Created: {} for pallet {}", id_, pallet_id_);
}

std::string PickPalletAction::getId() const {
    return id_;
}

std::string PickPalletAction::getType() const {
    return "PickPallet";
}

void PickPalletAction::execute(ExecutionContext& context) {
    if (status_ != ActionStatus::PENDING) {
        throw std::runtime_error("Action already executed: " + id_);
    }

    // 이미 팔렛을 들고 있는지 확인
    if (context.has("picked_pallet_id") && !context.get("picked_pallet_id").empty()) {
        throw std::runtime_error("Already holding pallet: " + context.get("picked_pallet_id"));
    }

    spdlog::info("[PickPalletAction] Executing: {} (pallet: {})", id_, pallet_id_);

    status_ = ActionStatus::RUNNING;
    progress_ = 0.0f;

    // Mock 픽업 시뮬레이션
    // 1. 그리퍼 하강
    progress_ = 0.2f;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (cancelled_) {
        status_ = ActionStatus::CANCELLED;
        return;
    }

    // 2. 그리퍼 닫기
    progress_ = 0.5f;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (cancelled_) {
        status_ = ActionStatus::CANCELLED;
        return;
    }

    // 3. 팔렛 센서 확인
    progress_ = 0.7f;
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    // 4. 그리퍼 상승
    progress_ = 0.9f;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Context 업데이트
    context.set("picked_pallet_id", pallet_id_);
    context.set("gripper_closed", "true");

    progress_ = 1.0f;
    status_ = ActionStatus::COMPLETED;

    spdlog::info("[PickPalletAction] Completed: {} (pallet: {})", id_, pallet_id_);
}

void PickPalletAction::cancel() {
    cancelled_ = true;
    spdlog::info("[PickPalletAction] Cancel requested: {}", id_);
}

ActionStatus PickPalletAction::getStatus() const {
    return status_;
}

float PickPalletAction::getProgress() const {
    return progress_;
}

} // namespace mxrc::robot::pallet_shuttle
