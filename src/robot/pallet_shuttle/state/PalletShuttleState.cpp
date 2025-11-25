// PalletShuttleState.cpp - 팔렛 셔틀 상태 관리 구현
// Copyright (C) 2025 MXRC Project
//
// Feature 016: Pallet Shuttle Control System (T081-T082)

#include "PalletShuttleState.h"
#include "core/datastore/DataStore.h"
#include "core/event/core/EventBus.h"
#include "core/event/dto/EventBase.h"
#include <spdlog/spdlog.h>

namespace mxrc::robot::pallet_shuttle {

using namespace core::datastore;
using namespace core::event;

// State change event
class StateChangeEvent : public EventBase {
public:
    ShuttleState old_state;
    ShuttleState new_state;

    StateChangeEvent(ShuttleState old_s, ShuttleState new_s)
        : EventBase(EventType::STATE_CHANGED, "shuttle_state", std::chrono::system_clock::now()),
          old_state(old_s), new_state(new_s) {}
};

// Position update event
class PositionUpdateEvent : public EventBase {
public:
    Position position;

    explicit PositionUpdateEvent(const Position& pos)
        : EventBase(EventType::POSITION_UPDATED, "shuttle_position", std::chrono::system_clock::now()),
          position(pos) {}
};

PalletShuttleState::PalletShuttleState(
    std::shared_ptr<DataStore> data_store,
    std::shared_ptr<EventBus> event_bus)
    : data_store_(data_store), event_bus_(event_bus) {

    // Initialize default values in DataStore
    data_store_->set(KEY_POSITION_X, 0.0, DataType::RobotState);
    data_store_->set(KEY_POSITION_Y, 0.0, DataType::RobotState);
    data_store_->set(KEY_POSITION_Z, 0.0, DataType::RobotState);
    data_store_->set(KEY_POSITION_THETA, 0.0, DataType::RobotState);
    data_store_->set(KEY_STATE, static_cast<int>(ShuttleState::IDLE), DataType::RobotState);
    data_store_->set(KEY_BATTERY, 1.0, DataType::RobotState);
    data_store_->set(KEY_SPEED, 0.0, DataType::RobotState);
    data_store_->set(KEY_DISTANCE, 0.0, DataType::RobotState);
    data_store_->set(KEY_COMPLETED_TASKS, static_cast<int>(0), DataType::RobotState);
    data_store_->set(KEY_PALLET_LOADED, false, DataType::RobotState);
}

// Position management
std::optional<Position> PalletShuttleState::getCurrentPosition() const {
    try {
        Position pos;
        pos.x = data_store_->get<double>(KEY_POSITION_X);
        pos.y = data_store_->get<double>(KEY_POSITION_Y);
        pos.z = data_store_->get<double>(KEY_POSITION_Z);
        pos.theta = data_store_->get<double>(KEY_POSITION_THETA);
        return pos;
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleState] Failed to get position: {}", e.what());
        return std::nullopt;
    }
}

std::optional<Position> PalletShuttleState::getTargetPosition() const {
    try {
        Position pos;
        pos.x = data_store_->get<double>(KEY_TARGET_X);
        pos.y = data_store_->get<double>(KEY_TARGET_Y);
        pos.z = data_store_->get<double>(KEY_TARGET_Z);
        pos.theta = 0; // Target doesn't have theta
        return pos;
    } catch (const std::exception&) {
        return std::nullopt; // No target set
    }
}

bool PalletShuttleState::updatePosition(const Position& position) {
    try {
        data_store_->set(KEY_POSITION_X, position.x, DataType::RobotState);
        data_store_->set(KEY_POSITION_Y, position.y, DataType::RobotState);
        data_store_->set(KEY_POSITION_Z, position.z, DataType::RobotState);
        data_store_->set(KEY_POSITION_THETA, position.theta, DataType::RobotState);

        publishPositionUpdateEvent(position);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleState] Failed to update position: {}", e.what());
        return false;
    }
}

bool PalletShuttleState::setTargetPosition(const Position& position) {
    try {
        data_store_->set(KEY_TARGET_X, position.x, DataType::RobotState);
        data_store_->set(KEY_TARGET_Y, position.y, DataType::RobotState);
        data_store_->set(KEY_TARGET_Z, position.z, DataType::RobotState);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleState] Failed to set target position: {}", e.what());
        return false;
    }
}

// State management
ShuttleState PalletShuttleState::getState() const {
    try {
        int state_int = data_store_->get<int>(KEY_STATE);
        return static_cast<ShuttleState>(state_int);
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleState] Failed to get state: {}", e.what());
        return ShuttleState::ERROR;
    }
}

bool PalletShuttleState::setState(ShuttleState state) {
    try {
        ShuttleState old_state = getState();
        data_store_->set(KEY_STATE, static_cast<int>(state), DataType::RobotState);

        if (old_state != state) {
            publishStateChangeEvent(old_state, state);
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleState] Failed to set state: {}", e.what());
        return false;
    }
}

// Pallet management
std::optional<PalletInfo> PalletShuttleState::getLoadedPallet() const {
    try {
        bool loaded = data_store_->get<bool>(KEY_PALLET_LOADED);
        if (!loaded) {
            return std::nullopt;
        }

        PalletInfo pallet;
        pallet.pallet_id = data_store_->get<std::string>(KEY_PALLET_ID);
        pallet.weight = data_store_->get<double>(KEY_PALLET_WEIGHT);
        pallet.is_loaded = true;
        return pallet;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

bool PalletShuttleState::updateLoadedPallet(const PalletInfo& pallet) {
    try {
        data_store_->set(KEY_PALLET_ID, pallet.pallet_id, DataType::RobotState);
        data_store_->set(KEY_PALLET_WEIGHT, pallet.weight, DataType::RobotState);
        data_store_->set(KEY_PALLET_LOADED, true, DataType::RobotState);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleState] Failed to update loaded pallet: {}", e.what());
        return false;
    }
}

bool PalletShuttleState::clearLoadedPallet() {
    try {
        data_store_->set(KEY_PALLET_LOADED, false, DataType::RobotState);
        // Keep ID and weight for history, just mark as not loaded
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleState] Failed to clear loaded pallet: {}", e.what());
        return false;
    }
}

