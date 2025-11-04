// src/core/task/contracts/TaskStateDto.h

#pragma once

#include <string>
#include <chrono>
#include <optional>

namespace mxrc {
namespace task {

/**
 * @struct TaskStateDto
 * @brief Task의 상태 이력 정보를 전송하기 위한 데이터 전송 객체(DTO)입니다.
 * 비즈니스 로직을 포함하지 않는 순수 데이터 구조체입니다.
 */
struct TaskStateDto {
    std::string taskId;
    std::string taskName;
    std::string taskStatus; // e.g., "PENDING", "RUNNING", "COMPLETED", "FAILED"
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::optional<std::string> failureReason;

    // 필요한 경우 여기에 더 많은 필드를 추가할 수 있습니다.
    // 예를 들어, 재시도 횟수, Task 파라미터 등
};

} // namespace task
} // namespace mxrc
