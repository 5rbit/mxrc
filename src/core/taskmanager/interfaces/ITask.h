#pragma once
#include <string>
#include <map>
#include <memory> // For std::unique_ptr
#include "../TaskDto.h"

namespace mxrc {
namespace core {
namespace taskmanager {

class ITask {
public:
    virtual ~ITask() = default;
    virtual void execute() = 0;
    virtual void cancel() = 0;
    virtual void pause() = 0;
    virtual std::string getType() const = 0; // To identify the task type
    virtual std::map<std::string, std::string> getParameters() const = 0; // To get current parameters

    // Status and progress methods
    virtual TaskStatus getStatus() const = 0;
    virtual float getProgress() const = 0;
    virtual const std::string& getId() const = 0;
    virtual TaskDto toDto() const = 0;
};

} // namespace taskmanager
} // namespace core
} // namespace mxrc
