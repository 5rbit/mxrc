#pragma once
#include "../interfaces/ITask.h" // Updated include
#include <iostream>
#include <map>
#include <string>

class LiftPalletTask : public ITask {
public:
    LiftPalletTask(const std::map<std::string, std::string>& params) : parameters_(params) {}

    void execute() override {
        std::cout << "LiftPalletTask::execute() called. Parameters: ";
        for (const auto& [key, value] : parameters_) {
            std::cout << key << "=" << value << " ";
        }
        std::cout << std::endl;
        // Simulate actual pallet lifting logic
    }

    void cancel() override {
        std::cout << "LiftPalletTask::cancel() called." << std::endl;
    }

    void pause() override {
        std::cout << "LiftPalletTask::pause() called." << std::endl;
    }

    std::string getType() const override {
        return "LiftPallet";
    }

    std::map<std::string, std::string> getParameters() const override {
        return parameters_;
    }

private:
    std::map<std::string, std::string> parameters_;
};
