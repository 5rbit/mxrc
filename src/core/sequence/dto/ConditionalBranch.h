#ifndef MXRC_CORE_SEQUENCE_CONDITIONAL_BRANCH_H
#define MXRC_CORE_SEQUENCE_CONDITIONAL_BRANCH_H

#include <string>
#include <vector>

namespace mxrc::core::sequence {

/**
 * @brief 조건부 분기 정의
 *
 * 특정 조건에 따라 다른 Action 경로를 실행하기 위한 정의
 */
struct ConditionalBranch {
    std::string condition;                  // 조건 표현식 (예: "result.status == COMPLETED")
    std::vector<std::string> trueActions;   // 조건이 참일 때 실행할 Action ID 목록
    std::vector<std::string> falseActions;  // 조건이 거짓일 때 실행할 Action ID 목록

    ConditionalBranch() = default;

    ConditionalBranch(const std::string& cond)
        : condition(cond) {}

    /**
     * @brief True 분기에 Action 추가
     */
    ConditionalBranch& addTrueAction(const std::string& actionId) {
        trueActions.push_back(actionId);
        return *this;
    }

    /**
     * @brief False 분기에 Action 추가
     */
    ConditionalBranch& addFalseAction(const std::string& actionId) {
        falseActions.push_back(actionId);
        return *this;
    }

    /**
     * @brief 조건 설정
     */
    ConditionalBranch& setCondition(const std::string& cond) {
        condition = cond;
        return *this;
    }
};

} // namespace mxrc::core::sequence

#endif // MXRC_CORE_SEQUENCE_CONDITIONAL_BRANCH_H
