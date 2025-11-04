#pragma once

#include <gmock/gmock.h>
#include "core/task/contracts/IDataStore.h"

namespace mxrc {
namespace task {

class MockDataStore : public IDataStore {
public:
    MOCK_METHOD(bool, saveMissionState, (const MissionStateDto& missionState), (override));
    MOCK_METHOD(std::optional<MissionStateDto>, loadMissionState, (const std::string& missionId), (override));
    MOCK_METHOD(bool, saveTaskHistory, (const std::string& missionId, const std::vector<TaskStateDto>& taskHistory), (override));
    MOCK_METHOD(std::vector<TaskStateDto>, loadTaskHistory, (const std::string& missionId), (override));
    MOCK_METHOD(std::vector<std::string>, getPendingMissionIds, (), (override));
};

} // namespace task
} // namespace mxrc
