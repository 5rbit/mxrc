#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "core/task/MissionManager.h"
#include "core/task/contracts/IDataStore.h"
#include "mocks/MockDataStore.h"
#include <chrono>
#include <thread>

using namespace mxrc::task;
using ::testing::AtLeast;
using ::testing::Return;

class MissionManagerRecoveryTest : public ::testing::Test {
protected:
    std::shared_ptr<MockDataStore> mockDataStore;
    MissionManager* missionManager; // Use a pointer to allow re-initialization

    void SetUp() override {
        mockDataStore = std::make_shared<MockDataStore>();
        // We need to control the singleton instance for each test
        missionManager = &MissionManager::getInstance(mockDataStore);
    }

    void TearDown() override {
        // Since MissionManager is a singleton, we can't easily destroy it.
        // For the purpose of these tests, we assume the state is reset or independent.
    }
};

TEST_F(MissionManagerRecoveryTest, RecoverMissionSuccessfully) {
    // 1. Setup the mock DataStore to return a mission state
    MissionStateDto recoveredStateDto;
    recoveredStateDto.missionId = "test_mission";
    recoveredStateDto.missionStatus = "RUNNING";

    EXPECT_CALL(*mockDataStore, loadMissionState("test_instance_id"))
        .Times(1)
        .WillOnce(Return(recoveredStateDto));

    // 2. Call recoverMission
    bool result = missionManager->recoverMission("test_instance_id");

    // 3. Assert the results
    ASSERT_TRUE(result);
    // Add more assertions to check the internal state of MissionManager if possible
}

TEST_F(MissionManagerRecoveryTest, RecoverMissionFailsWhenNoStateExists) {
    // 1. Setup the mock DataStore to return an empty optional
    EXPECT_CALL(*mockDataStore, loadMissionState("non_existent_instance_id"))
        .Times(1)
        .WillOnce(Return(std::nullopt));

    // 2. Call recoverMission
    bool result = missionManager->recoverMission("non_existent_instance_id");

    // 3. Assert the results
    ASSERT_FALSE(result);
}