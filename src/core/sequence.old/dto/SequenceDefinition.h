#ifndef MXRC_CORE_SEQUENCE_SEQUENCE_DEFINITION_H
#define MXRC_CORE_SEQUENCE_SEQUENCE_DEFINITION_H

#include <string>
#include <vector>
#include <map>
#include <chrono>

namespace mxrc::core::sequence {

/**
 * @brief Action 스텝 정의
 *
 * Sequence 내부의 개별 Action 실행 단위
 */
struct ActionStep {
    std::string actionId;                           // Action ID (unique within sequence)
    std::string actionType;                         // Action type
    std::map<std::string, std::string> parameters;  // Action parameters

    ActionStep(const std::string& id, const std::string& type)
        : actionId(id), actionType(type) {}

    ActionStep& addParameter(const std::string& key, const std::string& value) {
        parameters[key] = value;
        return *this;
    }
};

/**
 * @brief Sequence 정의
 *
 * Sequence의 메타데이터 및 실행할 Action 목록
 */
struct SequenceDefinition {
    std::string id;                                 // Sequence ID
    std::string name;                               // Sequence 이름
    std::string description;                        // 설명
    std::vector<ActionStep> steps;                  // Action 단계 목록
    std::chrono::milliseconds timeout{0};           // 전체 타임아웃 (0 = 무제한)

    SequenceDefinition(const std::string& seqId, const std::string& seqName = "")
        : id(seqId), name(seqName.empty() ? seqId : seqName) {}

    /**
     * @brief Action 스텝 추가
     */
    SequenceDefinition& addStep(const ActionStep& step) {
        steps.push_back(step);
        return *this;
    }

    /**
     * @brief 타임아웃 설정
     */
    SequenceDefinition& setTimeout(std::chrono::milliseconds ms) {
        timeout = ms;
        return *this;
    }

    /**
     * @brief 설명 설정
     */
    SequenceDefinition& setDescription(const std::string& desc) {
        description = desc;
        return *this;
    }
};

} // namespace mxrc::core::sequence

#endif // MXRC_CORE_SEQUENCE_SEQUENCE_DEFINITION_H
