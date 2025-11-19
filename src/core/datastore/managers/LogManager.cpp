#include "LogManager.h"
#include <sstream>
#include <iomanip>
#include <ctime>

namespace mxrc::core::datastore {

// ============================================================================
// AccessLogEntry 구현
// ============================================================================

std::string AccessLogEntry::toString() const {
    // 타임스탬프를 문자열로 변환
    auto time_t_val = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_val), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    ss << " [" << operation << "] key=" << key;

    if (!module_id.empty()) {
        ss << " module=" << module_id;
    }

    return ss.str();
}

// ============================================================================
// ErrorLogEntry 구현
// ============================================================================

std::string ErrorLogEntry::toString() const {
    // 타임스탬프를 문자열로 변환
    auto time_t_val = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_val), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    ss << " [ERROR:" << error_type << "] " << message;

    if (!context.empty()) {
        ss << " | " << context;
    }

    return ss.str();
}

// ============================================================================
// LogManager 구현
// ============================================================================

LogManager::LogManager(size_t max_access_logs, size_t max_error_logs)
    : max_access_logs_(max_access_logs),
      max_error_logs_(max_error_logs) {
    // deque는 reserve를 지원하지 않음 (연속된 메모리 구조가 아님)
}

void LogManager::logAccess(const std::string& operation,
                           const std::string& key,
                           const std::string& module_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 순환 버퍼: 최대 크기 초과 시 오래된 로그 제거
    if (access_logs_.size() >= max_access_logs_) {
        access_logs_.pop_front();
    }

    // 새 로그 추가
    AccessLogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.operation = operation;
    entry.key = key;
    entry.module_id = module_id;

    access_logs_.push_back(entry);
}

void LogManager::logError(const std::string& error_type,
                         const std::string& message,
                         const std::string& context) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 순환 버퍼: 최대 크기 초과 시 오래된 로그 제거
    if (error_logs_.size() >= max_error_logs_) {
        error_logs_.pop_front();
    }

    // 새 로그 추가
    ErrorLogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.error_type = error_type;
    entry.message = message;
    entry.context = context;

    error_logs_.push_back(entry);
}

std::vector<std::string> LogManager::getAccessLogs() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> result;
    result.reserve(access_logs_.size());

    for (const auto& entry : access_logs_) {
        result.push_back(entry.toString());
    }

    return result;
}

std::vector<std::string> LogManager::getErrorLogs() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> result;
    result.reserve(error_logs_.size());

    for (const auto& entry : error_logs_) {
        result.push_back(entry.toString());
    }

    return result;
}

void LogManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    access_logs_.clear();
    error_logs_.clear();
}

size_t LogManager::getAccessLogCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return access_logs_.size();
}

size_t LogManager::getErrorLogCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return error_logs_.size();
}

} // namespace mxrc::core::datastore
