#ifndef MXRC_CORE_ACTION_IACTION_FACTORY_H
#define MXRC_CORE_ACTION_IACTION_FACTORY_H

#include "IAction.h"
#include <memory>
#include <map>
#include <string>

namespace mxrc::core::action {

/**
 * @brief Action 팩토리 인터페이스
 *
 * Action 인스턴스를 생성하는 팩토리 인터페이스입니다.
 * 다양한 타입의 Action을 파라미터 기반으로 생성할 수 있어야 합니다.
 */
class IActionFactory {
public:
    virtual ~IActionFactory() = default;

    /**
     * @brief Action 생성
     *
     * @param type Action 타입
     * @param parameters Action 파라미터
     * @return 생성된 Action 인스턴스
     * @throws std::runtime_error 지원하지 않는 타입이거나 파라미터가 잘못된 경우
     */
    virtual std::shared_ptr<IAction> createAction(
        const std::string& type,
        const std::map<std::string, std::string>& parameters) = 0;
};

} // namespace mxrc::core::action

#endif // MXRC_CORE_ACTION_IACTION_FACTORY_H
