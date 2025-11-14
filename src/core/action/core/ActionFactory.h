#ifndef MXRC_CORE_ACTION_ACTION_FACTORY_H
#define MXRC_CORE_ACTION_ACTION_FACTORY_H

#include "core/action/interfaces/IActionFactory.h"
#include <functional>
#include <map>
#include <string>
#include <memory>

namespace mxrc::core::action {

/**
 * @brief Action 팩토리 구현
 *
 * 팩토리 함수를 등록하고 Action 인스턴스를 생성합니다.
 */
class ActionFactory : public IActionFactory {
public:
    /**
     * @brief 팩토리 함수 타입
     *
     * Action ID와 파라미터를 받아 Action 인스턴스를 생성하는 함수
     */
    using FactoryFunction = std::function<std::shared_ptr<IAction>(
        const std::string& id,
        const std::map<std::string, std::string>& parameters)>;

    ActionFactory() = default;
    ~ActionFactory() override = default;

    /**
     * @brief 팩토리 함수 등록
     *
     * @param type Action 타입
     * @param factoryFunc 팩토리 함수
     */
    void registerFactory(const std::string& type, FactoryFunction factoryFunc);

    /**
     * @brief Action 생성
     *
     * @param type Action 타입
     * @param parameters Action 파라미터 (id 포함)
     * @return 생성된 Action 인스턴스
     * @throws std::runtime_error 지원하지 않는 타입인 경우
     */
    std::shared_ptr<IAction> createAction(
        const std::string& type,
        const std::map<std::string, std::string>& parameters) override;

    /**
     * @brief 등록된 타입 확인
     *
     * @param type Action 타입
     * @return 등록되어 있으면 true
     */
    bool hasType(const std::string& type) const;

    /**
     * @brief 등록된 모든 타입 조회
     *
     * @return 등록된 타입 목록
     */
    std::vector<std::string> getRegisteredTypes() const;

private:
    std::map<std::string, FactoryFunction> factories_;
};

} // namespace mxrc::core::action

#endif // MXRC_CORE_ACTION_ACTION_FACTORY_H
