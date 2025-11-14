#pragma once

#include "core/sequence/interfaces/IActionFactory.h"
#include "core/sequence/interfaces/IAction.h"
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

namespace mxrc::core::sequence {

/**
 * @brief A dummy implementation of IActionFactory for testing and initial setup.
 *
 * This factory does not create any actual actions but serves to satisfy
 * the dependency of SequenceEngine.
 */
class DummyActionFactory : public IActionFactory {
public:
    std::shared_ptr<IAction> createAction(
        const std::string& type,
        const std::string& id,
        const std::map<std::string, std::string>& params) override {
        throw std::runtime_error("DummyActionFactory does not create real actions.");
    }

    std::vector<std::string> getSupportedTypes() const override {
        return {}; // No supported types
    }
};

} // namespace mxrc::core::sequence
