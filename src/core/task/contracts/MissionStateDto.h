// src/core/task/contracts/MissionStateDto.h

#pragma once

#include <string>
#include <chrono>

namespace mxrc {
namespace task {

/**
 * @struct MissionStateDto
 * @brief Mission의 상태 정보를 전송하기 위한 데이터 전송 객체(DTO)입니다.
 * 비즈니스 로직을 포함하지 않는 순수 데이터 구조체입니다.
 */
struct MissionStateDto {
    std::string missionId;
    std::string missionStatus; // e.g., "RUNNING", "PAUSED", "COMPLETED", "FAILED"
    std::chrono::system_clock::time_point lastUpdated;
    std::string currentTask_id;
    double missionProgress; // 0.0 to 1.0

    // 필요한 경우 여기에 더 많은 필드를 추가할 수 있습니다.
    // 예를 들어, Mission의 전체 실행 시간, 시작 시간 등
};

} // namespace task
} // namespace mxrc
