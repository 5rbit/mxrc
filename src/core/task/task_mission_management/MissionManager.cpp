#include "MissionManager.h"
#include "TaskFactory.h"
#include "ExecuteTaskNode.h" // Include for registering custom node
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <uuid/uuid.h> // For generating UUIDs

namespace mxrc {
namespace task_mission {

MissionManager* MissionManager::instance_ = nullptr;
std::mutex MissionManager::mutex_;

MissionManager::MissionManager()
    : current_mission_status_(MissionStatus::IDLE),
      blackboard_(BT::Blackboard::create()),
      bt_factory_(), // Initialize BehaviorTreeFactory
      shutdown_requested_(false),
      audit_logger_(&mxrc::DataStore::getInstance()) // Initialize AuditLogger with DataStore singleton
{
    // Register custom BehaviorTree nodes
    bt_factory_.registerNodeType<ExecuteTaskNode>("ExecuteTask");
    // Start the task scheduler
    task_scheduler_.start();
    // Start the mission thread
    mission_thread_ = std::thread(&MissionManager::missionLoop, this);
}

MissionManager::~MissionManager() {
    shutdown_requested_ = true;
    cv_mission_control_.notify_all(); // Notify the mission thread to wake up and exit
    if (mission_thread_.joinable()) {
        mission_thread_.join();
    }
    task_scheduler_.stop();
}

MissionManager& MissionManager::getInstance() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (instance_ == nullptr) {
        instance_ = new MissionManager();
    }
    return *instance_;
}

bool MissionManager::loadMissionDefinition(const std::string& missionDefinitionPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "Loading mission definition from: " << missionDefinitionPath << std::endl;

    MissionDefinition missionDef = mission_parser_.parseMissionDefinition(missionDefinitionPath);
    if (!mission_parser_.validateMissionDefinition(missionDef)) {
        std::cerr << "Mission definition validation failed for: " << missionDefinitionPath << std::endl;
        return false;
    }

    mission_definitions_[missionDef.id] = missionDef;
    std::cout << "Mission definition '" << missionDef.id << "' loaded successfully." << std::endl;

    // Save mission definition to DataStore
    audit_logger_.logEvent({
        std::chrono::system_clock::now(),
        "MissionDefinitionLoaded",
        "system",
        missionDef.id,
        "",
        {{"mission_name", missionDef.name}, {"version", missionDef.version}}
    });
    mxrc::DataStore::getInstance().save("mission_def_" + missionDef.id, missionDef.behavior_tree.dump(), mxrc::DataType::JSON);

    return true;
}

std::string MissionManager::startMission(const std::string& missionId, const TaskContext& initialContext) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (current_mission_status_ == MissionStatus::RUNNING || current_mission_status_ == MissionStatus::PAUSED) {
        std::cerr << "Cannot start new mission while one is active. Please cancel current mission first." << std::endl;
        return "";
    }

    auto it = mission_definitions_.find(missionId);
    if (it == mission_definitions_.end()) {
        std::cerr << "Mission definition '" << missionId << "' not found." << std::endl;
        return "";
    }

