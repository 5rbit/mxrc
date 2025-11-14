#ifndef MXRC_CORE_ACTION_ACTION_REGISTRY_H
#define MXRC_CORE_ACTION_ACTION_REGISTRY_H

#include "core/action/dto/ActionDefinition.h"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>

namespace mxrc::core::action {

/**
 * @brief Action 레지스트리
 *
 * Action 타입 및 정의를 등록하고 관리합니다.
 */
class ActionRegistry {
public:
    ActionRegistry() = default;
    ~ActionRegistry() = default;

    /**
     * @brief Action 정의 등록
     *
     * @param definition Action 정의
     */
    void registerDefinition(const ActionDefinition& definition);

    /**
     * @brief Action 정의 조회
     *
     * @param id Action ID
     * @return Action 정의 (존재하지 않으면 nullptr)
     */
    std::shared_ptr<const ActionDefinition> getDefinition(const std::string& id) const;

    /**
     * @brief Action 타입 등록 (메타데이터)
     *
     * Action 타입의 메타데이터를 등록합니다.
     *
     * @param type Action 타입
     * @param description 타입 설명
     */
    void registerType(const std::string& type, const std::string& description);

    /**
     * @brief Action 타입 존재 여부 확인
     *
     * @param type Action 타입
     * @return 등록되어 있으면 true
     */
    bool hasType(const std::string& type) const;

    /**
     * @brief Action 정의 존재 여부 확인
     *
     * @param id Action ID
     * @return 등록되어 있으면 true
     */
    bool hasDefinition(const std::string& id) const;

    /**
     * @brief 등록된 모든 Action ID 조회
     *
     * @return Action ID 목록
     */
    std::vector<std::string> getAllDefinitionIds() const;

    /**
     * @brief 등록된 모든 Action 타입 조회
     *
     * @return Action 타입 목록
     */
    std::vector<std::string> getAllTypes() const;

private:
    mutable std::mutex mutex_;                                      // 스레드 안전성
    std::map<std::string, ActionDefinition> definitions_;           // Action 정의 (ID -> Definition)
    std::map<std::string, std::string> typeDescriptions_;           // 타입 설명 (Type -> Description)
};

} // namespace mxrc::core::action

#endif // MXRC_CORE_ACTION_ACTION_REGISTRY_H
