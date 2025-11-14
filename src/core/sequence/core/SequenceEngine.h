#ifndef MXRC_CORE_SEQUENCE_SEQUENCE_ENGINE_H
#define MXRC_CORE_SEQUENCE_SEQUENCE_ENGINE_H

#include "core/sequence/interfaces/ISequenceEngine.h"
#include "core/sequence/core/ConditionEvaluator.h"
#include "core/sequence/core/RetryHandler.h"
#include "core/action/core/ActionFactory.h"
#include "core/action/core/ActionExecutor.h"
#include "core/action/util/Logger.h"
#include <map>
#include <mutex>
#include <atomic>
#include <set>

namespace mxrc::core::sequence {

/**
 * @brief Sequence 엔진 구현
 *
 * Sequence를 실행하고 관리하는 엔진입니다.
 * Phase 2B-1에서는 순차 실행만 지원합니다.
 */
class SequenceEngine : public ISequenceEngine {
public:
    /**
     * @brief 생성자
     *
     * @param factory Action 팩토리
     * @param executor Action 실행자
     */
    SequenceEngine(
        std::shared_ptr<mxrc::core::action::ActionFactory> factory,
        std::shared_ptr<mxrc::core::action::ActionExecutor> executor
    );

    ~SequenceEngine() = default;

    // 복사 및 이동 금지
    SequenceEngine(const SequenceEngine&) = delete;
    SequenceEngine& operator=(const SequenceEngine&) = delete;
    SequenceEngine(SequenceEngine&&) = delete;
    SequenceEngine& operator=(SequenceEngine&&) = delete;

    // ISequenceEngine 구현
    SequenceResult execute(
        const SequenceDefinition& definition,
        mxrc::core::action::ExecutionContext& context) override;

    void cancel(const std::string& sequenceId) override;
    void pause(const std::string& sequenceId) override;
    void resume(const std::string& sequenceId) override;
    SequenceStatus getStatus(const std::string& sequenceId) const override;
    float getProgress(const std::string& sequenceId) const override;

private:
    struct SequenceState {
        SequenceStatus status{SequenceStatus::PENDING};
        std::atomic<float> progress{0.0f};
        std::atomic<bool> cancelRequested{false};
        std::atomic<bool> pauseRequested{false};
        int completedSteps{0};
        int totalSteps{0};
    };

    std::shared_ptr<mxrc::core::action::ActionFactory> factory_;
    std::shared_ptr<mxrc::core::action::ActionExecutor> executor_;
    std::unique_ptr<ConditionEvaluator> conditionEvaluator_;
    std::unique_ptr<RetryHandler> retryHandler_;

    mutable std::mutex stateMutex_;
    std::map<std::string, SequenceState> states_;

    /**
     * @brief Sequence 상태 가져오기 또는 생성
     */
    SequenceState& getOrCreateState(const std::string& sequenceId);

    /**
     * @brief 순차 실행
     */
    SequenceResult executeSequential(
        const SequenceDefinition& definition,
        mxrc::core::action::ExecutionContext& context,
        SequenceState& state
    );

    /**
     * @brief 진행률 업데이트
     */
    void updateProgress(SequenceState& state, int completedSteps, int totalSteps);

    /**
     * @brief 조건부 분기 처리
     *
     * @param actionId 실행된 Action ID
     * @param definition Sequence 정의
     * @param context 실행 컨텍스트
     * @param executedActions 이미 실행된 Action ID 집합
     * @param state Sequence 상태
     * @return 분기로 인해 추가 실행된 스텝 수
     */
    int handleConditionalBranch(
        const std::string& actionId,
        const SequenceDefinition& definition,
        mxrc::core::action::ExecutionContext& context,
        std::set<std::string>& executedActions,
        SequenceState& state
    );
};

} // namespace mxrc::core::sequence

#endif // MXRC_CORE_SEQUENCE_SEQUENCE_ENGINE_H
