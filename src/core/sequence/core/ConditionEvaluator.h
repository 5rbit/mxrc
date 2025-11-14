#ifndef MXRC_CORE_SEQUENCE_CONDITION_EVALUATOR_H
#define MXRC_CORE_SEQUENCE_CONDITION_EVALUATOR_H

#include "core/sequence/interfaces/IConditionProvider.h"
#include "core/action/util/Logger.h"
#include <string>
#include <regex>

namespace mxrc::core::sequence {

/**
 * @brief 조건 평가기 구현
 *
 * 간단한 조건 표현식을 평가합니다.
 * 지원 형식: "variable == value", "variable != value", "variable > value", etc.
 */
class ConditionEvaluator : public IConditionProvider {
public:
    ConditionEvaluator() = default;
    ~ConditionEvaluator() override = default;

    /**
     * @brief 조건 표현식 평가
     *
     * 지원 형식:
     * - "variable == value"
     * - "variable != value"
     * - "variable > value"
     * - "variable < value"
     * - "variable >= value"
     * - "variable <= value"
     *
     * @param expression 조건 표현식
     * @param context 실행 컨텍스트
     * @return 평가 결과
     */
    bool evaluate(
        const std::string& expression,
        const mxrc::core::action::ExecutionContext& context) const override;

private:
    /**
     * @brief 문자열 trim
     */
    std::string trim(const std::string& str) const;

    /**
     * @brief 변수 값을 문자열로 변환
     */
    std::string getVariableAsString(
        const std::string& varName,
        const mxrc::core::action::ExecutionContext& context) const;

    /**
     * @brief 문자열 비교
     */
    bool compareStrings(
        const std::string& left,
        const std::string& op,
        const std::string& right) const;

    /**
     * @brief 숫자 비교
     */
    bool compareNumbers(
        const std::string& left,
        const std::string& op,
        const std::string& right) const;

    /**
     * @brief 문자열이 숫자인지 확인
     */
    bool isNumber(const std::string& str) const;
};

} // namespace mxrc::core::sequence

#endif // MXRC_CORE_SEQUENCE_CONDITION_EVALUATOR_H
