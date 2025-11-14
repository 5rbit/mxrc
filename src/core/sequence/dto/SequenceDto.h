#pragma once

#include <string>
#include <vector>
#include <map>
#include "ActionStatus.h"

namespace mxrc::core::sequence {

/**
 * @brief 시퀀스 정의 DTO
 */
struct SequenceDefinition {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::vector<std::string> actionIds;  // 순서대로 실행할 동작 ID 목록
    std::map<std::string, std::string> metadata;
    
    SequenceDefinition() = default;
    SequenceDefinition(const std::string& id, const std::string& name)
        : id(id), name(name), version("1.0.0"), description("") {}
};

/**
 * @brief 동작 실행 결과 DTO
 */
struct ActionExecutionResult {
    std::string actionId;
    ActionStatus status;
    float progress;
    std::string errorMessage;
    long long executionTimeMs;
    int retryCount;
    
    ActionExecutionResult() 
        : status(ActionStatus::PENDING), progress(0.0f), 
          executionTimeMs(0), retryCount(0) {}
};

/**
 * @brief 시퀀스 실행 결과 DTO
 */
struct SequenceExecutionResult {
    std::string executionId;
    std::string sequenceId;
    SequenceStatus status;
    float progress;
    std::vector<ActionExecutionResult> actionResults;
    long long totalExecutionTimeMs;
    
    SequenceExecutionResult() 
        : status(SequenceStatus::PENDING), progress(0.0f), 
          totalExecutionTimeMs(0) {}
};

} // namespace mxrc::core::sequence

