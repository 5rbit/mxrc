#include "MissionManager.h"
#include "TaskFactory.h"
#include "ExecuteTaskNode.h" // Include for registering custom node
#include <iostream>
#include <thread>
#include <chrono>
#include <uuid/uuid.h> // For generating UUIDs

namespace mxrc {
namespace task {

MissionManager* MissionManager::instance_ = nullptr;
std::mutex MissionManager::mutex_;

MissionManager::MissionManager(std::shared_ptr<IDataStore> dataStore)
    : data_store_(dataStore),
      current_mission_status_(MissionStatus::IDLE),
      blackboard_(BT::Blackboard::create()),
      bt_factory_(), // Initialize BehaviorTreeFactory
      shutdown_requested_(false)
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

MissionManager& MissionManager::getInstance(std::shared_ptr<IDataStore> dataStore) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (instance_ == nullptr) {
        instance_ = new MissionManager(dataStore);
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
        MissionStateDto initialStateDto; // Create and populate DTO
        initialStateDto.missionId = initialState.mission_id;
        initialStateDto.missionStatus = "RUNNING";
        initialStateDto.lastUpdated = std::chrono::system_clock::now();
        initialStateDto.currentTask_id = initialState.current_task_instance_id;
        initialStateDto.missionProgress = initialState.progress;
        if(data_store_) data_store_->saveMissionState(initialStateDto);
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
                MissionStateDto finalStateDto;
                finalStateDto.missionId = finalState.mission_id;
                finalStateDto.missionStatus = "COMPLETED";
                finalStateDto.lastUpdated = std::chrono::system_clock::now();
                if(data_store_) data_store_->saveMissionState(finalStateDto);
            } else if (status == BT::NodeStatus::FAILURE) {
                std::lock_guard<std::mutex> main_lock(mutex_);
                MissionStatus old_status = current_mission_status_;
                current_mission_status_ = MissionStatus::FAILED;
                std::cerr << "Mission failed." << std::endl;
                // Save final mission state
                MissionState finalState = getMissionState(current_mission_instance_id_);
                MissionStateDto finalStateDto;
                finalStateDto.missionId = finalState.mission_id;
                finalStateDto.missionStatus = "FAILED";
                finalStateDto.lastUpdated = std::chrono::system_clock::now();
                if(data_store_) data_store_->saveMissionState(finalStateDto);
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
            MissionStateDto finalStateDto;
            finalStateDto.missionId = finalState.mission_id;
            finalStateDto.missionStatus = "CANCELLED";
            finalStateDto.lastUpdated = std::chrono::system_clock::now();
            if(data_store_) data_store_->saveMissionState(finalStateDto);
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
        MissionStateDto pausedStateDto;
        pausedStateDto.missionId = pausedState.mission_id;
        pausedStateDto.missionStatus = "PAUSED";
        pausedStateDto.lastUpdated = std::chrono::system_clock::now();
        if(data_store_) data_store_->saveMissionState(pausedStateDto);
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
        MissionStateDto resumedStateDto;
        resumedStateDto.missionId = resumedState.mission_id;
        resumedStateDto.missionStatus = "RUNNING";
        resumedStateDto.lastUpdated = std::chrono::system_clock::now();
        if(data_store_) data_store_->saveMissionState(resumedStateDto);
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
    if (behavior_tree_ && blackboard_->portInfo("current_task_instance_id") != nullptr) {
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
    if (!data_store_) {
        std::cerr << "DataStore is not available." << std::endl;
        return false;
    }

    std::cout << "Attempting to recover mission with instance ID '" << missionInstanceId << "'." << std::endl;

    auto stored_state_dto = data_store_->loadMissionState(missionInstanceId);

    if (!stored_state_dto) {
        std::cerr << "No saved state found for mission instance ID: " << missionInstanceId << std::endl;
        return false;
    }

    try {
        // Restore MissionManager's internal state from DTO
        current_mission_id_ = stored_state_dto->missionId;
        current_mission_instance_id_ = missionInstanceId;

        if (stored_state_dto->missionStatus == "RUNNING") {
            current_mission_status_ = MissionStatus::PAUSED; // Load as paused if it was running
        } else if (stored_state_dto->missionStatus == "PAUSED") {
            current_mission_status_ = MissionStatus::PAUSED;
        } else {
            // Don't recover completed, failed or cancelled missions
            std::cerr << "Mission was in a final state and will not be recovered: " << stored_state_dto->missionStatus << std::endl;
            return false;
        }

        // Load the mission definition again to rebuild the BT
        auto it = mission_definitions_.find(current_mission_id_);
        if (it == mission_definitions_.end()) {
            // For now, we assume the definition is already loaded.
            // In a real scenario, we might need to load it from a definition store.
            std::cerr << "Mission definition '" << current_mission_id_ << "' not found for loading." << std::endl;
            return false;
        }

        std::string bt_xml = it->second.behavior_tree.dump();
        behavior_tree_ = std::make_unique<BT::Tree>(bt_factory_.createTreeFromText(bt_xml, blackboard_));

        // Here you would also recover the task history and blackboard state if needed
        // auto task_history = data_store_->loadTaskHistory(missionInstanceId);
        // ...

        std::cout << "Mission state for instance '" << missionInstanceId << "' recovered successfully." << std::endl;
        cv_mission_control_.notify_one(); // Wake up mission loop if it needs to resume
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error recovering mission state for instance '" << missionInstanceId << "': " << e.what() << std::endl;
        return false;
    }
}





void MissionManager::updateMissionState_internal() {
    // This method would be called periodically to update progress, current task, etc.
    // For now, it's a placeholder.
}

// This method is no longer directly used as ExecuteTaskNode handles task execution
BT::NodeStatus MissionManager::executeTaskNode(BT::TreeNode& self) {
    return BT::NodeStatus::FAILURE; // Should not be called directly
}

} // namespace task
} // namespace mxrc