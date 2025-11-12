#pragma once
#include <string>
#include <map>
#include <memory> // For std::unique_ptr

class ITask {
public:
    virtual ~ITask() = default;
    virtual void execute() = 0;
    virtual void cancel() = 0;
    virtual void pause() = 0;
    virtual std::string getType() const = 0; // To identify the task type
    virtual std::map<std::string, std::string> getParameters() const = 0; // To get current parameters
    // Potentially other methods for status updates, progress, etc.
};