    // Generate a unique instance ID
    uuid_t uuid;
    uuid_generate_time(uuid);
    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);
    current_mission_instance_id_ = uuid_str;

    current_mission_id_ = missionId;
    mission_context_ = initialContext;

    try {
        // Create Behavior Tree from the loaded definition
        std::string bt_xml = it->second.behavior_tree.dump(); // Assuming behavior_tree is JSON string
        behavior_tree_ = std::make_unique<BT::Tree>(bt_factory_.createTreeFromText(bt_xml, blackboard_));

        // Reset blackboard and tree state before starting
        behavior_tree_->haltTree();
        blackboard_->clear();

        std::cout << "Starting mission '" << current_mission_id_ << "' with instance ID: " << current_mission_instance_id_ << std::endl;
        current_mission_status_ = MissionStatus::RUNNING;

        // Save initial mission state to DataStore
        MissionState initialState = getMissionState(current_mission_instance_id_);
        mxrc::DataStore::getInstance().save("mission_state_" + current_mission_instance_id_, nlohmann::json(initialState).dump(), mxrc::DataType::JSON);
        audit_logger_.logEvent({
            std::chrono::system_clock::now(),
            "MissionStarted",
            "system",
            current_mission_instance_id_,
            "",
            {{"mission_id", current_mission_id_}}
        });

        cv_mission_control_.notify_one(); // Notify the mission thread to start ticking

        return current_mission_instance_id_;
    } catch (const BT::LogicError& e) {
        std::cerr << "Error creating Behavior Tree: " << e.what() << std::endl;
        current_mission_status_ = MissionStatus::FAILED;
        return "";
    } catch (const std::exception& e) {
        std::cerr << "An unexpected error occurred while starting mission: " << e.what() << std::endl;
        current_mission_status_ = MissionStatus::FAILED;
        return "";
    }
}

