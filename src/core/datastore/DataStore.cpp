#include "DataStore.h"
#include <mutex>
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <sstream>

// Initialize static members
DataStore* DataStore::instance_ = nullptr;
std::mutex DataStore::mutex_;

// Concrete Notifier implementation
class MapNotifier : public Notifier {
public:
    void subscribe(Observer* observer) override {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.push_back(observer);
    }

    void unsubscribe(Observer* observer) override {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.erase(std::remove(subscribers_.begin(), subscribers_.end(), observer), subscribers_.end());
    }

    void notify(const SharedData& changed_data) override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (Observer* observer : subscribers_) {
            observer->onDataChanged(changed_data);
        }
    }

private:
    std::vector<Observer*> subscribers_;
    std::mutex mutex_;
};

DataStore::DataStore() {
    // Private constructor implementation
    performance_metrics_["set_calls"] = 0;
    performance_metrics_["get_calls"] = 0;
    performance_metrics_["poll_calls"] = 0;
}

DataStore& DataStore::getInstance() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (instance_ == nullptr) {
        instance_ = new DataStore();
    }
    return *instance_;
}

void DataStore::subscribe(const std::string& id, Observer* observer) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (notifiers_.find(id) == notifiers_.end()) {
        notifiers_[id] = std::make_unique<MapNotifier>();
    }
    notifiers_[id]->subscribe(observer);
}

void DataStore::unsubscribe(const std::string& id, Observer* observer) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (notifiers_.find(id) != notifiers_.end()) {
        notifiers_[id]->unsubscribe(observer);
    }
}

void DataStore::notifySubscribers(const SharedData& changed_data) {
    // This method is called internally by set() if a Notifier exists for the ID
    if (notifiers_.count(changed_data.id)) {
        notifiers_[changed_data.id]->notify(changed_data);
    }
}

// FR-007: Data expiration policy management
void DataStore::applyExpirationPolicy(const std::string& id, const DataExpirationPolicy& policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    expiration_policies_[id] = policy;
}

void DataStore::removeExpirationPolicy(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    expiration_policies_.erase(id);
}

void DataStore::cleanExpiredData() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    for (auto it = data_map_.begin(); it != data_map_.end(); ) {
        if (it->second.expiration_time != std::chrono::time_point<std::chrono::system_clock>() &&
            it->second.expiration_time <= now) {
            it = data_map_.erase(it);
        } else {
            ++it;
        }
    }
}

// FR-008: Observability - Metrics and Logs
std::map<std::string, double> DataStore::getPerformanceMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return performance_metrics_;
}

std::vector<std::string> DataStore::getAccessLogs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return access_logs_;
}

std::vector<std::string> DataStore::getErrorLogs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return error_logs_;
}

// FR-011: Scalability monitoring
size_t DataStore::getCurrentDataCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_map_.size();
}

// FR-013: For SharedData memory usage (placeholder implementation)
size_t DataStore::getCurrentMemoryUsage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    // This is a very basic placeholder. Actual memory usage calculation is complex.
    // It would involve iterating through data_map_ and estimating size of each std::any.
    size_t total_size = 0;
    for (const auto& pair : data_map_) {
        total_size += sizeof(SharedData); // Base size
        // Add estimated size of std::any content if possible
        // This is highly dependent on the types stored in std::any
    }
    return total_size;
}

// FR-012: Data recovery (placeholder implementation)
void DataStore::saveState(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to open file for saving state: " + filepath);
    }
    // For a real implementation, serialize data_map_ content
    ofs << "DataStore state saved (placeholder)" << std::endl;
    ofs.close();
}

void DataStore::loadState(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open file for loading state: " + filepath);
    }
    // For a real implementation, deserialize data_map_ content
    std::string line;
    std::getline(ifs, line);
    if (line != "DataStore state saved (placeholder)") {
        throw std::runtime_error("Invalid DataStore state file: " + filepath);
    }
    ifs.close();
}

// FR-014: Security - Access Control
void DataStore::setAccessPolicy(const std::string& id, const std::string& module_id, bool can_access) {
    std::lock_guard<std::mutex> lock(mutex_);
    access_policies_[id][module_id] = can_access;
}

bool DataStore::hasAccess(const std::string& id, const std::string& module_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it_id = access_policies_.find(id);
    if (it_id != access_policies_.end()) {
        auto it_module = it_id->second.find(module_id);
        if (it_module != it_id->second.end()) {
            return it_module->second;
        }
    }
    // Default to no access if no specific policy is set
    return false;
}

// Template method implementations
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
        throw std::bad_any_cast("Type mismatch for ID: " + id);
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
        throw std::bad_any_cast("Type mismatch for ID: " + id);
    }

    // FR-008: Logging access (basic placeholder)
    // access_logs_.push_back("Poll data: " + id);

    // Update performance metrics (placeholder)
    performance_metrics_["poll_calls"]++;
    // Add latency measurement here

    return std::any_cast<T>(stored_data.value);
}
