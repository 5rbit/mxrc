#pragma once

#include "core/sequence/core/SequenceEngine.h"
#include "core/sequence/core/SequenceRegistry.h"
#include "core/sequence/interfaces/IActionFactory.h"
#include "core/sequence/dto/SequenceDto.h"
#include "core/sequence/core/ExecutionContext.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <map>
#include <any>

namespace mxrc::core::taskmanager {

// Mock SequenceEngine for unit testing TaskManager integration
class MockSequenceEngine : public mxrc::core::sequence::SequenceEngine {
public:
    MockSequenceEngine(
        std::shared_ptr<mxrc::core::sequence::SequenceRegistry> registry,
        std::shared_ptr<mxrc::core::sequence::IActionFactory> actionFactory)
        : SequenceEngine(registry, actionFactory) {}

    MOCK_METHOD(std::string, execute, (const std::string& sequenceId, const std::map<std::string, std::any>& parameters), (override));
    MOCK_METHOD(bool, pause, (const std::string& executionId), (override));
    MOCK_METHOD(bool, resume, (const std::string& executionId), (override));
    MOCK_METHOD(bool, cancel, (const std::string& executionId), (override));
    MOCK_METHOD(mxrc::core::sequence::SequenceExecutionResult, getStatus, (const std::string& executionId), (const override));
    MOCK_METHOD(std::shared_ptr<mxrc::core::sequence::ExecutionContext>, getExecutionContext, (const std::string& executionId), (const override));
};

} // namespace mxrc::core::taskmanager