void MissionManager::missionLoop() {
    while (!shutdown_requested_) {
        std::unique_lock<std::mutex> lock(mission_control_mutex_);
        cv_mission_control_.wait(lock, [&] {
            return shutdown_requested_.load() ||
                   current_mission_status_ == MissionStatus::RUNNING ||
                   current_mission_status_ == MissionStatus::PAUSED ||
                   current_mission_status_ == MissionStatus::CANCELLED;
        });

        if (shutdown_requested_.load()) {
            break; // Exit thread if scheduler is stopped
        }

        if (current_mission_status_ == MissionStatus::RUNNING && behavior_tree_) {
            BT::NodeStatus status = behavior_tree_->tickOnce();

            // Short sleep to avoid busy-looping inside the lock
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            lock.lock();

            if (status == BT::NodeStatus::SUCCESS) {
                std::lock_guard<std::mutex> main_lock(mutex_);
                MissionStatus old_status = current_mission_status_;
                current_mission_status_ = MissionStatus::COMPLETED;
                std::cout << "Mission completed successfully." << std::endl;
                // Save final mission state
                MissionState finalState = getMissionState(current_mission_instance_id_);
                mxrc::DataStore::getInstance().save("mission_state_" + current_mission_instance_id_, nlohmann::json(finalState).dump(), mxrc::DataType::JSON);
                audit_logger_.logEvent({
                    std::chrono::system_clock::now(),
                    "MissionCompleted",
                    "system",
                    current_mission_instance_id_,
                    "",
                    {{"mission_id", current_mission_id_}}
                });
                audit_logger_.logTaskStateChange({
                    current_mission_instance_id_,
                    std::chrono::system_clock::now(),
                    static_cast<TaskState>(old_status),
                    static_cast<TaskState>(current_mission_status_),
                    "Mission completed successfully.",
                    {}
                });
            } else if (status == BT::NodeStatus::FAILURE) {
                std::lock_guard<std::mutex> main_lock(mutex_);
                MissionStatus old_status = current_mission_status_;
                current_mission_status_ = MissionStatus::FAILED;
                std::cerr << "Mission failed." << std::endl;
                // Save final mission state
                MissionState finalState = getMissionState(current_mission_instance_id_);
                mxrc::DataStore::getInstance().save("mission_state_" + current_mission_instance_id_, nlohmann::json(finalState).dump(), mxrc::DataType::JSON);
                audit_logger_.logEvent({
                    std::chrono::system_clock::now(),
                    "MissionFailed",
                    "system",
                    current_mission_instance_id_,
                    "",
                    {{"mission_id", current_mission_id_}}
                });
                audit_logger_.logTaskStateChange({
                    current_mission_instance_id_,
                    std::chrono::system_clock::now(),
                    static_cast<TaskState>(old_status),
                    static_cast<TaskState>(current_mission_status_),
                    "Mission failed.",
                    {}
                });
            }
            // if RUNNING, loop continues
        } else if (current_mission_status_ == MissionStatus::CANCELLED) {
            std::lock_guard<std::mutex> main_lock(mutex_);
            MissionStatus old_status = current_mission_status_;
            current_mission_status_ = MissionStatus::IDLE; // Reset after cancellation
            if (behavior_tree_) {
                behavior_tree_->haltTree();
            }
            std::cout << "Mission instance '" << current_mission_instance_id_ << "' cancelled and reset." << std::endl;
            // Save final mission state
            MissionState finalState = getMissionState(current_mission_instance_id_);
            mxrc::DataStore::getInstance().save("mission_state_" + current_mission_instance_id_, nlohmann::json(finalState).dump(), mxrc::DataType::JSON);
            audit_logger_.logEvent({
                std::chrono::system_clock::now(),
                "MissionCancelled",
                "system",
                current_mission_instance_id_,
                "",
                {{"mission_id", current_mission_id_}}
            });
            audit_logger_.logTaskStateChange({
                current_mission_instance_id_,
                std::chrono::system_clock::now(),
                static_cast<TaskState>(old_status),
                static_cast<TaskState>(current_mission_status_),
                "Mission cancelled.",
                {}
            });
        } else {
             // if not running, just sleep to avoid busy loop
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    std::cout << "Mission loop terminated." << std::endl;
}

bool MissionManager::pauseMission(const std::string& missionInstanceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (current_mission_instance_id_ != missionInstanceId) {
        std::cerr << "Mission instance ID mismatch for pause: " << missionInstanceId << std::endl;
        return false;
    }
    if (current_mission_status_ == MissionStatus::RUNNING) {
        MissionStatus old_status = current_mission_status_;
        current_mission_status_ = MissionStatus::PAUSED;
        std::cout << "Mission '" << missionInstanceId << "' paused." << std::endl;
        // Save mission state
        MissionState pausedState = getMissionState(current_mission_instance_id_);
        mxrc::DataStore::getInstance().save("mission_state_" + current_mission_instance_id_, nlohmann::json(pausedState).dump(), mxrc::DataType::JSON);
        audit_logger_.logEvent({
            std::chrono::system_clock::now(),
            "MissionPaused",
            "system",
            current_mission_instance_id_,
            "",
            {{"mission_id", current_mission_id_}}
        });
        audit_logger_.logTaskStateChange({
            current_mission_instance_id_,
            std::chrono::system_clock::now(),
            static_cast<TaskState>(old_status),
            static_cast<TaskState>(current_mission_status_),
            "Mission paused.",
            {}
        });
        return true;
    }
    return false;
}

bool MissionManager::resumeMission(const std::string& missionInstanceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (current_mission_instance_id_ != missionInstanceId) {
        std::cerr << "Mission instance ID mismatch for resume: " << missionInstanceId << std::endl;
        return false;
    }
    if (current_mission_status_ == MissionStatus::PAUSED) {
        MissionStatus old_status = current_mission_status_;
        current_mission_status_ = MissionStatus::RUNNING;
        std::cout << "Mission '" << missionInstanceId << "' resumed." << std::endl;
        // Save mission state
        MissionState resumedState = getMissionState(current_mission_instance_id_);
        mxrc::DataStore::getInstance().save("mission_state_" + current_mission_instance_id_, nlohmann::json(resumedState).dump(), mxrc::DataType::JSON);
        audit_logger_.logEvent({
            std::chrono::system_clock::now(),
            "MissionResumed",
            "system",
            current_mission_instance_id_,
            "",
            {{"mission_id", current_mission_id_}}
        });
        audit_logger_.logTaskStateChange({
            current_mission_instance_id_,
            std::chrono::system_clock::now(),
            static_cast<TaskState>(old_status),
            static_cast<TaskState>(current_mission_status_),
            "Mission resumed.",
            {}
        });
        cv_mission_control_.notify_one(); // Notify the mission thread to resume
        return true;
    }
    return false;
}

bool MissionManager::cancelMission(const std::string& missionInstanceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (current_mission_instance_id_ != missionInstanceId) {
        std::cerr << "Mission instance ID mismatch for cancel: " << missionInstanceId << std::endl;
        return false;
    }
    if (current_mission_status_ == MissionStatus::RUNNING || current_mission_status_ == MissionStatus::PAUSED) {
        current_mission_status_ = MissionStatus::CANCELLED;
        std::cout << "Mission '" << missionInstanceId << "' cancelled." << std::endl;
        if (behavior_tree_) {
            behavior_tree_->haltTree(); // Halt the Behavior Tree
        }
        cv_mission_control_.notify_one(); // Wake up mission loop to process cancellation
        return true;
    }
    return false;
}

bool MissionManager::insertEmergencyTask(const std::string& missionInstanceId, std::unique_ptr<AbstractTask> emergencyTask, int priority) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (current_mission_instance_id_ != missionInstanceId) {
        std::cerr << "Mission instance ID mismatch for emergency task: " << missionInstanceId << std::endl;
        return false;
    }
    if (!emergencyTask) {
        std::cerr << "Attempted to insert a null emergency task." << std::endl;
        return false;
    }

    std::cout << "Inserting emergency task '" << emergencyTask->getTaskId() << "' into mission '" << missionInstanceId << "' with priority " << priority << std::endl;
    task_scheduler_.addTask(std::move(emergencyTask), priority);

    // Optionally, pause the main mission if emergency task needs immediate attention
    // if (current_mission_status_ == MissionStatus::RUNNING) {
    //     current_mission_status_ = MissionStatus::PAUSED;
    //     std::cout << "Main mission paused for emergency task." << std::endl;
    // }
    audit_logger_.logEvent({
        std::chrono::system_clock::now(),
        "EmergencyTaskInserted",
        "operator",
        current_mission_instance_id_,
        emergencyTask->getTaskId(),
        {{"priority", priority}}
    });
    return true;
}

bool MissionManager::skipCurrentTask(const std::string& missionInstanceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (current_mission_instance_id_ != missionInstanceId) {
        std::cerr << "Mission instance ID mismatch for skip task: " << missionInstanceId << std::endl;
        return false;
    }
    std::cout << "Skipping current task in mission '" << missionInstanceId << "' (implementation pending)." << std::endl;
    // This would require more advanced BT.CPP manipulation, e.g., changing blackboard state
    // or dynamically modifying the tree. For now, it's a placeholder.
    audit_logger_.logEvent({
        std::chrono::system_clock::now(),
        "TaskSkipped",
        "operator",
        current_mission_instance_id_,
        "", // current_task_id to be filled later
        {}
    });
    return false;
}

MissionState MissionManager::getMissionState(const std::string& missionInstanceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    MissionState state;
    if (current_mission_instance_id_ != missionInstanceId) {
        std::cerr << "Mission instance ID mismatch for get state: " << missionInstanceId << std::endl;
        state.current_status = MissionStatus::IDLE; // Indicate invalid state
        return state;
    }

    state.mission_id = current_mission_id_;
    state.instance_id = current_mission_instance_id_;
    state.current_status = current_mission_status_;

    // Try to get current_task_instance_id from blackboard
    if (behavior_tree_ && blackboard_->portExists("current_task_instance_id")) {
        state.current_task_instance_id = blackboard_->get<std::string>("current_task_instance_id");
    } else {
        state.current_task_instance_id = "N/A";
    }

    state.progress = 0.0; // To be updated
    state.estimated_completion_time = "N/A"; // To be updated

    // Populate active_task_states from TaskScheduler or BT blackboard
    return state;
}

TaskState MissionManager::getTaskState(const std::string& missionInstanceId, const std::string& taskInstanceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (current_mission_instance_id_ != missionInstanceId) {
        std::cerr << "Mission instance ID mismatch for get task state: " << missionInstanceId << std::endl;
        return TaskState::PENDING; // Indicate invalid state
    }
    // This would query the TaskScheduler or BT blackboard for the specific task's state
    std::cerr << "getTaskState not fully implemented." << std::endl;
    return TaskState::PENDING; // Placeholder
}

bool MissionManager::recoverMission(const std::string& missionInstanceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "Attempting to recover mission: " << missionInstanceId << std::endl;

    std::any stored_state_any = mxrc::DataStore::getInstance().load("mission_state_" + missionInstanceId);
    if (stored_state_any.has_value()) {
        try {
            std::string stored_state_json = std::any_cast<std::string>(stored_state_any);
            nlohmann::json json_state = nlohmann::json::parse(stored_state_json);

            // Reconstruct MissionState
            MissionState recoveredState;
            recoveredState.mission_id = json_state.value("mission_id", "");
            recoveredState.instance_id = json_state.value("instance_id", "");
            recoveredState.current_status = static_cast<MissionStatus>(json_state.value("current_status", static_cast<int>(MissionStatus::IDLE)));
            recoveredState.current_task_instance_id = json_state.value("current_task_instance_id", "");
            recoveredState.progress = json_state.value("progress", 0.0);
            recoveredState.estimated_completion_time = json_state.value("estimated_completion_time", "");
            // active_task_states would also need to be recovered

            // Restore MissionManager's internal state
            current_mission_id_ = recoveredState.mission_id;
            current_mission_instance_id_ = recoveredState.instance_id;
            current_mission_status_ = recoveredState.current_status;

            // Load the mission definition again to rebuild the BT
            auto it = mission_definitions_.find(current_mission_id_);
            if (it == mission_definitions_.end()) {
                // Try to load from DataStore if not in memory
                std::any def_any = mxrc::DataStore::getInstance().load("mission_def_" + current_mission_id_);
                if (def_any.has_value()) {
                    std::string def_json_str = std::any_cast<std::string>(def_any);
                    MissionDefinition def_from_db;
                    def_from_db.id = current_mission_id_;
                    def_from_db.behavior_tree = nlohmann::json::parse(def_json_str);
                    mission_definitions_[current_mission_id_] = def_from_db;
                    it = mission_definitions_.find(current_mission_id_);
                }
            }

            if (it != mission_definitions_.end()) {
                std::string bt_xml = it->second.behavior_tree.dump();
                behavior_tree_ = std::make_unique<BT::Tree>(bt_factory_.createTreeFromText(bt_xml, blackboard_));
                // Need to restore BT state if possible, which is complex with BT.CPP
                // For now, we just rebuild the tree and set the mission status.
            }

            std::cout << "Mission '" << missionInstanceId << "' recovered to status: " << static_cast<int>(current_mission_status_) << std::endl;
            audit_logger_.logEvent({
                std::chrono::system_clock::now(),
                "MissionRecovered",
                "system",
                current_mission_instance_id_,
                "",
                {{"mission_id", current_mission_id_}, {"recovered_status", static_cast<int>(current_mission_status_)}}
            });
            cv_mission_control_.notify_one(); // Wake up mission loop if it needs to resume
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error recovering mission state: " << e.what() << std::endl;
            return false;
        }
    }
    std::cerr << "No saved state found for mission: " << missionInstanceId << std::endl;
    return false;
}

void MissionManager::updateMissionState_internal() {
    // This method would be called periodically to update progress, current task, etc.
    // For now, it's a placeholder.
}

// This method is no longer directly used as ExecuteTaskNode handles task execution
BT::NodeStatus MissionManager::executeTaskNode(BT::TreeNode& self) {
    return BT::NodeStatus::FAILURE; // Should not be called directly
}

} // namespace task_mission
} // namespace mxrc