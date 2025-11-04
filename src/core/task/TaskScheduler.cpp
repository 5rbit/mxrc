#include "TaskScheduler.h"
#include <iostream>

namespace mxrc {
namespace task {

TaskScheduler::TaskScheduler()
    : running_(false),
      paused_(false),
      cancel_requested_(false) {
}

TaskScheduler::~TaskScheduler() {
    stop();
}

void TaskScheduler::start() {
    if (running_.load()) {
        return;
    }
    running_.store(true);
    paused_.store(false);
    cancel_requested_.store(false);

    for (size_t i = 0; i < num_worker_threads_; ++i) {
        worker_threads_.emplace_back(&TaskScheduler::schedulerLoop, this);
    }
}

void TaskScheduler::stop() {
    if (!running_.load()) {
        return;
    }
    running_.store(false);
    condition_.notify_all(); // Notify all threads to wake up and exit

    for (std::thread& worker : worker_threads_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    worker_threads_.clear();

    // Clear any remaining tasks in the queue
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!task_queue_.empty()) {
        task_queue_.pop();
    }
}

void TaskScheduler::pause() {
    paused_.store(true);
}

void TaskScheduler::resume() {
    paused_.store(false);
    condition_.notify_all(); // Notify threads that they can resume processing
}

void TaskScheduler::cancelAll() {
    cancel_requested_.store(true);
    condition_.notify_all(); // Wake up threads to process cancellation
}

void TaskScheduler::addTask(std::unique_ptr<AbstractTask> task, int priority) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    task_queue_.push({std::move(task), priority});
    condition_.notify_one(); // Notify one worker thread that a new task is available
}

void TaskScheduler::schedulerLoop() {
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        condition_.wait(lock, [this] {
            return !running_.load() || (!task_queue_.empty() && !paused_.load() && !cancel_requested_.load());
        });

        if (!running_.load()) {
            break; // Exit thread if scheduler is stopped
        }
        if (paused_.load()) {
            continue; // Continue waiting if paused
        }
        if (cancel_requested_.load()) {
            // Clear all tasks and reset cancel_requested_ flag
            while (!task_queue_.empty()) {
                task_queue_.pop();
            }
            cancel_requested_.store(false);
            continue; // Go back to waiting
        }

        ScheduledTask scheduled_task = std::move(task_queue_.top());
        task_queue_.pop();
        lock.unlock(); // Unlock mutex before executing task

        // Execute the task
        std::cout << "Executing task: " << scheduled_task.task->getTaskId() << std::endl;
        TaskContext context; // Create a new context for each task execution
        scheduled_task.task->initialize(context);
        scheduled_task.task->execute(context);
        scheduled_task.task->terminate(context);
        std::cout << "Finished task: " << scheduled_task.task->getTaskId() << std::endl;
    }
}

} // namespace task
} // namespace mxrc
