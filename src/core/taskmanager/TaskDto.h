#pragma once

#include <string>
#include <map>

// Enum for Task status
enum class TaskStatus {
    PENDING,
    RUNNING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED
};

// Helper to convert TaskStatus to string
inline std::string taskStatusToString(TaskStatus status) {
    switch (status) {
        case TaskStatus::PENDING: return "PENDING";
        case TaskStatus::RUNNING: return "RUNNING";
        case TaskStatus::PAUSED: return "PAUSED";
        case TaskStatus::COMPLETED: return "COMPLETED";
        case TaskStatus::FAILED: return "FAILED";
        case TaskStatus::CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

// Simple DTO for Task information
struct TaskDto {
    std::string id;
    std::string name;
    std::string type;
    std::map<std::string, std::string> parameters;
    std::string status; // PENDING, RUNNING, COMPLETED, FAILED, CANCELLED
    int progress; // 0-100
    std::string created_at;
    std::string updated_at;

    // Default constructor
    TaskDto() : progress(0) {}

    // Constructor with id, name, status, and progress (simplified)
    TaskDto(std::string id, std::string name, TaskStatus taskStatus, float taskProgress)
        : id(std::move(id)), name(std::move(name)), type(""),
          parameters(), status(taskStatusToString(taskStatus)),
          progress(static_cast<int>(taskProgress * 100)), created_at(""), updated_at("") {}

    // Full constructor to initialize all members
    TaskDto(std::string id, std::string name, std::string type,
            std::map<std::string, std::string> parameters, std::string status,
            int progress, std::string created_at, std::string updated_at)
        : id(std::move(id)), name(std::move(name)), type(std::move(type)),
          parameters(std::move(parameters)), status(std::move(status)),
          progress(progress), created_at(std::move(created_at)),
          updated_at(std::move(updated_at)) {}

    // Constructor with TaskStatus enum
    TaskDto(std::string id, std::string name, std::string type, TaskStatus taskStatus,
            float taskProgress, std::map<std::string, std::string> parameters)
        : id(std::move(id)), name(std::move(name)), type(std::move(type)),
          parameters(std::move(parameters)), status(taskStatusToString(taskStatus)),
          progress(static_cast<int>(taskProgress * 100)), created_at(""), updated_at("") {}
};

