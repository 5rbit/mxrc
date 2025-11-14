#ifndef MXRC_CORE_ACTION_DELAY_ACTION_H
#define MXRC_CORE_ACTION_DELAY_ACTION_H

#include "core/action/interfaces/IAction.h"
#include "core/action/dto/ActionStatus.h"
#include "core/action/util/ExecutionContext.h"
#include <string>
#include <chrono>
#include <atomic>

namespace mxrc::core::action {

/**
 * @brief 지연 Action
 *
 * 지정된 시간만큼 대기하는 간단한 Action입니다.
 * 주로 테스트 및 시퀀스 내 대기 시간 삽입에 사용됩니다.
 */
class DelayAction : public IAction {
public:
    /**
     * @brief 생성자
     *
     * @param id Action ID
     * @param delayMs 대기 시간 (밀리초)
     */
    DelayAction(const std::string& id, long delayMs);

    ~DelayAction() override = default;

    // IAction 인터페이스 구현
    std::string getId() const override;
    std::string getType() const override;
    void execute(ExecutionContext& context) override;
    void cancel() override;
    ActionStatus getStatus() const override;
    float getProgress() const override;

private:
    std::string id_;
    std::chrono::milliseconds delay_;
    std::atomic<ActionStatus> status_{ActionStatus::PENDING};
    std::atomic<float> progress_{0.0f};
    std::atomic<bool> cancelled_{false};
};

} // namespace mxrc::core::action

#endif // MXRC_CORE_ACTION_DELAY_ACTION_H
