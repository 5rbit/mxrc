#include "StructuredLogger.h"
#include <nlohmann/json.hpp>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <thread>
#include <unistd.h>
#include <pthread.h>

using json = nlohmann::json;

namespace mxrc {
namespace monitoring {

// ============================================================================
// Thread-local trace context storage
// ============================================================================

namespace {
    thread_local LogTraceContext g_thread_trace_context;
}

// ============================================================================
// Helper functions for thread-local trace context
// ============================================================================

LogTraceContext getThreadTraceContext() {
    return g_thread_trace_context;
}

void setThreadTraceContext(const std::string& trace_id, const std::string& span_id) {
    g_thread_trace_context.trace_id = trace_id;
    g_thread_trace_context.span_id = span_id;
}

void clearThreadTraceContext() {
    g_thread_trace_context.trace_id.clear();
    g_thread_trace_context.span_id.clear();
}

// ============================================================================
// StructuredLogEvent methods
// ============================================================================

std::string StructuredLogEvent::toJson() const {
    json j;

    // ECS standard fields
    j["@timestamp"] = timestamp;
    j["log"]["level"] = log_level;
    j["log"]["logger"] = log_logger;
    j["process"]["name"] = process_name;
    j["process"]["pid"] = process_pid;
    j["process"]["thread"]["id"] = thread_id;

    if (!thread_name.empty()) {
        j["process"]["thread"]["name"] = thread_name;
    }

    j["message"] = message;
    j["ecs"]["version"] = ecs_version;

    // Distributed tracing fields (optional)
    if (!trace_id.empty()) {
        j["trace"]["id"] = trace_id;
    }
    if (!span_id.empty()) {
        j["span"]["id"] = span_id;
    }

    // MXRC custom fields (optional)
    if (!mxrc_task_id.empty()) {
        j["mxrc"]["task_id"] = mxrc_task_id;
    }
    if (!mxrc_sequence_id.empty()) {
        j["mxrc"]["sequence_id"] = mxrc_sequence_id;
    }
    if (!mxrc_action_id.empty()) {
        j["mxrc"]["action_id"] = mxrc_action_id;
    }
    if (mxrc_cycle_time_us > 0) {
        j["mxrc"]["cycle_time_us"] = mxrc_cycle_time_us;
    }

    // Custom labels
    if (!labels.empty()) {
        j["labels"] = labels;
    }

    return j.dump();
}

bool StructuredLogEvent::isValid() const {
    // Required fields validation
    if (timestamp.empty()) return false;
    if (log_level.empty()) return false;
    if (log_logger.empty()) return false;
    if (message.empty()) return false;
    if (ecs_version.empty()) return false;

    // Validate trace_id format (32 hex chars) if present
    if (!trace_id.empty() && trace_id.length() != 32) return false;

    // Validate span_id format (16 hex chars) if present
    if (!span_id.empty() && span_id.length() != 16) return false;

    return true;
}

// ============================================================================
// EcsJsonFormatter - ECS-compliant JSON formatter
// ============================================================================

class EcsJsonFormatter : public ILogFormatter {
public:
    EcsJsonFormatter(const std::string& process_name)
        : process_name_(process_name)
        , process_pid_(getpid()) {
    }

    std::string format(const spdlog::details::log_msg& msg) override {
        StructuredLogEvent event;

        // Basic ECS fields
        event.timestamp = getIso8601Timestamp();
        event.log_level = levelToString(msg.level);
        event.log_logger = std::string(msg.logger_name.begin(), msg.logger_name.end());
        event.process_name = process_name_;
        event.process_pid = process_pid_;
        event.thread_id = msg.thread_id;
        event.thread_name = getThreadName();
        event.message = std::string(msg.payload.begin(), msg.payload.end());
        event.ecs_version = "8.11";

        // Inject trace context from thread-local storage
        auto trace_ctx = getThreadTraceContext();
        event.trace_id = trace_ctx.trace_id;
        event.span_id = trace_ctx.span_id;

        // Add custom labels
        {
            std::lock_guard<std::mutex> lock(labels_mutex_);
            event.labels = labels_;
        }

        return event.toJson();
    }

    std::string formatAsJson(const StructuredLogEvent& event) override {
        return event.toJson();
    }

    void setTraceContext(const std::string& trace_id, const std::string& span_id) override {
        setThreadTraceContext(trace_id, span_id);
    }

    void clearTraceContext() override {
        clearThreadTraceContext();
    }

    void addLabel(const std::string& key, const std::string& value) override {
        std::lock_guard<std::mutex> lock(labels_mutex_);
        labels_[key] = value;
    }

    void removeLabel(const std::string& key) override {
        std::lock_guard<std::mutex> lock(labels_mutex_);
        labels_.erase(key);
    }

private:
    std::string getThreadName() const {
        char name[16];
        if (pthread_getname_np(pthread_self(), name, sizeof(name)) == 0) {
            return std::string(name);
        }
        return "";
    }

