#pragma once

#include "../interfaces/IFieldbus.h"
#include "../../ethercat/core/EtherCATMaster.h"
#include <atomic>
#include <mutex>
#include <memory>

namespace mxrc::core::fieldbus {

/**
 * @brief EtherCAT fieldbus driver implementing IFieldbus interface
 *
 * Feature 019 - US4: Fieldbus Abstraction Layer (T041)
 *
 * This adapter wraps the existing mxrc::ethercat::EtherCATMaster to provide
 * a protocol-agnostic IFieldbus interface for the RTExecutive.
 *
 * Design Goals:
 * - Reuse existing EtherCATMaster implementation
 * - Provide IFieldbus interface for abstraction layer
 * - Maintain backward compatibility with existing code
 * - Support real-time operations
 *
 * Usage Example:
 * @code
 * FieldbusConfig config;
 * config.protocol = "EtherCAT";
 * config.config_file = "ethercat_slaves.yaml";
 * config.cycle_time_us = 1000;
 *
 * EtherCATDriver driver(config);
 * driver.initialize();
 * driver.start();
 *
 * std::vector<double> sensors(64);
 * driver.readSensors(sensors);
 *
 * std::vector<double> commands(64, 0.0);
 * driver.writeActuators(commands);
 * @endcode
 */
class EtherCATDriver : public IFieldbus {
public:
    /**
     * @brief Construct EtherCAT driver
     *
     * @param config Fieldbus configuration
     * @param master_index EtherCAT master index (default: 0)
     */
    explicit EtherCATDriver(const FieldbusConfig& config, uint32_t master_index = 0);

    /**
     * @brief Destructor
     */
    ~EtherCATDriver() override;

    // IFieldbus interface implementation
    bool initialize() override;
    bool start() override;
    bool stop() override;
    void shutdown() override;

    bool readSensors(std::vector<double>& data) override;
    bool writeActuators(const std::vector<double>& data) override;
    bool readDigitalInputs(std::vector<bool>& data) override;
    bool writeDigitalOutputs(const std::vector<bool>& data) override;

    FieldbusStatus getStatus() const override;
    FieldbusStats getStatistics() const override;
    std::string getProtocolName() const override;
    size_t getDeviceCount() const override;
    std::optional<std::string> getLastError() const override;

    bool emergencyStop() override;
    bool resetErrors() override;

    /**
     * @brief Get underlying EtherCAT master (for advanced use cases)
     *
     * @return Pointer to EtherCATMaster
     */
    mxrc::ethercat::EtherCATMaster* getEtherCATMaster() {
        return ethercat_master_.get();
    }

private:
    /**
     * @brief Map EtherCAT master state to FieldbusStatus
     *
     * @param master_state EtherCAT master state
     * @return Corresponding FieldbusStatus
     */
    FieldbusStatus mapEtherCATState(mxrc::ethercat::MasterState master_state) const;

    /**
     * @brief Update statistics from EtherCAT master
     */
    void updateStatistics();

    // Configuration
    FieldbusConfig config_;
    uint32_t master_index_;

    // EtherCAT Master (wrapped)
    std::unique_ptr<mxrc::ethercat::EtherCATMaster> ethercat_master_;

    // State
    mutable std::mutex mutex_;
    std::atomic<FieldbusStatus> status_{FieldbusStatus::UNINITIALIZED};

    // Statistics
    FieldbusStats stats_;

    // Error handling
    std::optional<std::string> last_error_;

    // Device mapping
    // PDO data offsets and sizes would be configured here
    size_t device_count_{0};
};

} // namespace mxrc::core::fieldbus
