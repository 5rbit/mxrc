#include "AuditLogger.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace mxrc {
namespace task_mission {

AuditLogger::AuditLogger(mxrc::datastore::DataStore* dataStore)
    : data_store_(dataStore) {
    if (!data_store_) {
        std::cerr << "AuditLogger initialized with a null DataStore pointer!" << std::endl;
    }
}

bool AuditLogger::logEvent(const AuditLogEntry& entry) {
    if (!data_store_) {
        std::cerr << "AuditLogger: No DataStore available to log event." << std::endl;
        return false;
    }

    // Convert AuditLogEntry to a storable format (e.g., JSON string)
    // This is a simplified conversion. A more robust solution would handle std::any types properly.
    nlohmann::json details_json;
    for (const auto& [key, val_any] : entry.details) {
        // Attempt to convert std::any to common types. This is incomplete.
        if (val_any.type() == typeid(std::string)) {
            details_json[key] = std::any_cast<std::string>(val_any);
        } else if (val_any.type() == typeid(int)) {
            details_json[key] = std::any_cast<int>(val_any);
        } else if (val_any.type() == typeid(double)) {
            details_json[key] = std::any_cast<double>(val_any);
        } else if (val_any.type() == typeid(bool)) {
            details_json[key] = std::any_cast<bool>(val_any);
        } else {
            details_json[key] = "[unsupported_type]"; // Fallback for unsupported types
        }
    }

    nlohmann::json log_json;
    log_json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                entry.timestamp.time_since_epoch()).count();
    log_json["event_type"] = entry.event_type;
    log_json["user_id"] = entry.user_id;
    log_json["mission_instance_id"] = entry.mission_instance_id;
    log_json["task_instance_id"] = entry.task_instance_id;
    log_json["details"] = details_json;

    std::string log_id = "audit_" + std::to_string(log_json["timestamp"].get<long long>());

    // Save to DataStore
    // Assuming DataType::JSON for audit logs
    return data_store_->save(log_id, log_json.dump(), mxrc::datastore::DataType::JSON);
}

bool AuditLogger::logTaskStateChange(const TaskStateHistory& entry) {
    if (!data_store_) {
        std::cerr << "AuditLogger: No DataStore available to log task state change." << std::endl;
        return false;
    }

    nlohmann::json error_info_json;
    for (const auto& [key, val_any] : entry.error_info) {
        if (val_any.type() == typeid(std::string)) {
            error_info_json[key] = std::any_cast<std::string>(val_any);
        } else if (val_any.type() == typeid(int)) {
            error_info_json[key] = std::any_cast<int>(val_any);
        } else {
            error_info_json[key] = "[unsupported_type]";
        }
    }

    nlohmann::json log_json;
    log_json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                entry.timestamp.time_since_epoch()).count();
    log_json["task_instance_id"] = entry.task_instance_id;
    log_json["old_state"] = static_cast<int>(entry.old_state);
    log_json["new_state"] = static_cast<int>(entry.new_state);
    log_json["reason"] = entry.reason;
    log_json["error_info"] = error_info_json;

    std::string log_id = "task_history_" + entry.task_instance_id + "_" + std::to_string(log_json["timestamp"].get<long long>());

    return data_store_->save(log_id, log_json.dump(), mxrc::datastore::DataType::JSON);
}

} // namespace task_mission
} // namespace mxrc
