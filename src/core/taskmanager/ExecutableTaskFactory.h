#pragma once
#include "IExecutableTask.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <stdexcept>

class ExecutableTaskFactory {
public:
    using TaskCreator = std::function<std::unique_ptr<IExecutableTask>(const std::map<std::string, std::string>&)>;

    static ExecutableTaskFactory& getInstance();

    void registerTaskType(const std::string& type, TaskCreator creator);
    std::unique_ptr<IExecutableTask> createExecutableTask(const std::string& type, const std::map<std::string, std::string>& parameters);

private:
    ExecutableTaskFactory() = default;
    std::map<std::string, TaskCreator> creators_;
};

