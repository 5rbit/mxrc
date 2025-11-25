// PalletShuttleState.h - 팔렛 셔틀 상태 관리
// Copyright (C) 2025 MXRC Project
//
// Feature 016: Pallet Shuttle Control System (T081)

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <chrono>
#include "robot/pallet_shuttle/interfaces/IPalletShuttleStateAccessor.h"

namespace mxrc::core::datastore { class DataStore; }
namespace mxrc::core::event { class EventBus; }

namespace mxrc::robot::pallet_shuttle {

/**
 * @brief 팔렛 셔틀 상태 관리 클래스
 *
 * DataStore와 EventBus를 통해 실시간 상태를 관리하고
 * IPalletShuttleStateAccessor 인터페이스를 구현합니다.
 *
 * Phase 7: Status monitoring
 */
class PalletShuttleState : public IPalletShuttleStateAccessor {
public:
    PalletShuttleState(
        std::shared_ptr<core::datastore::DataStore> data_store,
        std::shared_ptr<core::event::EventBus> event_bus);

    ~PalletShuttleState() = default;

    // Position management
    std::optional<Position> getCurrentPosition() const override;
    std::optional<Position> getTargetPosition() const override;
    bool updatePosition(const Position& position) override;
    bool setTargetPosition(const Position& position) override;

    // State management
    ShuttleState getState() const override;
    bool setState(ShuttleState state) override;

    // Pallet management
    std::optional<PalletInfo> getLoadedPallet() const override;
    bool updateLoadedPallet(const PalletInfo& pallet) override;
    bool clearLoadedPallet() override;

    // Metrics
    double getCurrentSpeed() const override;
    double getBatteryLevel() const override;
    double getTotalDistance() const override;
    uint32_t getCompletedTaskCount() const override;
    void incrementCompletedTaskCount() override;

    // Time tracking
    std::chrono::system_clock::time_point getLastUpdateTime() const override;
    std::optional<std::chrono::system_clock::time_point> getTaskStartTime() const override;
    void setTaskStartTime(const std::chrono::system_clock::time_point& time) override;
    void clearTaskStartTime() override;

    // Additional state management methods
    bool canTransitionTo(ShuttleState new_state) const;
    void setBatteryLevel(double level);
    void setCurrentSpeed(double speed);
    void addDistance(double distance);
    bool isLowBattery() const;
    void setErrorMessage(const std::string& message);
    std::optional<std::string> getErrorMessage() const;

private:
    std::shared_ptr<core::datastore::DataStore> data_store_;
    std::shared_ptr<core::event::EventBus> event_bus_;

    // DataStore keys
    static constexpr const char* KEY_POSITION_X = "pallet_shuttle/position/current/x";
    static constexpr const char* KEY_POSITION_Y = "pallet_shuttle/position/current/y";
    static constexpr const char* KEY_POSITION_Z = "pallet_shuttle/position/current/z";
    static constexpr const char* KEY_POSITION_THETA = "pallet_shuttle/position/current/theta";
    static constexpr const char* KEY_TARGET_X = "pallet_shuttle/position/target/x";
    static constexpr const char* KEY_TARGET_Y = "pallet_shuttle/position/target/y";
    static constexpr const char* KEY_TARGET_Z = "pallet_shuttle/position/target/z";
    static constexpr const char* KEY_STATE = "pallet_shuttle/state";
    static constexpr const char* KEY_BATTERY = "pallet_shuttle/metrics/battery_level";
    static constexpr const char* KEY_SPEED = "pallet_shuttle/metrics/current_speed";
    static constexpr const char* KEY_DISTANCE = "pallet_shuttle/metrics/total_distance";
    static constexpr const char* KEY_COMPLETED_TASKS = "pallet_shuttle/metrics/completed_tasks";
    static constexpr const char* KEY_PALLET_ID = "pallet_shuttle/pallet/id";
    static constexpr const char* KEY_PALLET_WEIGHT = "pallet_shuttle/pallet/weight";
    static constexpr const char* KEY_PALLET_LOADED = "pallet_shuttle/pallet/loaded";
    static constexpr const char* KEY_ERROR_MESSAGE = "pallet_shuttle/error_message";

    static constexpr double LOW_BATTERY_THRESHOLD = 0.10; // 10%

    void publishStateChangeEvent(ShuttleState old_state, ShuttleState new_state);
    void publishPositionUpdateEvent(const Position& position);
};

} // namespace mxrc::robot::pallet_shuttle