// Metrics
double PalletShuttleState::getCurrentSpeed() const {
    try {
        return data_store_->get<double>(KEY_SPEED);
    } catch (const std::exception&) {
        return 0.0;
    }
}

double PalletShuttleState::getBatteryLevel() const {
    try {
        return data_store_->get<double>(KEY_BATTERY);
    } catch (const std::exception&) {
        return 1.0; // Default to full
    }
}

double PalletShuttleState::getTotalDistance() const {
    try {
        return data_store_->get<double>(KEY_DISTANCE);
    } catch (const std::exception&) {
        return 0.0;
    }
}

uint32_t PalletShuttleState::getCompletedTaskCount() const {
    try {
        return static_cast<uint32_t>(data_store_->get<int>(KEY_COMPLETED_TASKS));
    } catch (const std::exception&) {
        return 0;
    }
}

void PalletShuttleState::incrementCompletedTaskCount() {
    try {
        uint32_t current = getCompletedTaskCount();
        data_store_->set(KEY_COMPLETED_TASKS, static_cast<int>(current + 1), DataType::RobotState);
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleState] Failed to increment task count: {}", e.what());
    }
}

// Time tracking
std::chrono::system_clock::time_point PalletShuttleState::getLastUpdateTime() const {
    return std::chrono::system_clock::now(); // Always current for now
}

std::optional<std::chrono::system_clock::time_point> PalletShuttleState::getTaskStartTime() const {
    try {
        auto timestamp = data_store_->get<int64_t>("pallet_shuttle/task_start_time");
        return std::chrono::system_clock::time_point(std::chrono::milliseconds(timestamp));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

void PalletShuttleState::setTaskStartTime(const std::chrono::system_clock::time_point& time) {
    try {
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            time.time_since_epoch()).count();
        data_store_->set("pallet_shuttle/task_start_time", timestamp, DataType::RobotState);
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleState] Failed to set task start time: {}", e.what());
    }
}

void PalletShuttleState::clearTaskStartTime() {
    try {
        // Remove the key by setting invalid value
        data_store_->set("pallet_shuttle/task_start_time", int64_t(-1), DataType::RobotState);
    } catch (const std::exception&) {
        // Ignore
    }
}

// Additional methods
bool PalletShuttleState::canTransitionTo(ShuttleState new_state) const {
    ShuttleState current = getState();

    // ERROR state can always be entered
    if (new_state == ShuttleState::ERROR) {
        return true;
    }

    // From ERROR, only IDLE is allowed
    if (current == ShuttleState::ERROR) {
        return new_state == ShuttleState::IDLE;
    }

    // Define valid transitions
    switch (current) {
        case ShuttleState::IDLE:
            return new_state == ShuttleState::MOVING ||
                   new_state == ShuttleState::PICKING ||
                   new_state == ShuttleState::PLACING;

        case ShuttleState::MOVING:
            return new_state == ShuttleState::IDLE ||
                   new_state == ShuttleState::PICKING ||
                   new_state == ShuttleState::PLACING;

        case ShuttleState::PICKING:
        case ShuttleState::PLACING:
            return new_state == ShuttleState::IDLE ||
                   new_state == ShuttleState::MOVING;

        default:
            return false;
    }
}

void PalletShuttleState::setBatteryLevel(double level) {
    try {
        data_store_->set(KEY_BATTERY, level, DataType::RobotState);
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleState] Failed to set battery level: {}", e.what());
    }
}

void PalletShuttleState::setCurrentSpeed(double speed) {
    try {
        data_store_->set(KEY_SPEED, speed, DataType::RobotState);
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleState] Failed to set speed: {}", e.what());
    }
}

void PalletShuttleState::addDistance(double distance) {
    try {
        double current = getTotalDistance();
        data_store_->set(KEY_DISTANCE, current + distance, DataType::RobotState);
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleState] Failed to add distance: {}", e.what());
    }
}

bool PalletShuttleState::isLowBattery() const {
    return getBatteryLevel() < LOW_BATTERY_THRESHOLD;
}

void PalletShuttleState::setErrorMessage(const std::string& message) {
    try {
        data_store_->set(KEY_ERROR_MESSAGE, message, DataType::RobotState);
    } catch (const std::exception& e) {
        spdlog::error("[PalletShuttleState] Failed to set error message: {}", e.what());
    }
}

std::optional<std::string> PalletShuttleState::getErrorMessage() const {
    try {
        return data_store_->get<std::string>(KEY_ERROR_MESSAGE);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

void PalletShuttleState::publishStateChangeEvent(ShuttleState old_state, ShuttleState new_state) {
    if (event_bus_) {
        auto event = std::make_shared<StateChangeEvent>(old_state, new_state);
        event_bus_->publish(event);
        spdlog::debug("[PalletShuttleState] Published state change: {} -> {}",
                     static_cast<int>(old_state), static_cast<int>(new_state));
    }
}

void PalletShuttleState::publishPositionUpdateEvent(const Position& position) {
    if (event_bus_) {
        auto event = std::make_shared<PositionUpdateEvent>(position);
        event_bus_->publish(event);
        spdlog::trace("[PalletShuttleState] Published position update: ({}, {}, {})",
                     position.x, position.y, position.z);
    }
}

} // namespace mxrc::robot::pallet_shuttle