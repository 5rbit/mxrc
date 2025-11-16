#include "core/task/core/TriggerManager.h"

namespace mxrc::core::task {

using namespace mxrc::core::action;

TriggerManager::TriggerManager() {
    Logger::get()->info("[TriggerManager] Initialized");
}

void TriggerManager::registerTrigger(
    const std::string& taskId,
    const std::string& eventName,
    TriggerCallback callback
) {
    std::lock_guard<std::mutex> lock(mutex_);

    TriggerInfo info;
    info.taskId = taskId;
    info.eventName = eventName;
    info.callback = callback;

    triggers_[eventName].push_back(std::move(info));

    Logger::get()->info("[TriggerManager] REGISTER - Task: {} for event: '{}'",
                       taskId, eventName);
    Logger::get()->debug("[TriggerManager] Total triggers for event '{}': {}",
                        eventName, triggers_[eventName].size());
}

void TriggerManager::unregisterTrigger(const std::string& taskId, const std::string& eventName) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (eventName.empty()) {
        // 모든 이벤트에서 해당 Task의 트리거 제거
        int removedCount = 0;
        for (auto& [event, triggerList] : triggers_) {
            auto originalSize = triggerList.size();
            triggerList.erase(
                std::remove_if(triggerList.begin(), triggerList.end(),
                    [&taskId](const TriggerInfo& info) {
                        return info.taskId == taskId;
                    }),
                triggerList.end()
            );
            removedCount += (originalSize - triggerList.size());
        }

        taskExecutionCounts_.erase(taskId);

        Logger::get()->info("[TriggerManager] UNREGISTER - Task: {} from all events (removed {} triggers)",
                           taskId, removedCount);
    } else {
        // 특정 이벤트에서만 제거
        auto it = triggers_.find(eventName);
        if (it != triggers_.end()) {
            auto& triggerList = it->second;
            auto originalSize = triggerList.size();

            triggerList.erase(
                std::remove_if(triggerList.begin(), triggerList.end(),
                    [&taskId](const TriggerInfo& info) {
                        return info.taskId == taskId;
                    }),
                triggerList.end()
            );

            Logger::get()->info("[TriggerManager] UNREGISTER - Task: {} from event: '{}' (removed {} triggers)",
                               taskId, eventName, originalSize - triggerList.size());

            if (triggerList.empty()) {
                triggers_.erase(it);
            }
        } else {
            Logger::get()->warn("[TriggerManager] UNREGISTER - Event '{}' not found for task: {}",
                               eventName, taskId);
        }
    }
}

void TriggerManager::fireEvent(const std::string& eventName, const std::string& eventData) {
    std::vector<TriggerInfo> triggersToExecute;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = triggers_.find(eventName);
        if (it != triggers_.end()) {
            triggersToExecute = it->second;  // 복사
        }
    }

    if (triggersToExecute.empty()) {
        Logger::get()->debug("[TriggerManager] FIRE - Event: '{}' (no triggers registered)",
                            eventName);
        return;
    }

    Logger::get()->info("[TriggerManager] FIRE - Event: '{}' (data: '{}', {} triggers)",
                       eventName, eventData, triggersToExecute.size());

    ExecutionContext context;
    if (!eventData.empty()) {
        context.setVariable("event_data", eventData);
    }

    for (auto& trigger : triggersToExecute) {
        try {
            trigger.callback(eventData, context);

            // 실행 횟수 증가
            std::lock_guard<std::mutex> lock(mutex_);
            taskExecutionCounts_[trigger.taskId]++;

            Logger::get()->debug("[TriggerManager] Task {} triggered by event '{}' (count: {})",
                                trigger.taskId, eventName, taskExecutionCounts_[trigger.taskId]);

        } catch (const std::exception& e) {
            Logger::get()->error("[TriggerManager] Task {} trigger execution failed: {}",
                                trigger.taskId, e.what());
        }
    }
}

bool TriggerManager::hasTrigger(const std::string& taskId, const std::string& eventName) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = triggers_.find(eventName);
    if (it == triggers_.end()) {
        return false;
    }

    for (const auto& trigger : it->second) {
        if (trigger.taskId == taskId) {
            return true;
        }
    }

    return false;
}

int TriggerManager::getTriggerCount(const std::string& taskId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = taskExecutionCounts_.find(taskId);
    if (it != taskExecutionCounts_.end()) {
        return it->second;
    }
    return 0;
}

void TriggerManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t totalTriggers = 0;
    for (const auto& [_, triggerList] : triggers_) {
        totalTriggers += triggerList.size();
    }

    triggers_.clear();
    taskExecutionCounts_.clear();

    Logger::get()->info("[TriggerManager] CLEAR - Removed all {} triggers", totalTriggers);
}

} // namespace mxrc::core::task
