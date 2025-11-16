// ExecutionTimeCollector.cpp - 실행 시간 메트릭 수집기 구현
// Copyright (C) 2025 MXRC Project

#include "ExecutionTimeCollector.h"
#include <algorithm>
#include <numeric>

using namespace mxrc::core::event;

namespace mxrc::examples::event_monitoring {

ExecutionTimeCollector::~ExecutionTimeCollector() {
    unsubscribe();
}

void ExecutionTimeCollector::subscribeToEventBus(std::shared_ptr<IEventBus> eventBus) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (eventBus_) {
        unsubscribe();
    }

    eventBus_ = eventBus;

    if (!eventBus_) {
        return;
    }

    // ACTION_COMPLETED 이벤트 구독
    auto actionSubId = eventBus_->subscribe(
        [](auto e) { return e->getType() == EventType::ACTION_COMPLETED; },
        [this](std::shared_ptr<IEvent> event) {
            handleActionCompleted(event);
        });
    subscriptionIds_.push_back(actionSubId);

    // SEQUENCE_COMPLETED 이벤트 구독
    auto sequenceSubId = eventBus_->subscribe(
        [](auto e) { return e->getType() == EventType::SEQUENCE_COMPLETED; },
        [this](std::shared_ptr<IEvent> event) {
            handleSequenceCompleted(event);
        });
    subscriptionIds_.push_back(sequenceSubId);

    // TASK_COMPLETED 이벤트 구독
    auto taskSubId = eventBus_->subscribe(
        [](auto e) { return e->getType() == EventType::TASK_COMPLETED; },
        [this](std::shared_ptr<IEvent> event) {
            handleTaskCompleted(event);
        });
    subscriptionIds_.push_back(taskSubId);
}

void ExecutionTimeCollector::unsubscribe() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (eventBus_) {
        for (const auto& subId : subscriptionIds_) {
            eventBus_->unsubscribe(subId);
        }
        subscriptionIds_.clear();
        eventBus_.reset();
    }
}

void ExecutionTimeCollector::recordExecutionTime(const std::string& id, long durationMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    executionTimes_[id].push_back(durationMs);
}

bool ExecutionTimeCollector::hasMetrics(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return executionTimes_.find(id) != executionTimes_.end() &&
           !executionTimes_.at(id).empty();
}

double ExecutionTimeCollector::getAverageExecutionTime(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = executionTimes_.find(id);
    if (it == executionTimes_.end() || it->second.empty()) {
        return 0.0;
    }

    long sum = std::accumulate(it->second.begin(), it->second.end(), 0L);
    return static_cast<double>(sum) / it->second.size();
}

ExecutionTimeCollector::Statistics ExecutionTimeCollector::getStatistics(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    Statistics stats;
    auto it = executionTimes_.find(id);

    if (it == executionTimes_.end() || it->second.empty()) {
        return stats;
    }

    const auto& times = it->second;
    stats.count = times.size();
    stats.totalTime = std::accumulate(times.begin(), times.end(), 0L);
    stats.minTime = *std::min_element(times.begin(), times.end());
    stats.maxTime = *std::max_element(times.begin(), times.end());
    stats.avgTime = static_cast<double>(stats.totalTime) / stats.count;

    return stats;
}

std::vector<std::string> ExecutionTimeCollector::getAllIds() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> ids;
    ids.reserve(executionTimes_.size());

    for (const auto& pair : executionTimes_) {
        ids.push_back(pair.first);
    }

    return ids;
}

void ExecutionTimeCollector::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    executionTimes_.clear();
}

size_t ExecutionTimeCollector::getTotalExecutionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t total = 0;
    for (const auto& pair : executionTimes_) {
        total += pair.second.size();
    }

    return total;
}

void ExecutionTimeCollector::handleActionCompleted(std::shared_ptr<IEvent> event) {
    auto actionEvent = std::static_pointer_cast<ActionCompletedEvent>(event);
    recordExecutionTime(actionEvent->actionId, actionEvent->durationMs);
}

void ExecutionTimeCollector::handleSequenceCompleted(std::shared_ptr<IEvent> event) {
    auto sequenceEvent = std::static_pointer_cast<SequenceCompletedEvent>(event);
    recordExecutionTime(sequenceEvent->sequenceId, sequenceEvent->durationMs);
}

void ExecutionTimeCollector::handleTaskCompleted(std::shared_ptr<IEvent> event) {
    auto taskEvent = std::static_pointer_cast<TaskCompletedEvent>(event);

    // Task는 duration 필드가 없으므로 타임스탬프로 계산
    // 여기서는 간단히 0으로 처리 (실제로는 TaskStarted와 매칭 필요)
    recordExecutionTime(taskEvent->taskId, 0);
}

} // namespace mxrc::examples::event_monitoring
