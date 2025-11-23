#pragma once

#include "../interfaces/IFieldbus.h"
#include <atomic>
#include <mutex>

namespace mxrc::core::fieldbus {

/**
 * @brief Mock fieldbus driver for testing
 *
 * Simulates a fieldbus without requiring actual hardware.
 * Useful for unit testing, integration testing, and development.
 *
 * Features:
 * - Simulates sensor data (sine wave pattern)
 * - Echoes actuator commands back as sensor readings
 * - Configurable device count
 * - Thread-safe operations
 * - Cycle time tracking
 *
 * Usage Example:
 * @code
 * FieldbusConfig config;
 * config.protocol = "Mock";
 * config.cycle_time_us = 1000;
 *
 * MockDriver driver(config);
 * driver.initialize();
 * driver.start();
 *
 * std::vector<double> sensors(64);
 * driver.readSensors(sensors);  // Returns simulated data
 *
 * std::vector<double> commands(64, 1.0);
 * driver.writeActuators(commands);  // Stored for next read
 * @endcode
 */
class MockDriver : public IFieldbus {
public:
    /**
     * @brief Construct mock driver
     *
     * @param config Fieldbus configuration
     * @param device_count Number of simulated devices (default: 64)
     */
    explicit MockDriver(const FieldbusConfig& config, size_t device_count = 64);

    /**
     * @brief Destructor
     */
    ~MockDriver() override;

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
     * @brief Set simulated error state (for testing)
     *
     * @param error_msg Error message, or empty to clear error
     */
    void setSimulatedError(const std::string& error_msg);

    /**
     * @brief Get current cycle count (for testing)
     *
     * @return Number of completed cycles
     */
    uint64_t getCycleCount() const {
        return stats_.total_cycles;
    }

private:
    /**
     * @brief Update statistics
     *
     * @param cycle_time_us Cycle time in microseconds
     */
    void updateStatistics(double cycle_time_us);

    // Configuration
    FieldbusConfig config_;
    size_t device_count_;

    // State
    std::atomic<FieldbusStatus> status_{FieldbusStatus::UNINITIALIZED};
    mutable std::mutex mutex_;

    // Data storage
    std::vector<double> sensor_data_;
    std::vector<double> actuator_data_;
    std::vector<bool> digital_inputs_;
    std::vector<bool> digital_outputs_;

    // Statistics
    FieldbusStats stats_;
    std::chrono::steady_clock::time_point last_cycle_time_;

    // Error handling
    std::optional<std::string> last_error_;
    std::atomic<bool> emergency_stopped_{false};

    // Simulation state
    uint64_t simulation_tick_{0};
};

} // namespace mxrc::core::fieldbus
