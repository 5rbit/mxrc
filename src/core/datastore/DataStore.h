#ifndef DATASTORE_H
#define DATASTORE_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <any>
#include <chrono>
#include <functional> // For std::function
#include <mutex>
#include <stdexcept>

// Forward declarations
class Observer;

// Enum for data types
enum class DataType {
    RobotMode,
    InterfaceData, // Generic for Drive, I/O, Communication
    Config,
    Para,
    Alarm,
    Event,
    MissionState,
    TaskState,
    // Add more as needed
};

// Struct to hold shared data
struct SharedData {
    std::string id;
    DataType type;
    std::any value; // Use std::any for flexible data types
    std::chrono::time_point<std::chrono::system_clock> timestamp;
    std::chrono::time_point<std::chrono::system_clock> expiration_time; // Optional
};

// Interface for data expiration policies
enum class ExpirationPolicyType {
    TTL, // Time To Live
    LRU, // Least Recently Used (not fully implemented in this interface)
    None,
};

struct DataExpirationPolicy {
    ExpirationPolicyType policy_type;
    std::chrono::milliseconds duration; // For TTL
};

// Notifier interface for Observer pattern
class Notifier {
public:
    virtual ~Notifier() = default;
    virtual void subscribe(Observer* observer) = 0;
    virtual void unsubscribe(Observer* observer) = 0;
    virtual void notify(const SharedData& changed_data) = 0;
};

// Observer interface
class Observer {
public:
    virtual ~Observer() = default;
    virtual void onDataChanged(const SharedData& changed_data) = 0;
};

// DataStore class (Singleton)
class DataStore {
public:
    // FR-001: Singleton pattern
    static DataStore& getInstance();

    // Delete copy constructor and assignment operator
    DataStore(const DataStore&) = delete;
    DataStore& operator=(const DataStore&) = delete;

    // FR-006: Set/Get interfaces for data manipulation
    template<typename T>
    void set(const std::string& id, const T& data, DataType type,
             const DataExpirationPolicy& policy = {ExpirationPolicyType::None, std::chrono::milliseconds(0)});

    template<typename T>
    T get(const std::string& id);

    // FR-002: Polling interface for specific data types (e.g., Interface Module data)
    template<typename T>
    T poll(const std::string& id);

    // FR-003: Notifier (Observer pattern) for Alarm and Event data
    void subscribe(const std::string& id, Observer* observer);
    void unsubscribe(const std::string& id, Observer* observer);

    // FR-007: Data expiration policy management
    void applyExpirationPolicy(const std::string& id, const DataExpirationPolicy& policy);
    void removeExpirationPolicy(const std::string& id);
    void cleanExpiredData(); // To be called periodically

    // FR-008: Observability - Metrics and Logs
    // These would typically be integrated with a separate logging/metrics module
    // For simplicity, we'll just define interfaces here.
    std::map<std::string, double> getPerformanceMetrics() const;
    std::vector<std::string> getAccessLogs() const;
    std::vector<std::string> getErrorLogs() const;

    // FR-009: Error handling - exceptions
    // get() and poll() methods will throw if data not found or type mismatch
    // set() will throw if data type is inconsistent with existing data (if strict type checking is desired)

    // FR-011: Scalability monitoring
    size_t getCurrentDataCount() const;
    size_t getCurrentMemoryUsage() const; // FR-013: For SharedData memory usage

    // FR-012: Data recovery
    void saveState(const std::string& filepath);
    void loadState(const std::string& filepath);

    // FR-014: Security - Access Control
    void setAccessPolicy(const std::string& id, const std::string& module_id, bool can_access);
    bool hasAccess(const std::string& id, const std::string& module_id) const;

private:
    DataStore(); // Private constructor for singleton

    std::map<std::string, SharedData> data_map_;
    std::map<std::string, std::unique_ptr<Notifier>> notifiers_;
    std::map<std::string, DataExpirationPolicy> expiration_policies_;
    std::map<std::string, std::map<std::string, bool>> access_policies_; // id -> module_id -> can_access

    // Internal helper for notification
    void notifySubscribers(const SharedData& changed_data);

