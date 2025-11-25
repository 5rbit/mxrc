// SafetyCheckSequence.h - 주기적 안전 점검 시퀀스
// Copyright (C) 2025 MXRC Project
//
// Feature 016: Pallet Shuttle Control System (T087)

#pragma once

#include <memory>
#include <vector>
#include <string>
#include "core/sequence/interfaces/ISequence.h"
#include "core/sequence/dto/SequenceDefinition.h"
#include "core/sequence/dto/SequenceStatus.h"

namespace mxrc::robot::pallet_shuttle {
    class IPalletShuttleStateAccessor;
}
namespace mxrc::core::alarm { class IAlarmManager; }
namespace mxrc::core::fieldbus { class IFieldbusDriver; }

namespace mxrc::robot::pallet_shuttle {

/**
 * @brief 안전 점검 결과
 */
struct SafetyCheckResult {
    std::string name;        ///< 점검 항목명
    bool passed;             ///< 통과 여부
    std::string details;     ///< 상세 정보
    double value;            ///< 측정값 (해당하는 경우)
    double threshold;        ///< 임계값 (해당하는 경우)
};

/**
 * @brief 주기적 안전 점검 시퀀스
 *
 * 다음 항목들을 점검합니다:
 * - 배터리 상태
 * - 센서 진단
 * - 모터 상태
 * - 비상 정지 상태
 * - 정비 주기
 * - 통신 상태
 *
 * Phase 8: Periodic safety checks
 */
class SafetyCheckSequence : public core::sequence::ISequence {
public:
    SafetyCheckSequence(
        const std::string& sequence_id,
        std::shared_ptr<IPalletShuttleStateAccessor> state_accessor,
        std::shared_ptr<core::alarm::IAlarmManager> alarm_manager,
        std::shared_ptr<core::fieldbus::IFieldbusDriver> fieldbus_driver);

    ~SafetyCheckSequence() = default;

    // ISequence interface
    const std::string& getId() const override { return sequence_id_; }
    const core::sequence::SequenceDefinition& getDefinition() const override { return definition_; }
    core::sequence::SequenceStatus getStatus() const override { return status_; }
    double getProgress() const override { return progress_; }

    core::sequence::SequenceResult execute() override;
    void start() override;
    void pause() override;
    void resume() override;
    void cancel() override;

    // Safety check specific methods
    const std::vector<SafetyCheckResult>& getCheckResults() const { return check_results_; }
    bool hasFailedChecks() const;
    bool hasCriticalFailures() const;

    // Maintenance thresholds
    static constexpr double MAINTENANCE_DISTANCE_KM = 50.0;     // 50km
    static constexpr uint32_t MAINTENANCE_TASK_COUNT = 500;     // 500 작업
    static constexpr double LOW_BATTERY_WARNING = 0.20;         // 20%
    static constexpr double CRITICAL_BATTERY_LEVEL = 0.10;      // 10%

private:
    std::string sequence_id_;
    core::sequence::SequenceDefinition definition_;
    core::sequence::SequenceStatus status_;
    double progress_;

    std::shared_ptr<IPalletShuttleStateAccessor> state_accessor_;
    std::shared_ptr<core::alarm::IAlarmManager> alarm_manager_;
    std::shared_ptr<core::fieldbus::IFieldbusDriver> fieldbus_driver_;

    std::vector<SafetyCheckResult> check_results_;

    // Individual check methods
    SafetyCheckResult checkBatteryLevel();
    SafetyCheckResult checkSensorDiagnostics();
    SafetyCheckResult checkMotorStatus();
    SafetyCheckResult checkEmergencyStop();
    SafetyCheckResult checkMaintenanceSchedule();
    SafetyCheckResult checkCommunicationStatus();

    void raiseAlarmIfNeeded(const SafetyCheckResult& result);
    void updateProgress(double current, double total);
};

} // namespace mxrc::robot::pallet_shuttle