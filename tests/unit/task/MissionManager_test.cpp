#include <gtest/gtest.h>
#include "core/task/task_mission_management/MissionManager.h"
#include "core/task/task_mission_management/AbstractTask.h"
#include "core/task/task_mission_management/TaskFactory.h"
#include "core/task/task_mission_management/DriveToPositionTask.h" // Ensure DriveToPositionTask is linked

using namespace mxrc::task_mission;

// Test fixture for MissionManager
class MissionManagerTest : public ::testing::Test {
protected:
    MissionManager& missionManager = MissionManager::getInstance();

    void SetUp() override {
        // Reset mission manager state for each test if necessary
        // For now, we assume a clean state or handle it within tests
    }
};

TEST_F(MissionManagerTest, LoadAndStartMission) {
    // Ensure DriveToPositionTask is registered for this test
    // It should be registered statically by DriveToPositionTask.cpp
    std::unique_ptr<AbstractTask> testTask = TaskFactory::getInstance().createTask("DriveToPosition");
    ASSERT_NE(testTask, nullptr) << "DriveToPositionTask not registered!";

    // Simulate loading a mission (which internally simulates loading DriveToPositionTask)
    bool loaded = missionManager.loadMission("path/to/dummy_mission.json");
    ASSERT_TRUE(loaded);

    MissionState initialState = missionManager.getMissionState();
    ASSERT_EQ(initialState.status, MissionStatus::IDLE);
    ASSERT_EQ(initialState.mission_id, "mission_from_path/to/dummy_mission.json");

    bool started = missionManager.startMission();
    ASSERT_TRUE(started);

    MissionState runningState = missionManager.getMissionState();
    ASSERT_EQ(runningState.status, MissionStatus::COMPLETED); // Simulated mission completes immediately
    ASSERT_EQ(runningState.task_states.at("DriveToPosition"), TaskState::COMPLETED);
}

TEST_F(MissionManagerTest, PauseResumeCancelMission) {
    // Load a mission first
    missionManager.loadMission("path/to/another_dummy_mission.json");
    missionManager.startMission(); // This will complete immediately in current simulation

    // For testing pause/resume/cancel, we need a mission that doesn't complete immediately.
    // Since the current startMission() simulates immediate completion, these tests will be limited.
    // Once BehaviorTree.CPP is fully integrated, these tests can be more robust.

    // Simulate a running mission (manually set status for testing purposes)
    missionManager.cancelMission(); // Ensure previous mission is cleared
    missionManager.loadMission("path/to/pausable_mission.json");
    // Manually set status to RUNNING for testing pause/resume
    // In a real scenario, startMission would manage this.
    // This requires internal access or a more sophisticated mock for MissionManager.
    // For now, we'll test the state transitions as best as possible with current simulation.

    // Test pause (will not change state if not running)
    missionManager.pauseMission();
    ASSERT_EQ(missionManager.getMissionState().status, MissionStatus::IDLE); // Still IDLE after load

    // A more realistic test would require mocking the BT execution loop
    // For now, we'll just test the state changes if we assume it was running.
    // This part will be improved with actual BT integration.

    // Let's assume a mission is running for the sake of testing the methods
    // This is a temporary workaround until BT integration is complete.
    // We cannot directly set private members, so we'll rely on startMission's current behavior.
    missionManager.loadMission("path/to/test_mission.json");
    missionManager.startMission(); // This will make it COMPLETED

    // Test cancel on a completed mission (should not change status)
    missionManager.cancelMission();
    ASSERT_EQ(missionManager.getMissionState().status, MissionStatus::COMPLETED);

    // To properly test pause/resume/cancel, we need a way to control the mission's running state.
    // This will be addressed when BehaviorTree.CPP is fully integrated and mission execution is asynchronous.
}

TEST_F(MissionManagerTest, GetMissionState) {
    missionManager.loadMission("path/to/state_check_mission.json");
    MissionState state = missionManager.getMissionState();
    ASSERT_EQ(state.mission_id, "mission_from_path/to/state_check_mission.json");
    ASSERT_EQ(state.status, MissionStatus::IDLE);
    ASSERT_TRUE(state.task_states.count("DriveToPosition"));
    ASSERT_EQ(state.task_states.at("DriveToPosition"), TaskState::PENDING);
}