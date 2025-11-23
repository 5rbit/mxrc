#include "EtherCATDriver.h"
#include "../../ethercat/interfaces/ISlaveConfig.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace mxrc::core::fieldbus {

EtherCATDriver::EtherCATDriver(const FieldbusConfig& config, uint32_t master_index)
    : config_(config)
    , master_index_(master_index)
    , ethercat_master_(nullptr)
    , status_(FieldbusStatus::UNINITIALIZED) {

    spdlog::info("EtherCATDriver created: master_index={}, cycle_time={}us",
                 master_index_, config_.cycle_time_us);
}

EtherCATDriver::~EtherCATDriver() {
    shutdown();
}

bool EtherCATDriver::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::UNINITIALIZED) {
        last_error_ = "Driver already initialized";
        spdlog::warn("EtherCATDriver::initialize() - {}", last_error_.value());
        return false;
    }

    try {
        // Create EtherCAT master instance
        // Note: Slave configuration should be loaded from config.config_file
        // For now, we create master without slave config (can be added later)
        ethercat_master_ = std::make_unique<mxrc::ethercat::EtherCATMaster>(
            master_index_,
            nullptr  // TODO: Load ISlaveConfig from config_.config_file
        );

        // Initialize EtherCAT master
        int result = ethercat_master_->initialize();
        if (result != 0) {
            last_error_ = "EtherCAT master initialization failed";
            spdlog::error("EtherCATDriver::initialize() - {}", last_error_.value());
            status_ = FieldbusStatus::ERROR;
            return false;
        }

        // Scan slaves
        int slave_count = ethercat_master_->scanSlaves();
        if (slave_count < 0) {
            last_error_ = "EtherCAT slave scan failed";
            spdlog::error("EtherCATDriver::initialize() - {}", last_error_.value());
            status_ = FieldbusStatus::ERROR;
            return false;
        }

        device_count_ = static_cast<size_t>(slave_count);
        spdlog::info("EtherCATDriver::initialize() - Discovered {} slaves", device_count_);

        // Configure slaves
        result = ethercat_master_->configureSlaves();
        if (result != 0) {
            last_error_ = "EtherCAT slave configuration failed";
            spdlog::error("EtherCATDriver::initialize() - {}", last_error_.value());
            status_ = FieldbusStatus::ERROR;
            return false;
        }

        status_ = FieldbusStatus::INITIALIZED;
        last_error_ = std::nullopt;

        spdlog::info("EtherCATDriver initialized successfully");
        return true;

    } catch (const std::exception& e) {
        last_error_ = std::string("Exception during initialization: ") + e.what();
        spdlog::error("EtherCATDriver::initialize() - {}", last_error_.value());
        status_ = FieldbusStatus::ERROR;
        return false;
    }
}

bool EtherCATDriver::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::INITIALIZED) {
        last_error_ = "Driver not initialized";
        spdlog::warn("EtherCATDriver::start() - {}", last_error_.value());
        return false;
    }

    try {
        // Transition to OP state
        int result = ethercat_master_->transitionToOP();
        if (result != 0) {
            last_error_ = "EtherCAT transition to OP state failed";
            spdlog::error("EtherCATDriver::start() - {}", last_error_.value());
            status_ = FieldbusStatus::ERROR;
            return false;
        }

        // Activate master
        result = ethercat_master_->activate();
        if (result != 0) {
            last_error_ = "EtherCAT master activation failed";
            spdlog::error("EtherCATDriver::start() - {}", last_error_.value());
            status_ = FieldbusStatus::ERROR;
            return false;
        }

        status_ = FieldbusStatus::RUNNING;
        last_error_ = std::nullopt;

        spdlog::info("EtherCATDriver started successfully");
        return true;

    } catch (const std::exception& e) {
        last_error_ = std::string("Exception during start: ") + e.what();
        spdlog::error("EtherCATDriver::start() - {}", last_error_.value());
        status_ = FieldbusStatus::ERROR;
        return false;
    }
}

bool EtherCATDriver::stop() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::RUNNING) {
        last_error_ = "Driver not running";
        spdlog::warn("EtherCATDriver::stop() - {}", last_error_.value());
        return false;
    }

    try {
        // Deactivate master
        int result = ethercat_master_->deactivate();
        if (result != 0) {
            last_error_ = "EtherCAT master deactivation failed";
            spdlog::error("EtherCATDriver::stop() - {}", last_error_.value());
            status_ = FieldbusStatus::ERROR;
            return false;
        }

        status_ = FieldbusStatus::STOPPED;
        last_error_ = std::nullopt;

        spdlog::info("EtherCATDriver stopped successfully");
        return true;

    } catch (const std::exception& e) {
        last_error_ = std::string("Exception during stop: ") + e.what();
        spdlog::error("EtherCATDriver::stop() - {}", last_error_.value());
        status_ = FieldbusStatus::ERROR;
        return false;
    }
}

