#ifndef I_DATA_STORE_H
#define I_DATA_STORE_H

#include <string>
#include <vector>
#include <memory>
#include <any>
#include <chrono>
#include "../../../../src/core/datastore/DataStore.h" // Include the actual DataStore.h for SharedData, DataType, etc.

namespace mxrc {
namespace task_mission {

// Forward declarations for DataStore's types if not directly included
// class SharedData;
// enum class DataType;
// struct DataExpirationPolicy;
// class Observer;

/**
 * @brief Generic interface for persisting and retrieving data, aligned with the core DataStore functionality.
 * This interface allows for storing and retrieving various data types using a common mechanism.
 */
class IDataStore {
public:
    virtual ~IDataStore() = default;

    /**
     * @brief Saves or updates a piece of data in the store.
     * @param id Unique identifier for the data.
     * @param value The data to store (can be any type).
     * @param type The DataType enum indicating the type of data.
     * @param policy Optional expiration policy for the data.
     * @return True if successful, false otherwise.
     */
    virtual bool save(const std::string& id, const std::any& value, DataType type,
                      const DataExpirationPolicy& policy = {ExpirationPolicyType::None, std::chrono::milliseconds(0)}) = 0;

    /**
     * @brief Retrieves a piece of data from the store.
     * @param id Unique identifier for the data.
     * @return The retrieved data as std::any. Throws if not found or type mismatch.
     */
    virtual std::any load(const std::string& id) = 0;

    /**
     * @brief Removes a piece of data from the store.
     * @param id Unique identifier for the data to remove.
     * @return True if successful, false otherwise.
     */
    virtual bool remove(const std::string& id) = 0;

    // Observer pattern methods
    virtual void subscribe(const std::string& id, Observer* observer) = 0;
    virtual void unsubscribe(const std::string& id, Observer* observer) = 0;

    // State saving/loading for recovery
    virtual void saveState(const std::string& filepath) = 0;
    virtual void loadState(const std::string& filepath) = 0;

    // Other utility methods
    virtual size_t getCurrentDataCount() const = 0;
    virtual size_t getCurrentMemoryUsage() const = 0;
    virtual void cleanExpiredData() = 0;
};

} // namespace task_mission
} // namespace mxrc

#endif // I_DATA_STORE_H