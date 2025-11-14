#ifndef MXRC_CORE_TASK_TASK_EXECUTOR_H
#define MXRC_CORE_TASK_TASK_EXECUTOR_H

#include "core/task/interfaces/ITaskExecutor.h"
#include "core/action/core/ActionFactory.h"
#include "core/action/core/ActionExecutor.h"
#include "core/sequence/core/SequenceEngine.h"
#include "core/action/util/Logger.h"
#include <map>
#include <mutex>
#include <atomic>

namespace mxrc::core::task {

/**
 * @brief Task 실행자 구현
 *
 * Task를 실행하고 관리합니다.
 * Phase 3B-1에서는 단일 실행(ONCE)만 지원합니다.
 */
class TaskExecutor : public ITaskExecutor {
public:
    /**
     * @brief 생성자
     */
    TaskExecutor(
        std::shared_ptr<mxrc::core::action::ActionFactory> actionFactory,
        std::shared_ptr<mxrc::core::action::ActionExecutor> actionExecutor,
        std::shared_ptr<mxrc::core::sequence::SequenceEngine> sequenceEngine
    );

    ~TaskExecutor() = default;

    // 복사 및 이동 금지
    TaskExecutor(const TaskExecutor&) = delete;
    TaskExecutor& operator=(const TaskExecutor&) = delete;
    TaskExecutor(TaskExecutor&&) = delete;
    TaskExecutor& operator=(TaskExecutor&&) = delete;

    // ITaskExecutor 구현
    TaskExecution execute(
        const TaskDefinition& definition,
        mxrc::core::action::ExecutionContext& context) override;

    void cancel(const std::string& taskId) override;
    void pause(const std::string& taskId) override;
    void resume(const std::string& taskId) override;
    TaskStatus getStatus(const std::string& taskId) const override;
    float getProgress(const std::string& taskId) const override;

private:
    struct TaskState {
        TaskStatus status{TaskStatus::IDLE};
        std::atomic<float> progress{0.0f};
        std::atomic<bool> cancelRequested{false};
        std::atomic<bool> pauseRequested{false};
    };

    std::shared_ptr<mxrc::core::action::ActionFactory> actionFactory_;
    std::shared_ptr<mxrc::core::action::ActionExecutor> actionExecutor_;
    std::shared_ptr<mxrc::core::sequence::SequenceEngine> sequenceEngine_;

    mutable std::mutex stateMutex_;
    std::map<std::string, TaskState> states_;

    /**
     * @brief Task 상태 가져오기 또는 생성
     */
    TaskState& getOrCreateState(const std::string& taskId);

    /**
     * @brief Action 실행
     */
    TaskExecution executeAction(
        const TaskDefinition& definition,
        mxrc::core::action::ExecutionContext& context,
        TaskState& state
    );

    /**
     * @brief Sequence 실행
     */
    TaskExecution executeSequence(
        const TaskDefinition& definition,
        mxrc::core::action::ExecutionContext& context,
        TaskState& state
    );
};

} // namespace mxrc::core::task

#endif // MXRC_CORE_TASK_TASK_EXECUTOR_H
