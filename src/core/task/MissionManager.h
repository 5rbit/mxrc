#include <behaviortree_cpp/bt_factory.h>
#include <nlohmann/json.hpp>
#include <fstream> // For file operations
#include <sstream> // For string streams

#ifndef MXRC_MISSION_MANAGER_H
#define MXRC_MISSION_MANAGER_H

#include "AbstractTask.h"
#include "TaskFactory.h"
#include "TaskContext.h"
#include "TaskScheduler.h"
#include "MissionParser.h"
#include "TaskDependencyManager.h"
#include "contracts/IDataStore.h"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <thread> // For std::thread
#include <condition_variable> // For std::condition_variable
#include <atomic> // For std::atomic

namespace mxrc {
namespace task {



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
    std::string instance_id;
    MissionStatus current_status;
    std::string current_task_instance_id;
    double progress; // 0.0 to 1.0
    std::string estimated_completion_time; // Placeholder for now
    std::map<std::string, TaskState> active_task_states; // Use TaskState from TaskDependencyManager
};

// to_json for MissionState
void to_json(nlohmann::json& j, const MissionState& p) {
    j = nlohmann::json{
        {"mission_id", p.mission_id},
        {"instance_id", p.instance_id},
        {"current_status", p.current_status},
        {"current_task_instance_id", p.current_task_instance_id},
        {"progress", p.progress},
        {"estimated_completion_time", p.estimated_completion_time}
    };
}

// from_json for MissionState (optional, but good practice)
void from_json(const nlohmann::json& j, MissionState& p) {
    j.at("mission_id").get_to(p.mission_id);
    j.at("instance_id").get_to(p.instance_id);
    j.at("current_status").get_to(p.current_status);
    j.at("current_task_instance_id").get_to(p.current_task_instance_id);
    j.at("progress").get_to(p.progress);
    j.at("estimated_completion_time").get_to(p.estimated_completion_time);
}

class MissionManager {
public:
    static MissionManager& getInstance(std::shared_ptr<IDataStore> dataStore);
    static void resetForTesting();

    MissionManager(const MissionManager&) = delete;
    MissionManager& operator=(const MissionManager&) = delete;

    bool loadMissionDefinition(const std::string& missionDefinitionPath);
    std::string startMission(const std::string& missionId, const TaskContext& initialContext);
    bool pauseMission(const std::string& missionInstanceId);
    bool resumeMission(const std::string& missionInstanceId);
    bool cancelMission(const std::string& missionInstanceId);
    bool insertEmergencyTask(const std::string& missionInstanceId, std::unique_ptr<AbstractTask> emergencyTask, int priority);
    bool skipCurrentTask(const std::string& missionInstanceId);

    MissionState getMissionState(const std::string& missionInstanceId) const;
    TaskState getTaskState(const std::string& missionInstanceId, const std::string& taskInstanceId) const;
    bool recoverMission(const std::string& missionInstanceId);



private:
    explicit MissionManager(std::shared_ptr<IDataStore> dataStore);
    ~MissionManager();

    std::shared_ptr<IDataStore> data_store_;

    std::string current_mission_id_;
    std::string current_mission_instance_id_;
    MissionStatus current_mission_status_;
    std::unique_ptr<BT::Tree> behavior_tree_;
    BT::Blackboard::Ptr blackboard_;
    BT::BehaviorTreeFactory bt_factory_; // Behavior Tree Factory
    TaskContext mission_context_;
    TaskScheduler task_scheduler_;
    TaskDependencyManager task_dependency_manager_;
    MissionParser mission_parser_;
    std::map<std::string, MissionDefinition> mission_definitions_;

    static MissionManager* instance_; // Static instance pointer
    static std::mutex mutex_; // Mutex for thread-safe singleton

    std::thread mission_thread_; // Thread for mission execution
    std::atomic<bool> shutdown_requested_;
    std::condition_variable cv_mission_control_;
    std::mutex mission_control_mutex_;

    void missionLoop();
    void updateMissionState_internal();

    // Custom BT node for executing AbstractTask
    BT::NodeStatus executeTaskNode(BT::TreeNode& self);
};

} // namespace task
} // namespace mxrc

#endif // MXRC_MISSION_MANAGER_H