    // For observability
    std::map<std::string, double> performance_metrics_;
    std::vector<std::string> access_logs_;
    std::vector<std::string> error_logs_;

    // FR-004, FR-005: Thread safety and low latency
    // Implementation details would involve mutexes, atomic operations, or lock-free data structures
    // For this interface, we just acknowledge the requirement.

    static DataStore* instance_; // Static instance pointer
    static std::mutex mutex_; // Mutex for thread-safe singleton
};

// Template method implementations (typically in a .tpp or .h for header-only)
template<typename T>
void DataStore::set(const std::string& id, const T& data, DataType type,
                    const DataExpirationPolicy& policy) {
    // FR-014: Access control check
    // For simplicity, assuming current_module_id is available in context or passed.
    // if (!hasAccess(id, "current_module_id")) { throw std::runtime_error("Access denied for ID: " + id); }

    // FR-004, FR-005: Locking mechanism for thread safety
    std::lock_guard<std::mutex> lock(mutex_);

    // FR-009: Error handling - check for type consistency if data already exists
    if (data_map_.count(id) && data_map_[id].type != type) {
        throw std::runtime_error("Data type mismatch for existing ID: " + id);
    }

    SharedData new_data;
    new_data.id = id;
    new_data.type = type;
    new_data.value = data;
    new_data.timestamp = std::chrono::system_clock::now();
    new_data.expiration_time = (policy.policy_type == ExpirationPolicyType::TTL) ?
                                new_data.timestamp + policy.duration :
                                std::chrono::time_point<std::chrono::system_clock>();

    data_map_[id] = new_data;

    // FR-007: Applying expiration policy (stored, actual cleanup by cleanExpiredData)
    expiration_policies_[id] = policy;

    // FR-003: Notifying subscribers if applicable
    notifySubscribers(new_data);

    // FR-008: Logging access (basic placeholder)
    // access_logs_.push_back("Set data: " + id);

    // Update performance metrics (placeholder)
    performance_metrics_["set_calls"]++;
    // Add latency measurement here
}

template<typename T>
T DataStore::get(const std::string& id) {
    // FR-014: Access control check
    // if (!hasAccess(id, "current_module_id")) { throw std::runtime_error("Access denied for ID: " + id); }

    // FR-004, FR-005: Locking mechanism for thread safety
    std::lock_guard<std::mutex> lock(mutex_);

    // FR-009: Error handling - throw if not found
    if (data_map_.find(id) == data_map_.end()) {
        throw std::out_of_range("Data not found for ID: " + id);
    }

    SharedData& stored_data = data_map_[id];

    // FR-009: Error handling - type checking and casting
    if (stored_data.value.type() != typeid(T)) {
        throw std::runtime_error("Type mismatch for ID: " + id);
    }

    // FR-008: Logging access (basic placeholder)
    // access_logs_.push_back("Get data: " + id);

    // Update performance metrics (placeholder)
    performance_metrics_["get_calls"]++;
    // Add latency measurement here

    return std::any_cast<T>(stored_data.value);
}

template<typename T>
T DataStore::poll(const std::string& id) {
    // FR-014: Access control check
    // if (!hasAccess(id, "current_module_id")) { throw std::runtime_error("Access denied for ID: " + id); }

    // FR-004, FR-005: Locking mechanism for thread safety
    std::lock_guard<std::mutex> lock(mutex_);

    // FR-009: Error handling - throw if not found
    if (data_map_.find(id) == data_map_.end()) {
        throw std::out_of_range("Data not found for ID: " + id);
    }

    SharedData& stored_data = data_map_[id];

    // FR-009: Error handling - type checking and casting
    if (stored_data.value.type() != typeid(T)) {
        throw std::runtime_error("Type mismatch for ID: " + id);
    }

    // FR-008: Logging access (basic placeholder)
    // access_logs_.push_back("Poll data: " + id);

    // Update performance metrics (placeholder)
    performance_metrics_["poll_calls"]++;
    // Add latency measurement here

    return std::any_cast<T>(stored_data.value);
}

#endif // DATASTORE_H