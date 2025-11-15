#ifndef MXRC_CORE_TASK_TRIGGER_MANAGER_H
#define MXRC_CORE_TASK_TRIGGER_MANAGER_H

#include "core/task/dto/TaskDefinition.h"
#include "core/action/util/ExecutionContext.h"
#include "core/action/util/Logger.h"
#include <functional>
#include <string>
#include <map>
#include <mutex>
#include <any>

namespace mxrc::core::task {

/**
 * @brief 이벤트 기반 Task 실행을 위한 트리거 매니저
 *
 * Phase 3B-3에서 구현
 * 특정 이벤트가 발생했을 때 Task를 실행합니다.
 */
class TriggerManager {
public:
    using TriggerCallback = std::function<void(const std::string& eventData, mxrc::core::action::ExecutionContext&)>;

    TriggerManager();
    ~TriggerManager() = default;

    // 복사 및 이동 금지
    TriggerManager(const TriggerManager&) = delete;
    TriggerManager& operator=(const TriggerManager&) = delete;
    TriggerManager(TriggerManager&&) = delete;
    TriggerManager& operator=(TriggerManager&&) = delete;

    /**
     * @brief 트리거 등록
     * @param taskId Task 식별자
     * @param eventName 이벤트 이름
     * @param callback 실행할 콜백 함수
     */
    void registerTrigger(
        const std::string& taskId,
        const std::string& eventName,
        TriggerCallback callback
    );

    /**
     * @brief 트리거 해제
     * @param taskId Task 식별자
     * @param eventName 이벤트 이름 (비어있으면 해당 Task의 모든 트리거 해제)
     */
    void unregisterTrigger(const std::string& taskId, const std::string& eventName = "");

    /**
     * @brief 이벤트 발생
     * @param eventName 이벤트 이름
     * @param eventData 이벤트 데이터 (JSON, key-value 등)
     */
    void fireEvent(const std::string& eventName, const std::string& eventData = "");

    /**
     * @brief 트리거가 등록되어 있는지 확인
     * @param taskId Task 식별자
     * @param eventName 이벤트 이름
     */
    bool hasTrigger(const std::string& taskId, const std::string& eventName) const;

    /**
     * @brief Task의 트리거 실행 횟수 조회
     * @param taskId Task 식별자
     */
    int getTriggerCount(const std::string& taskId) const;

    /**
     * @brief 모든 트리거 해제
     */
    void clear();

private:
    struct TriggerInfo {
        std::string taskId;
        std::string eventName;
        TriggerCallback callback;
        int executionCount{0};
    };

    mutable std::mutex mutex_;
    // eventName -> list of TriggerInfo
    std::map<std::string, std::vector<TriggerInfo>> triggers_;
    // taskId -> execution count
    std::map<std::string, int> taskExecutionCounts_;
};

} // namespace mxrc::core::task

#endif // MXRC_CORE_TASK_TRIGGER_MANAGER_H
