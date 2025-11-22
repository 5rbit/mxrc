#pragma once

#include "IDataAccessor.h"
#include "core/datastore/core/VersionedData.h"
#include <vector>
#include <cstdint>

namespace mxrc::core::datastore {

/**
 * @brief Simple 3D vector for position/velocity data
 *
 * Lightweight alternative to Eigen::Vector3d for RT-safe operations.
 * POD type with value semantics.
 */
struct Vector3d {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    Vector3d() = default;
    Vector3d(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    bool operator==(const Vector3d& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const Vector3d& other) const {
        return !(*this == other);
    }
};

/**
 * @brief Interface for robot state domain access
 *
 * This interface provides type-safe access to robot state data
 * stored in the DataStore. All robot state is versioned to support
 * RT/Non-RT synchronization and torn-read detection.
 *
 * Accessible Keys (robot_state.* domain):
 * - robot_state.position (Vector3d) - Cartesian position in meters (x, y, z)
 * - robot_state.velocity (Vector3d) - Cartesian velocity in m/s (vx, vy, vz)
 * - robot_state.joint_angles (std::vector<double>) - Joint angles in radians
 * - robot_state.joint_velocities (std::vector<double>) - Joint velocities in rad/s
 *
 * Thread Safety:
 * - All getters are thread-safe (lock-free reads)
 * - All setters are thread-safe (atomic version increment)
 * - No blocking on RT paths (RT-safe)
 * - Vector pre-allocation required for RT paths (no dynamic allocation)
 *
 * Performance Requirements (Feature 022):
 * - Getter latency: < 60ns average
 * - Setter latency: < 110ns average
 * - Version check latency: < 10ns average
 */
class IRobotStateAccessor : public IDataAccessor {
public:
    /**
     * @brief Virtual destructor
     */
    ~IRobotStateAccessor() override = default;

    // ========================================================================
    // Getter Methods (Read Operations)
    // ========================================================================

    /**
     * @brief Get versioned Cartesian position
     *
     * @return VersionedData<Vector3d> with position, version, and timestamp_ns
     * @throw std::runtime_error if key does not exist
     */
    virtual VersionedData<Vector3d> getPosition() const = 0;

    /**
     * @brief Get versioned Cartesian velocity
     *
     * @return VersionedData<Vector3d> with velocity, version, and timestamp_ns
     * @throw std::runtime_error if key does not exist
     */
    virtual VersionedData<Vector3d> getVelocity() const = 0;

    /**
     * @brief Get versioned joint angles
     *
     * @return VersionedData<std::vector<double>> with angles, version, and timestamp_ns
     * @throw std::runtime_error if key does not exist
     */
    virtual VersionedData<std::vector<double>> getJointAngles() const = 0;

    /**
     * @brief Get versioned joint velocities
     *
     * @return VersionedData<std::vector<double>> with velocities, version, and timestamp_ns
     * @throw std::runtime_error if key does not exist
     */
    virtual VersionedData<std::vector<double>> getJointVelocities() const = 0;

    // ========================================================================
    // Setter Methods (Write Operations)
    // ========================================================================

    /**
     * @brief Set Cartesian position (RT-safe)
     *
     * Atomically increments version and updates timestamp.
     *
     * @param value Cartesian position in meters (x, y, z)
     * @throw std::runtime_error if DataStore is not initialized
     */
    virtual void setPosition(const Vector3d& value) = 0;

    /**
     * @brief Set Cartesian velocity (RT-safe)
     *
     * Atomically increments version and updates timestamp.
     *
     * @param value Cartesian velocity in m/s (vx, vy, vz)
     * @throw std::runtime_error if DataStore is not initialized
     */
    virtual void setVelocity(const Vector3d& value) = 0;

    /**
     * @brief Set joint angles (RT-safe, requires pre-allocated vector)
     *
     * Atomically increments version and updates timestamp.
     *
     * WARNING: For RT paths, the vector must be pre-allocated with the correct size.
     * Dynamic memory allocation in RT context will cause latency spikes.
     *
     * @param value Joint angles in radians (pre-allocated vector)
     * @throw std::runtime_error if DataStore is not initialized
     */
    virtual void setJointAngles(const std::vector<double>& value) = 0;

    /**
     * @brief Set joint velocities (RT-safe, requires pre-allocated vector)
     *
     * Atomically increments version and updates timestamp.
     *
     * WARNING: For RT paths, the vector must be pre-allocated with the correct size.
     * Dynamic memory allocation in RT context will cause latency spikes.
     *
     * @param value Joint velocities in rad/s (pre-allocated vector)
     * @throw std::runtime_error if DataStore is not initialized
     */
    virtual void setJointVelocities(const std::vector<double>& value) = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    IRobotStateAccessor() = default;
};

} // namespace mxrc::core::datastore
