#include <gtest/gtest.h>
#include "core/task/MissionManager.h"
#include "core/datastore/DataStore.h"
#include "core/task/AbstractTask.h"
#include "core/task/TaskFactory.h"
#include "core/task/DriveToPositionTask.h"
#include <chrono>
#include <thread>
#include <filesystem>
#include "nlohmann/json.hpp"

using namespace mxrc::task;
namespace fs = std::filesystem;

// A failing task for testing recovery
class FailingTask : public AbstractTask {
public:
    bool initialize(TaskContext& context) override {
        std::cout << "FailingTask initialized." << std::endl;
        return true;
    }

    bool execute(TaskContext& context) override {
        std::cout << "FailingTask executing and failing." << std::endl;
        return false; // Simulate failure
    }

    void terminate(TaskContext& context) override {
        std::cout << "FailingTask terminated." << std::endl;
    }

    std::string getTaskId() const override {
        return "FailingTask";
    }
};

class MissionManagerRecoveryTest : public ::testing::Test {
protected:
    MissionManager& missionManager = MissionManager::getInstance();
    mxrc::DataStore& dataStore = mxrc::DataStore::getInstance();
    const std::string TEST_MISSION_FILE = "/Users/tory/workspace/mxrc/missions/simple_mission.xml";
    const std::string RECOVERY_KEY = "test_mission_recovery";
    const std::string FAILED_MISSION_INSTANCE_ID = "failed_mission_instance";
    const std::string RECOVERED_MISSION_INSTANCE_ID = "recovered_mission_instance";

    void SetUp() override {
        missionManager.cancelMission(); // Global cancel
        missionManager.cancelMission(FAILED_MISSION_INSTANCE_ID);
        missionManager.cancelMission(RECOVERED_MISSION_INSTANCE_ID);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        dataStore.remove(RECOVERY_KEY);

        // Register the failing task
        TaskFactory::getInstance().registerTask("FailingTask", []() {
            return std::make_unique<FailingTask>();
        });
    }

    void TearDown() override {
        missionManager.cancelMission();
        missionManager.cancelMission(FAILED_MISSION_INSTANCE_ID);
        missionManager.cancelMission(RECOVERED_MISSION_INSTANCE_ID);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        dataStore.remove(RECOVERY_KEY);
    }
};

TEST_F(MissionManagerRecoveryTest, RecoverAfterFailure) {
    // 1. Load and start a mission (that will eventually fail)
    // For this, we need a mission XML that includes a FailingTask.
    // Since simple_mission.xml doesn't have it, we'll simulate a failure
    // or assume a mission definition that includes FailingTask is loaded.
    // For now, let's assume simple_mission.xml is loaded, and we'll simulate failure
    // by directly setting the mission status to FAILED and saving it.

    // Load a mission definition (e.g., simple_mission.xml)
    ASSERT_TRUE(missionManager.loadMissionDefinition(TEST_MISSION_FILE));

    // Simulate a mission that ran for a bit and then failed
    // We'll manually create a MissionState that represents a failed mission
    MissionState failedState;
    failedState.mission_id = TEST_MISSION_FILE;
    failedState.instance_id = FAILED_MISSION_INSTANCE_ID;
    failedState.current_status = MissionStatus::FAILED;
    failedState.current_task_instance_id = "FailingTaskInstance"; // Example of a task that failed
    failedState.progress = 0.5; // Example progress

    // Manually save this failed state to DataStore
    nlohmann::json json_failed_state = {
        {"mission_id", failedState.mission_id},
        {"instance_id", failedState.instance_id},
        {"current_status", static_cast<int>(failedState.current_status)},
        {"current_task_instance_id", failedState.current_task_instance_id},
        {"progress", failedState.progress},
        {"estimated_completion_time", failedState.estimated_completion_time}
    };
    mxrc::DataStore::getInstance().save(RECOVERY_KEY, json_failed_state.dump(), mxrc::DataType::JSON);

    // 2. Attempt to recover the mission using loadMissionState
    bool loaded = missionManager.loadMissionState(RECOVERY_KEY, RECOVERED_MISSION_INSTANCE_ID);
    ASSERT_TRUE(loaded) << "Failed to load mission state for recovery.";

    // 3. Verify the recovered state
    MissionState postRecoveryState = missionManager.getMissionState(RECOVERED_MISSION_INSTANCE_ID);
    ASSERT_EQ(postRecoveryState.mission_id, failedState.mission_id);
    ASSERT_EQ(postRecoveryState.instance_id, RECOVERED_MISSION_INSTANCE_ID);
    // Expect the recovered mission to be in IDLE or PAUSED, ready for restart/resume
    ASSERT_TRUE(postRecoveryState.status == MissionStatus::IDLE || postRecoveryState.status == MissionStatus::PAUSED)
        << "Recovered mission status is " << static_cast<int>(postRecoveryState.status) << ", expected IDLE or PAUSED.";

    // Optionally: Start the recovered mission and check behaviour
    ASSERT_TRUE(missionManager.startMission(RECOVERED_MISSION_INSTANCE_ID));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(missionManager.getMissionState(RECOVERED_MISSION_INSTANCE_ID).status, MissionStatus::RUNNING);
}
