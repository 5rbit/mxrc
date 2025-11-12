#pragma once
#include "../interfaces/ITask.h" // Updated include
#include <iostream>
#include <map>
#include <string>

class FailureTypeTask : public ITask {
public:
    FailureTypeTask(const std::map<std::string, std::string>& params) : parameters_(params) {}

    void execute() override {
        std::cout << "FailureTypeTask::execute() called. Parameters: ";
        for (const auto& [key, value] : parameters_) {
            std::cout << key << "=" << value << " ";
        }
        std::cout << std::endl;
        // Simulate a task that might fail
        std::cerr << "Simulating a task failure within FailureTypeTask." << std::endl;
    }

    void cancel() override {
        std::cout << "FailureTypeTask::cancel() called." << std::endl;
    }

    void pause() override {
        std::cout << "FailureTypeTask::pause() called." << std::endl;
    }

    std::string getType() const override {
        return "FailureType";
    }

    std::map<std::string, std::string> getParameters() const override {
        return parameters_;
    }

private:
    std::map<std::string, std::string> parameters_;
};