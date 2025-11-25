#include "BehaviorPriorityQueue.h"

namespace mxrc::core::control {

bool BehaviorPriorityQueue::push(const BehaviorRequest& request) {
    // 우선순위에 따라 해당 큐에 추가
    switch (request.priority) {
        case Priority::EMERGENCY_STOP:
            emergency_stop_.push(request);
            break;
        case Priority::SAFETY_ISSUE:
            safety_issue_.push(request);
            break;
        case Priority::URGENT_TASK:
            urgent_task_.push(request);
            break;
        case Priority::NORMAL_TASK:
            normal_task_.push(request);
            break;
        case Priority::MAINTENANCE:
            maintenance_.push(request);
            break;
        default:
            return false;  // 알 수 없는 우선순위
    }

    return true;
}

std::optional<BehaviorRequest> BehaviorPriorityQueue::pop() {
    // 우선순위 순서대로 검색 (높은 순 → 낮은 순)
    if (auto req = tryPop(emergency_stop_)) return req;
    if (auto req = tryPop(safety_issue_)) return req;
    if (auto req = tryPop(urgent_task_)) return req;
    if (auto req = tryPop(normal_task_)) return req;
    if (auto req = tryPop(maintenance_)) return req;

    return std::nullopt;  // 대기 중인 요청 없음
}

bool BehaviorPriorityQueue::isEmpty() const {
    return emergency_stop_.empty() &&
           safety_issue_.empty() &&
           urgent_task_.empty() &&
           normal_task_.empty() &&
           maintenance_.empty();
}

size_t BehaviorPriorityQueue::size() const {
    return emergency_stop_.unsafe_size() +
           safety_issue_.unsafe_size() +
           urgent_task_.unsafe_size() +
           normal_task_.unsafe_size() +
           maintenance_.unsafe_size();
}

void BehaviorPriorityQueue::clear() {
    emergency_stop_.clear();
    safety_issue_.clear();
    urgent_task_.clear();
    normal_task_.clear();
    maintenance_.clear();
}

size_t BehaviorPriorityQueue::sizeOf(Priority priority) const {
    switch (priority) {
        case Priority::EMERGENCY_STOP:
            return emergency_stop_.unsafe_size();
        case Priority::SAFETY_ISSUE:
            return safety_issue_.unsafe_size();
        case Priority::URGENT_TASK:
            return urgent_task_.unsafe_size();
        case Priority::NORMAL_TASK:
            return normal_task_.unsafe_size();
        case Priority::MAINTENANCE:
            return maintenance_.unsafe_size();
        default:
            return 0;
    }
}

std::optional<BehaviorRequest> BehaviorPriorityQueue::tryPop(
    tbb::concurrent_queue<BehaviorRequest>& queue) {

    BehaviorRequest request;
    if (queue.try_pop(request)) {
        return request;
    }
    return std::nullopt;
}

} // namespace mxrc::core::control
