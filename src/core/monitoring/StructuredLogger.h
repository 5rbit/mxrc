#pragma once

#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <spdlog/spdlog.h>

namespace mxrc {
namespace monitoring {

/**
 * @brief Structured log event following ECS (Elastic Common Schema)
 *
 * Represents a JSON log event with standardized fields for Elasticsearch.
 */
struct StructuredLogEvent {
    // ECS standard fields
    std::string timestamp;          // ISO 8601 format
    std::string log_level;          // trace, debug, info, warn, error, critical
    std::string log_logger;         // Logger name
    std::string process_name;       // Process name
    int process_pid;                // Process ID
    uint64_t thread_id;             // Thread ID
    std::string thread_name;        // Thread name (optional)
    std::string message;            // Log message
    std::string ecs_version;        // ECS version (e.g., "8.11")

    // Custom labels
    std::map<std::string, std::string> labels;

    // Distributed tracing fields (optional)
    std::string trace_id;           // 16-byte hex (32 chars)
    std::string span_id;            // 8-byte hex (16 chars)

    // MXRC custom fields
    std::string mxrc_task_id;       // Task ID
    std::string mxrc_sequence_id;   // Sequence ID
    std::string mxrc_action_id;     // Action ID
    double mxrc_cycle_time_us;      // RT cycle time in microseconds
};

/**
 * @brief Log formatter interface
 *
 * Interface for implementing custom log formatters.
 * Follows MXRC Constitution principle: Interface-based design (I-prefix).
 */
class ILogFormatter {
public:
    virtual ~ILogFormatter() = default;

    /**
     * @brief Format log record as string
     *
     * @param record spdlog log record
     * @return std::string Formatted log string
     */
    virtual std::string format(const spdlog::details::log_msg& record) = 0;

    /**
     * @brief Format log event as JSON string
     *
     * @param event Structured log event
     * @return std::string JSON formatted log
     */
    virtual std::string formatAsJson(const StructuredLogEvent& event) = 0;

    /**
     * @brief Set trace context for log correlation
     *
     * Called by tracing system to inject trace ID into logs.
     *
     * @param trace_id Trace ID (hex string)
     * @param span_id Span ID (hex string)
     */
    virtual void setTraceContext(const std::string& trace_id, const std::string& span_id) = 0;

    /**
     * @brief Clear trace context
     *
     * Called when span ends to stop injecting trace IDs.
     */
    virtual void clearTraceContext() = 0;

    /**
     * @brief Add custom label to all subsequent logs
     *
     * @param key Label key
     * @param value Label value
     */
    virtual void addLabel(const std::string& key, const std::string& value) = 0;

    /**
     * @brief Remove custom label
     *
     * @param key Label key to remove
     */
    virtual void removeLabel(const std::string& key) = 0;
};

/**
 * @brief Structured logger interface
 *
 * Extends spdlog with structured logging capabilities.
 */
class IStructuredLogger {
public:
    virtual ~IStructuredLogger() = default;

    /**
     * @brief Log structured event
     *
     * @param event Structured log event
     */
    virtual void log(const StructuredLogEvent& event) = 0;

    /**
     * @brief Log with automatic context injection
     *
     * @param level Log level
     * @param message Log message
     * @param context Optional context map
     */
    virtual void log(spdlog::level::level_enum level,
                    const std::string& message,
                    const std::map<std::string, std::string>& context = {}) = 0;

    /**
     * @brief Get underlying spdlog logger
     *
     * For compatibility with existing code.
     *
     * @return std::shared_ptr<spdlog::logger> spdlog logger instance
     */
    virtual std::shared_ptr<spdlog::logger> getSpdlogLogger() = 0;

    /**
     * @brief Set log formatter
     *
     * @param formatter Custom log formatter
     */
    virtual void setFormatter(std::shared_ptr<ILogFormatter> formatter) = 0;

    /**
     * @brief Flush all buffered logs
     *
     * Ensures logs are written to output before returning.
     */
    virtual void flush() = 0;

    /**
     * @brief Set log level
     *
     * @param level Minimum log level to output
     */
    virtual void setLevel(spdlog::level::level_enum level) = 0;

    /**
     * @brief Get current log level
     *
     * @return spdlog::level::level_enum Current log level
     */
    virtual spdlog::level::level_enum getLevel() const = 0;
};

/**
 * @brief Convert spdlog level to string
 */
inline std::string levelToString(spdlog::level::level_enum level) {
    switch (level) {
        case spdlog::level::trace: return "trace";
        case spdlog::level::debug: return "debug";
        case spdlog::level::info: return "info";
        case spdlog::level::warn: return "warn";
        case spdlog::level::err: return "error";
        case spdlog::level::critical: return "critical";
        default: return "unknown";
    }
}

/**
 * @brief Get current timestamp in ISO 8601 format
 */
inline std::string getIso8601Timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", std::gmtime(&time_t_now));

    return std::string(buffer) + "." + std::to_string(ms.count()) + "Z";
}

} // namespace monitoring
} // namespace mxrc
