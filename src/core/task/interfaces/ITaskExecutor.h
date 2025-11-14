#ifndef MXRC_CORE_TASK_ITASK_EXECUTOR_H
#define MXRC_CORE_TASK_ITASK_EXECUTOR_H

#include "core/task/dto/TaskDefinition.h"
#include "core/task/dto/TaskExecution.h"
#include "core/action/util/ExecutionContext.h"
#include <memory>

namespace mxrc::core::task {

/**
 * @brief Task 실행자 인터페이스
 *
 * Task를 실행하고 관리하는 인터페이스입니다.
 */
class ITaskExecutor {
public:
    virtual ~ITaskExecutor() = default;

    /**
     * @brief Task 실행
     *
     * @param definition Task 정의
     * @param context 실행 컨텍스트
     * @return Task 실행 결과
     */
    virtual TaskExecution execute(
        const TaskDefinition& definition,
        mxrc::core::action::ExecutionContext& context) = 0;

    /**
     * @brief Task 취소
     *
     * @param taskId Task ID
     */
    virtual void cancel(const std::string& taskId) = 0;

    /**
     * @brief Task 일시정지
     *
     * @param taskId Task ID
     */
    virtual void pause(const std::string& taskId) = 0;

    /**
     * @brief Task 재개
     *
     * @param taskId Task ID
     */
    virtual void resume(const std::string& taskId) = 0;

    /**
     * @brief Task 상태 조회
     *
     * @param taskId Task ID
     * @return Task 상태
     */
    virtual TaskStatus getStatus(const std::string& taskId) const = 0;

    /**
     * @brief Task 진행률 조회
     *
     * @param taskId Task ID
     * @return 진행률 (0.0 ~ 1.0)
     */
    virtual float getProgress(const std::string& taskId) const = 0;
};

} // namespace mxrc::core::task

#endif // MXRC_CORE_TASK_ITASK_EXECUTOR_H
