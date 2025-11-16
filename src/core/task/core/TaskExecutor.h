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

// Forward declaration for EventBus (optional dependency)
namespace mxrc::core::event {
    class IEventBus;
    class IEvent;
}

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
     *
     * @param actionFactory Action 팩토리
     * @param actionExecutor Action 실행자
     * @param sequenceEngine Sequence 엔진
     * @param eventBus 이벤트 버스 (nullptr이면 이벤트 발행하지 않음)
     */
    TaskExecutor(
        std::shared_ptr<mxrc::core::action::ActionFactory> actionFactory,
        std::shared_ptr<mxrc::core::action::ActionExecutor> actionExecutor,
        std::shared_ptr<mxrc::core::sequence::SequenceEngine> sequenceEngine,
        std::shared_ptr<mxrc::core::event::IEventBus> eventBus = nullptr
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

    /**
     * @brief 완료된 태스크 상태 정리
     *
     * 완료, 실패, 취소된 태스크의 상태를 메모리에서 제거합니다.
     * @return 정리된 태스크 개수
     */
    int clearCompletedTasks();

private:
    struct TaskState {
        TaskStatus status{TaskStatus::IDLE};
        std::atomic<float> progress{0.0f};
        std::atomic<bool> cancelRequested{false};
        std::atomic<bool> pauseRequested{false};
        std::chrono::steady_clock::time_point startTime;  // 태스크 시작 시간
        float lastReportedProgress{0.0f};  // 마지막 보고된 진행률 (5% 임계값)
    };

    std::shared_ptr<mxrc::core::action::ActionFactory> actionFactory_;
    std::shared_ptr<mxrc::core::action::ActionExecutor> actionExecutor_;
    std::shared_ptr<mxrc::core::sequence::SequenceEngine> sequenceEngine_;
    std::shared_ptr<mxrc::core::event::IEventBus> eventBus_;

    /**
     * @brief 이벤트 발행 헬퍼 (non-blocking)
     *
     * @tparam EventType 이벤트 타입
     * @param event 발행할 이벤트
     */
    template<typename EventType>
    void publishEvent(std::shared_ptr<EventType> event);

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
