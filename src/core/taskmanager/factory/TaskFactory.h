#pragma once
#include "../interfaces/ITask.h" // Updated include
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <stdexcept>

class TaskFactory {
public:
    using TaskCreator = std::function<std::unique_ptr<ITask>(const std::map<std::string, std::string>&)>;

    static TaskFactory& getInstance();

    void registerTaskType(const std::string& type, TaskCreator creator);
    std::unique_ptr<ITask> createTask(const std::string& type, const std::map<std::string, std::string>& parameters);

private:
    TaskFactory() = default;
    std::map<std::string, TaskCreator> creators_;
};
