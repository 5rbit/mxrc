#include "DataStore.h"

template<typename T>
void DataStore::set(const ::std::string& id, const T& data, DataType type,
                    const DataExpirationPolicy& policy) {
    // FR-014: Access control check
    // For simplicity, assuming current_module_id is available in context or passed.
    // if (!hasAccess(id, "current_module_id")) { throw ::std::runtime_error("Access denied for ID: " + id); }

    // FR-004, FR-005: Locking mechanism for thread safety
    ::std::lock_guard<::std::mutex> lock(mutex_);

    // FR-009: Error handling - check for type consistency if data already exists
    if (data_map_.count(id) && data_map_[id].type != type) {
        throw ::std::runtime_error("Data type mismatch for existing ID: " + id);
    }

    SharedData new_data;
    new_data.id = id;
    new_data.type = type;
    new_data.value = data;
    new_data.timestamp = ::std::chrono::system_clock::now();
    new_data.expiration_time = (policy.policy_type == ExpirationPolicyType::TTL) ?
                                new_data.timestamp + policy.duration :
                                ::std::chrono::time_point<::std::chrono::system_clock>();

    data_map_[id] = new_data;

    // FR-007: Applying expiration policy (stored, actual cleanup by cleanExpiredData)
    expiration_policies_[id] = policy;

    // FR-003: Notifying subscribers if applicable
    notifySubscribers(new_data);

    // FR-008: Logging access (basic placeholder)
    // access_logs_.push_back("Set data: " + id);

    // Update performance metrics (placeholder)
    performance_metrics_["set_calls"]++;
    ::std::cout << "[DEBUG] set() called. set_calls: " << performance_metrics_["set_calls"] << ::std::endl;
    // Add latency measurement here
}

template<typename T>
T DataStore::get(const ::std::string& id) {
    // FR-014: Access control check
    // if (!hasAccess(id, "current_module_id")) { throw ::std::runtime_error("Access denied for ID: " + id); }

    // FR-004, FR-005: Locking mechanism for thread safety
    ::std::lock_guard<::std::mutex> lock(mutex_);

    // FR-009: Error handling - throw if not found
    if (data_map_.find(id) == data_map_.end()) {
        throw ::std::out_of_range("Data not found for ID: " + id);
    }

    SharedData& stored_data = data_map_[id];

    // FR-009: Error handling - type checking and casting
    try {
        // Update performance metrics (placeholder)
        performance_metrics_["get_calls"]++;
        ::std::cout << "[DEBUG] get() called. get_calls: " << performance_metrics_["get_calls"] << ::std::endl;
        return ::std::any_cast<T>(stored_data.value);
    } catch (const ::std::bad_any_cast& e) {
        throw ::std::bad_any_cast();
    }

    // FR-008: Logging access (basic placeholder)
    // access_logs_.push_back("Get data: " + id);

    // Add latency measurement here
}

template<typename T>
T DataStore::poll(const ::std::string& id) {
    // FR-014: Access control check
    // if (!hasAccess(id, "current_module_id")) { throw ::std::runtime_error("Access denied for ID: " + id); }

    // FR-004, FR-005: Locking mechanism for thread safety
    ::std::lock_guard<::std::mutex> lock(mutex_);

    // FR-009: Error handling - throw if not found
    if (data_map_.find(id) == data_map_.end()) {
        throw ::std::out_of_range("Data not found for ID: " + id);
    }

    SharedData& stored_data = data_map_[id];

    // FR-009: Error handling - type checking and casting
    try {
        // Update performance metrics (placeholder)
        performance_metrics_["poll_calls"]++;
        ::std::cout << "[DEBUG] poll() called. poll_calls: " << performance_metrics_["poll_calls"] << ::std::endl;
        return ::std::any_cast<T>(stored_data.value);
    } catch (const ::std::bad_any_cast& e) {
        throw ::std::bad_any_cast();
    }

    // FR-008: Logging access (basic placeholder)
    // access_logs_.push_back("Poll data: " + id);

    // Add latency measurement here
}