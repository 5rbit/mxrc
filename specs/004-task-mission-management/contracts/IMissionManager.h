#ifndef I_MISSION_MANAGER_H
#define I_MISSION_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include "../../../../src/core/task/TaskContext.h"
#include "../../../../src/core/task/AbstractTask.h"

namespace mxrc {
namespace task {

// Forward declarations
struct MissionState;

/**
 * @brief Interface for managing the lifecycle and execution of Missions.
 */
class IMissionManager {
public:
    virtual ~IMissionManager() = default;

    /**
     * @brief Loads a mission definition from a file.
     * @param missionDefinitionPath Path to the JSON/YAML mission definition file.
     * @return True if mission definition is successfully loaded and validated, false otherwise.
     */
    virtual bool loadMissionDefinition(const std::string& missionDefinitionPath) = 0;

    /**
     * @brief Starts a loaded mission.
     * @param missionId The ID of the mission to start.
     * @param initialContext Initial context parameters for the mission.
     * @return Mission instance ID if successful, empty string otherwise.
     */
    virtual std::string startMission(const std::string& missionId, const TaskContext& initialContext) = 0;

    /**
     * @brief Pauses a running mission.
     * @param missionInstanceId The instance ID of the mission to pause.
     * @return True if successful, false otherwise.
     */
    virtual bool pauseMission(const std::string& missionInstanceId) = 0;

    /**
     * @brief Resumes a paused mission.
     * @param missionInstanceId The instance ID of the mission to resume.
     * @return True if successful, false otherwise.
     */
    virtual bool resumeMission(const std::string& missionInstanceId) = 0;

    /**
     * @brief Cancels a running or paused mission.
     * @param missionInstanceId The instance ID of the mission to cancel.
     * @return True if successful, false otherwise.
     */
    virtual bool cancelMission(const std::string& missionInstanceId) = 0;

    /**
     * @brief Inserts an emergency task into a running mission.
     * @param missionInstanceId The instance ID of the mission.
     * @param emergencyTask The emergency task to insert.
     * @param priority Priority of the emergency task (higher value = higher priority).
     * @return True if successful, false otherwise.
     */
    virtual bool insertEmergencyTask(const std::string& missionInstanceId, std::unique_ptr<AbstractTask> emergencyTask, int priority) = 0;

    /**
     * @brief Skips the currently executing task in a mission.
     * @param missionInstanceId The instance ID of the mission.
     * @return True if successful, false otherwise.
     */
    virtual bool skipCurrentTask(const std::string& missionInstanceId) = 0;

    /**
     * @brief Retrieves the current state of a mission.
     * @param missionInstanceId The instance ID of the mission.
     * @return MissionState object if found, or an empty/invalid state if not.
     */
    virtual MissionState getMissionState(const std::string& missionInstanceId) const = 0;

    /**
     * @brief Retrieves the current state of a specific task within a mission.
     * @param missionInstanceId The instance ID of the mission.
     * @param taskInstanceId The instance ID of the task.
     * @return TaskState object if found, or an empty/invalid state if not.
     */
    virtual TaskState getTaskState(const std::string& missionInstanceId, const std::string& taskInstanceId) const = 0;

    /**
     * @brief Recovers a mission from its last known safe state after an unexpected shutdown.
     * @param missionInstanceId The instance ID of the mission to recover.
     * @return True if recovery is successful, false otherwise.
     */
    virtual bool recoverMission(const std::string& missionInstanceId) = 0;
};

} // namespace task
} // namespace mxrc

#endif // I_MISSION_MANAGER_H