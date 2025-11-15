#pragma once

#include <string>
#include <map>
#include <any>
#include <chrono>

namespace mxrc::core::task {

/**
 * @brief Task 실행 모드
 */
enum class TaskExecutionMode {
    ONCE,      ///< 단일 실행
    PERIODIC,  ///< 주기적 실행
    TRIGGERED  ///< 트리거 기반 실행
};

/**
 * @brief Task 타입
 */
enum class TaskType {
    SINGLE_ACTION,   ///< 단일 Action 기반 Task
    SEQUENCE_BASED   ///< Sequence 기반 Task
};

/**
 * @brief Task 상태
 */
enum class TaskStatus {
    PENDING,    ///< 대기 중
    RUNNING,    ///< 실행 중
    PAUSED,     ///< 일시정지
    COMPLETED,  ///< 완료
    FAILED,     ///< 실패
    CANCELLED   ///< 취소됨
};

/**
 * @brief TaskStatus를 문자열로 변환
 */
inline std::string taskStatusToString(TaskStatus status) {
    switch (status) {
        case TaskStatus::PENDING:   return "PENDING";
        case TaskStatus::RUNNING:   return "RUNNING";
        case TaskStatus::PAUSED:    return "PAUSED";
        case TaskStatus::COMPLETED: return "COMPLETED";
        case TaskStatus::FAILED:    return "FAILED";
        case TaskStatus::CANCELLED: return "CANCELLED";
        default:                    return "UNKNOWN";
    }
}

/**
 * @brief Task 정의 DTO
 */
struct TaskDefinition {
    std::string id;                                    ///< Task 식별자
    std::string name;                                  ///< Task 이름
    TaskType type;                                     ///< Task 타입
    std::string actionId;                              ///< 단일 Action ID (SINGLE_ACTION인 경우)
    std::string sequenceId;                            ///< Sequence ID (SEQUENCE_BASED인 경우)
    TaskExecutionMode executionMode;                   ///< 실행 모드
    std::chrono::milliseconds interval{0};             ///< 주기 (PERIODIC 모드인 경우)
    std::string triggerCondition;                      ///< 트리거 조건 (TRIGGERED 모드인 경우)
    int priority{0};                                   ///< 우선순위 (0-100)
    std::map<std::string, std::any> parameters;        ///< 파라미터 맵
};

/**
 * @brief Task 실행 정보 DTO
 */
struct TaskExecution {
    std::string id;                                                ///< 실행 식별자
    std::string taskId;                                            ///< 참조 Task ID
    TaskStatus status{TaskStatus::PENDING};                        ///< 상태
    float progress{0.0f};                                          ///< 진행률 (0-100)
    int executionCount{0};                                         ///< 실행 횟수
    std::chrono::system_clock::time_point lastExecutionTime;       ///< 마지막 실행 시간
    std::chrono::system_clock::time_point nextExecutionTime;       ///< 다음 실행 시간
    std::chrono::system_clock::time_point startTime;               ///< 시작 시간
    std::chrono::system_clock::time_point endTime;                 ///< 종료 시간
};

} // namespace mxrc::core::task
