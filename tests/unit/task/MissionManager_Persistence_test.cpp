#include <gtest/gtest.h>
#include "core/task/MissionManager.h"
#include "core/datastore/DataStore.h"
#include "core/task/AbstractTask.h"
#include "core/task/TaskFactory.h"
#include "core/task/DriveToPositionTask.h"
#include <chrono>
#include <thread>
#include <filesystem>

using namespace mxrc::task;
namespace fs = std::filesystem;

class MissionManagerPersistenceTest : public ::testing::Test {
protected:
    MissionManager& missionManager = MissionManager::getInstance();
    mxrc::DataStore& dataStore = mxrc::DataStore::getInstance();
    const std::string TEST_MISSION_FILE = "/Users/tory/workspace/mxrc/missions/simple_mission.xml";
    const std::string PERSISTENCE_KEY = "test_mission_persistence";
    const std::string TEST_STATE_FILE = "test_mission_state.json"; // This might be used by DataStore's internal saveState/loadState

    void SetUp() override {
        // Ensure mission is idle before each test
        missionManager.cancelMission(); // Use the global cancel if no instance ID is provided
        missionManager.cancelMission("test_mission_instance"); // Cancel specific instances
        missionManager.cancelMission("test_mission_instance_restored");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        // Clear any previous test data from DataStore
        dataStore.remove(PERSISTENCE_KEY);
        if (fs::exists(TEST_STATE_FILE)) {
            fs::remove(TEST_STATE_FILE);
        }
    }

    void TearDown() override {
        // Clean up after each test
        missionManager.cancelMission();
        missionManager.cancelMission("test_mission_instance");
        missionManager.cancelMission("test_mission_instance_restored");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        dataStore.remove(PERSISTENCE_KEY);
        if (fs::exists(TEST_STATE_FILE)) {
            fs::remove(TEST_STATE_FILE);
        }
    }
};

TEST_F(MissionManagerPersistenceTest, SaveAndLoadMissionState) {
    // 1. Load and start a mission
    ASSERT_TRUE(missionManager.loadMission(TEST_MISSION_FILE, "test_mission_instance"));
    ASSERT_TRUE(missionManager.startMission("test_mission_instance"));

    // Give it a moment to run
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get current state
    MissionState preSaveState = missionManager.getMissionState("test_mission_instance");
    ASSERT_EQ(preSaveState.instance_id, "test_mission_instance");
    // Ensure it's not IDLE before saving
    ASSERT_NE(preSaveState.status, MissionStatus::IDLE);

    // 2. Save the mission state
    // This assumes MissionManager has a method to save its state to the DataStore
    bool saved = missionManager.saveMissionState(PERSISTENCE_KEY, "test_mission_instance");
    ASSERT_TRUE(saved) << "Failed to save mission state.";

    // 3. Cancel the current mission
    missionManager.cancelMission("test_mission_instance");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    MissionState cancelledState = missionManager.getMissionState("test_mission_instance");
    ASSERT_EQ(cancelledState.status, MissionStatus::CANCELLED);

    // 4. Load the previously saved mission state
    bool loaded = missionManager.loadMissionState(PERSISTENCE_KEY, "test_mission_instance_restored");
    ASSERT_TRUE(loaded) << "Failed to load mission state.";

    // 5. Verify the restored state
    MissionState postLoadState = missionManager.getMissionState("test_mission_instance_restored");
    ASSERT_EQ(postLoadState.mission_id, preSaveState.mission_id);
    // The status might change based on how "load" works, e.g., it might resume or go to paused
    // For this test, we expect it to be LOADED or IDLE (ready to be started), or even PAUSED if implemented this way.
    // Let's assume for now it goes to IDLE if it's not RUNNING, reflecting a deserialized but not yet resumed state.
    ASSERT_TRUE(postLoadState.status == MissionStatus::IDLE || postLoadState.status == MissionStatus::PAUSED)
        << "Restored mission status is " << static_cast<int>(postLoadState.status) << ", expected IDLE or PAUSED.";
    // The instance ID should be the one provided during load, not the original
    ASSERT_EQ(postLoadState.instance_id, "test_mission_instance_restored");

    // Optional: Start the restored mission and check behaviour
    ASSERT_TRUE(missionManager.startMission("test_mission_instance_restored"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(missionManager.getMissionState("test_mission_instance_restored").status, MissionStatus::RUNNING);
}
