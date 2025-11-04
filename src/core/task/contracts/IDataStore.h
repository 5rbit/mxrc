// src/core/task/contracts/IDataStore.h

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <optional>

#include "MissionStateDto.h"
#include "TaskStateDto.h"

namespace mxrc {
namespace task {

class IDataStore {
public:
    virtual ~IDataStore() = default;

    /**
     * @brief Mission의 현재 상태를 저장하거나 업데이트합니다.
     * 
     * @param missionState 저장할 Mission의 상태 DTO
     * @return 성공 시 true, 실패 시 false
     */
    virtual bool saveMissionState(const MissionStateDto& missionState) = 0;

    /**
     * @brief 특정 ID의 Mission 상태를 불러옵니다.
     * 
     * @param missionId 불러올 Mission의 ID
     * @return MissionStateDto가 존재하면 포함하는 std::optional, 그렇지 않으면 std::nullopt
     */
    virtual std::optional<MissionStateDto> loadMissionState(const std::string& missionId) = 0;

    /**
     * @brief 특정 Mission의 모든 Task 상태 이력을 저장합니다.
     * 
     * @param missionId Task 이력을 저장할 Mission의 ID
     * @param taskHistory 저장할 Task 상태 DTO의 벡터
     * @return 성공 시 true, 실패 시 false
     */
    virtual bool saveTaskHistory(const std::string& missionId, const std::vector<TaskStateDto>& taskHistory) = 0;

    /**
     * @brief 특정 Mission의 Task 상태 이력을 불러옵니다.
     * 
     * @param missionId 불러올 Mission의 ID
     * @return TaskStateDto의 벡터. 이력이 없으면 빈 벡터를 반환합니다.
     */
    virtual std::vector<TaskStateDto> loadTaskHistory(const std::string& missionId) = 0;

    /**
     * @brief 시스템 재시작 시 복구해야 할 진행 중이던 Mission의 ID 목록을 가져옵니다.
     * 
     * @return 진행 중인 Mission ID의 벡터
     */
    virtual std::vector<std::string> getPendingMissionIds() = 0;
};

} // namespace task
} // namespace mxrc

