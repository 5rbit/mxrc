#ifndef MXRC_CORE_TASK_TASK_DEFINITION_H
#define MXRC_CORE_TASK_TASK_DEFINITION_H

#include "TaskExecutionMode.h"
#include <string>
#include <chrono>
#include <optional>

namespace mxrc::core::task {

/**
 * @brief Task가 실행할 작업의 타입
 */
enum class TaskWorkType {
    ACTION,     // 단일 Action 실행
    SEQUENCE    // Sequence 실행
};

/**
 * @brief Task 정의
 *
 * Task의 메타데이터 및 실행 설정
 */
struct TaskDefinition {
    std::string id;                                     // Task ID
    std::string name;                                   // Task 이름
    std::string description;                            // 설명
    
    TaskExecutionMode executionMode{TaskExecutionMode::ONCE};  // 실행 모드
    TaskWorkType workType{TaskWorkType::ACTION};       // 작업 타입
    
    std::string workId;                                 // Action ID 또는 Sequence ID
    
    // Periodic 모드 설정
    std::optional<std::chrono::milliseconds> periodicInterval;  // 주기적 실행 간격
    
    // Triggered 모드 설정
    std::optional<std::string> triggerCondition;        // 트리거 조건 표현식
    
    std::chrono::milliseconds timeout{0};               // 타임아웃 (0 = 무제한)

    TaskDefinition(const std::string& taskId, const std::string& taskName = "")
        : id(taskId), name(taskName.empty() ? taskId : taskName) {}

    /**
     * @brief 작업 설정 (Action)
     */
    TaskDefinition& setWork(const std::string& actionId) {
        workType = TaskWorkType::ACTION;
        workId = actionId;
        return *this;
    }

    /**
     * @brief 작업 설정 (Sequence)
     */
    TaskDefinition& setWorkSequence(const std::string& sequenceId) {
        workType = TaskWorkType::SEQUENCE;
        workId = sequenceId;
        return *this;
    }

    /**
     * @brief 단일 실행 모드 설정
     */
    TaskDefinition& setOnceMode() {
        executionMode = TaskExecutionMode::ONCE;
        return *this;
    }

    /**
     * @brief 주기적 실행 모드 설정
     */
    TaskDefinition& setPeriodicMode(std::chrono::milliseconds interval) {
        executionMode = TaskExecutionMode::PERIODIC;
        periodicInterval = interval;
        return *this;
    }

    /**
     * @brief 트리거 실행 모드 설정
     */
    TaskDefinition& setTriggeredMode(const std::string& condition) {
        executionMode = TaskExecutionMode::TRIGGERED;
        triggerCondition = condition;
        return *this;
    }

    /**
     * @brief 타임아웃 설정
     */
    TaskDefinition& setTimeout(std::chrono::milliseconds ms) {
        timeout = ms;
        return *this;
    }

    /**
     * @brief 설명 설정
     */
    TaskDefinition& setDescription(const std::string& desc) {
        description = desc;
        return *this;
    }
};

} // namespace mxrc::core::task

#endif // MXRC_CORE_TASK_TASK_DEFINITION_H