void EtherCATDriver::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ == FieldbusStatus::RUNNING) {
        ethercat_master_->deactivate();
    }

    ethercat_master_.reset();
    status_ = FieldbusStatus::UNINITIALIZED;

    spdlog::info("EtherCATDriver shutdown complete");
}

bool EtherCATDriver::readSensors(std::vector<double>& data) {
    // RT-safe operation
    if (status_ != FieldbusStatus::RUNNING) {
        return false;
    }

    if (!ethercat_master_) {
        return false;
    }

    try {
        // Receive EtherCAT data
        int result = ethercat_master_->receive();
        if (result != 0) {
            stats_.communication_errors++;
            return false;
        }

        // TODO: Parse PDO data from domain and populate sensor vector
        // For now, return success (actual PDO parsing requires domain mapping)
        // This would involve reading from ethercat_master_->getDomainData()
        // and parsing based on PDO configuration

        stats_.bytes_received += data.size() * sizeof(double);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("EtherCATDriver::readSensors() - Exception: {}", e.what());
        stats_.communication_errors++;
        return false;
    }
}

bool EtherCATDriver::writeActuators(const std::vector<double>& data) {
    // RT-safe operation
    if (status_ != FieldbusStatus::RUNNING) {
        return false;
    }

    if (!ethercat_master_) {
        return false;
    }

    try {
        // TODO: Write actuator commands to PDO domain
        // For now, just send the frame
        // This would involve writing to ethercat_master_->getDomainData()
        // and updating based on PDO configuration

        // Send EtherCAT data
        int result = ethercat_master_->send();
        if (result != 0) {
            stats_.communication_errors++;
            return false;
        }

        stats_.bytes_sent += data.size() * sizeof(double);
        stats_.total_cycles++;

        return true;

    } catch (const std::exception& e) {
        spdlog::error("EtherCATDriver::writeActuators() - Exception: {}", e.what());
        stats_.communication_errors++;
        return false;
    }
}

bool EtherCATDriver::readDigitalInputs(std::vector<bool>& data) {
    // RT-safe operation
    if (status_ != FieldbusStatus::RUNNING) {
        return false;
    }

    // TODO: Implement digital input reading from PDO domain
    // For now, return success with no data modification
    return true;
}

bool EtherCATDriver::writeDigitalOutputs(const std::vector<bool>& data) {
    // RT-safe operation
    if (status_ != FieldbusStatus::RUNNING) {
        return false;
    }

    // TODO: Implement digital output writing to PDO domain
    // For now, return success
    return true;
}

FieldbusStatus EtherCATDriver::getStatus() const {
    return status_.load(std::memory_order_relaxed);
}

FieldbusStats EtherCATDriver::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Create local copy of stats and update from EtherCAT master
    FieldbusStats stats = stats_;
    if (ethercat_master_) {
        stats.total_cycles = ethercat_master_->getTotalCycles();
        stats.communication_errors = ethercat_master_->getSendErrorCount() +
                                     ethercat_master_->getReceiveErrorCount();
    }

    return stats;
}

std::string EtherCATDriver::getProtocolName() const {
    return "EtherCAT";
}

size_t EtherCATDriver::getDeviceCount() const {
    return device_count_;
}

std::optional<std::string> EtherCATDriver::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

bool EtherCATDriver::emergencyStop() {
    // RT-safe operation
    if (status_ != FieldbusStatus::RUNNING) {
        return false;
    }

    // TODO: Implement emergency stop
    // This would set all actuator outputs to safe values (typically 0)
    // and potentially trigger EtherCAT safe state transition

    spdlog::warn("EtherCATDriver::emergencyStop() called");

    // For now, just stop communication
    return stop();
}

bool EtherCATDriver::resetErrors() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::ERROR) {
        return true;  // No error to reset
    }

    // Clear error state
    last_error_ = std::nullopt;
    status_ = FieldbusStatus::INITIALIZED;

    spdlog::info("EtherCATDriver errors reset");
    return true;
}

FieldbusStatus EtherCATDriver::mapEtherCATState(mxrc::ethercat::MasterState master_state) const {
    switch (master_state) {
        case mxrc::ethercat::MasterState::UNINITIALIZED:
            return FieldbusStatus::UNINITIALIZED;
        case mxrc::ethercat::MasterState::INITIALIZED:
        case mxrc::ethercat::MasterState::CONFIGURED:
            return FieldbusStatus::INITIALIZED;
        case mxrc::ethercat::MasterState::ACTIVATED:
            return FieldbusStatus::RUNNING;
        case mxrc::ethercat::MasterState::ERROR:
            return FieldbusStatus::ERROR;
        default:
            return FieldbusStatus::ERROR;
    }
}

void EtherCATDriver::updateStatistics() {
    if (ethercat_master_) {
        stats_.total_cycles = ethercat_master_->getTotalCycles();
        stats_.communication_errors = ethercat_master_->getSendErrorCount() +
                                     ethercat_master_->getReceiveErrorCount();
    }
}

} // namespace mxrc::core::fieldbus
