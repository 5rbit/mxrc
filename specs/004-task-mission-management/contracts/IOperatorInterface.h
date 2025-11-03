#ifndef I_OPERATOR_INTERFACE_H
#define I_OPERATOR_INTERFACE_H

#include <string>
#include <vector>
#include <memory>
#include "../../../../src/core/task_mission_management/TaskContext.h"
#include "../../../../src/core/task_mission_management/AbstractTask.h"

namespace mxrc {
namespace task_mission {

/**
 * @brief Interface for operator interactions with the Mission and Task system.
 * This interface provides methods for operators to monitor and control missions and tasks.
 */
class IOperatorInterface {
public:
    virtual ~IOperatorInterface() = default;

    /**
     * @brief Requests to start a mission.
     * @param missionId The ID of the mission definition to start.
     * @param initialContext Initial context parameters for the mission.
     * @return Mission instance ID if successful, empty string otherwise.
     */
    virtual std::string requestStartMission(const std::string& missionId, const TaskContext& initialContext) = 0;

    /**
     * @brief Requests to pause a running mission.
     * @param missionInstanceId The instance ID of the mission to pause.
     * @return True if successful, false otherwise.
     */
    virtual bool requestPauseMission(const std::string& missionInstanceId) = 0;

    /**
     * @brief Requests to resume a paused mission.
     * @param missionInstanceId The instance ID of the mission to resume.
     * @return True if successful, false otherwise.
     */
    virtual bool requestResumeMission(const std::string& missionInstanceId) = 0;

    /**
     * @brief Requests to cancel a running or paused mission.
     * @param missionInstanceId The instance ID of the mission to cancel.
     * @return True if successful, false otherwise.
     */
    virtual bool requestCancelMission(const std::string& missionInstanceId) = 0;

    /**
     * @brief Requests to insert an emergency task into a running mission.
     * @param missionInstanceId The instance ID of the mission.
     * @param emergencyTask The emergency task to insert.
     * @param priority Priority of the emergency task (higher value = higher priority).
     * @return True if successful, false otherwise.
     */
    virtual bool requestInsertEmergencyTask(const std::string& missionInstanceId, std::unique_ptr<AbstractTask> emergencyTask, int priority) = 0;

    /**
     * @brief Requests to skip the currently executing task in a mission.
     * @param missionInstanceId The instance ID of the mission.
     * @return True if successful, false otherwise.
     */
    virtual bool requestSkipCurrentTask(const std::string& missionInstanceId) = 0;

    /**
     * @brief Retrieves the current state of a mission for monitoring.
     * @param missionInstanceId The instance ID of the mission.
     * @return MissionState object if found, or an empty/invalid state if not.
     */
    virtual MissionState getMissionStatus(const std::string& missionInstanceId) const = 0;

    /**
     * @brief Retrieves the current state of a specific task within a mission for monitoring.
     * @param missionInstanceId The instance ID of the mission.
     * @param taskInstanceId The instance ID of the task.
     * @return TaskState object if found, or an empty/invalid state if not.
     */
    virtual TaskState getTaskStatus(const std::string& missionInstanceId, const std::string& taskInstanceId) const = 0;

    /**
     * @brief Retrieves the historical state changes for a specific task.
     * @param taskInstanceId The instance ID of the task.
     * @return A vector of TaskStateHistory entries.
     */
    virtual std::vector<TaskStateHistory> getTaskHistory(const std::string& taskInstanceId) const = 0;

    /**
     * @brief Retrieves historical mission data from the DataStore.
     * @param missionInstanceId The instance ID of the mission.
     * @return A vector of historical MissionState entries.
     */
    virtual std::vector<MissionState> getMissionHistory(const std::string& missionInstanceId) const = 0;
};

} // namespace task_mission
} // namespace mxrc

#endif // I_OPERATOR_INTERFACE_H
