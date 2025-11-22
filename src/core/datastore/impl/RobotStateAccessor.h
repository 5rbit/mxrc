#pragma once

#include "core/datastore/interfaces/IRobotStateAccessor.h"
#include "core/datastore/DataStore.h"
#include <array>
#include <string_view>

namespace mxrc::core::datastore {

/**
 * @brief Concrete implementation of IRobotStateAccessor
 *
 * This class provides high-performance, type-safe access to robot state data
 * stored in the DataStore. All methods are inline for zero-overhead abstraction.
 *
 * Design Principles (Feature 022):
 * - Non-owning reference to DataStore (RAII compliance)
 * - Inline methods for performance (target: < 60ns getter, < 110ns setter)
 * - Compile-time validation of allowed keys
 * - RT-safe: no dynamic allocation for Vector3d, pre-allocated vectors required
 *
 * Usage Example:
 * @code
 * auto datastore = DataStore::create();
 * RobotStateAccessor accessor(*datastore);
 *
 * // RT path: Read position
 * auto pos = accessor.getPosition();
 * spdlog::info("Position: ({:.2f}, {:.2f}, {:.2f})",
 *              pos.value.x, pos.value.y, pos.value.z);
 *
 * // RT path: Write position (stack-allocated)
 * accessor.setPosition(Vector3d{1.0, 2.0, 3.0});
 *
 * // CAUTION: Joint angles require pre-allocated vector for RT safety
 * std::vector<double> angles(6);  // Pre-allocate for 6-DOF robot
 * angles = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6};
 * accessor.setJointAngles(angles);
 * @endcode
 */
class RobotStateAccessor : public IRobotStateAccessor {
public:
    /**
     * @brief Construct accessor with DataStore reference
     *
     * @param datastore Reference to the DataStore instance (must outlive this accessor)
     */
    explicit RobotStateAccessor(DataStore& datastore)
        : datastore_(datastore) {}

    /**
     * @brief Get domain name
     * @return "robot_state"
     */
    std::string getDomain() const override {
        return "robot_state";
    }

    // ========================================================================
    // Getter Methods (Inline for Performance)
    // ========================================================================

    inline VersionedData<Vector3d> getPosition() const override {
        return datastore_.getVersioned<Vector3d>(KEYS[0]);
    }

    inline VersionedData<Vector3d> getVelocity() const override {
        return datastore_.getVersioned<Vector3d>(KEYS[1]);
    }

    inline VersionedData<std::vector<double>> getJointAngles() const override {
        return datastore_.getVersioned<std::vector<double>>(KEYS[2]);
    }

    inline VersionedData<std::vector<double>> getJointVelocities() const override {
        return datastore_.getVersioned<std::vector<double>>(KEYS[3]);
    }

    // ========================================================================
    // Setter Methods (Inline for Performance)
    // ========================================================================

    inline void setPosition(const Vector3d& value) override {
        datastore_.setVersioned<Vector3d>(KEYS[0], value, DataType::RobotMode);
    }

    inline void setVelocity(const Vector3d& value) override {
        datastore_.setVersioned<Vector3d>(KEYS[1], value, DataType::RobotMode);
    }

    inline void setJointAngles(const std::vector<double>& value) override {
        // WARNING: For RT paths, the vector MUST be pre-allocated
        // Dynamic allocation inside this method will cause latency spikes
        datastore_.setVersioned<std::vector<double>>(KEYS[2], value, DataType::RobotMode);
    }

    inline void setJointVelocities(const std::vector<double>& value) override {
        // WARNING: For RT paths, the vector MUST be pre-allocated
        // Dynamic allocation inside this method will cause latency spikes
        datastore_.setVersioned<std::vector<double>>(KEYS[3], value, DataType::RobotMode);
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
     * All keys follow the "robot_state.*" naming convention.
     */
    static constexpr std::array<const char*, 4> KEYS = {
        "robot_state.position",         // Index 0
        "robot_state.velocity",         // Index 1
        "robot_state.joint_angles",     // Index 2
        "robot_state.joint_velocities"  // Index 3
    };
};

} // namespace mxrc::core::datastore
