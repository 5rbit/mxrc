#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include "AbstractTask.h"

namespace mxrc {
namespace task {

struct ScheduledTask {
    std::unique_ptr<AbstractTask> task;
    int priority; // Higher value means higher priority
    // Add other scheduling related metadata here, e.g., dependencies, state

    // Custom comparator for priority_queue (max-heap)
    bool operator<(const ScheduledTask& other) const {
        return priority < other.priority;
    }
};

class TaskScheduler {
public:
    TaskScheduler();
    ~TaskScheduler();

    void start();
    void stop();
    void pause();
    void resume();
    void cancelAll();

    void addTask(std::unique_ptr<AbstractTask> task, int priority);

private:
    void schedulerLoop();

    std::priority_queue<ScheduledTask> task_queue_;
    std::vector<std::thread> worker_threads_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> running_;
    std::atomic<bool> paused_;
    std::atomic<bool> cancel_requested_;

    // Configuration for thread pool size
    size_t num_worker_threads_ = 2; // Example: 2 worker threads
};

} // namespace task
} // namespace mxrc

#endif // TASK_SCHEDULER_H
