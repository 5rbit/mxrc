#include "core/sequence/core/ConditionEvaluator.h"
#include <sstream>
#include <algorithm>
#include <any>

namespace mxrc::core::sequence {

using namespace mxrc::core::action;

bool ConditionEvaluator::evaluate(
    const std::string& expression,
    const ExecutionContext& context) const {
    
    // 표현식 파싱: "variable operator value"
    std::string expr = trim(expression);
    
    // 연산자 찾기 (==, !=, >=, <=, >, <)
    std::string op;
    size_t opPos = std::string::npos;
    
    // 2글자 연산자 먼저 체크
    for (const auto& candidate : {"==", "!=", ">=", "<="}) {
        opPos = expr.find(candidate);
        if (opPos != std::string::npos) {
            op = candidate;
            break;
        }
    }
    
    // 1글자 연산자 체크
    if (op.empty()) {
        for (const auto& candidate : {">", "<"}) {
            opPos = expr.find(candidate);
            if (opPos != std::string::npos) {
                op = candidate;
                break;
            }
        }
    }
    
    if (op.empty() || opPos == std::string::npos) {
        Logger::get()->error("Invalid condition expression: {}", expression);
        throw std::runtime_error("Invalid condition expression: " + expression);
    }
    
    // 좌변과 우변 추출
    std::string left = trim(expr.substr(0, opPos));
    std::string right = trim(expr.substr(opPos + op.length()));
    
    // 좌변이 변수명이면 컨텍스트에서 값 조회
    std::string leftValue = getVariableAsString(left, context);
    
    // 비교 수행
    if (isNumber(leftValue) && isNumber(right)) {
        return compareNumbers(leftValue, op, right);
    } else {
        return compareStrings(leftValue, op, right);
    }
}

std::string ConditionEvaluator::trim(const std::string& str) const {
    auto start = str.find_first_not_of(" \t\n\r");
    auto end = str.find_last_not_of(" \t\n\r");
    
    if (start == std::string::npos) {
        return "";
    }
    
    return str.substr(start, end - start + 1);
}

std::string ConditionEvaluator::getVariableAsString(
    const std::string& varName,
    const ExecutionContext& context) const {
    
    auto value = context.getVariable(varName);
    if (!value.has_value()) {
        // 변수가 없으면 리터럴 값으로 간주
        return varName;
    }
    
    // std::any에서 값 추출 시도
    try {
        // int 시도
        try {
            return std::to_string(std::any_cast<int>(value.value()));
        } catch (...) {}
        
        // long 시도
        try {
            return std::to_string(std::any_cast<long>(value.value()));
        } catch (...) {}
        
        // double 시도
        try {
            return std::to_string(std::any_cast<double>(value.value()));
        } catch (...) {}
        
        // float 시도
        try {
            return std::to_string(std::any_cast<float>(value.value()));
        } catch (...) {}
        
        // bool 시도
        try {
            return std::any_cast<bool>(value.value()) ? "true" : "false";
        } catch (...) {}
        
        // string 시도
        try {
            return std::any_cast<std::string>(value.value());
        } catch (...) {}
        
        // const char* 시도
        try {
            return std::string(std::any_cast<const char*>(value.value()));
        } catch (...) {}
        
        // 변환 실패 시 변수명 그대로 반환
        return varName;
        
    } catch (const std::exception& e) {
        Logger::get()->warn("Failed to cast variable '{}': {}", varName, e.what());
        return varName;
    }
}

bool ConditionEvaluator::compareStrings(
    const std::string& left,
    const std::string& op,
    const std::string& right) const {
    
    if (op == "==") {
        return left == right;
    } else if (op == "!=") {
        return left != right;
    } else if (op == ">") {
        return left > right;
    } else if (op == "<") {
        return left < right;
    } else if (op == ">=") {
        return left >= right;
    } else if (op == "<=") {
        return left <= right;
    }
    
    return false;
}

bool ConditionEvaluator::compareNumbers(
    const std::string& left,
    const std::string& op,
    const std::string& right) const {
    
    double leftNum = std::stod(left);
    double rightNum = std::stod(right);
    
    if (op == "==") {
        return std::abs(leftNum - rightNum) < 1e-9;
    } else if (op == "!=") {
        return std::abs(leftNum - rightNum) >= 1e-9;
    } else if (op == ">") {
        return leftNum > rightNum;
    } else if (op == "<") {
        return leftNum < rightNum;
    } else if (op == ">=") {
        return leftNum >= rightNum;
    } else if (op == "<=") {
        return leftNum <= rightNum;
    }
    
    return false;
}

bool ConditionEvaluator::isNumber(const std::string& str) const {
    if (str.empty()) {
        return false;
    }
    
    std::istringstream iss(str);
    double d;
    iss >> std::noskipws >> d;
    
    return iss.eof() && !iss.fail();
}

} // namespace mxrc::core::sequence
