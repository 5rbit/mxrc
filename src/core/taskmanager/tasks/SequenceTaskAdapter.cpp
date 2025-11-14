#include "SequenceTaskAdapter.h"
#include "core/sequence/dto/SequenceDto.h"
#include <stdexcept>

namespace mxrc::core::taskmanager {

SequenceTaskAdapter::SequenceTaskAdapter(
    const std::string& id,
    const std::string& name,
    const std::string& sequenceId,
    const std::map<std::string, std::string>& parameters,
    std::shared_ptr<mxrc::core::sequence::SequenceEngine> sequenceEngine)
    : id_(id),
      name_(name),
      sequenceId_(sequenceId),
      parameters_(parameters),
      sequenceEngine_(std::move(sequenceEngine)),
      executionId_(""),
      currentTaskStatus_(TaskStatus::PENDING),
      currentProgress_(0.0f) {}

void SequenceTaskAdapter::execute() {
    if (!sequenceEngine_) {
        currentTaskStatus_ = TaskStatus::FAILED;
        throw std::runtime_error("SequenceEngine is not initialized.");
    }

    try {
        currentTaskStatus_ = TaskStatus::RUNNING;
        std::map<std::string, std::any> sequenceParams = convertTaskParamsToSequenceParams(parameters_);
        executionId_ = sequenceEngine_->execute(sequenceId_, sequenceParams);
        // The actual status and progress will be updated by getStatus/getProgress calls
        // as the sequence runs in the SequenceEngine's internal threads.
    } catch (const std::exception& e) {
        currentTaskStatus_ = TaskStatus::FAILED;
        // Log the error or store it for toDto()
        throw; // Re-throw to indicate failure to TaskExecutor
    }
}

void SequenceTaskAdapter::cancel() {
    if (!sequenceEngine_ || executionId_.empty()) {
        return; // Nothing to cancel or engine not ready
    }
    sequenceEngine_->cancel(executionId_);
    currentTaskStatus_ = TaskStatus::CANCELLED;
}

void SequenceTaskAdapter::pause() {
    if (!sequenceEngine_ || executionId_.empty()) {
        return; // Nothing to pause or engine not ready
    }
    sequenceEngine_->pause(executionId_);
    currentTaskStatus_ = TaskStatus::PAUSED;
}

TaskStatus SequenceTaskAdapter::getStatus() const {
    if (executionId_.empty() || !sequenceEngine_) {
        return currentTaskStatus_; // Return initial status if not yet executed or engine not ready
    }

    try {
        mxrc::core::sequence::SequenceExecutionResult result = sequenceEngine_->getStatus(executionId_);
        currentProgress_ = result.progress;
        currentTaskStatus_ = mapSequenceStatusToTaskStatus(result.status);
    } catch (const std::exception& e) {
        // If we can't get status from engine, assume failed or keep last known status
        currentTaskStatus_ = TaskStatus::FAILED;
        currentProgress_ = 0.0f;
    }
    return currentTaskStatus_;
}

float SequenceTaskAdapter::getProgress() const {
    // Call getStatus to ensure currentProgress_ is updated
    getStatus();
    return currentProgress_;
}

const std::string& SequenceTaskAdapter::getId() const {
    return id_;
}

std::string SequenceTaskAdapter::getType() const {
    return "SequenceTask";
}

std::map<std::string, std::string> SequenceTaskAdapter::getParameters() const {
    return parameters_;
}

TaskDto SequenceTaskAdapter::toDto() const {
    // Ensure status and progress are up-to-date before creating DTO
    getStatus();
    return TaskDto(id_, name_, getType(), currentTaskStatus_, currentProgress_, parameters_);
}

TaskStatus SequenceTaskAdapter::mapSequenceStatusToTaskStatus(
    mxrc::core::sequence::SequenceStatus sequenceStatus) const {
    switch (sequenceStatus) {
        case mxrc::core::sequence::SequenceStatus::PENDING:
            return TaskStatus::PENDING;
        case mxrc::core::sequence::SequenceStatus::RUNNING:
            return TaskStatus::RUNNING;
        case mxrc::core::sequence::SequenceStatus::PAUSED:
            return TaskStatus::PAUSED;
        case mxrc::core::sequence::SequenceStatus::COMPLETED:
            return TaskStatus::COMPLETED;
        case mxrc::core::sequence::SequenceStatus::FAILED:
            return TaskStatus::FAILED;
        case mxrc::core::sequence::SequenceStatus::CANCELLED:
            return TaskStatus::CANCELLED;
        default:
            return TaskStatus::FAILED; // Unknown status
    }
}

std::map<std::string, std::any> SequenceTaskAdapter::convertTaskParamsToSequenceParams(
    const std::map<std::string, std::string>& taskParams) const {
    std::map<std::string, std::any> sequenceParams;
    for (const auto& pair : taskParams) {
        // For now, assume all parameters are strings.
        // In a more advanced scenario, we might need to parse types (e.g., "123" -> int, "true" -> bool)
        sequenceParams[pair.first] = pair.second;
    }
    return sequenceParams;
}

} // namespace mxrc::core::taskmanager
