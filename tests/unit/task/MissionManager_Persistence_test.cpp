#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "core/task/MissionManager.h"
#include "core/task/contracts/IDataStore.h"
#include "mocks/MockDataStore.h"
#include "core/task/AbstractTask.h"
#include "core/task/TaskFactory.h"
#include "core/task/DriveToPositionTask.h"
#include <chrono>
#include <thread>
#include <filesystem>

using namespace mxrc::task;
namespace fs = std::filesystem;
using ::testing::_; // For matching any argument
using ::testing::Return;

class MissionManagerPersistenceTest : public ::testing::Test {
protected:
    std::shared_ptr<MockDataStore> mockDataStore;
    MissionManager* missionManager;
    const std::string TEST_MISSION_FILE = "/Users/tory/workspace/mxrc/missions/simple_mission.xml";
const std::string TEST_MISSION_INSTANCE_ID = "test_mission_instance";
    const std::string RESTORED_MISSION_INSTANCE_ID = "test_mission_instance_restored";

    void SetUp() override {
        // Create a new mock for each test to ensure clean state
        mockDataStore = std::make_shared<MockDataStore>();
        // Ensure MissionManager uses our mock
        missionManager = &MissionManager::getInstance(mockDataStore);

        // Ensure mission is idle before each test
        missionManager->cancelMission(); // Use the global cancel if no instance ID is provided
        missionManager->cancelMission(TEST_MISSION_INSTANCE_ID); // Cancel specific instances
        missionManager->cancelMission(RESTORED_MISSION_INSTANCE_ID);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Register the DriveToPositionTask so the mission can be loaded
        TaskFactory::getInstance().registerTask("DriveToPosition", []() {
            return std::make_unique<DriveToPositionTask>();
        });
    }

    void TearDown() override {
        // Clean up after each test
        missionManager->cancelMission();
        missionManager->cancelMission(TEST_MISSION_INSTANCE_ID);
        missionManager->cancelMission(RESTORED_MISSION_INSTANCE_ID);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
};

TEST_F(MissionManagerPersistenceTest, SaveAndLoadMissionState) {
    // 1. Load and start a mission
    ASSERT_TRUE(missionManager->loadMission(TEST_MISSION_FILE, TEST_MISSION_INSTANCE_ID));
    ASSERT_TRUE(missionManager->startMission(TEST_MISSION_INSTANCE_ID));

    // Give it a moment to run
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get current state - this will be the state that MissionManager attempts to save
    MissionState preSaveState = missionManager->getMissionState(TEST_MISSION_INSTANCE_ID);
    ASSERT_EQ(preSaveState.instance_id, TEST_MISSION_INSTANCE_ID);
    ASSERT_NE(preSaveState.status, MissionStatus::IDLE);

    // ************* EXPECT_CALL for IDataStore::saveMissionState *************
    // We expect MissionManager to call IDataStore::saveMissionState with a MissionStateDto
    // that reflects the current state of the mission.
    // The exact MissionStateDto::missionStatus might be RUNNING, but we can be flexible.
    EXPECT_CALL(*mockDataStore, saveMissionState(
        AllOf(
            Field(&MissionStateDto::missionId, TEST_MISSION_FILE),
            Field(&MissionStateDto::instanceId, TEST_MISSION_INSTANCE_ID)
            // If you need to check status, you can add: Field(&MissionStateDto::missionStatus, to_string(MissionStatus::RUNNING))
        )
    )).Times(1).WillOnce(Return(true));

    // 2. Save the mission state using MissionManager's public method
    // This call should trigger the mocked IDataStore::saveMissionState
    bool saved = missionManager->saveMissionState(TEST_MISSION_INSTANCE_ID);
    ASSERT_TRUE(saved) << "Failed to trigger saveMissionState in MissionManager.";

    // 3. Cancel the current mission
    missionManager->cancelMission(TEST_MISSION_INSTANCE_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    MissionState cancelledState = missionManager->getMissionState(TEST_MISSION_INSTANCE_ID);
    ASSERT_EQ(cancelledState.status, MissionStatus::CANCELLED);

    // ************* EXPECT_CALL for IDataStore::loadMissionState *************
    // When MissionManager needs to load a mission state for restoration, it will query
    // IDataStore. Simulate the state it would have saved.
    MissionStateDto mockLoadedDto;
    mockLoadedDto.missionId = TEST_MISSION_FILE;
    mockLoadedDto.instanceId = TEST_MISSION_INSTANCE_ID; // IDataStore holds the original instance ID
    mockLoadedDto.missionStatus = to_string(MissionStatus::IDLE); // Or some other appropriate restored status

    EXPECT_CALL(*mockDataStore, loadMissionState(TEST_MISSION_INSTANCE_ID)) // IDataStore is asked for the original instance ID
        .Times(1)
        .WillOnce(Return(mockLoadedDto));

    // 4. Load the previously saved mission state into a new instance via MissionManager
    // This call should trigger the mocked IDataStore::loadMissionState
    bool loaded = missionManager->loadMissionState(TEST_MISSION_INSTANCE_ID, RESTORED_MISSION_INSTANCE_ID);
    ASSERT_TRUE(loaded) << "Failed to trigger loadMissionState in MissionManager.";

    // 5. Verify the restored state
    MissionState postLoadState = missionManager->getMissionState(RESTORED_MISSION_INSTANCE_ID);
    ASSERT_EQ(postLoadState.mission_id, TEST_MISSION_FILE);
    ASSERT_EQ(postLoadState.instance_id, RESTORED_MISSION_INSTANCE_ID); // It should have the new instance ID
    ASSERT_EQ(postLoadState.status, MissionStatus::IDLE); // Based on how we mocked it to return IDLE

    // Optional: Start the restored mission and check behaviour
    ASSERT_TRUE(missionManager->startMission(RESTORED_MISSION_INSTANCE_ID));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(missionManager->getMissionState(RESTORED_MISSION_INSTANCE_ID).status, MissionStatus::RUNNING);
}
