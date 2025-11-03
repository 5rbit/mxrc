#include "MissionManager.h"
#include "TaskFactory.h"
#include <iostream>
#include <fstream>
#include <sstream>

namespace mxrc {
namespace task_mission {

MissionManager* MissionManager::instance_ = nullptr;
std::mutex MissionManager::mutex_;

MissionManager::MissionManager()
    : current_mission_status_(MissionStatus::IDLE),
      blackboard_(BT::Blackboard::create())
{
    // Register custom BehaviorTree nodes
    // This will be expanded as more task types are defined
    // For now, we can register a generic TaskRunner node
    // BT::BehaviorTreeFactory factory;
    // factory.registerNodeType<TaskRunner>("TaskRunner");
}

MissionManager& MissionManager::getInstance() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (instance_ == nullptr) {
        instance_ = new MissionManager();
    }
    return *instance_;
}

bool MissionManager::loadMission(const std::string& mission_filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "Loading mission from: " << mission_filepath << std::endl;

    // Placeholder for actual mission loading and BT parsing
    // In a real scenario, this would parse a JSON/YAML file into a BehaviorTree
    // For now, we'll simulate a successful load.
    current_mission_id_ = "mission_from_" + mission_filepath;
    current_mission_status_ = MissionStatus::IDLE;
    // Simulate BT creation
    // behavior_tree_ = std::make_unique<BT::Tree>(factory.createTreeFromFile(mission_filepath, blackboard_));

    // For testing, let's assume a simple BT that runs DriveToPositionTask
    // This part will be replaced by actual BT.CPP integration
    // For now, we'll just set up a dummy task in active_tasks_
    // This is a temporary simulation of a loaded mission.
    std::unique_ptr<AbstractTask> driveTask = TaskFactory::getInstance().createTask("DriveToPosition");
    if (driveTask) {
        active_tasks_["DriveToPosition"] = std::move(driveTask);
        mission_context_.set<double>("target_x", 10.0);
        mission_context_.set<double>("target_y", 20.0);
        mission_context_.set<double>("speed", 1.5);
        std::cout << "Simulated loading of DriveToPosition task." << std::endl;
        return true;
    } else {
        std::cerr << "Failed to create DriveToPosition task during simulated mission load." << std::endl;
        return false;
    }
}

bool MissionManager::startMission() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (current_mission_status_ != MissionStatus::IDLE && current_mission_status_ != MissionStatus::COMPLETED && current_mission_status_ != MissionStatus::FAILED) {
        std::cerr << "Mission already running or in an unstartable state." << std::endl;
        return false;
    }

    std::cout << "Starting mission: " << current_mission_id_ << std::endl;
    current_mission_status_ = MissionStatus::RUNNING;

    // In a real scenario, this would tick the BehaviorTree
    // For now, we'll simulate executing the loaded task
    if (active_tasks_.count("DriveToPosition")) {
        AbstractTask* task = active_tasks_["DriveToPosition"].get();
        if (task->initialize(mission_context_)) {
            task->setState(TaskState::RUNNING);
            current_mission_id_ = task->getTaskId(); // Set current task for monitoring
            if (task->execute(mission_context_)) {
                task->setState(TaskState::COMPLETED);
                current_mission_status_ = MissionStatus::COMPLETED;
                std::cout << "Mission completed successfully (simulated)." << std::endl;
                return true;
            } else {
                task->setState(TaskState::FAILED);
                current_mission_status_ = MissionStatus::FAILED;
                std::cerr << "Mission failed during task execution (simulated)." << std::endl;
                return false;
            }
        } else {
            task->setState(TaskState::FAILED);
            current_mission_status_ = MissionStatus::FAILED;
            std::cerr << "Mission failed during task initialization (simulated)." << std::endl;
            return false;
        }
    }
    std::cerr << "No tasks to run in simulated mission." << std::endl;
    current_mission_status_ = MissionStatus::FAILED;
    return false;
}

void MissionManager::pauseMission() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (current_mission_status_ == MissionStatus::RUNNING) {
        current_mission_status_ = MissionStatus::PAUSED;
        std::cout << "Mission paused." << std::endl;
    }
}

void MissionManager::resumeMission() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (current_mission_status_ == MissionStatus::PAUSED) {
        current_mission_status_ = MissionStatus::RUNNING;
        std::cout << "Mission resumed." << std::endl;
    }
}

void MissionManager::cancelMission() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (current_mission_status_ == MissionStatus::RUNNING || current_mission_status_ == MissionStatus::PAUSED) {
        current_mission_status_ = MissionStatus::CANCELLED;
        std::cout << "Mission cancelled." << std::endl;
        // Terminate all active tasks
        for (auto& pair : active_tasks_) {
            pair.second->terminate(mission_context_);
        }
    }
}

void MissionManager::skipTask(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "Skipping task: " << task_id << " (simulated)" << std::endl;
    // In a real BT, this would involve manipulating the BT state or blackboard
}

void MissionManager::insertEmergencyTask(const std::string& task_id, TaskContext& context) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "Inserting emergency task: " << task_id << " (simulated)" << std::endl;
    // In a real BT, this would involve pausing current BT and running a sub-tree
}

MissionState MissionManager::getMissionState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    MissionState state;
    state.mission_id = current_mission_id_;
    state.status = current_mission_status_;
    state.current_task_id = current_mission_id_; // Placeholder, should be actual current task
    state.progress = 0.0; // Placeholder
    state.estimated_completion_time = "N/A"; // Placeholder

    for (const auto& pair : active_tasks_) {
        state.task_states[pair.first] = pair.second->getState();
    }
    return state;
}

void MissionManager::updateMissionState() {
    // This method would be called periodically to update progress, current task, etc.
    // For now, it's a placeholder.
}

// Placeholder for custom BT node execution
BT::NodeStatus MissionManager::executeTaskNode(BT::TreeNode& self) {
    // This is where a custom BT node would interact with TaskFactory and AbstractTask
    std::string task_id = self.name(); // Assuming node name is task ID
    std::cout << "BT Node executing task: " << task_id << std::endl;

    // This part needs proper integration with TaskFactory and TaskContext
    // For now, it's a very simplified simulation
    std::unique_ptr<AbstractTask> task = TaskFactory::getInstance().createTask(task_id);
    if (!task) {
        std::cerr << "Failed to create task: " << task_id << std::endl;
        return BT::NodeStatus::FAILURE;
    }

    TaskContext context; // Needs to be populated from BT blackboard
    // Populate context from blackboard
    // For example: context.set<double>("target_x", self.getInput<double>("target_x").value());

    if (task->initialize(context)) {
        task->setState(TaskState::RUNNING);
        if (task->execute(context)) {
            task->setState(TaskState::COMPLETED);
            return BT::NodeStatus::SUCCESS;
        } else {
            task->setState(TaskState::FAILED);
            return BT::NodeStatus::FAILURE;
        }
    } else {
        task->setState(TaskState::FAILED);
        return BT::NodeStatus::FAILURE;
    }
}

} // namespace task_mission
} // namespace mxrc