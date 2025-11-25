// PalletShuttleController.h - 팔렛 셔틀 제어기
// Copyright (C) 2025 MXRC Project
//
// Feature 016: Pallet Shuttle Control System

#pragma once

#include <memory>
#include <string>
#include "core/control/interfaces/IRobotController.h"
#include "core/control/dto/Priority.h"
#include "robot/pallet_shuttle/dto/Position.h"

namespace mxrc::core::control {
    class IBehaviorArbiter;
    class TaskQueue;
}
namespace mxrc::core::alarm { class IAlarmManager; }
namespace mxrc::core::fieldbus { class IFieldbusDriver; }
namespace mxrc::robot::pallet_shuttle {
    class IPalletShuttleStateAccessor;
    class IPalletTaskAccessor;
}

namespace mxrc::robot::pallet_shuttle {

/**
 * @brief 팔렛 셔틀 메인 컨트롤러
 *
 * 팔렛 셔틀 시스템의 중앙 제어를 담당합니다.
 * Task 제출, 상태 관리, 알람 처리 등을 조율합니다.
 */
class PalletShuttleController : public core::control::IRobotController {
public:
    PalletShuttleController(
        std::shared_ptr<IPalletShuttleStateAccessor> state_accessor,
        std::shared_ptr<core::control::IBehaviorArbiter> behavior_arbiter,
        std::shared_ptr<core::alarm::IAlarmManager> alarm_manager,
        std::shared_ptr<core::fieldbus::IFieldbusDriver> fieldbus_driver);

    ~PalletShuttleController() = default;

    // IRobotController interface
    bool initialize() override;
    bool start() override;
    bool stop() override;
    bool reset() override;
    core::control::ControlMode getMode() const override;
    bool setMode(core::control::ControlMode mode) override;
    bool executeCommand(const std::string& command, const std::any& params) override;
    std::string getStatus() const override;

    // Transport task submission
    bool submitTransportTask(
        const std::string& task_id,
        const std::string& pallet_id,
        const Position& pickup_pos,
        const Position& place_pos,
        core::control::Priority priority = core::control::Priority::NORMAL);

    // Task management
    bool cancelTask(const std::string& task_id);
    bool cancelAllTasks();
    size_t getPendingTaskCount() const;

    // Emergency operations
    void emergencyStop();
    void clearEmergencyStop();

    // Maintenance operations
    void runSafetyCheck();
    void acknowledgeMaintenance();

private:
    std::shared_ptr<IPalletShuttleStateAccessor> state_accessor_;
    std::shared_ptr<IPalletTaskAccessor> task_accessor_;
    std::shared_ptr<core::control::IBehaviorArbiter> behavior_arbiter_;
    std::shared_ptr<core::alarm::IAlarmManager> alarm_manager_;
    std::shared_ptr<core::fieldbus::IFieldbusDriver> fieldbus_driver_;
    std::shared_ptr<core::control::TaskQueue> task_queue_;

    bool initialized_;
    bool running_;

    void handleTaskCompletion(const std::string& task_id);
    void handleAlarmRaised(const std::string& alarm_id);
    void updateMetrics();
};

} // namespace mxrc::robot::pallet_shuttle