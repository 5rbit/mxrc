#pragma once

#include "core/sequence/core/SequenceRegistry.h"
#include "core/sequence/core/ActionExecutor.h"
#include "core/sequence/core/ConditionEvaluator.h"
#include "core/sequence/core/ExecutionMonitor.h"
#include "core/sequence/core/ExecutionContext.h"
#include "core/sequence/core/ConditionalBranch.h"
#include "core/sequence/interfaces/IActionFactory.h"
#include "core/sequence/dto/SequenceDto.h"
#include <string>
#include <map>
#include <any>
#include <memory>

namespace mxrc::core::sequence {

/**
 * @brief 시퀀스 실행 엔진
 *
 * 시퀀스 정의를 로드하여 순차적, 조건부, 병렬 실행을 관리합니다.
 */
class SequenceEngine {
public:
    /**
     * @brief 생성자
     * @param registry 시퀀스 레지스트리
     * @param actionFactory 동작 생성 팩토리
     */
    SequenceEngine(
        std::shared_ptr<SequenceRegistry> registry,
        std::shared_ptr<IActionFactory> actionFactory);

    ~SequenceEngine() = default;

    // Copy/Move
    SequenceEngine(const SequenceEngine&) = delete;
    SequenceEngine& operator=(const SequenceEngine&) = delete;
    SequenceEngine(SequenceEngine&&) = default;
    SequenceEngine& operator=(SequenceEngine&&) = default;

    /**
     * @brief 시퀀스 실행 시작
     * @param sequenceId 시퀀스 ID
     * @param parameters 파라미터 맵
     * @return 실행 ID
     * @throw std::runtime_error 시퀀스를 찾을 수 없으면 예외
     */
    std::string execute(
        const std::string& sequenceId,
        const std::map<std::string, std::any>& parameters = {});

    /**
     * @brief 실행 중인 시퀀스 일시정지
     * @param executionId 실행 ID
     * @return 성공 여부
     */
    bool pause(const std::string& executionId);

    /**
     * @brief 일시정지된 시퀀스 재개
     * @param executionId 실행 ID
     * @return 성공 여부
     */
    bool resume(const std::string& executionId);

    /**
     * @brief 실행 중인 시퀀스 취소
     * @param executionId 실행 ID
     * @return 성공 여부
     */
    bool cancel(const std::string& executionId);

    /**
     * @brief 시퀀스 실행 상태 조회
     * @param executionId 실행 ID
     * @return 실행 결과
     */
    SequenceExecutionResult getStatus(const std::string& executionId) const;

    /**
     * @brief 모든 실행 중인 시퀀스 조회
     * @return 실행 ID 목록
     */
    std::vector<std::string> getRunningExecutions() const;

    /**
     * @brief 완료된 시퀀스 조회
     * @return 완료된 실행 ID 목록
     */
    std::vector<std::string> getCompletedExecutions() const;

    /**
     * @brief 실행 컨텍스트 조회
     * @param executionId 실행 ID
     * @return 실행 컨텍스트
     */
    std::shared_ptr<ExecutionContext> getExecutionContext(const std::string& executionId) const;

    /**
     * @brief 조건부 분기 등록
     * @param branch 등록할 분기
     */
    void registerBranch(const ConditionalBranch& branch);

    /**
     * @brief 조건부 분기 조회
     * @param branchId 분기 ID
     * @return 분기 정의 (없으면 nullptr)
     */
    const ConditionalBranch* getBranch(const std::string& branchId) const;

private:
    std::shared_ptr<SequenceRegistry> registry_;
    std::shared_ptr<IActionFactory> actionFactory_;
    std::shared_ptr<ActionExecutor> actionExecutor_;
    std::shared_ptr<ConditionEvaluator> conditionEvaluator_;
    std::shared_ptr<ExecutionMonitor> monitor_;

    // 실행 추적: executionId -> ExecutionContext
    std::map<std::string, std::shared_ptr<ExecutionContext>> executions_;

    // 실행 상태: executionId -> (isRunning, isPaused)
    std::map<std::string, std::pair<bool, bool>> executionState_;

    // 조건부 분기: branchId -> ConditionalBranch
    std::map<std::string, ConditionalBranch> branches_;

    /**
     * @brief 고유 실행 ID 생성
     * @return 실행 ID
     */
    std::string generateExecutionId();

    /**
     * @brief 순차 실행 로직
     * @param definition 시퀀스 정의
     * @param context 실행 컨텍스트
     * @param executionId 실행 ID
     * @return 성공 여부
     */
    bool executeSequentially(
        const std::shared_ptr<const SequenceDefinition>& definition,
        std::shared_ptr<ExecutionContext> context,
        const std::string& executionId);

    /**
     * @brief 조건부 분기 실행
     * @param branch 분기 정의
     * @param context 실행 컨텍스트
     * @param executionId 실행 ID
     * @return 성공 여부
     */
    bool executeBranch(
        const ConditionalBranch& branch,
        std::shared_ptr<ExecutionContext> context,
        const std::string& executionId);

    /**
     * @brief 실행 ID 카운터
     */
    static int executionCounter_;
};

} // namespace mxrc::core::sequence

