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
#include <iosfwd> // Forward declarations for iostream types
#include <iostream> // For debug prints

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
    STRING,
    INT,
    DOUBLE,
    BOOL,
    JSON,
    BINARY // For images, etc.
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

    // Generic save/load for std::any values
    bool save(const std::string& id, const std::any& value, DataType type,
              const DataExpirationPolicy& policy = {ExpirationPolicyType::None, std::chrono::milliseconds(0)});
    std::any load(const std::string& id);
    bool remove(const std::string& id);

    // Query method for historical data
    std::vector<std::any> query(const std::string& pattern);

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
    void resetPerformanceMetrics();
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
#include "DataStore.tpp"

#endif // DATASTORE_H