#include "PalletTransportTask.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <chrono>

namespace mxrc::robot::pallet_shuttle {

using namespace mxrc::core::task;

PalletTransportTask::PalletTransportTask(
    const std::string& task_id,
    double pickup_x,
    double pickup_y,
    double place_x,
    double place_y,
    const std::string& pallet_id
) : task_id_(task_id),
    definition_("PalletTransport", "Pallet Transport Task") {

    if (pallet_id.empty()) {
        throw std::invalid_argument("Pallet ID cannot be empty");
    }

    // Sequence 생성
    sequence_ = std::make_unique<PalletTransportSequence>(
        pickup_x, pickup_y, place_x, place_y, pallet_id
    );

    // TaskDefinition 설정
    definition_.description = "Transport pallet " + pallet_id +
                             " from (" + std::to_string(pickup_x) + "," +
                             std::to_string(pickup_y) + ") to (" +
                             std::to_string(place_x) + "," +
                             std::to_string(place_y) + ")";

    spdlog::debug("[PalletTransportTask] Created: {}", task_id_);
}

std::string PalletTransportTask::getId() const {
    return task_id_;
}

std::string PalletTransportTask::start() {
    if (status_ != TaskStatus::IDLE) {
        throw std::runtime_error("Task already started: " + task_id_);
    }

    spdlog::info("[PalletTransportTask] Starting: {}", task_id_);

    // 실행 ID 생성 (타임스탬프 기반)
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    execution_id_ = task_id_ + "_" + std::to_string(timestamp);

    status_ = TaskStatus::RUNNING;
    progress_ = 0.0f;

    // 실제로는 SequenceEngine에 Sequence를 제출하고 실행
    // 여기서는 Mock으로 간단히 시뮬레이션
    spdlog::info("[PalletTransportTask] Sequence submitted: {}", execution_id_);

    return execution_id_;
}

void PalletTransportTask::stop() {
    if (status_ != TaskStatus::RUNNING) {
        throw std::runtime_error("Task not running: " + task_id_);
    }

    spdlog::info("[PalletTransportTask] Stopping: {}", task_id_);
    status_ = TaskStatus::CANCELLED;
}

void PalletTransportTask::pause() {
    if (status_ != TaskStatus::RUNNING) {
        throw std::runtime_error("Task not running: " + task_id_);
    }

    spdlog::info("[PalletTransportTask] Pausing: {}", task_id_);
    status_ = TaskStatus::PAUSED;
}

void PalletTransportTask::resume() {
    if (status_ != TaskStatus::PAUSED) {
        throw std::runtime_error("Task not paused: " + task_id_);
    }

    spdlog::info("[PalletTransportTask] Resuming: {}", task_id_);
    status_ = TaskStatus::RUNNING;
}

TaskStatus PalletTransportTask::getStatus() const {
    return status_;
}

float PalletTransportTask::getProgress() const {
    return progress_;
}

const TaskDefinition& PalletTransportTask::getDefinition() const {
    return definition_;
}

} // namespace mxrc::robot::pallet_shuttle
