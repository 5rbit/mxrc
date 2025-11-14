#ifndef MXRC_CORE_ACTION_IACTION_H
#define MXRC_CORE_ACTION_IACTION_H

#include "core/action/dto/ActionStatus.h"
#include "core/action/util/ExecutionContext.h"
#include <string>
#include <memory>

namespace mxrc::core::action {

/**
 * @brief Action 인터페이스
 *
 * 모든 Action 구현체는 이 인터페이스를 상속받아야 합니다.
 * Action은 로봇 동작의 기본 단위이며, 실행, 취소, 상태 조회 등의
 * 기본 동작을 제공해야 합니다.
 */
class IAction {
public:
    virtual ~IAction() = default;

    /**
     * @brief Action ID 조회
     *
     * @return Action 고유 식별자
     */
    virtual std::string getId() const = 0;

    /**
     * @brief Action 타입 조회
     *
     * @return Action 타입 (예: "Move", "Delay", "SetGripper")
     */
    virtual std::string getType() const = 0;

    /**
     * @brief Action 실행
     *
     * @param context 실행 컨텍스트 (Action 간 데이터 공유)
     * @throws std::runtime_error 실행 중 오류 발생 시
     */
    virtual void execute(ExecutionContext& context) = 0;

    /**
     * @brief Action 취소
     *
     * 실행 중인 Action을 취소합니다.
     * 이 메서드는 비동기적으로 동작할 수 있으며,
     * 호출 즉시 취소가 완료되지 않을 수 있습니다.
     */
    virtual void cancel() = 0;

    /**
     * @brief Action 상태 조회
     *
     * @return 현재 Action 상태
     */
    virtual ActionStatus getStatus() const = 0;

    /**
     * @brief Action 진행률 조회
     *
     * @return 진행률 (0.0 ~ 1.0)
     */
    virtual float getProgress() const = 0;
};

} // namespace mxrc::core::action

#endif // MXRC_CORE_ACTION_IACTION_H
