#pragma once

#include <string>
#include <map>

// Enum for Task status
enum class TaskStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELLED
};

// Helper to convert TaskStatus to string
inline std::string taskStatusToString(TaskStatus status) {
    switch (status) {
        case TaskStatus::PENDING: return "PENDING";
        case TaskStatus::RUNNING: return "RUNNING";
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
};

