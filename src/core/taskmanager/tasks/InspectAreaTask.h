#pragma once
#include "../interfaces/ITask.h" // Updated include
#include <iostream>
#include <map>
#include <string>

class InspectAreaTask : public ITask {
public:
    InspectAreaTask(const std::map<std::string, std::string>& params) : parameters_(params) {}

    void execute() override {
        
        std::cout << "InspectAreaTask::execute() called. Parameters: ";
        for (const auto& [key, value] : parameters_) {
            std::cout << key << "=" << value << " ";
        }
        std::cout << std::endl;
        // Simulate actual area inspection logic
    }

    void cancel() override {
        std::cout << "InspectAreaTask::cancel() called." << std::endl;
    }

    void pause() override {
        std::cout << "InspectAreaTask::pause() called." << std::endl;
    }

    std::string getType() const override {
        return "Inspection";
    }

    std::map<std::string, std::string> getParameters() const override {
        return parameters_;
    }

private:
    std::map<std::string, std::string> parameters_;
};
