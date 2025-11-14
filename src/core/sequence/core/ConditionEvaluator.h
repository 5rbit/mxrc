#pragma once

#include "core/sequence/core/ExecutionContext.h"
#include <string>
#include <any>
#include <memory>
#include <vector>

namespace mxrc::core::sequence {

/**
 * @brief 조건식 평가자
 *
 * 단순 조건식을 파싱하고 ExecutionContext에서 평가합니다.
 * 지원하는 연산자: ==, !=, <, >, <=, >=, AND, OR, NOT
 */
class ConditionEvaluator {
public:
    ConditionEvaluator() = default;
    ~ConditionEvaluator() = default;

    // Copy/Move
    ConditionEvaluator(const ConditionEvaluator&) = delete;
    ConditionEvaluator& operator=(const ConditionEvaluator&) = delete;
    ConditionEvaluator(ConditionEvaluator&&) = default;
    ConditionEvaluator& operator=(ConditionEvaluator&&) = default;

    /**
     * @brief 조건식 평가
     * @param expression 조건식 (예: "weight > 10 AND pressure <= 100")
     * @param context 실행 컨텍스트 (변수 저장소)
     * @return 평가 결과
     * @throw std::runtime_error 파싱 또는 평가 실패 시
     */
    bool evaluate(const std::string& expression, const ExecutionContext& context);

    /**
     * @brief 마지막 평가 오류 메시지
     * @return 에러 메시지
     */
    const std::string& getLastError() const { return lastError_; }

    /**
     * @brief 조건식 문법 검증
     * @param expression 검증할 조건식
     * @return 유효하면 true
     */
    bool isValidExpression(const std::string& expression) const;

private:
    std::string lastError_;

    /**
     * @brief 조건식 토큰화
     * @param expression 조건식
     * @return 토큰 목록
     */
    std::vector<std::string> tokenize(const std::string& expression) const;

    /**
     * @brief 비교 연산 수행
     * @param left 왼쪽 피연산자
     * @param op 연산자 (==, !=, <, >, <=, >=)
     * @param right 오른쪽 피연산자
     * @return 연산 결과
     */
    bool performComparison(
        const std::any& left,
        const std::string& op,
        const std::any& right) const;

    /**
     * @brief 문자열을 값으로 변환 시도
     * @param str 문자열
     * @return 변환된 값 (숫자 또는 문자열)
     */
    std::any parseValue(const std::string& str) const;

    /**
     * @brief 값이 숫자인지 확인
     * @param value 값
     * @return 숫자면 true
     */
    bool isNumeric(const std::any& value) const;

    /**
     * @brief 값을 double로 변환
     * @param value 값
     * @return double 값
     * @throw std::bad_any_cast 변환 불가 시
     */
    double toDouble(const std::any& value) const;
};

} // namespace mxrc::core::sequence

