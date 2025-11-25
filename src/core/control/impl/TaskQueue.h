#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>
#include "../interfaces/ITaskQueue.h"
#include "../../task/interfaces/ITask.h"
#include "../dto/Priority.h"

namespace mxrc::core::control {

/**
 * @brief Priority-based Task Queue 구현
 *
 * BehaviorPriorityQueue와 유사하게 5개의 Priority 레벨을 가진 큐를 관리합니다.
 * Task를 Priority별로 관리하며, dequeue 시 가장 높은 우선순위의 Task를 반환합니다.
 *
 * Thread-safety:
 * - 모든 public 메서드는 mutex로 보호됩니다
 * - BehaviorPriorityQueue는 lock-free지만, TaskQueue는 Task 객체 관리가 필요하여 mutex 사용
 *
 * 주요 차이점:
 * - BehaviorPriorityQueue: BehaviorRequest (경량 구조체) 관리, lock-free
 * - TaskQueue: ITask 스마트 포인터 관리, mutex 보호
 *
 * @see BehaviorPriorityQueue
 *
 * Feature 016: Pallet Shuttle Control System
 */
class TaskQueue : public ITaskQueue {
public:
    TaskQueue() = default;
    ~TaskQueue() override = default;

    // ITaskQueue 인터페이스 구현
    bool enqueue(std::shared_ptr<task::ITask> task, Priority priority) override;

    std::optional<std::shared_ptr<task::ITask>> dequeue() override;

    bool isEmpty() const override;

    size_t size() const override;

    void clear() override;

    bool remove(const std::string& task_id) override;

    std::vector<std::shared_ptr<task::ITask>> getAllTasks() const override;

    std::optional<std::shared_ptr<task::ITask>> peek() const override;

private:
    /**
     * @brief Priority별 Task 큐
     *
     * Priority → Task ID → Task 매핑
     * 각 Priority별로 FIFO 순서를 보장하기 위해 삽입 순서를 추적합니다.
     */
    struct PriorityQueue {
        std::vector<std::shared_ptr<task::ITask>> tasks;

        void push(std::shared_ptr<task::ITask> task) {
            tasks.push_back(std::move(task));
        }

        std::optional<std::shared_ptr<task::ITask>> pop() {
            if (tasks.empty()) {
                return std::nullopt;
            }

            auto task = tasks.front();
            tasks.erase(tasks.begin());
            return task;
        }

        bool remove(const std::string& task_id) {
            auto it = std::find_if(tasks.begin(), tasks.end(),
                [&task_id](const auto& task) {
                    return task->getId() == task_id;
                });

            if (it != tasks.end()) {
                tasks.erase(it);
                return true;
            }

            return false;
        }

        size_t size() const {
            return tasks.size();
        }

        bool empty() const {
            return tasks.empty();
        }

        void clear() {
            tasks.clear();
        }
    };

    // 5개의 우선순위별 큐
    PriorityQueue emergency_stop_;   // Priority 0
    PriorityQueue safety_issue_;     // Priority 1
    PriorityQueue urgent_task_;      // Priority 2
    PriorityQueue normal_task_;      // Priority 3
    PriorityQueue maintenance_;      // Priority 4

    // Task ID → Priority 매핑 (빠른 제거를 위해)
    std::unordered_map<std::string, Priority> task_priority_map_;

    // Thread safety
    mutable std::mutex mutex_;

    /**
     * @brief Priority에 해당하는 큐 가져오기
     */
    PriorityQueue& getQueue(Priority priority);
};

} // namespace mxrc::core::control
