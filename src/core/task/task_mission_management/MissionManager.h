#ifndef MXRC_MISSION_MANAGER_H
#define MXRC_MISSION_MANAGER_H

#include "AbstractTask.h"
#include "TaskFactory.h"
#include "TaskContext.h"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <behaviortree_cpp/behavior_tree.h>

namespace mxrc {
namespace task_mission {

enum class MissionStatus {
    IDLE,
    RUNNING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED
};

struct MissionState {
    std::string mission_id;
    MissionStatus status;
    std::string current_task_id; // ID of the currently executing task
    double progress; // 0.0 to 1.0
    std::string estimated_completion_time; // Placeholder for now
    std::map<std::string, TaskState> task_states; // States of all tasks in the mission
};

class MissionManager {
public:
    static MissionManager& getInstance();

    MissionManager(const MissionManager&) = delete;
    MissionManager& operator=(const MissionManager&) = delete;

    // FR-004: Load mission from file
    bool loadMission(const std::string& mission_filepath);

    // FR-006: Execute mission
    bool startMission();

    // FR-008, FR-010: Control mission execution
    void pauseMission();
    void resumeMission();
    void cancelMission();
    void skipTask(const std::string& task_id);
    void insertEmergencyTask(const std::string& task_id, TaskContext& context);

    // FR-009: Get mission status
    MissionState getMissionState() const;

    // FR-016: Get task history (placeholder for now, will integrate with DataStore)
    // std::vector<TaskHistoryEntry> getTaskHistory(const std::string& task_id) const;

private:
    MissionManager();

    std::string current_mission_id_;
    MissionStatus current_mission_status_;
    std::unique_ptr<BT::Tree> behavior_tree_;
    BT::Blackboard::Ptr blackboard_;
    TaskContext mission_context_;
    std::map<std::string, std::unique_ptr<AbstractTask>> active_tasks_;
    std::mutex mutex_;

    // Helper to update mission state
    void updateMissionState();

    // Helper to execute a single task (called by BT nodes)
    BT::NodeStatus executeTaskNode(BT::TreeNode& self);
};

} // namespace task_mission
} // namespace mxrc

#endif // MXRC_MISSION_MANAGER_H