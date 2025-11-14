#pragma once

#include <string>
#include <memory>

namespace mxrc::core::sequence {

class ExecutionContext;

/**
 * @brief 시퀀스의 조건식을 평가하는 조건 제공자 인터페이스
 * 
 * 조건부 분기(IF-THEN-ELSE)에서 사용되는 조건을 평가합니다.
 */
class IConditionProvider {
public:
    virtual ~IConditionProvider() = default;
    
    /**
     * @brief 조건식 평가
     * @param expression 조건식 (예: "result > 10")
     * @param context 실행 컨텍스트
     * @return 조건식 평가 결과
     * @throws std::exception 조건식 평가 중 오류 발생 시
     */
    virtual bool evaluate(const std::string& expression, 
                         const ExecutionContext& context) = 0;
};

} // namespace mxrc::core::sequence

