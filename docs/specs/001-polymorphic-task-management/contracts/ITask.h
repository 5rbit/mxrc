#pragma once

#include <string>
#include <map>
#include <memory>
#include "TaskDto.h" // Assuming TaskDto is defined in a common header

// Base interface for all polymorphic Task implementations
class ITask {
public:
    virtual ~ITask() = default;

    virtual const std::string& getId() const = 0;
    virtual const std::string& getName() const = 0;
    virtual const std::string& getType() const = 0;
    virtual const std::map<std::string, std::string>& getParameters() const = 0;
    virtual TaskStatus getStatus() const = 0;
    virtual int getProgress() const = 0;
    virtual const std::string& getCreatedAt() const = 0;
    virtual const std::string& getUpdatedAt() const = 0;

    virtual void setStatus(TaskStatus status) = 0;
    virtual void setProgress(int progress) = 0;
    virtual void setParameters(const std::map<std::string, std::string>& parameters) = 0;

    // FR-003: Task 유형에 따라 다른 실행 로직을 동적으로 호출 (다형성)
    virtual void execute(const std::map<std::string, std::string>& runtimeParameters) = 0;
};
