#ifndef MXRC_CORE_TASK_ITRIGGER_PROVIDER_H
#define MXRC_CORE_TASK_ITRIGGER_PROVIDER_H

#include "core/action/util/ExecutionContext.h"
#include <string>

namespace mxrc::core::task {

/**
 * @brief 트리거 제공자 인터페이스
 *
 * Task의 트리거 조건을 평가하기 위한 인터페이스입니다.
 */
class ITriggerProvider {
public:
    virtual ~ITriggerProvider() = default;

    /**
     * @brief 트리거 조건 평가
     *
     * @param condition 트리거 조건 표현식
     * @param context 실행 컨텍스트
     * @return 트리거 발생 여부 (true면 Task 실행)
     */
    virtual bool shouldTrigger(
        const std::string& condition,
        const mxrc::core::action::ExecutionContext& context) const = 0;
};

} // namespace mxrc::core::task

#endif // MXRC_CORE_TASK_ITRIGGER_PROVIDER_H
