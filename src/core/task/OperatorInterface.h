#ifndef OPERATOR_INTERFACE_H
#define OPERATOR_INTERFACE_H

#include <string>
#include <vector>
#include <memory>
#include "TaskContext.h"
#include "AbstractTask.h"
#include "MissionManager.h"

namespace mxrc {
namespace task {

// Forward declarations
struct MissionState;
enum class TaskState;
struct TaskStateHistory;

class OperatorInterface {
public:
    static OperatorInterface& getInstance();

    OperatorInterface(const OperatorInterface&) = delete;
    OperatorInterface& operator=(const OperatorInterface&) = delete;

    std::string requestStartMission(const std::string& missionId, const TaskContext& initialContext);
    bool requestPauseMission(const std::string& missionInstanceId);
    bool requestResumeMission(const std::string& missionInstanceId);
    bool requestCancelMission(const std::string& missionInstanceId);
    bool requestInsertEmergencyTask(const std::string& missionInstanceId, std::unique_ptr<AbstractTask> emergencyTask, int priority);
    bool requestSkipCurrentTask(const std::string& missionInstanceId);

    MissionState getMissionStatus(const std::string& missionInstanceId) const;
    TaskState getTaskStatus(const std::string& missionInstanceId, const std::string& taskInstanceId) const;

    std::vector<TaskStateHistory> getTaskHistory(const std::string& taskInstanceId) const;
    std::vector<MissionState> getMissionHistory(const std::string& missionInstanceId) const;

private:
    OperatorInterface();
    ~OperatorInterface() = default;

    MissionManager& mission_manager_;
};

} // namespace task
} // namespace mxrc

#endif // OPERATOR_INTERFACE_H
