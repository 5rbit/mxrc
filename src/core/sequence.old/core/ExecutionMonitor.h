#pragma once

#include "core/sequence/dto/SequenceDto.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>

namespace mxrc::core::sequence {

/**
 * @brief 시퀀스 실행 모니터링 및 로깅
 *
 * 시퀀스 실행 진행 상황, 동작 결과, 로그를 기록하고 조회합니다.
 */
class ExecutionMonitor {
public:
    /**
     * @brief 실행 로그 항목
     */
    struct ExecutionLogEntry {
        std::string executionId;      // 실행 ID
        std::string actionId;         // 동작 ID
        ActionStatus actionStatus;    // 동작 상태
        long long timestampMs;        // 기록 시간 (밀리초)
        float progress;               // 진행률
        std::string errorMessage;     // 에러 메시지
    };

    ExecutionMonitor() = default;
    ~ExecutionMonitor() = default;

    // Copy/Move
    ExecutionMonitor(const ExecutionMonitor&) = delete;
    ExecutionMonitor& operator=(const ExecutionMonitor&) = delete;
    ExecutionMonitor(ExecutionMonitor&&) = default;
    ExecutionMonitor& operator=(ExecutionMonitor&&) = default;

    /**
     * @brief 새로운 시퀀스 실행 추적 시작
     * @param executionId 실행 ID
     * @param sequenceId 시퀀스 ID
     * @param totalActions 총 동작 개수
     */
    void startExecution(
        const std::string& executionId,
        const std::string& sequenceId,
        int totalActions);

    /**
     * @brief 동작 실행 로그 기록
     * @param executionId 실행 ID
     * @param actionId 동작 ID
     * @param status 동작 상태
     * @param errorMessage 에러 메시지 (에러 시)
     */
    void logActionExecution(
        const std::string& executionId,
        const std::string& actionId,
        ActionStatus status,
        const std::string& errorMessage = "");

    /**
     * @brief 진행률 업데이트
     * @param executionId 실행 ID
     * @param progress 진행률 (0.0 ~ 1.0)
     */
    void updateProgress(
        const std::string& executionId,
        float progress);

    /**
     * @brief 시퀀스 실행 완료
     * @param executionId 실행 ID
     * @param finalStatus 최종 상태
     */
    void endExecution(
        const std::string& executionId,
        SequenceStatus finalStatus);

    /**
     * @brief 실행 상태 조회
     * @param executionId 실행 ID
     * @return 실행 결과
     */
    SequenceExecutionResult getExecutionStatus(const std::string& executionId) const;

    /**
     * @brief 실행 로그 조회
     * @param executionId 실행 ID
     * @return 로그 항목 목록
     */
    std::vector<ExecutionLogEntry> getExecutionLogs(const std::string& executionId) const;

    /**
     * @brief 특정 동작의 실행 결과 조회
     * @param executionId 실행 ID
     * @param actionId 동작 ID
     * @return 실행 결과
     */
    ActionExecutionResult getActionResult(
        const std::string& executionId,
        const std::string& actionId) const;

    /**
     * @brief 모든 완료된 실행 ID 조회
     * @return 완료된 실행 ID 목록
     */
    std::vector<std::string> getCompletedExecutions() const;

    /**
     * @brief 진행 중인 실행 ID 조회
     * @return 진행 중인 실행 ID 목록
     */
    std::vector<std::string> getRunningExecutions() const;

    /**
     * @brief 실행 히스토리 초기화
     */
    void clear();

    /**
     * @brief 특정 실행 삭제
     * @param executionId 실행 ID
     * @return 삭제 여부
     */
    bool removeExecution(const std::string& executionId);

    /**
     * @brief 등록된 실행 개수
     * @return 실행 개수
     */
    size_t getExecutionCount() const;

private:
    /**
     * @brief 실행 추적 정보
     */
    struct ExecutionTracker {
        SequenceExecutionResult result;
        std::vector<ExecutionLogEntry> logs;
        std::chrono::steady_clock::time_point startTime;
    };

    std::map<std::string, ExecutionTracker> executions_;  // ID -> 추적 정보

    /**
     * @brief 현재 시간을 밀리초로 반환
     * @return 시간 (밀리초)
     */
    long long getCurrentTimeMs() const;
};

} // namespace mxrc::core::sequence

