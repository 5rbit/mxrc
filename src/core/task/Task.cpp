#include "Task.h"

namespace mxrc {
namespace core {
namespace task {

Task::Task(const std::string& name, const std::string& type, const std::map<std::string, std::string>& parameters)
    : id_(generateUuid()),
      name_(name),
      type_(type),
      parameters_(parameters),
      status_(TaskStatus::PENDING),
      progress_(0),
      created_at_(getCurrentTimestamp()),
      updated_at_(getCurrentTimestamp())
{
}

} // namespace task
} // namespace core
} // namespace mxrc
