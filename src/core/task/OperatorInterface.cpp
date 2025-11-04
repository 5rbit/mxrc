#include "OperatorInterface.h"
#include <iostream>
#include "../../datastore/DataStore.h"
#include "nlohmann/json.hpp"

namespace mxrc {
namespace task {

OperatorInterface::OperatorInterface()
    : mission_manager_(MissionManager::getInstance()) {
}

OperatorInterface& OperatorInterface::getInstance() {
    static OperatorInterface instance;
    return instance;
}

std::string OperatorInterface::requestStartMission(const std::string& missionId, const TaskContext& initialContext) {
    std::cout << "OperatorInterface: Requesting to start mission '" << missionId << "'" << std::endl;
    return mission_manager_.startMission(missionId, initialContext);
}

bool OperatorInterface::requestPauseMission(const std::string& missionInstanceId) {
    std::cout << "OperatorInterface: Requesting to pause mission '" << missionInstanceId << "'" << std::endl;
    return mission_manager_.pauseMission(missionInstanceId);
}

bool OperatorInterface::requestResumeMission(const std::string& missionInstanceId) {
    std::cout << "OperatorInterface: Requesting to resume mission '" << missionInstanceId << "'" << std::endl;
    return mission_manager_.resumeMission(missionInstanceId);
}

bool OperatorInterface::requestCancelMission(const std::string& missionInstanceId) {
    std::cout << "OperatorInterface: Requesting to cancel mission '" << missionInstanceId << "'" << std::endl;
    return mission_manager_.cancelMission(missionInstanceId);
}

bool OperatorInterface::requestInsertEmergencyTask(const std::string& missionInstanceId, std::unique_ptr<AbstractTask> emergencyTask, int priority) {
    std::cout << "OperatorInterface: Requesting to insert emergency task into mission '" << missionInstanceId << "'" << std::endl;
    return mission_manager_.insertEmergencyTask(missionInstanceId, std::move(emergencyTask), priority);
}

bool OperatorInterface::requestSkipCurrentTask(const std::string& missionInstanceId) {
    std::cout << "OperatorInterface: Requesting to skip current task in mission '" << missionInstanceId << "'" << std::endl;
    return mission_manager_.skipCurrentTask(missionInstanceId);
}

MissionState OperatorInterface::getMissionStatus(const std::string& missionInstanceId) const {
    return mission_manager_.getMissionState(missionInstanceId);
}

TaskState OperatorInterface::getTaskStatus(const std::string& missionInstanceId, const std::string& taskInstanceId) const {
    return mission_manager_.getTaskState(missionInstanceId, taskInstanceId);
}

std::vector<TaskStateHistory> OperatorInterface::getTaskHistory(const std::string& taskInstanceId) const {
    std::vector<TaskStateHistory> history;
    std::string query_pattern = "task_history_" + taskInstanceId;
    std::vector<std::any> results = mxrc::DataStore::getInstance().query(query_pattern);

    for (const auto& any_val : results) {
        if (any_val.type() == typeid(std::string)) {
            try {
                nlohmann::json json_entry = nlohmann::json::parse(std::any_cast<std::string>(any_val));
                TaskStateHistory entry;
                entry.task_instance_id = json_entry.value("task_instance_id", "");
                entry.timestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(json_entry.value("timestamp", 0LL)));
                entry.old_state = static_cast<TaskState>(json_entry.value("old_state", static_cast<int>(TaskState::PENDING)));
                entry.new_state = static_cast<TaskState>(json_entry.value("new_state", static_cast<int>(TaskState::PENDING)));
                entry.reason = json_entry.value("reason", "");
                // error_info would need more complex deserialization if it contains std::any
                history.push_back(entry);
            } catch (const nlohmann::json::exception& e) {
                std::cerr << "Error parsing task history JSON: " << e.what() << std::endl;
            } catch (const std::bad_any_cast& e) {
                std::cerr << "Bad any_cast in task history: " << e.what() << std::endl;
            }
        }
    }
    return history;
}

std::vector<MissionState> OperatorInterface::getMissionHistory(const std::string& missionInstanceId) const {
    std::vector<MissionState> history;
    std::string query_pattern = "mission_state_" + missionInstanceId;
    std::vector<std::any> results = mxrc::DataStore::getInstance().query(query_pattern);

    for (const auto& any_val : results) {
        if (any_val.type() == typeid(std::string)) {
            try {
                nlohmann::json json_entry = nlohmann::json::parse(std::any_cast<std::string>(any_val));
                MissionState entry;
                entry.mission_id = json_entry.value("mission_id", "");
                entry.instance_id = json_entry.value("instance_id", "");
                entry.current_status = static_cast<MissionStatus>(json_entry.value("current_status", static_cast<int>(MissionStatus::IDLE)));
                entry.current_task_instance_id = json_entry.value("current_task_instance_id", "");
                entry.progress = json_entry.value("progress", 0.0);
                entry.estimated_completion_time = json_entry.value("estimated_completion_time", "");
                // active_task_states would need more complex deserialization
                history.push_back(entry);
            } catch (const nlohmann::json::exception& e) {
                std::cerr << "Error parsing mission history JSON: " << e.what() << std::endl;
            } catch (const std::bad_any_cast& e) {
                std::cerr << "Bad any_cast in mission history: " << e.what() << std::endl;
            }
        }
    }
    return history;
}

} // namespace task
} // namespace mxrc
