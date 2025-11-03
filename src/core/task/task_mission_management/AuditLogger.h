#ifndef AUDIT_LOGGER_H
#define AUDIT_LOGGER_H

#include <string>
#include <chrono>
#include <map>
#include <any>
#include <memory>
#include "../../datastore/DataStore.h" // Assuming DataStore is here
#include "TaskDependencyManager.h" // For TaskState enum

namespace mxrc {
namespace task_mission {

// AuditLog Entry structure as defined in data-model.md
struct AuditLogEntry {
    std::chrono::system_clock::time_point timestamp;
    std::string event_type;
    std::string user_id;
    std::string mission_instance_id;
    std::string task_instance_id;
    std::map<std::string, std::any> details;

    AuditLogEntry() : timestamp(std::chrono::system_clock::now()) {}
};

// TaskStateHistory Entry structure as defined in data-model.md
struct TaskStateHistory {
    std::string task_instance_id;
    std::chrono::system_clock::time_point timestamp;
    TaskState old_state;
    TaskState new_state;
    std::string reason;
    std::map<std::string, std::any> error_info;

    TaskStateHistory() : timestamp(std::chrono::system_clock::now()) {}
};

class AuditLogger {
public:
    AuditLogger(mxrc::datastore::DataStore* dataStore);
    ~AuditLogger() = default;

    /**
     * @brief Logs an audit event to the DataStore.
     * @param entry The AuditLogEntry to log.
     * @return True if logging is successful, false otherwise.
     */
    bool logEvent(const AuditLogEntry& entry);

    /**
     * @brief Logs a task state change event to the DataStore.
     * @param entry The TaskStateHistory to log.
     * @return True if logging is successful, false otherwise.
     */
    bool logTaskStateChange(const TaskStateHistory& entry);

private:
    mxrc::datastore::DataStore* data_store_;
};

} // namespace task_mission
} // namespace mxrc

#endif // AUDIT_LOGGER_H
