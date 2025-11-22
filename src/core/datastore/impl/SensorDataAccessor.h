#pragma once

#include "core/datastore/interfaces/ISensorDataAccessor.h"
#include "core/datastore/DataStore.h"
#include <array>
#include <string_view>

namespace mxrc::core::datastore {

/**
 * @brief Concrete implementation of ISensorDataAccessor
 *
 * This class provides high-performance, type-safe access to sensor data
 * stored in the DataStore. All methods are inline for zero-overhead abstraction.
 *
 * Design Principles (Feature 022):
 * - Non-owning reference to DataStore (RAII compliance)
 * - Inline methods for performance (target: < 60ns getter, < 110ns setter)
 * - Compile-time validation of allowed keys
 * - RT-safe: no dynamic allocation, lock-free operations
 *
 * Usage Example:
 * @code
 * auto datastore = DataStore::create();
 * SensorDataAccessor accessor(*datastore);
 *
 * // RT path: Direct read
 * auto temp = accessor.getTemperature();
 * spdlog::info("Temperature: {:.2f}°C (version {})", temp.value, temp.getVersion());
 *
 * // RT path: Direct write
 * accessor.setTemperature(25.3);
 * @endcode
 */
class SensorDataAccessor : public ISensorDataAccessor {
public:
    /**
     * @brief Construct accessor with DataStore reference
     *
     * @param datastore Reference to the DataStore instance (must outlive this accessor)
     */
    explicit SensorDataAccessor(DataStore& datastore)
        : datastore_(datastore) {}

    /**
     * @brief Get domain name
     * @return "sensor"
     */
    std::string getDomain() const override {
        return "sensor";
    }

    // ========================================================================
    // Getter Methods (Inline for Performance)
    // ========================================================================

    inline VersionedData<double> getTemperature() const override {
        return datastore_.getVersioned<double>(KEYS[0]);
    }

    inline VersionedData<double> getPressure() const override {
        return datastore_.getVersioned<double>(KEYS[1]);
    }

    inline VersionedData<double> getHumidity() const override {
        return datastore_.getVersioned<double>(KEYS[2]);
    }

    inline VersionedData<double> getVibration() const override {
        return datastore_.getVersioned<double>(KEYS[3]);
    }

    inline VersionedData<double> getCurrent() const override {
        return datastore_.getVersioned<double>(KEYS[4]);
    }

    // ========================================================================
    // Setter Methods (Inline for Performance)
    // ========================================================================

    inline void setTemperature(double value) override {
        datastore_.setVersioned<double>(KEYS[0], value, DataType::RobotMode);
    }

    inline void setPressure(double value) override {
        datastore_.setVersioned<double>(KEYS[1], value, DataType::RobotMode);
    }

    inline void setHumidity(double value) override {
        datastore_.setVersioned<double>(KEYS[2], value, DataType::RobotMode);
    }

    inline void setVibration(double value) override {
        datastore_.setVersioned<double>(KEYS[3], value, DataType::RobotMode);
    }

    inline void setCurrent(double value) override {
        datastore_.setVersioned<double>(KEYS[4], value, DataType::RobotMode);
    }

private:
    /**
     * @brief Reference to DataStore (non-owning)
     *
     * The accessor does not own the DataStore, so it must outlive this accessor.
     * This ensures RAII compliance and prevents ownership issues.
     */
    DataStore& datastore_;

    /**
     * @brief Compile-time validated key list
     *
     * Using constexpr array ensures zero runtime overhead for key validation.
     * All keys follow the "sensor.*" naming convention.
     */
    static constexpr std::array<const char*, 5> KEYS = {
        "sensor.temperature",  // Index 0
        "sensor.pressure",     // Index 1
        "sensor.humidity",     // Index 2
        "sensor.vibration",    // Index 3
        "sensor.current"       // Index 4
    };
};

} // namespace mxrc::core::datastore
