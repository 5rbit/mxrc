#include "MockDriver.h"
#include <spdlog/spdlog.h>
#include <cmath>

namespace mxrc::core::fieldbus {

MockDriver::MockDriver(const FieldbusConfig& config, size_t device_count)
    : config_(config),
      device_count_(device_count),
      sensor_data_(device_count, 0.0),
      actuator_data_(device_count, 0.0),
      digital_inputs_(device_count, false),
      digital_outputs_(device_count, false) {
    spdlog::debug("[MockDriver] Created with {} devices", device_count);
}

MockDriver::~MockDriver() {
    if (status_ != FieldbusStatus::UNINITIALIZED &&
        status_ != FieldbusStatus::STOPPED) {
        shutdown();
    }
}

bool MockDriver::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::UNINITIALIZED) {
        spdlog::warn("[MockDriver] Already initialized");
        return false;
    }

    // Simulate initialization delay
    spdlog::info("[MockDriver] Initializing {} devices...", device_count_);

    // Initialize sensor data with pattern
    for (size_t i = 0; i < device_count_; ++i) {
        sensor_data_[i] = std::sin(i * 0.1);
    }

    status_ = FieldbusStatus::INITIALIZED;
    spdlog::info("[MockDriver] Initialized successfully");
    return true;
}

bool MockDriver::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Allow starting from INITIALIZED or STOPPED state
    if (status_ != FieldbusStatus::INITIALIZED &&
        status_ != FieldbusStatus::STOPPED) {
        last_error_ = "Cannot start: not initialized or stopped";
        spdlog::error("[MockDriver] {}", *last_error_);
        return false;
    }

    status_ = FieldbusStatus::RUNNING;
    last_cycle_time_ = std::chrono::steady_clock::now();
    simulation_tick_ = 0;
    emergency_stopped_ = false;

    spdlog::info("[MockDriver] Started cyclic communication");
    return true;
}

bool MockDriver::stop() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::RUNNING) {
        spdlog::warn("[MockDriver] Not running");
        return false;
    }

    status_ = FieldbusStatus::STOPPED;
    spdlog::info("[MockDriver] Stopped cyclic communication");
    return true;
}

void MockDriver::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    spdlog::info("[MockDriver] Shutting down...");

    // Clear all data
    std::fill(sensor_data_.begin(), sensor_data_.end(), 0.0);
    std::fill(actuator_data_.begin(), actuator_data_.end(), 0.0);
    std::fill(digital_inputs_.begin(), digital_inputs_.end(), false);
    std::fill(digital_outputs_.begin(), digital_outputs_.end(), false);

    status_ = FieldbusStatus::UNINITIALIZED;
    last_error_.reset();
    emergency_stopped_ = false;

    spdlog::info("[MockDriver] Shutdown complete");
}

bool MockDriver::readSensors(std::vector<double>& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::RUNNING) {
        last_error_ = "Cannot read: not running";
        return false;
    }

    if (emergency_stopped_) {
        // Return zeros when emergency stopped
        data.assign(device_count_, 0.0);
        return true;
    }

    // Simulate sensor data (sine wave + actuator echo)
    simulation_tick_++;
    for (size_t i = 0; i < device_count_; ++i) {
        sensor_data_[i] = actuator_data_[i] +
                          0.1 * std::sin(simulation_tick_ * 0.01 + i * 0.1);
    }

    // Copy to output
    if (data.size() != device_count_) {
        data.resize(device_count_);
    }
    data = sensor_data_;

    // Update statistics
    auto now = std::chrono::steady_clock::now();
    auto cycle_time_us = std::chrono::duration<double, std::micro>(
        now - last_cycle_time_).count();
    last_cycle_time_ = now;
    updateStatistics(cycle_time_us);

    return true;
}

bool MockDriver::writeActuators(const std::vector<double>& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::RUNNING) {
        last_error_ = "Cannot write: not running";
        return false;
    }

    if (emergency_stopped_) {
        last_error_ = "Cannot write: emergency stopped";
        return false;
    }

    if (data.size() != device_count_) {
        last_error_ = "Data size mismatch: expected " +
                      std::to_string(device_count_) +
                      ", got " + std::to_string(data.size());
        return false;
    }

    // Store actuator commands
    actuator_data_ = data;

    stats_.bytes_sent += data.size() * sizeof(double);
    return true;
}

bool MockDriver::readDigitalInputs(std::vector<bool>& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::RUNNING) {
        return false;
    }

    if (data.size() != device_count_) {
        data.resize(device_count_);
    }

    data = digital_inputs_;
    return true;
}

bool MockDriver::writeDigitalOutputs(const std::vector<bool>& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::RUNNING) {
        return false;
    }

    if (data.size() != device_count_) {
        last_error_ = "Digital output size mismatch";
        return false;
    }

    digital_outputs_ = data;
    return true;
}

FieldbusStatus MockDriver::getStatus() const {
    return status_.load();
}

FieldbusStats MockDriver::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

std::string MockDriver::getProtocolName() const {
    return "Mock";
}

size_t MockDriver::getDeviceCount() const {
    return device_count_;
}

std::optional<std::string> MockDriver::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

bool MockDriver::emergencyStop() {
    std::lock_guard<std::mutex> lock(mutex_);

    emergency_stopped_ = true;

    // Zero all actuators
    std::fill(actuator_data_.begin(), actuator_data_.end(), 0.0);
    std::fill(digital_outputs_.begin(), digital_outputs_.end(), false);

    spdlog::warn("[MockDriver] EMERGENCY STOP activated");
    return true;
}

bool MockDriver::resetErrors() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ == FieldbusStatus::ERROR) {
        status_ = FieldbusStatus::INITIALIZED;
        last_error_.reset();
        emergency_stopped_ = false;
        spdlog::info("[MockDriver] Errors reset");
        return true;
    }

    return false;
}

void MockDriver::setSimulatedError(const std::string& error_msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (error_msg.empty()) {
        last_error_.reset();
        if (status_ == FieldbusStatus::ERROR) {
            status_ = FieldbusStatus::INITIALIZED;
        }
    } else {
        last_error_ = error_msg;
        status_ = FieldbusStatus::ERROR;
        spdlog::error("[MockDriver] Simulated error: {}", error_msg);
    }
}

void MockDriver::updateStatistics(double cycle_time_us) {
    stats_.total_cycles++;
    stats_.bytes_received += device_count_ * sizeof(double);

    // Update average cycle time (exponential moving average)
    const double alpha = 0.1;
    if (stats_.avg_cycle_time_us == 0.0) {
        stats_.avg_cycle_time_us = cycle_time_us;
    } else {
        stats_.avg_cycle_time_us = alpha * cycle_time_us +
                                    (1.0 - alpha) * stats_.avg_cycle_time_us;
    }

    // Update max cycle time
    if (cycle_time_us > stats_.max_cycle_time_us) {
        stats_.max_cycle_time_us = cycle_time_us;
    }

    // Check for missed deadline
    if (cycle_time_us > config_.cycle_time_us * 1.1) {  // 10% tolerance
        stats_.missed_cycles++;
    }
}

} // namespace mxrc::core::fieldbus
