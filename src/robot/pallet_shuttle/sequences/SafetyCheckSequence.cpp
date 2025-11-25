// SafetyCheckSequence.cpp - 주기적 안전 점검 시퀀스 구현
// Copyright (C) 2025 MXRC Project
//
// Feature 016: Pallet Shuttle Control System (T087-T090)

#include "SafetyCheckSequence.h"
#include "robot/pallet_shuttle/interfaces/IPalletShuttleStateAccessor.h"
#include "core/alarm/interfaces/IAlarmManager.h"
#include "core/fieldbus/interfaces/IFieldbusDriver.h"
#include <spdlog/spdlog.h>
#include <any>

namespace mxrc::robot::pallet_shuttle {

using namespace core::sequence;
using namespace core::alarm;
using namespace core::fieldbus;

SafetyCheckSequence::SafetyCheckSequence(
    const std::string& sequence_id,
    std::shared_ptr<IPalletShuttleStateAccessor> state_accessor,
    std::shared_ptr<IAlarmManager> alarm_manager,
    std::shared_ptr<IFieldbusDriver> fieldbus_driver)
    : sequence_id_(sequence_id),
      status_(SequenceStatus::IDLE),
      progress_(0.0),
      state_accessor_(state_accessor),
      alarm_manager_(alarm_manager),
      fieldbus_driver_(fieldbus_driver) {

    // Initialize sequence definition
    definition_.name = "Safety Check Sequence";
    definition_.description = "Periodic safety and maintenance checks";
    definition_.steps = {
        {"Battery Check", "Check battery level"},
        {"Sensor Diagnostics", "Verify sensor status"},
        {"Motor Status", "Check motor health"},
        {"Emergency Stop", "Verify E-stop status"},
        {"Maintenance Schedule", "Check maintenance intervals"},
        {"Communication", "Verify fieldbus communication"}
    };
}

SequenceResult SafetyCheckSequence::execute() {
    spdlog::info("[SafetyCheckSequence] Starting safety check: {}", sequence_id_);

    status_ = SequenceStatus::RUNNING;
    check_results_.clear();
    progress_ = 0.0;

    // Execute all checks
    const size_t total_checks = 6;
    size_t current_check = 0;

    // T089: Battery level check
    check_results_.push_back(checkBatteryLevel());
    updateProgress(++current_check, total_checks);

    // T087: Sensor diagnostics
    check_results_.push_back(checkSensorDiagnostics());
    updateProgress(++current_check, total_checks);

    // Motor status check
    check_results_.push_back(checkMotorStatus());
    updateProgress(++current_check, total_checks);

    // Emergency stop check
    check_results_.push_back(checkEmergencyStop());
    updateProgress(++current_check, total_checks);

    // T089: Maintenance schedule check
    check_results_.push_back(checkMaintenanceSchedule());
    updateProgress(++current_check, total_checks);

    // Communication check
    check_results_.push_back(checkCommunicationStatus());
    updateProgress(++current_check, total_checks);

    // Determine overall result
    bool has_failures = hasFailedChecks();
    bool has_critical = hasCriticalFailures();

    if (has_critical) {
        status_ = SequenceStatus::ERROR;
        spdlog::error("[SafetyCheckSequence] Critical failures detected");
        return {SequenceStatus::ERROR, "Critical safety check failures"};
    } else if (has_failures) {
        status_ = SequenceStatus::COMPLETED;
        spdlog::warn("[SafetyCheckSequence] Non-critical failures detected");
        return {SequenceStatus::COMPLETED, "Completed with warnings"};
    } else {
        status_ = SequenceStatus::COMPLETED;
        spdlog::info("[SafetyCheckSequence] All checks passed");
        return {SequenceStatus::COMPLETED, "All safety checks passed"};
    }
}

void SafetyCheckSequence::start() {
    status_ = SequenceStatus::RUNNING;
    execute();
}

void SafetyCheckSequence::pause() {
    if (status_ == SequenceStatus::RUNNING) {
        status_ = SequenceStatus::PAUSED;
    }
}

void SafetyCheckSequence::resume() {
    if (status_ == SequenceStatus::PAUSED) {
        status_ = SequenceStatus::RUNNING;
    }
}

void SafetyCheckSequence::cancel() {
    status_ = SequenceStatus::CANCELLED;
    spdlog::info("[SafetyCheckSequence] Cancelled: {}", sequence_id_);
}

bool SafetyCheckSequence::hasFailedChecks() const {
    for (const auto& result : check_results_) {
        if (!result.passed) {
            return true;
        }
    }
    return false;
}

bool SafetyCheckSequence::hasCriticalFailures() const {
    for (const auto& result : check_results_) {
        if (!result.passed && result.name == "Emergency Stop") {
            return true;
        }
    }
    return false;
}

// T089: Battery level check with threshold
SafetyCheckResult SafetyCheckSequence::checkBatteryLevel() {
    SafetyCheckResult result{"Battery Level", true, "", 0, 0};

    double battery_level = state_accessor_->getBatteryLevel();
    result.value = battery_level;
    result.threshold = LOW_BATTERY_WARNING;

    if (battery_level < CRITICAL_BATTERY_LEVEL) {
        result.passed = false;
        result.details = fmt::format("Battery critically low: {:.1f}%", battery_level * 100);

        // T090: Raise critical alarm
        alarm_manager_->raiseAlarm("E002", "SafetyCheck", result.details);
    } else if (battery_level < LOW_BATTERY_WARNING) {
        result.passed = false;
        result.details = fmt::format("Battery low: {:.1f}%", battery_level * 100);

        // T090: Raise warning alarm
        alarm_manager_->raiseAlarm("W001", "SafetyCheck", result.details);
    } else {
        result.details = fmt::format("Battery OK: {:.1f}%", battery_level * 100);
    }

    return result;
}

// T087: Sensor diagnostics implementation
SafetyCheckResult SafetyCheckSequence::checkSensorDiagnostics() {
    SafetyCheckResult result{"Sensor Diagnostics", true, "", 0, 0};

    try {
        std::any value;
        if (fieldbus_driver_->read("sensor/diagnostic/all_ok", value)) {
            bool sensors_ok = std::any_cast<bool>(value);
            result.passed = sensors_ok;
            result.value = sensors_ok ? 1.0 : 0.0;

            if (!sensors_ok) {
                result.details = "Sensor fault detected";
                alarm_manager_->raiseAlarm("W002", "SafetyCheck", "Sensor diagnostic failure");
            } else {
                result.details = "All sensors operational";
            }
        } else {
            result.passed = false;
            result.details = "Failed to read sensor status";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.details = fmt::format("Sensor check error: {}", e.what());
    }

    return result;
}

SafetyCheckResult SafetyCheckSequence::checkMotorStatus() {
    SafetyCheckResult result{"Motor Status", true, "", 0, 0};

    try {
        std::any value;
        if (fieldbus_driver_->read("motor/diagnostic/status", value)) {
            std::string status = std::any_cast<std::string>(value);
            result.passed = (status == "OK");
            result.details = fmt::format("Motor status: {}", status);

            if (!result.passed) {
                alarm_manager_->raiseAlarm("W003", "SafetyCheck", "Motor fault detected");
            }
        } else {
            result.passed = false;
            result.details = "Failed to read motor status";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.details = fmt::format("Motor check error: {}", e.what());
    }

    return result;
}

SafetyCheckResult SafetyCheckSequence::checkEmergencyStop() {
    SafetyCheckResult result{"Emergency Stop", true, "", 0, 0};

    try {
        std::any value;
        if (fieldbus_driver_->read("sensor/safety/emergency_stop", value)) {
            bool estop_active = std::any_cast<bool>(value);
            result.passed = !estop_active;
            result.value = estop_active ? 1.0 : 0.0;

            if (estop_active) {
                result.details = "Emergency stop is ACTIVE";
                alarm_manager_->raiseAlarm("E001", "SafetyCheck", "Emergency stop activated");
            } else {
                result.details = "Emergency stop OK";
            }
        } else {
            result.passed = false;
            result.details = "Failed to read emergency stop status";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.details = fmt::format("E-stop check error: {}", e.what());
    }

    return result;
}

// T089: Maintenance schedule check with thresholds
SafetyCheckResult SafetyCheckSequence::checkMaintenanceSchedule() {
    SafetyCheckResult result{"Maintenance Schedule", true, "", 0, 0};

    double total_distance = state_accessor_->getTotalDistance() / 1000.0; // Convert to km
    uint32_t completed_tasks = state_accessor_->getCompletedTaskCount();

    bool distance_due = total_distance >= MAINTENANCE_DISTANCE_KM;
    bool tasks_due = completed_tasks >= MAINTENANCE_TASK_COUNT;

    if (distance_due || tasks_due) {
        result.passed = false;
        result.value = distance_due ? total_distance : completed_tasks;
        result.threshold = distance_due ? MAINTENANCE_DISTANCE_KM : MAINTENANCE_TASK_COUNT;

        result.details = fmt::format("Maintenance due - Distance: {:.1f}km/{:.0f}km, Tasks: {}/{}",
                                     total_distance, MAINTENANCE_DISTANCE_KM,
                                     completed_tasks, MAINTENANCE_TASK_COUNT);

        // T090: Raise info-level maintenance alarm
        alarm_manager_->raiseAlarm("I001", "SafetyCheck", result.details);
    } else {
        result.details = fmt::format("Next maintenance - Distance: {:.1f}km/{:.0f}km, Tasks: {}/{}",
                                     total_distance, MAINTENANCE_DISTANCE_KM,
                                     completed_tasks, MAINTENANCE_TASK_COUNT);
    }

    return result;
}

SafetyCheckResult SafetyCheckSequence::checkCommunicationStatus() {
    SafetyCheckResult result{"Communication Status", true, "", 0, 0};

    bool connected = fieldbus_driver_->isConnected();
    result.passed = connected;
    result.value = connected ? 1.0 : 0.0;

    if (!connected) {
        result.details = "Fieldbus communication lost";
        alarm_manager_->raiseAlarm("W004", "SafetyCheck", "Communication failure");
    } else {
        result.details = "Communication OK";
    }

    return result;
}

void SafetyCheckSequence::raiseAlarmIfNeeded(const SafetyCheckResult& result) {
    // Already handled in individual check methods
}

void SafetyCheckSequence::updateProgress(double current, double total) {
    progress_ = current / total;
    spdlog::trace("[SafetyCheckSequence] Progress: {:.0f}%", progress_ * 100);
}

} // namespace mxrc::robot::pallet_shuttle