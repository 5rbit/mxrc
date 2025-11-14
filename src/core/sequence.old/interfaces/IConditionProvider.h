#ifndef MXRC_CORE_SEQUENCE_ICONDITION_PROVIDER_H
#define MXRC_CORE_SEQUENCE_ICONDITION_PROVIDER_H

#include "core/action/util/ExecutionContext.h"
#include <string>

namespace mxrc::core::sequence {

/**
 * @brief 조건 평가 제공자 인터페이스
 *
 * Sequence 내 조건부 분기를 평가하기 위한 인터페이스입니다.
 * 사용자가 커스텀 조건 평가 로직을 제공할 수 있습니다.
 */
class IConditionProvider {
public:
    virtual ~IConditionProvider() = default;

    /**
     * @brief 조건 표현식 평가
     *
     * @param expression 조건 표현식 문자열 (예: "result.status == COMPLETED")
     * @param context 실행 컨텍스트 (변수 접근용)
     * @return 조건 평가 결과 (true/false)
     * @throws std::runtime_error 평가 실패 시
     */
    virtual bool evaluate(
        const std::string& expression,
        const mxrc::core::action::ExecutionContext& context) const = 0;
};

} // namespace mxrc::core::sequence

#endif // MXRC_CORE_SEQUENCE_ICONDITION_PROVIDER_H
