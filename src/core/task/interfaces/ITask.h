#ifndef MXRC_CORE_TASK_ITASK_H
#define MXRC_CORE_TASK_ITASK_H

#include "core/task/dto/TaskStatus.h"
#include "core/task/dto/TaskDefinition.h"
#include "core/task/dto/TaskExecution.h"

namespace mxrc::core::task {

/**
 * @brief Task 인터페이스
 *
 * Task의 기본 동작을 정의합니다.
 */
class ITask {
public:
    virtual ~ITask() = default;

    /**
     * @brief Task ID 조회
     */
    virtual std::string getId() const = 0;

    /**
     * @brief Task 시작
     *
     * @return 실행 ID
     */
    virtual std::string start() = 0;

    /**
     * @brief Task 중지
     */
    virtual void stop() = 0;

    /**
     * @brief Task 일시정지
     */
    virtual void pause() = 0;

    /**
     * @brief Task 재개
     */
    virtual void resume() = 0;

    /**
     * @brief Task 상태 조회
     */
    virtual TaskStatus getStatus() const = 0;

    /**
     * @brief Task 진행률 조회
     */
    virtual float getProgress() const = 0;

    /**
     * @brief Task 정의 조회
     */
    virtual const TaskDefinition& getDefinition() const = 0;
};

} // namespace mxrc::core::task

#endif // MXRC_CORE_TASK_ITASK_H
