#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace mxrc::core::sequence {

/**
 * @brief 조건부 분기 정의
 *
 * IF-THEN-ELSE 구조로 조건에 따라 다른 동작을 실행합니다.
 */
struct ConditionalBranch {
    /**
     * @brief 조건식
     *
     * 예: "weight > 10 AND pressure <= 100"
     * 지원 연산자: ==, !=, <, >, <=, >=, AND, OR, NOT
     */
    std::string condition;

    /**
     * @brief 조건이 참일 때 실행할 동작 ID 목록
     */
    std::vector<std::string> thenActions;

    /**
     * @brief 조건이 거짓일 때 실행할 동작 ID 목록 (선택)
     */
    std::vector<std::string> elseActions;

    /**
     * @brief 이 분기의 고유 ID
     */
    std::string id;

    /**
     * @brief 분기 설명
     */
    std::string description;

    ConditionalBranch() = default;

    ConditionalBranch(
        const std::string& id,
        const std::string& condition,
        const std::vector<std::string>& thenActions)
        : id(id), condition(condition), thenActions(thenActions) {}

    ConditionalBranch(
        const std::string& id,
        const std::string& condition,
        const std::vector<std::string>& thenActions,
        const std::vector<std::string>& elseActions)
        : id(id), condition(condition), thenActions(thenActions), elseActions(elseActions) {}
};

/**
 * @brief 분기 시퀀스 정의
 *
 * 순차 실행 중에 조건부 분기를 포함합니다.
 * ConditionalBranch는 일반 동작 대신 SequenceDefinition의 actionIds에 포함될 수 있습니다.
 */
struct BranchingSequenceDefinition {
    std::string id;
    std::string name;
    std::string version;
    std::string description;

    /**
     * @brief 실행할 항목 ID 목록
     *
     * 각 항목은 다음 중 하나:
     * - 일반 동작 ID (예: "pick_object")
     * - 조건부 분기 ID (예: "check_weight", 등록된 ConditionalBranch 참조)
     */
    std::vector<std::string> sequenceItems;

    /**
     * @brief 등록된 조건부 분기들
     *
     * ID를 키로 하는 분기 맵
     */
    std::map<std::string, ConditionalBranch> branches;

    BranchingSequenceDefinition() = default;

    BranchingSequenceDefinition(const std::string& id, const std::string& name)
        : id(id), name(name), version("1.0.0") {}
};

} // namespace mxrc::core::sequence

