#pragma once
#include "../interfaces/ITask.h" // Updated include
#include <iostream>
#include <map>
#include <string>

class DummyTask : public ITask {
public:
    DummyTask(const std::map<std::string, std::string>& params) : parameters_(params) {}

    void execute() override {
        std::cout << "DummyTask::execute() called. Parameters: ";
        for (const auto& [key, value] : parameters_) {
            std::cout << key << "=" << value << " ";
        }
        std::cout << std::endl;
    }

    void cancel() override {
        std::cout << "DummyTask::cancel() called." << std::endl;
    }

    void pause() override {
        std::cout << "DummyTask::pause() called." << std::endl;
    }

    std::string getType() const override {
        return "DummyTask";
    }

    std::map<std::string, std::string> getParameters() const override {
        return parameters_;
    }

private:
    std::map<std::string, std::string> parameters_;
};
