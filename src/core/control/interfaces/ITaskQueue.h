#pragma once

#include <memory>
#include <vector>
#include <optional>
#include "../dto/Priority.h"
#include "../../task/interfaces/ITask.h"

namespace mxrc::core::control {

/**
 * @brief 범용 작업 큐 인터페이스
 *
 * 우선순위 기반 Task 스케줄링을 담당합니다.
 * 여러 Task를 접수하고, 우선순위에 따라 정렬하여 순차 실행합니다.
 *
 * 특징:
 * - 우선순위 기반 정렬 (Priority enum 사용)
 * - 동일 우선순위 내에서 FIFO 보장
 * - 스레드 안전 (concurrent_queue 사용)
 * - Task 삽입/제거 O(log N)
 *
 * BehaviorArbiter와의 협업:
 * - BehaviorArbiter가 행동을 선택하면 TaskQueue에 추가
 * - TaskQueue는 우선순위 순서대로 TaskExecutor에 전달
 *
 * @see IBehaviorArbiter 행동 의사 결정
 * @see Priority 우선순위 체계
 *
 * Feature 016: Pallet Shuttle Control System
 */
class ITaskQueue {
public:
    virtual ~ITaskQueue() = default;

    /**
     * @brief Task 추가
     *
     * 새로운 Task를 큐에 추가합니다.
     * 우선순위에 따라 적절한 위치에 삽입됩니다.
     *
     * @param task 추가할 Task
     * @param priority Task의 우선순위
     * @return true 추가 성공
     * @return false 추가 실패 (큐가 가득 참, nullptr 등)
     *
     * 스레드 안전성: 스레드 안전
     *
     * @throws std::invalid_argument task가 nullptr인 경우
     */
    virtual bool enqueue(
        std::shared_ptr<mxrc::core::task::ITask> task,
        Priority priority) = 0;

    /**
     * @brief 다음 Task 가져오기
     *
     * 가장 높은 우선순위의 Task를 큐에서 제거하고 반환합니다.
     * 동일 우선순위가 여러 개면 가장 먼저 추가된 것(FIFO)을 반환합니다.
     *
     * @return 다음 Task, 큐가 비어있으면 std::nullopt
     *
     * 스레드 안전성: 스레드 안전
     */
    virtual std::optional<std::shared_ptr<mxrc::core::task::ITask>> dequeue() = 0;

    /**
     * @brief 큐가 비어있는지 확인
     *
     * @return true 비어있음
     * @return false 대기 중인 Task가 있음
     */
    virtual bool isEmpty() const = 0;

    /**
     * @brief 큐에 있는 Task 개수
     *
     * @return size_t 대기 중인 Task 총 개수
     */
    virtual size_t size() const = 0;

    /**
     * @brief 큐 비우기
     *
     * 모든 대기 중인 Task를 제거합니다.
     * 현재 실행 중인 Task는 영향받지 않습니다.
     */
    virtual void clear() = 0;

    /**
     * @brief 특정 Task 제거
     *
     * task_id로 식별되는 Task를 큐에서 제거합니다.
     *
     * @param task_id 제거할 Task의 ID
     * @return true 제거 성공
     * @return false 해당 ID의 Task를 찾을 수 없음
     */
    virtual bool remove(const std::string& task_id) = 0;

    /**
     * @brief 모든 Task 목록 조회 (우선순위 순)
     *
     * 큐를 수정하지 않고 현재 대기 중인 Task 목록을 반환합니다.
     * 디버깅 및 모니터링에 사용됩니다.
     *
     * @return std::vector 우선순위 순으로 정렬된 Task 목록
     *
     * 주의: 이 메서드는 큐를 복사하므로 성능 영향이 있을 수 있습니다.
     */
    virtual std::vector<std::shared_ptr<mxrc::core::task::ITask>> getAllTasks() const = 0;

    /**
     * @brief 다음에 실행될 Task 미리보기
     *
     * dequeue()와 달리 큐에서 제거하지 않습니다.
     *
     * @return 다음 Task, 큐가 비어있으면 std::nullopt
     */
    virtual std::optional<std::shared_ptr<mxrc::core::task::ITask>> peek() const = 0;
};

} // namespace mxrc::core::control