    std::string process_name_;
    pid_t process_pid_;
    std::map<std::string, std::string> labels_;
    mutable std::mutex labels_mutex_;
};

// ============================================================================
// Custom spdlog formatter that wraps EcsJsonFormatter
// ============================================================================

class SpdlogEcsFormatter : public spdlog::formatter {
public:
    explicit SpdlogEcsFormatter(std::shared_ptr<EcsJsonFormatter> ecs_formatter)
        : ecs_formatter_(ecs_formatter) {
    }

    void format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) override {
        std::string formatted = ecs_formatter_->format(msg);
        dest.append(formatted.data(), formatted.data() + formatted.size());
    }

    std::unique_ptr<spdlog::formatter> clone() const override {
        return std::make_unique<SpdlogEcsFormatter>(ecs_formatter_);
    }

private:
    std::shared_ptr<EcsJsonFormatter> ecs_formatter_;
};

// ============================================================================
// StructuredLogger implementation
// ============================================================================

class StructuredLogger : public IStructuredLogger {
public:
    StructuredLogger(std::shared_ptr<spdlog::logger> logger,
                    std::shared_ptr<ILogFormatter> formatter)
        : logger_(logger)
        , formatter_(formatter) {
    }

    void log(const StructuredLogEvent& event) override {
        if (!event.isValid()) {
            // Skip invalid events silently
            return;
        }

        // Direct JSON output using spdlog
        std::string json_str = event.toJson();

        // Map string level to spdlog level
        spdlog::level::level_enum level = spdlog::level::info;
        if (event.log_level == "trace") level = spdlog::level::trace;
        else if (event.log_level == "debug") level = spdlog::level::debug;
        else if (event.log_level == "info") level = spdlog::level::info;
        else if (event.log_level == "warn") level = spdlog::level::warn;
        else if (event.log_level == "error") level = spdlog::level::err;
        else if (event.log_level == "critical") level = spdlog::level::critical;

        // Log using spdlog - formatter will output just the message
        logger_->log(level, json_str);
    }

    void log(spdlog::level::level_enum level,
            const std::string& message,
            const std::map<std::string, std::string>& context) override {
        StructuredLogEvent event;

        event.timestamp = getIso8601Timestamp();
        event.log_level = levelToString(level);
        event.log_logger = logger_->name();
        event.process_name = "mxrc";  // TODO: Make configurable
        event.process_pid = getpid();
        event.thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
        event.message = message;
        event.ecs_version = "8.11";

        // Inject trace context
        auto trace_ctx = getThreadTraceContext();
        event.trace_id = trace_ctx.trace_id;
        event.span_id = trace_ctx.span_id;

        // Add context as labels
        event.labels = context;

        log(event);
    }

    std::shared_ptr<spdlog::logger> getSpdlogLogger() override {
        return logger_;
    }

    void setFormatter(std::shared_ptr<ILogFormatter> formatter) override {
        formatter_ = formatter;
    }

    void flush() override {
        logger_->flush();
    }

    void setLevel(spdlog::level::level_enum level) override {
        logger_->set_level(level);
    }

    spdlog::level::level_enum getLevel() const override {
        return logger_->level();
    }

private:
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<ILogFormatter> formatter_;
};

// ============================================================================
// Factory function
// ============================================================================

std::shared_ptr<IStructuredLogger> createStructuredLogger(
    const std::string& logger_name,
    const std::string& log_file_path,
    size_t max_file_size,
    size_t max_files,
    bool async_logging,
    size_t async_queue_size) {

    try {
        // Drop existing logger with same name if it exists
        spdlog::drop(logger_name);

        // Initialize async logging thread pool if needed
        if (async_logging) {
            static std::once_flag init_flag;
            std::call_once(init_flag, []() {
                spdlog::init_thread_pool(8192, 1);  // queue size, thread count
            });
        }

        // Create sinks
        std::vector<spdlog::sink_ptr> sinks;

        // Rotating file sink
        if (!log_file_path.empty()) {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_file_path, max_file_size, max_files);
            sinks.push_back(file_sink);
        }

        // Console sink (optional, for debugging)
        // auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        // sinks.push_back(console_sink);

        // Create logger
        std::shared_ptr<spdlog::logger> spdlog_logger;

        if (async_logging) {
            // Async logger with ring buffer
            spdlog_logger = std::make_shared<spdlog::async_logger>(
                logger_name,
                sinks.begin(),
                sinks.end(),
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::overrun_oldest);
        } else {
            // Synchronous logger
            spdlog_logger = std::make_shared<spdlog::logger>(
                logger_name,
                sinks.begin(),
                sinks.end());
        }

        // Create ECS formatter
        auto ecs_formatter = std::make_shared<EcsJsonFormatter>("mxrc");

        // Set pattern formatter to output just the message (JSON is pre-formatted)
        // Pattern "%v" means "message only"
        spdlog_logger->set_pattern("%v");

        // Set default level
        spdlog_logger->set_level(spdlog::level::info);

        // Register logger with spdlog
        spdlog::register_logger(spdlog_logger);

        // Create and return structured logger
        return std::make_shared<StructuredLogger>(spdlog_logger, ecs_formatter);

    } catch (const spdlog::spdlog_ex& ex) {
        throw std::runtime_error(std::string("Failed to create structured logger: ") + ex.what());
    }
}

} // namespace monitoring
} // namespace mxrc
