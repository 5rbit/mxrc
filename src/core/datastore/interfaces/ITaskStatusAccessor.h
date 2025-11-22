#pragma once

#include "IDataAccessor.h"
#include "core/datastore/core/VersionedData.h"
#include <cstdint>
#include <string>

namespace mxrc::core::datastore {

/**
 * @brief Task execution state enum
 *
 * Represents the lifecycle states of a task in the system.
 * Used for tracking task progress and debugging.
 */
enum class TaskState : uint8_t {
    IDLE = 0,      ///< Task not started, waiting for trigger
    RUNNING = 1,   ///< Task currently executing
    PAUSED = 2,    ///< Task paused, can be resumed
    COMPLETED = 3, ///< Task finished successfully
    FAILED = 4     ///< Task failed with error
};

/**
 * @brief Convert TaskState enum to string for logging
 *
 * @param state Task state enum value
 * @return String representation ("IDLE", "RUNNING", etc.)
 */
inline const char* taskStateToString(TaskState state) {
    switch (state) {
        case TaskState::IDLE:      return "IDLE";
        case TaskState::RUNNING:   return "RUNNING";
        case TaskState::PAUSED:    return "PAUSED";
        case TaskState::COMPLETED: return "COMPLETED";
        case TaskState::FAILED:    return "FAILED";
        default:                   return "UNKNOWN";
    }
}

/**
 * @brief Interface for task status domain access
 *
 * This interface provides type-safe access to task execution status
 * stored in the DataStore. All task status is versioned to support
 * RT/Non-RT synchronization and torn-read detection.
 *
 * Accessible Keys (task_status.* domain):
 * - task_status.state (TaskState) - Current task execution state
 * - task_status.progress (double) - Task completion progress (0.0-1.0)
 * - task_status.error_code (int) - Error code if FAILED (0 = no error)
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
class ITaskStatusAccessor : public IDataAccessor {
public:
    /**
     * @brief Virtual destructor
     */
    ~ITaskStatusAccessor() override = default;

    // ========================================================================
    // Getter Methods (Read Operations)
    // ========================================================================

    /**
     * @brief Get versioned task execution state
     *
     * @return VersionedData<TaskState> with state, version, and timestamp_ns
     * @throw std::runtime_error if key does not exist
     */
    virtual VersionedData<TaskState> getTaskState() const = 0;

    /**
     * @brief Get versioned task completion progress
     *
     * @return VersionedData<double> with progress (0.0-1.0), version, and timestamp_ns
     * @throw std::runtime_error if key does not exist
     */
    virtual VersionedData<double> getProgress() const = 0;

    /**
     * @brief Get versioned error code
     *
     * @return VersionedData<int> with error code, version, and timestamp_ns
     * @note Error code 0 means no error (success)
     * @throw std::runtime_error if key does not exist
     */
    virtual VersionedData<int> getErrorCode() const = 0;

    // ========================================================================
    // Setter Methods (Write Operations)
    // ========================================================================

    /**
     * @brief Set task execution state (RT-safe)
     *
     * Atomically increments version and updates timestamp.
     *
     * @param value Task state (IDLE, RUNNING, PAUSED, COMPLETED, FAILED)
     * @throw std::runtime_error if DataStore is not initialized
     */
    virtual void setTaskState(TaskState value) = 0;

    /**
     * @brief Set task completion progress (RT-safe)
     *
     * Atomically increments version and updates timestamp.
     *
     * @param value Progress from 0.0 (not started) to 1.0 (complete)
     * @throw std::runtime_error if DataStore is not initialized
     * @throw std::invalid_argument if value < 0.0 or value > 1.0
     */
    virtual void setProgress(double value) = 0;

    /**
     * @brief Set error code (RT-safe)
     *
     * Atomically increments version and updates timestamp.
     *
     * @param value Error code (0 = no error, non-zero = error occurred)
     * @throw std::runtime_error if DataStore is not initialized
     */
    virtual void setErrorCode(int value) = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    ITaskStatusAccessor() = default;
};

} // namespace mxrc::core::datastore
