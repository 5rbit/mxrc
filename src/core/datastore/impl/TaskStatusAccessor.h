#pragma once

#include "core/datastore/interfaces/ITaskStatusAccessor.h"
#include "core/datastore/DataStore.h"
#include <array>
#include <string_view>
#include <stdexcept>

namespace mxrc::core::datastore {

/**
 * @brief Concrete implementation of ITaskStatusAccessor
 *
 * This class provides high-performance, type-safe access to task status data
 * stored in the DataStore. All methods are inline for zero-overhead abstraction.
 *
 * Design Principles (Feature 022):
 * - Non-owning reference to DataStore (RAII compliance)
 * - Inline methods for performance (target: < 60ns getter, < 110ns setter)
 * - Compile-time validation of allowed keys
 * - RT-safe: no dynamic allocation, lock-free operations
 * - Input validation for progress (0.0-1.0 range)
 *
 * Usage Example:
 * @code
 * auto datastore = DataStore::create();
 * TaskStatusAccessor accessor(*datastore);
 *
 * // RT path: Update task state
 * accessor.setTaskState(TaskState::RUNNING);
 * accessor.setProgress(0.5);  // 50% complete
 *
 * // RT path: Read task state
 * auto state = accessor.getTaskState();
 * if (state.value == TaskState::FAILED) {
 *     auto error = accessor.getErrorCode();
 *     spdlog::error("Task failed with error code: {}", error.value);
 * }
 * @endcode
 */
class TaskStatusAccessor : public ITaskStatusAccessor {
public:
    /**
     * @brief Construct accessor with DataStore reference
     *
     * @param datastore Reference to the DataStore instance (must outlive this accessor)
     */
    explicit TaskStatusAccessor(DataStore& datastore)
        : datastore_(datastore) {}

    /**
     * @brief Get domain name
     * @return "task_status"
     */
    std::string getDomain() const override {
        return "task_status";
    }

    // ========================================================================
    // Getter Methods (Inline for Performance)
    // ========================================================================

    inline VersionedData<TaskState> getTaskState() const override {
        return datastore_.getVersioned<TaskState>(KEYS[0]);
    }

    inline VersionedData<double> getProgress() const override {
        return datastore_.getVersioned<double>(KEYS[1]);
    }

    inline VersionedData<int> getErrorCode() const override {
        return datastore_.getVersioned<int>(KEYS[2]);
    }

    // ========================================================================
    // Setter Methods (Inline for Performance)
    // ========================================================================

    inline void setTaskState(TaskState value) override {
        datastore_.setVersioned<TaskState>(KEYS[0], value, DataType::TaskState);
    }

    inline void setProgress(double value) override {
        // Input validation: progress must be in range [0.0, 1.0]
        if (value < 0.0 || value > 1.0) {
            throw std::invalid_argument(
                "TaskStatusAccessor::setProgress: value must be in range [0.0, 1.0], got " +
                std::to_string(value));
        }
        datastore_.setVersioned<double>(KEYS[1], value, DataType::TaskState);
    }

    inline void setErrorCode(int value) override {
        datastore_.setVersioned<int>(KEYS[2], value, DataType::TaskState);
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
     * All keys follow the "task_status.*" naming convention.
     */
    static constexpr std::array<const char*, 3> KEYS = {
        "task_status.state",       // Index 0
        "task_status.progress",    // Index 1
        "task_status.error_code"   // Index 2
    };
};

} // namespace mxrc::core::datastore
