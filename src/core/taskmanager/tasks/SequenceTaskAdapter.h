#pragma once

#include "core/taskmanager/interfaces/ITask.h"
#include "core/sequence/core/SequenceEngine.h"
#include "core/sequence/dto/SequenceDto.h"
#include <memory>
#include <string>
#include <map>
#include <any>

namespace mxrc::core::taskmanager {

/**
 * @brief Adapts a SequenceEngine sequence to the ITask interface.
 *
 * This class allows a sequence managed by the SequenceEngine to be executed
 * as a task within the TaskManager framework. It translates TaskManager
 * calls (execute, cancel, pause, getStatus, getProgress) into corresponding
 * SequenceEngine calls.
 */
class SequenceTaskAdapter : public ITask {
public:
    /**
     * @brief Constructs a SequenceTaskAdapter.
     * @param id Unique ID for this task instance.
     * @param name Name of the task.
     * @param sequenceId The ID of the sequence to be executed by the SequenceEngine.
     * @param parameters Parameters to be passed to the sequence.
     * @param sequenceEngine Shared pointer to the SequenceEngine instance.
     */
    SequenceTaskAdapter(
        const std::string& id,
        const std::string& name,
        const std::string& sequenceId,
        const std::map<std::string, std::string>& parameters,
        std::shared_ptr<mxrc::core::sequence::SequenceEngine> sequenceEngine);

    ~SequenceTaskAdapter() override = default;

    // ITask interface implementation
    void execute() override;
    void cancel() override;
    void pause() override;
    TaskStatus getStatus() const override;
    float getProgress() const override;
    const std::string& getId() const override;
    std::string getType() const override;
    std::map<std::string, std::string> getParameters() const override;
    TaskDto toDto() const override;

private:
    std::string id_;
    std::string name_;
    std::string sequenceId_;
    std::map<std::string, std::string> parameters_;
    std::shared_ptr<mxrc::core::sequence::SequenceEngine> sequenceEngine_;
    std::string executionId_; // The execution ID returned by SequenceEngine
    mutable TaskStatus currentTaskStatus_; // Current status of this task adapter
    mutable float currentProgress_; // Current progress of this task adapter

    /**
     * @brief Maps SequenceStatus to TaskStatus.
     * @param sequenceStatus The status from SequenceEngine.
     * @return The corresponding TaskStatus.
     */
    TaskStatus mapSequenceStatusToTaskStatus(mxrc::core::sequence::SequenceStatus sequenceStatus) const;

    /**
     * @brief Converts TaskManager parameters (string, string) to SequenceEngine parameters (string, any).
     * @param taskParams The parameters from TaskManager.
     * @return The converted parameters for SequenceEngine.
     */
    std::map<std::string, std::any> convertTaskParamsToSequenceParams(
        const std::map<std::string, std::string>& taskParams) const;
};

} // namespace mxrc::core::taskmanager
