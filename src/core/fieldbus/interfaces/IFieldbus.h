#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <optional>

namespace mxrc::core::fieldbus {

/**
 * @brief Fieldbus status enumeration
 */
enum class FieldbusStatus {
    UNINITIALIZED,  ///< Not yet initialized
    INITIALIZED,    ///< Initialized but not started
    RUNNING,        ///< Actively communicating
    ERROR,          ///< Error state
    STOPPED         ///< Stopped
};

/**
 * @brief Fieldbus configuration
 */
struct FieldbusConfig {
    std::string protocol;        ///< Protocol name (e.g., "EtherCAT", "CANopen")
    std::string config_file;     ///< Path to configuration file
    uint32_t cycle_time_us;      ///< Cycle time in microseconds
    bool enable_diagnostics{false}; ///< Enable diagnostics
    size_t device_count{64};     ///< Number of devices (default: 64)
};

/**
 * @brief Fieldbus statistics
 */
struct FieldbusStats {
    uint64_t total_cycles{0};       ///< Total number of cycles
    uint64_t missed_cycles{0};      ///< Number of missed deadlines
    uint64_t communication_errors{0}; ///< Communication errors
    uint64_t bytes_sent{0};         ///< Total bytes sent
    uint64_t bytes_received{0};     ///< Total bytes received
    double avg_cycle_time_us{0.0};  ///< Average cycle time
    double max_cycle_time_us{0.0};  ///< Maximum cycle time
};

/**
 * @brief Abstract interface for fieldbus communication
 *
 * This interface provides a protocol-agnostic abstraction for fieldbus
 * communication. Concrete implementations (EtherCAT, CANopen, etc.) must
 * implement this interface.
 *
 * Design Goals (Feature 019 US4):
 * - Protocol independence: Support multiple fieldbus protocols
 * - Lifecycle management: Initialize, start, stop, shutdown
 * - Data exchange: Read sensors, write actuators
 * - Diagnostics: Status monitoring and statistics
 * - Real-time safety: Thread-safe, deterministic operations
 *
 * Usage Example:
 * @code
 * auto fieldbus = FieldbusFactory::create("EtherCAT", config);
 * fieldbus->initialize();
 * fieldbus->start();
 *
 * // RT cycle
 * std::vector<double> sensor_data(64);
 * fieldbus->readSensors(sensor_data);
 *
 * std::vector<double> motor_commands(64);
 * fieldbus->writeActuators(motor_commands);
 *
 * fieldbus->stop();
 * @endcode
 */
class IFieldbus {
public:
    virtual ~IFieldbus() = default;

    /**
     * @brief Initialize fieldbus hardware and configuration
     *
     * Performs one-time initialization of the fieldbus:
     * - Load configuration file
     * - Discover connected devices
     * - Initialize communication
     * - Allocate resources
     *
     * @return true on success, false on failure
     */
    virtual bool initialize() = 0;

    /**
     * @brief Start cyclic communication
     *
     * Begins real-time cyclic data exchange:
     * - Activate fieldbus communication
     * - Start cyclic data transfer
     * - Enter operational state
     *
     * @return true on success, false on failure
     */
    virtual bool start() = 0;

    /**
     * @brief Stop cyclic communication
     *
     * Stops real-time communication:
     * - Halt cyclic data transfer
     * - Enter safe state
     * - Preserve hardware state
     *
     * @return true on success, false on failure
     */
    virtual bool stop() = 0;

    /**
     * @brief Shutdown fieldbus and release resources
     *
     * Final cleanup:
     * - Release all resources
     * - Close communication
     * - Reset hardware
     */
    virtual void shutdown() = 0;

    /**
     * @brief Read sensor data from fieldbus (RT-safe)
     *
     * Reads current sensor values from all connected devices.
     * Must be called from RT cycle.
     *
     * @param[out] data Vector to store sensor data (size must match device count)
     * @return true on success, false on communication error
     */
    virtual bool readSensors(std::vector<double>& data) = 0;

    /**
     * @brief Write actuator commands to fieldbus (RT-safe)
     *
     * Sends command values to all connected actuators.
     * Must be called from RT cycle.
     *
     * @param[in] data Vector of actuator commands (size must match device count)
     * @return true on success, false on communication error
     */
    virtual bool writeActuators(const std::vector<double>& data) = 0;

    /**
     * @brief Read digital inputs (RT-safe)
     *
     * @param[out] data Vector to store digital input states
     * @return true on success, false on error
     */
    virtual bool readDigitalInputs(std::vector<bool>& data) = 0;

    /**
     * @brief Write digital outputs (RT-safe)
     *
     * @param[in] data Vector of digital output states
     * @return true on success, false on error
     */
    virtual bool writeDigitalOutputs(const std::vector<bool>& data) = 0;

    /**
     * @brief Get current fieldbus status
     *
     * @return Current status
     */
    virtual FieldbusStatus getStatus() const = 0;

    /**
     * @brief Get fieldbus statistics
     *
     * @return Statistics structure
     */
    virtual FieldbusStats getStatistics() const = 0;

    /**
     * @brief Get protocol name
     *
     * @return Protocol identifier (e.g., "EtherCAT", "CANopen")
     */
    virtual std::string getProtocolName() const = 0;

    /**
     * @brief Get number of connected devices
     *
     * @return Device count
     */
    virtual size_t getDeviceCount() const = 0;

    /**
     * @brief Get last error message
     *
     * @return Error description, or std::nullopt if no error
     */
    virtual std::optional<std::string> getLastError() const = 0;

    /**
     * @brief Perform emergency stop (RT-safe)
     *
     * Immediately halt all actuators and enter safe state.
     * Must be callable from RT context.
     *
     * @return true on success, false on failure
     */
    virtual bool emergencyStop() = 0;

    /**
     * @brief Reset error state
     *
     * Attempts to recover from error state.
     *
     * @return true if reset successful, false otherwise
     */
    virtual bool resetErrors() = 0;
};

/**
 * @brief Shared pointer type for IFieldbus
 */
using IFieldbusPtr = std::shared_ptr<IFieldbus>;

} // namespace mxrc::core::fieldbus
