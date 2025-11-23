#pragma once

#include <string>

namespace mxrc::core::datastore {

/**
 * @brief Base interface for all domain-specific DataStore accessors
 *
 * This interface establishes the foundation for the Accessor pattern,
 * which provides type-safe, domain-specific access to the DataStore.
 *
 * Design Principles (Feature 022):
 * - I-prefix naming convention for interfaces (Principle II)
 * - Abstract base class with virtual destructor (RAII compliance)
 * - Domain isolation: Each accessor manages a specific data domain
 *
 * @see ISensorDataAccessor for sensor domain access
 * @see IRobotStateAccessor for robot state domain access
 * @see ITaskStatusAccessor for task status domain access
 */
class IDataAccessor {
public:
    /**
     * @brief Virtual destructor for proper cleanup in derived classes
     */
    virtual ~IDataAccessor() = default;

    /**
     * @brief Get the domain name for this accessor
     *
     * The domain name is used for logging, debugging, and metrics.
     * Common domains: "sensor", "robot_state", "task_status"
     *
     * @return Domain name string (e.g., "sensor", "robot_state")
     */
    virtual std::string getDomain() const = 0;

protected:
    /**
     * @brief Protected default constructor
     *
     * Prevents direct instantiation of the interface.
     * Only derived classes can call this constructor.
     */
    IDataAccessor() = default;

    /**
     * @brief Protected copy constructor (deleted)
     *
     * Accessors should not be copied to avoid ownership issues.
     * Use references or pointers to share accessors.
     */
    IDataAccessor(const IDataAccessor&) = delete;

    /**
     * @brief Protected copy assignment operator (deleted)
     *
     * Accessors should not be copied to avoid ownership issues.
     */
    IDataAccessor& operator=(const IDataAccessor&) = delete;

    /**
     * @brief Protected move constructor (deleted)
     *
     * Accessors hold references to DataStore, so moving is not meaningful.
     */
    IDataAccessor(IDataAccessor&&) = delete;

    /**
     * @brief Protected move assignment operator (deleted)
     *
     * Accessors hold references to DataStore, so moving is not meaningful.
     */
    IDataAccessor& operator=(IDataAccessor&&) = delete;
};

} // namespace mxrc::core::datastore
