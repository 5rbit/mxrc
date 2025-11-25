// PalletShuttleStateAccessor.cpp - 팔렛 셔틀 상태 접근 구현
// Copyright (C) 2025 MXRC Project

#include "PalletShuttleStateAccessor.h"
#include "core/datastore/DataStore.h"
#include "core/event/interfaces/IEventBus.h"
#include <spdlog/spdlog.h>
#include <map>
#include <any>

namespace mxrc::robot::pallet_shuttle {

PalletShuttleStateAccessor::PalletShuttleStateAccessor(
    std::shared_ptr<DataStore> data_store,
    std::shared_ptr<core::event::IEventBus> event_bus)
    : data_store_(std::move(data_store)),
      event_bus_(std::move(event_bus)) {
    spdlog::info("[PalletShuttleStateAccessor] Initialized");
}

// ===== 위치 정보 구현 =====

std::optional<Position> PalletShuttleStateAccessor::getCurrentPosition() const {
    if (!data_store_) return std::nullopt;

    try {
        auto data = data_store_->get(KEY_CURRENT_POSITION);
        if (data.has_value()) {
            return mapToPosition(data.value());
        }
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to get current position: {}", e.what());
    }

    return std::nullopt;
}

std::optional<Position> PalletShuttleStateAccessor::getTargetPosition() const {
    if (!data_store_) return std::nullopt;

    try {
        auto data = data_store_->get(KEY_TARGET_POSITION);
        if (data.has_value()) {
            return mapToPosition(data.value());
        }
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to get target position: {}", e.what());
    }

    return std::nullopt;
}

bool PalletShuttleStateAccessor::updatePosition(const Position& position) {
    if (!data_store_) return false;

    try {
        auto map_data = positionToMap(position);
        data_store_->set(KEY_CURRENT_POSITION, DataType::RobotMode, map_data);

        // 속도 계산 및 업데이트 (간단한 구현)
        auto now = std::chrono::system_clock::now();
        data_store_->set(KEY_LAST_UPDATE_TIME, DataType::RobotMode,
                        std::chrono::system_clock::to_time_t(now));

        publishStateChangeEvent("position_updated",
                               std::string("x:") + std::to_string(position.x) +
                               " y:" + std::to_string(position.y));
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to update position: {}", e.what());
        return false;
    }
}

bool PalletShuttleStateAccessor::setTargetPosition(const Position& position) {
    if (!data_store_) return false;

    try {
        auto map_data = positionToMap(position);
        data_store_->set(KEY_TARGET_POSITION, DataType::RobotMode, map_data);

        publishStateChangeEvent("target_position_set",
                               std::string("x:") + std::to_string(position.x) +
                               " y:" + std::to_string(position.y));
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to set target position: {}", e.what());
        return false;
    }
}

// ===== 상태 정보 구현 =====

ShuttleState PalletShuttleStateAccessor::getState() const {
    if (!data_store_) return ShuttleState::ERROR;

    try {
        auto data = data_store_->get(KEY_STATE);
        if (data.has_value()) {
            int state_int = std::any_cast<int>(data.value());
            return static_cast<ShuttleState>(state_int);
        }
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to get state: {}", e.what());
    }

    return ShuttleState::IDLE;
}

bool PalletShuttleStateAccessor::setState(ShuttleState state) {
    if (!data_store_) return false;

    try {
        data_store_->set(KEY_STATE, DataType::RobotMode, static_cast<int>(state));

        std::string state_str;
        switch (state) {
            case ShuttleState::IDLE: state_str = "IDLE"; break;
            case ShuttleState::MOVING: state_str = "MOVING"; break;
            case ShuttleState::PICKING: state_str = "PICKING"; break;
            case ShuttleState::PLACING: state_str = "PLACING"; break;
            case ShuttleState::ERROR: state_str = "ERROR"; break;
            case ShuttleState::MAINTENANCE: state_str = "MAINTENANCE"; break;
        }

        publishStateChangeEvent("state_changed", state_str);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to set state: {}", e.what());
        return false;
    }
}

// ===== 팔렛 정보 구현 =====

std::optional<PalletInfo> PalletShuttleStateAccessor::getLoadedPallet() const {
    if (!data_store_) return std::nullopt;

    try {
        auto data = data_store_->get(KEY_LOADED_PALLET);
        if (data.has_value()) {
            return mapToPalletInfo(data.value());
        }
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to get loaded pallet: {}", e.what());
    }

    return std::nullopt;
}

bool PalletShuttleStateAccessor::updateLoadedPallet(const PalletInfo& pallet) {
    if (!data_store_) return false;

    try {
        auto map_data = palletInfoToMap(pallet);
        data_store_->set(KEY_LOADED_PALLET, DataType::RobotMode, map_data);

        publishStateChangeEvent("pallet_loaded", pallet.pallet_id);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to update loaded pallet: {}", e.what());
        return false;
    }
}

bool PalletShuttleStateAccessor::clearLoadedPallet() {
    if (!data_store_) return false;

    try {
        data_store_->remove(KEY_LOADED_PALLET);
        publishStateChangeEvent("pallet_unloaded", "");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to clear loaded pallet: {}", e.what());
        return false;
    }
}

// ===== 성능 메트릭 구현 =====

double PalletShuttleStateAccessor::getCurrentSpeed() const {
    if (!data_store_) return 0.0;

    try {
        auto data = data_store_->get(KEY_CURRENT_SPEED);
        if (data.has_value()) {
            return std::any_cast<double>(data.value());
        }
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to get current speed: {}", e.what());
    }

    return 0.0;
}

double PalletShuttleStateAccessor::getBatteryLevel() const {
    if (!data_store_) return 0.0;

    try {
        auto data = data_store_->get(KEY_BATTERY_LEVEL);
        if (data.has_value()) {
            return std::any_cast<double>(data.value());
        }
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to get battery level: {}", e.what());
    }

    // 기본값 100%
    return 1.0;
}

double PalletShuttleStateAccessor::getTotalDistance() const {
    if (!data_store_) return 0.0;

    try {
        auto data = data_store_->get(KEY_TOTAL_DISTANCE);
        if (data.has_value()) {
            return std::any_cast<double>(data.value());
        }
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to get total distance: {}", e.what());
    }

    return 0.0;
}

uint32_t PalletShuttleStateAccessor::getCompletedTaskCount() const {
    if (!data_store_) return 0;

    try {
        auto data = data_store_->get(KEY_COMPLETED_TASKS);
        if (data.has_value()) {
            return static_cast<uint32_t>(std::any_cast<int>(data.value()));
        }
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to get completed task count: {}", e.what());
    }

    return 0;
}

void PalletShuttleStateAccessor::incrementCompletedTaskCount() {
    if (!data_store_) return;

    try {
        uint32_t current = getCompletedTaskCount();
        data_store_->set(KEY_COMPLETED_TASKS, DataType::RobotMode,
                        static_cast<int>(current + 1));
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to increment completed task count: {}", e.what());
    }
}

// ===== 시간 정보 구현 =====

std::chrono::system_clock::time_point PalletShuttleStateAccessor::getLastUpdateTime() const {
    if (!data_store_) {
        return std::chrono::system_clock::now();
    }

    try {
        auto data = data_store_->get(KEY_LAST_UPDATE_TIME);
        if (data.has_value()) {
            auto time_t = std::any_cast<std::time_t>(data.value());
            return std::chrono::system_clock::from_time_t(time_t);
        }
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to get last update time: {}", e.what());
    }

    return std::chrono::system_clock::now();
}

std::optional<std::chrono::system_clock::time_point>
PalletShuttleStateAccessor::getTaskStartTime() const {
    if (!data_store_) return std::nullopt;

    try {
        auto data = data_store_->get(KEY_TASK_START_TIME);
        if (data.has_value()) {
            auto time_t = std::any_cast<std::time_t>(data.value());
            return std::chrono::system_clock::from_time_t(time_t);
        }
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to get task start time: {}", e.what());
    }

    return std::nullopt;
}

void PalletShuttleStateAccessor::setTaskStartTime(
    const std::chrono::system_clock::time_point& time) {
    if (!data_store_) return;

    try {
        auto time_t = std::chrono::system_clock::to_time_t(time);
        data_store_->set(KEY_TASK_START_TIME, DataType::RobotMode, time_t);
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to set task start time: {}", e.what());
    }
}

void PalletShuttleStateAccessor::clearTaskStartTime() {
    if (!data_store_) return;

    try {
        data_store_->remove(KEY_TASK_START_TIME);
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to clear task start time: {}", e.what());
    }
}

// ===== Private helper methods =====

void PalletShuttleStateAccessor::publishStateChangeEvent(
    const std::string& event_type,
    const std::string& details) {

    if (!event_bus_) return;

    // EventBus를 통한 이벤트 발행
    // TODO: 적절한 이벤트 타입 정의 필요
    spdlog::debug("[PalletShuttleStateAccessor] Event: {} - {}", event_type, details);
}

std::map<std::string, std::any>
PalletShuttleStateAccessor::positionToMap(const Position& pos) const {
    std::map<std::string, std::any> map;
    map["x"] = pos.x;
    map["y"] = pos.y;
    map["z"] = pos.z;
    map["theta"] = pos.theta;
    return map;
}

std::optional<Position>
PalletShuttleStateAccessor::mapToPosition(const std::any& data) const {
    try {
        auto map = std::any_cast<std::map<std::string, std::any>>(data);
        Position pos;
        pos.x = std::any_cast<double>(map["x"]);
        pos.y = std::any_cast<double>(map["y"]);
        pos.z = std::any_cast<double>(map["z"]);
        pos.theta = std::any_cast<double>(map["theta"]);
        return pos;
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to convert map to Position: {}", e.what());
        return std::nullopt;
    }
}

std::map<std::string, std::any>
PalletShuttleStateAccessor::palletInfoToMap(const PalletInfo& info) const {
    std::map<std::string, std::any> map;
    map["pallet_id"] = info.pallet_id;
    map["weight"] = info.weight;
    map["is_loaded"] = info.is_loaded;
    return map;
}

std::optional<PalletInfo>
PalletShuttleStateAccessor::mapToPalletInfo(const std::any& data) const {
    try {
        auto map = std::any_cast<std::map<std::string, std::any>>(data);
        PalletInfo info;
        info.pallet_id = std::any_cast<std::string>(map["pallet_id"]);
        info.weight = std::any_cast<double>(map["weight"]);
        info.is_loaded = std::any_cast<bool>(map["is_loaded"]);
        return info;
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleStateAccessor] Failed to convert map to PalletInfo: {}", e.what());
        return std::nullopt;
    }
}

} // namespace mxrc::robot::pallet_shuttle