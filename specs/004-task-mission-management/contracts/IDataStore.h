#ifndef I_DATA_STORE_H
#define I_DATA_STORE_H

#include <string>
#include <vector>
#include <memory>
#include "../../../../src/core/datastore/DataStore.h" // Assuming DataStore.h defines core DataStore types

namespace mxrc {
namespace task_mission {

// Forward declarations for data model structs
struct MissionDefinition;
struct MissionState;
struct Task;
struct TaskStateHistory;
struct AuditLog;

/**
 * @brief Interface for persisting and retrieving Mission and Task related data.
 * This abstraction allows for different underlying storage implementations (e.g., SQLite, PostgreSQL).
 */
class IDataStore {
public:
    virtual ~IDataStore() = default;

    // Mission Definition Management
    virtual bool saveMissionDefinition(const MissionDefinition& definition) = 0;
    virtual std::unique_ptr<MissionDefinition> loadMissionDefinition(const std::string& missionId) = 0;
    virtual bool deleteMissionDefinition(const std::string& missionId) = 0;

    // Mission State Management
    virtual bool saveMissionState(const MissionState& state) = 0;
    virtual std::unique_ptr<MissionState> loadMissionState(const std::string& missionInstanceId) = 0;
    virtual std::vector<MissionState> loadMissionHistory(const std::string& missionInstanceId, size_t limit = 0, size_t offset = 0) = 0;

    // Task State Management
    virtual bool saveTaskState(const Task& task) = 0;
    virtual std::unique_ptr<Task> loadTaskState(const std::string& taskInstanceId) = 0;
    virtual std::vector<TaskStateHistory> loadTaskHistory(const std::string& taskInstanceId, size_t limit = 0, size_t offset = 0) = 0;

    // Audit Log Management
    virtual bool saveAuditLog(const AuditLog& logEntry) = 0;
    virtual std::vector<AuditLog> loadAuditLogs(const std::string& missionInstanceId = "", const std::string& taskInstanceId = "", size_t limit = 0, size_t offset = 0) = 0;

    // Recovery
    virtual bool persistCurrentMissionState(const MissionState& state) = 0; // For crash recovery
    virtual std::unique_ptr<MissionState> loadLastKnownMissionState() = 0; // For crash recovery
};

} // namespace task_mission
} // namespace mxrc

#endif // I_DATA_STORE_H
