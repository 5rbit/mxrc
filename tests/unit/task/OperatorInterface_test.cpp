#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "core/task/OperatorInterface.h"
#include "core/task/MissionManager.h"
#include "core/task/AbstractTask.h"
#include "core/task/TaskContext.h"
#include "core/task/contracts/IDataStore.h"
#include "mocks/MockDataStore.h"
#include <chrono>
#include <thread>

using namespace mxrc::task;
using ::testing::AtLeast;

// Dummy Emergency Task for testing OperatorInterface
class OperatorEmergencyTask : public AbstractTask {
public:
    bool initialize(TaskContext& context) override { return true; }
    bool execute(TaskContext& context) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return true;
    }
    void terminate(TaskContext& context) override { }
    std::string getTaskId() const override { return "OperatorEmergencyTask"; }
};

// Test fixture for OperatorInterface
class OperatorInterfaceTest : public ::testing::Test {
protected:
    OperatorInterface& opInterface = OperatorInterface::getInstance();
    std::shared_ptr<MockDataStore> mockDataStore;
    MissionManager* missionManager;

    void SetUp() override {
        mockDataStore = std::make_shared<MockDataStore>();
        missionManager = &MissionManager::getInstance(mockDataStore);
        // Ensure mission is idle before each test
        missionManager->cancelMission("any_mission_instance_id"); // Use a dummy ID for cancellation
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Load a mission definition for testing
        missionManager->loadMissionDefinition("/Users/tory/workspace/mxrc/missions/simple_mission.xml");
    }

    void TearDown() override {
        missionManager->cancelMission("any_mission_instance_id");
    }
};

TEST_F(OperatorInterfaceTest, RequestStartAndGetMissionStatus) {
    TaskContext initialContext;
    std::string missionInstanceId = opInterface.requestStartMission("simple_mission", initialContext);
    ASSERT_FALSE(missionInstanceId.empty());

    // Give mission some time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    MissionState state = opInterface.getMissionStatus(missionInstanceId);
    ASSERT_TRUE(state.current_status == MissionStatus::RUNNING || state.current_status == MissionStatus::COMPLETED);

    // Wait for mission to complete
    int max_wait_iter = 10;
    while (state.current_status == MissionStatus::RUNNING && max_wait_iter > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        state = opInterface.getMissionStatus(missionInstanceId);
        max_wait_iter--;
    }
    ASSERT_EQ(state.current_status, MissionStatus::COMPLETED);
}

TEST_F(OperatorInterfaceTest, RequestPauseResumeCancelMission) {
    TaskContext initialContext;
    std::string missionInstanceId = opInterface.requestStartMission("simple_mission", initialContext);
    ASSERT_FALSE(missionInstanceId.empty());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Pause
    ASSERT_TRUE(opInterface.requestPauseMission(missionInstanceId));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_EQ(opInterface.getMissionStatus(missionInstanceId).current_status, MissionStatus::PAUSED);

    // Resume
    ASSERT_TRUE(opInterface.requestResumeMission(missionInstanceId));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_EQ(opInterface.getMissionStatus(missionInstanceId).current_status, MissionStatus::RUNNING);

    // Cancel
    ASSERT_TRUE(opInterface.requestCancelMission(missionInstanceId));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_EQ(opInterface.getMissionStatus(missionInstanceId).current_status, MissionStatus::CANCELLED);
}

TEST_F(OperatorInterfaceTest, RequestInsertEmergencyTask) {
    TaskContext initialContext;
    std::string missionInstanceId = opInterface.requestStartMission("simple_mission", initialContext);
    ASSERT_FALSE(missionInstanceId.empty());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::unique_ptr<AbstractTask> emergencyTask = std::make_unique<OperatorEmergencyTask>();
    ASSERT_TRUE(opInterface.requestInsertEmergencyTask(missionInstanceId, std::move(emergencyTask), 100));

    // Verify that the mission continues or completes after emergency task
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    MissionState state = opInterface.getMissionStatus(missionInstanceId);
    ASSERT_TRUE(state.current_status == MissionStatus::RUNNING || state.current_status == MissionStatus::COMPLETED);
}