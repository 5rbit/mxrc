#pragma once

#include <tbb/concurrent_queue.h>
#include <optional>
#include "../dto/BehaviorRequest.h"
#include "../dto/Priority.h"

namespace mxrc::core::control {

/**
 * @brief Custom Priority Queue for BehaviorArbiter
 *
 * 5개의 독립된 concurrent_queue를 사용하여 우선순위별 행동 요청을 관리합니다.
 *
 * 설계 이유:
 * - tbb::concurrent_priority_queue는 동적 우선순위 변경 시 성능 저하
 * - 우선순위가 5단계로 고정되어 있어 별도 큐로 분리하는 것이 효율적
 * - Lock-free 동작으로 RT 성능 보장
 *
 * 동작 방식:
 * - push(): Priority에 따라 해당 큐에 삽입 (O(1))
 * - pop(): 우선순위 순서대로 검색 (O(1) amortized)
 * - 동일 우선순위 내에서 FIFO 보장
 *
 * @see research.md "3.3.2 Custom Priority Queue" 섹션
 *
 * Feature 016: Pallet Shuttle Control System
 */
class BehaviorPriorityQueue {
public:
    BehaviorPriorityQueue() = default;
    ~BehaviorPriorityQueue() = default;

    /**
     * @brief 행동 요청 추가
     *
     * @param request 행동 요청 (복사됨)
     * @return true 추가 성공
     * @return false 추가 실패 (큐가 가득 참)
     */
    bool push(const BehaviorRequest& request);

    /**
     * @brief 다음 행동 요청 가져오기
     *
     * 우선순위 순서대로 검색:
     * EMERGENCY_STOP → SAFETY_ISSUE → URGENT_TASK → NORMAL_TASK → MAINTENANCE
     *
     * @return 다음 행동 요청, 큐가 비어있으면 std::nullopt
     */
    std::optional<BehaviorRequest> pop();

    /**
     * @brief 큐가 비어있는지 확인
     *
     * @return true 모든 큐가 비어있음
     * @return false 대기 중인 요청이 있음
     */
    bool isEmpty() const;

    /**
     * @brief 전체 요청 개수
     *
     * @return size_t 모든 큐의 요청 총합
     */
    size_t size() const;

    /**
     * @brief 모든 큐 비우기
     */
    void clear();

    /**
     * @brief 특정 우선순위 큐의 크기
     *
     * @param priority 우선순위
     * @return size_t 해당 큐의 요청 개수
     */
    size_t sizeOf(Priority priority) const;

private:
    /**
     * @brief 특정 큐에서 pop 시도
     *
     * @param queue 대상 큐
     * @return 요청, 큐가 비어있으면 std::nullopt
     */
    std::optional<BehaviorRequest> tryPop(
        tbb::concurrent_queue<BehaviorRequest>& queue);

    // 5개의 독립된 우선순위 큐
    tbb::concurrent_queue<BehaviorRequest> emergency_stop_;   // Priority 0
    tbb::concurrent_queue<BehaviorRequest> safety_issue_;     // Priority 1
    tbb::concurrent_queue<BehaviorRequest> urgent_task_;      // Priority 2
    tbb::concurrent_queue<BehaviorRequest> normal_task_;      // Priority 3
    tbb::concurrent_queue<BehaviorRequest> maintenance_;      // Priority 4
};

} // namespace mxrc::core::control
