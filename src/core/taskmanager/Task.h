#pragma once

#include <string>
#include <map>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>
#include "TaskDto.h" // Updated include for TaskDto.h

// Helper to generate a UUID-like string
inline std::string generateUuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
        ss << dis(gen);
    }
    return ss.str();
}

// Helper to get current timestamp as string
inline std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
    return ss.str();
}

class Task {
public:
    Task(const std::string& name, const std::string& type, const std::map<std::string, std::string>& parameters);

    const std::string& getId() const { return id_; }
    const std::string& getName() const { return name_; }
    const std::string& getType() const { return type_; }
    const std::map<std::string, std::string>& getParameters() const { return parameters_; }
    TaskStatus getStatus() const { return status_; }
    int getProgress() const { return progress_; }
    const std::string& getCreatedAt() const { return created_at_; }
    const std::string& getUpdatedAt() const { return updated_at_; }

    void setStatus(TaskStatus status) { status_ = status; updated_at_ = getCurrentTimestamp(); }
    void setProgress(int progress) { progress_ = progress; updated_at_ = getCurrentTimestamp(); }
    void setParameters(const std::map<std::string, std::string>& parameters) { parameters_ = parameters; updated_at_ = getCurrentTimestamp(); }

private:
    std::string id_;
    std::string name_;
    std::string type_;
    std::map<std::string, std::string> parameters_;
    TaskStatus status_;
    int progress_;
    std::string created_at_;
    std::string updated_at_;
};
