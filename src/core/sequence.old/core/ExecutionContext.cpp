#include "ExecutionContext.h"

namespace mxrc::core::sequence {

void ExecutionContext::setActionResult(const std::string& actionId, const std::any& result) {
    actionResults_[actionId] = result;
}

std::any ExecutionContext::getActionResult(const std::string& actionId) const {
    auto it = actionResults_.find(actionId);
    if (it != actionResults_.end()) {
        return it->second;
    }
    return std::any();
}

bool ExecutionContext::hasActionResult(const std::string& actionId) const {
    return actionResults_.find(actionId) != actionResults_.end();
}

const std::map<std::string, std::any>& ExecutionContext::getAllResults() const {
    return actionResults_;
}

void ExecutionContext::clear() {
    actionResults_.clear();
    variables_.clear();
}

void ExecutionContext::setVariable(const std::string& key, const std::any& value) {
    variables_[key] = value;
}

std::any ExecutionContext::getVariable(const std::string& key) const {
    auto it = variables_.find(key);
    if (it != variables_.end()) {
        return it->second;
    }
    return std::any();
}

} // namespace mxrc::core::sequence

