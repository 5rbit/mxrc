#pragma once

#include "interfaces/ITask.h"
#include "TaskDto.h"
#include <string>

namespace mxrc::core::taskmanager {

class Task : public ITask {
public:
    Task(std::string id, std::string name, std::string type,
         std::map<std::string, std::string> parameters);
    ~Task() override = default;

    void execute() override = 0;
    void cancel() override = 0;
    void pause() override;

    TaskStatus getStatus() const override;
    float getProgress() const override;
    const std::string& getId() const override;
    std::string getType() const override;
    std::map<std::string, std::string> getParameters() const override;
    TaskDto toDto() const override;

protected:
    std::string id_;
    std::string name_;
    std::string type_;
    TaskStatus status_;
    float progress_;
    std::map<std::string, std::string> parameters_;
};

} // namespace mxrc::core::taskmanager