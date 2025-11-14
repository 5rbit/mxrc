#pragma once

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <map>

namespace mxrc::core::taskmanager {

class ITask;

class TaskExecutor {
public:
    TaskExecutor();
    ~TaskExecutor();

    void submit(std::shared_ptr<ITask> task);
    void cancel(const std::string& taskId);
    std::shared_ptr<ITask> getTask(const std::string& taskId) const;

private:
    void workerLoop();

    std::vector<std::thread> threadPool_;
    std::map<std::string, std::shared_ptr<ITask>> activeTasks_; // Map of taskId to ITask
    mutable std::mutex tasksMutex_; // Mutex for thread-safe access to activeTasks_
    // ... Other synchronization and task management members
};

} // namespace mxrc::core::taskmanager
