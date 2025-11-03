#include "DataStore.h"
#include <mutex>
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream> // For debug prints

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
    std::cout << "[DEBUG] getPerformanceMetrics: set_calls=" << performance_metrics_.at("set_calls")
              << ", get_calls=" << performance_metrics_.at("get_calls")
              << ", poll_calls=" << performance_metrics_.at("poll_calls") << std::endl;
    return performance_metrics_;
}

void DataStore::resetPerformanceMetrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    performance_metrics_["set_calls"] = 0;
    performance_metrics_["get_calls"] = 0;
    performance_metrics_["poll_calls"] = 0;
    std::cout << "[DEBUG] resetPerformanceMetrics called. Metrics reset to 0." << std::endl;
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
