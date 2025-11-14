#include "TaskExecutor.h"
#include "core/taskmanager/interfaces/ITask.h"
#include <iostream> // For logging/debugging

namespace mxrc::core::taskmanager {

TaskExecutor::TaskExecutor() {
    // Initialize thread pool or other resources
    // For simplicity, let's assume a single worker thread for now
    threadPool_.emplace_back(&TaskExecutor::workerLoop, this);
}

TaskExecutor::~TaskExecutor() {
    // Clean up resources, join threads
    // For simplicity, just detach for now (not ideal for production)
    for (auto& thread : threadPool_) {
        if (thread.joinable()) {
            thread.detach(); 
        }
    }
}

void TaskExecutor::submit(std::shared_ptr<ITask> task) {
    // In a real implementation, this would add the task to a queue
    // and a worker thread would pick it up.
    // For now, we'll just execute it directly in a new thread for demonstration.

    // Store the task in activeTasks_
    {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        activeTasks_[task->getId()] = task;
    }

    std::thread([task, this]() {
        task->execute();
        // After execution, the task remains in the activeTasks_ map
        // so its final status can be queried. A separate cleanup
        // mechanism would be needed in a production system.
    }).detach(); // Detach for simplicity, proper management needed for production
}

void TaskExecutor::cancel(const std::string& taskId) {
    // In a real implementation, this would find the task in the queue
    // or active tasks and call its cancel method.
    std::cout << "TaskExecutor: Attempting to cancel task " << taskId << std::endl;

    std::lock_guard<std::mutex> lock(tasksMutex_);
    auto it = activeTasks_.find(taskId);
    if (it != activeTasks_.end()) {
        it->second->cancel();
    }
}

std::shared_ptr<ITask> TaskExecutor::getTask(const std::string& taskId) const {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    auto it = activeTasks_.find(taskId);
    if (it != activeTasks_.end()) {
        return it->second;
    }
    return nullptr;
}

void TaskExecutor::workerLoop() {
    // This would be the main loop for a worker thread,
    // continuously pulling tasks from a queue and executing them.
    // For now, it's a placeholder as submit() directly detaches threads.
    std::cout << "TaskExecutor: Worker loop started." << std::endl;
}

} // namespace mxrc::core::taskmanager