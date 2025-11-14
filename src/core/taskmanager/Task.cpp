#include "Task.h"
#include <sstream>
#include <chrono>
#include <random>
#include <iomanip>

namespace mxrc::core::taskmanager {

Task::Task(std::string id, std::string name, std::string type,
           std::map<std::string, std::string> parameters)
    : id_(std::move(id)),
      name_(std::move(name)),
      type_(std::move(type)),
      status_(TaskStatus::PENDING),
      progress_(0.0f),
      parameters_(std::move(parameters)) {
}

void Task::pause() {
    if (status_ == TaskStatus::RUNNING) {
        status_ = TaskStatus::PAUSED;
    }
}

TaskStatus Task::getStatus() const {
    return status_;
}

float Task::getProgress() const {
    return progress_;
}

const std::string& Task::getId() const {
    return id_;
}

std::string Task::getType() const {
    return type_;
}

std::map<std::string, std::string> Task::getParameters() const {
    return parameters_;
}

TaskDto Task::toDto() const {
    return TaskDto{id_, name_, type_, status_, progress_, parameters_};
}

} // namespace mxrc::core::taskmanager
