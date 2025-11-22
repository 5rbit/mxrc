#pragma once

#include "IDataAccessor.h"
#include "core/datastore/core/VersionedData.h"
#include <cstdint>

namespace mxrc::core::datastore {

/**
 * @brief Interface for sensor data domain access
 *
 * This interface provides type-safe access to sensor measurements
 * stored in the DataStore. All sensor data is versioned to support
 * RT/Non-RT synchronization and torn-read detection.
 *
 * Accessible Keys (sensor.* domain):
 * - sensor.temperature (double) - Temperature in Celsius
 * - sensor.pressure (double) - Pressure in kPa
 * - sensor.humidity (double) - Relative humidity (0-100%)
 * - sensor.vibration (double) - Vibration amplitude in m/s²
 * - sensor.current (double) - Current draw in Amperes
 *
 * Thread Safety:
 * - All getters are thread-safe (lock-free reads)
 * - All setters are thread-safe (atomic version increment)
 * - No blocking on RT paths (RT-safe)
 *
 * Performance Requirements (Feature 022):
 * - Getter latency: < 60ns average
 * - Setter latency: < 110ns average
 * - Version check latency: < 10ns average
 */
class ISensorDataAccessor : public IDataAccessor {
public:
    /**
     * @brief Virtual destructor
     */
    ~ISensorDataAccessor() override = default;

    // ========================================================================
    // Getter Methods (Read Operations)
    // ========================================================================

    /**
     * @brief Get versioned temperature measurement
     *
     * @return VersionedData<double> with value, version, and timestamp_ns
     * @throw std::runtime_error if key does not exist
     */
    virtual VersionedData<double> getTemperature() const = 0;

    /**
     * @brief Get versioned pressure measurement
     *
     * @return VersionedData<double> with value, version, and timestamp_ns
     * @throw std::runtime_error if key does not exist
     */
    virtual VersionedData<double> getPressure() const = 0;

    /**
     * @brief Get versioned humidity measurement
     *
     * @return VersionedData<double> with value, version, and timestamp_ns
     * @throw std::runtime_error if key does not exist
     */
    virtual VersionedData<double> getHumidity() const = 0;

    /**
     * @brief Get versioned vibration measurement
     *
     * @return VersionedData<double> with value, version, and timestamp_ns
     * @throw std::runtime_error if key does not exist
     */
    virtual VersionedData<double> getVibration() const = 0;

    /**
     * @brief Get versioned current measurement
     *
     * @return VersionedData<double> with value, version, and timestamp_ns
     * @throw std::runtime_error if key does not exist
     */
    virtual VersionedData<double> getCurrent() const = 0;

    // ========================================================================
    // Setter Methods (Write Operations)
    // ========================================================================

    /**
     * @brief Set temperature measurement (RT-safe)
     *
     * Atomically increments version and updates timestamp.
     *
     * @param value Temperature in Celsius
     * @throw std::runtime_error if DataStore is not initialized
     */
    virtual void setTemperature(double value) = 0;

    /**
     * @brief Set pressure measurement (RT-safe)
     *
     * Atomically increments version and updates timestamp.
     *
     * @param value Pressure in kPa
     * @throw std::runtime_error if DataStore is not initialized
     */
    virtual void setPressure(double value) = 0;

    /**
     * @brief Set humidity measurement (RT-safe)
     *
     * Atomically increments version and updates timestamp.
     *
     * @param value Relative humidity (0-100%)
     * @throw std::runtime_error if DataStore is not initialized
     */
    virtual void setHumidity(double value) = 0;

    /**
     * @brief Set vibration measurement (RT-safe)
     *
     * Atomically increments version and updates timestamp.
     *
     * @param value Vibration amplitude in m/s²
     * @throw std::runtime_error if DataStore is not initialized
     */
    virtual void setVibration(double value) = 0;

    /**
     * @brief Set current measurement (RT-safe)
     *
     * Atomically increments version and updates timestamp.
     *
     * @param value Current draw in Amperes
     * @throw std::runtime_error if DataStore is not initialized
     */
    virtual void setCurrent(double value) = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    ISensorDataAccessor() = default;
};

} // namespace mxrc::core::datastore
