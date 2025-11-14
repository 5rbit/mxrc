#ifndef MXRC_CORE_ACTION_ACTION_DEFINITION_H
#define MXRC_CORE_ACTION_ACTION_DEFINITION_H

#include <string>
#include <map>
#include <chrono>

namespace mxrc::core::action {

/**
 * @brief Action 정의
 *
 * Action의 메타데이터와 설정 정보를 포함합니다.
 */
struct ActionDefinition {
    std::string id;                                      // Action 고유 식별자
    std::string type;                                    // Action 타입 (예: "Move", "Delay", "SetGripper")
    std::map<std::string, std::string> parameters;       // Action 파라미터
    std::chrono::milliseconds timeout{0};               // 타임아웃 (0 = 무제한)
    std::string description;                            // Action 설명

    /**
     * @brief 기본 생성자
     */
    ActionDefinition() = default;

    /**
     * @brief 생성자
     *
     * @param id Action ID
     * @param type Action 타입
     */
    ActionDefinition(const std::string& id, const std::string& type)
        : id(id), type(type) {}

    /**
     * @brief 파라미터 추가
     *
     * @param key 파라미터 키
     * @param value 파라미터 값
     * @return 자기 자신에 대한 참조 (메서드 체이닝을 위해)
     */
    ActionDefinition& addParameter(const std::string& key, const std::string& value) {
        parameters[key] = value;
        return *this;
    }

    /**
     * @brief 타임아웃 설정
     *
     * @param timeout_ms 타임아웃 (밀리초)
     * @return 자기 자신에 대한 참조
     */
    ActionDefinition& setTimeout(long timeout_ms) {
        timeout = std::chrono::milliseconds(timeout_ms);
        return *this;
    }
};

} // namespace mxrc::core::action

#endif // MXRC_CORE_ACTION_ACTION_DEFINITION_H
