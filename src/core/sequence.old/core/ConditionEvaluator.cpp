#include "ConditionEvaluator.h"
#include <algorithm>
#include <cctype>
#include <spdlog/spdlog.h>

namespace mxrc::core::sequence {

bool ConditionEvaluator::evaluate(
    const std::string& expression,
    const ExecutionContext& context) {

    auto logger = spdlog::get("mxrc");

    lastError_.clear();

    if (expression.empty()) {
        lastError_ = "Expression is empty";
        return false;
    }

    try {
        std::vector<std::string> tokens = tokenize(expression);

        if (tokens.empty()) {
            lastError_ = "No tokens in expression";
            return false;
        }

        // 간단한 재귀적 평가 (AND/OR 우선순위)
        // 이 구현은 기본 조건 평가만 지원
        // 예: "a == 10" "a > 5 AND b < 20" "a == 10 OR b == 20"

        bool result = false;
        std::string currentOp = "AND";  // 첫 번째 토큰은 비교식
        size_t i = 0;

        while (i < tokens.size()) {
            // 비교식 평가
            if (i + 2 < tokens.size()) {
                std::string varName = tokens[i];
                std::string op = tokens[i + 1];
                std::string varValue = tokens[i + 2];

                // 변수 조회
                std::any leftValue = context.getVariable(varName);
                if (!leftValue.has_value()) {
                    // 숫자 리터럴일 수 있음
                    leftValue = parseValue(varName);
                }

                std::any rightValue = parseValue(varValue);

                bool comparison = performComparison(leftValue, op, rightValue);

                // AND/OR 로직
                if (i == 0) {
                    result = comparison;
                } else {
                    if (currentOp == "AND") {
                        result = result && comparison;
                    } else if (currentOp == "OR") {
                        result = result || comparison;
                    }
                }

                i += 3;

                // 다음 논리 연산자 확인
                if (i < tokens.size()) {
                    std::string nextToken = tokens[i];
                    if (nextToken == "AND" || nextToken == "OR") {
                        currentOp = nextToken;
                        i++;
                    }
                }
            } else {
                lastError_ = "Invalid expression format";
                return false;
            }
        }

        logger->debug("조건 평가: expression={}, result={}", expression, result);
        return result;

    } catch (const std::exception& e) {
        lastError_ = std::string("Exception: ") + e.what();
        logger->error("조건 평가 예외: {}", lastError_);
        return false;
    }
}

bool ConditionEvaluator::isValidExpression(const std::string& expression) const {
    if (expression.empty()) {
        return false;
    }

    // 기본 유효성 검사
    std::vector<std::string> tokens = tokenize(expression);
    return !tokens.empty();
}

std::vector<std::string> ConditionEvaluator::tokenize(const std::string& expression) const {
    std::vector<std::string> tokens;
    std::string token;

    for (size_t i = 0; i < expression.length(); ++i) {
        char c = expression[i];

        if (std::isspace(c)) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += c;
        }
    }

    if (!token.empty()) {
        tokens.push_back(token);
    }

    return tokens;
}

bool ConditionEvaluator::performComparison(
    const std::any& left,
    const std::string& op,
    const std::any& right) const {

    // 숫자 비교
    if (isNumeric(left) && isNumeric(right)) {
        double leftVal = toDouble(left);
        double rightVal = toDouble(right);

        if (op == "==") return leftVal == rightVal;
        if (op == "!=") return leftVal != rightVal;
        if (op == "<") return leftVal < rightVal;
        if (op == ">") return leftVal > rightVal;
        if (op == "<=") return leftVal <= rightVal;
        if (op == ">=") return leftVal >= rightVal;
    }

    // 문자열 비교
    try {
        std::string leftStr = left.has_value() ? std::any_cast<std::string>(left) : "";
        std::string rightStr = right.has_value() ? std::any_cast<std::string>(right) : "";

        if (op == "==") return leftStr == rightStr;
        if (op == "!=") return leftStr != rightStr;
    } catch (...) {
        // 타입 변환 실패
    }

    return false;
}

std::any ConditionEvaluator::parseValue(const std::string& str) const {
    // 숫자인지 확인
    try {
        size_t idx;
        double value = std::stod(str, &idx);
        if (idx == str.length()) {
            return value;
        }
    } catch (...) {
        // 숫자가 아님
    }

    // 문자열로 반환
    return str;
}

bool ConditionEvaluator::isNumeric(const std::any& value) const {
    return value.type() == typeid(int) ||
           value.type() == typeid(float) ||
           value.type() == typeid(double) ||
           value.type() == typeid(long) ||
           value.type() == typeid(long long);
}

double ConditionEvaluator::toDouble(const std::any& value) const {
    if (value.type() == typeid(int)) {
        return static_cast<double>(std::any_cast<int>(value));
    } else if (value.type() == typeid(float)) {
        return static_cast<double>(std::any_cast<float>(value));
    } else if (value.type() == typeid(double)) {
        return std::any_cast<double>(value);
    } else if (value.type() == typeid(long)) {
        return static_cast<double>(std::any_cast<long>(value));
    } else if (value.type() == typeid(long long)) {
        return static_cast<double>(std::any_cast<long long>(value));
    }

    throw std::bad_any_cast();
}

} // namespace mxrc::core::sequence

