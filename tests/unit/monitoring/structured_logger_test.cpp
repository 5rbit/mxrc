#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "core/monitoring/StructuredLogger.h"
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>

using namespace mxrc::monitoring;
using json = nlohmann::json;

namespace fs = std::filesystem;

class StructuredLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp directory for test logs
        test_dir_ = "/tmp/mxrc_structured_logger_test";
        fs::create_directories(test_dir_);

        test_log_file_ = test_dir_ + "/test.log";

        // Clean up any existing test files
        if (fs::exists(test_log_file_)) {
            fs::remove(test_log_file_);
        }
    }

    void TearDown() override {
        // Clean up test files
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }

        // Clear thread-local trace context
        clearThreadTraceContext();
    }

    std::string readLogFile() {
        std::ifstream file(test_log_file_);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::vector<std::string> readLogLines() {
        std::vector<std::string> lines;
        std::ifstream file(test_log_file_);
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
        return lines;
    }

    std::string test_dir_;
    std::string test_log_file_;
};

// ============================================================================
// T081: Unit test for JSON format validation
// ============================================================================

TEST_F(StructuredLoggerTest, StructuredLogEventToJson) {
    StructuredLogEvent event;
    event.timestamp = "2025-11-21T10:30:45.123Z";
    event.log_level = "info";
    event.log_logger = "test.logger";
    event.process_name = "mxrc_test";
    event.process_pid = 12345;
    event.thread_id = 67890;
    event.thread_name = "worker-1";
    event.message = "Test message";
    event.ecs_version = "8.11";

    std::string json_str = event.toJson();
    ASSERT_FALSE(json_str.empty());

    // Parse and validate JSON structure
    json j = json::parse(json_str);

    EXPECT_EQ(j["@timestamp"], "2025-11-21T10:30:45.123Z");
    EXPECT_EQ(j["log"]["level"], "info");
    EXPECT_EQ(j["log"]["logger"], "test.logger");
    EXPECT_EQ(j["process"]["name"], "mxrc_test");
    EXPECT_EQ(j["process"]["pid"], 12345);
    EXPECT_EQ(j["process"]["thread"]["id"], 67890);
    EXPECT_EQ(j["process"]["thread"]["name"], "worker-1");
    EXPECT_EQ(j["message"], "Test message");
    EXPECT_EQ(j["ecs"]["version"], "8.11");
}

TEST_F(StructuredLoggerTest, StructuredLogEventWithTracing) {
    StructuredLogEvent event;
    event.timestamp = "2025-11-21T10:30:45.123Z";
    event.log_level = "info";
    event.log_logger = "test.logger";
    event.process_name = "mxrc_test";
    event.process_pid = 12345;
    event.thread_id = 67890;
    event.message = "Test message with trace";
    event.ecs_version = "8.11";

    // Add trace context
    event.trace_id = "abcdef0123456789abcdef0123456789";  // 32 hex chars
    event.span_id = "0123456789abcdef";  // 16 hex chars

    std::string json_str = event.toJson();
    json j = json::parse(json_str);

    EXPECT_EQ(j["trace"]["id"], "abcdef0123456789abcdef0123456789");
    EXPECT_EQ(j["span"]["id"], "0123456789abcdef");
}

TEST_F(StructuredLoggerTest, StructuredLogEventWithMxrcFields) {
    StructuredLogEvent event;
    event.timestamp = "2025-11-21T10:30:45.123Z";
    event.log_level = "info";
    event.log_logger = "test.logger";
    event.process_name = "mxrc_test";
    event.process_pid = 12345;
    event.thread_id = 67890;
    event.message = "Test message with MXRC fields";
    event.ecs_version = "8.11";

    // Add MXRC custom fields
    event.mxrc_task_id = "task-001";
    event.mxrc_sequence_id = "seq-123";
    event.mxrc_action_id = "action-456";
    event.mxrc_cycle_time_us = 125.5;

    std::string json_str = event.toJson();
    json j = json::parse(json_str);

    EXPECT_EQ(j["mxrc"]["task_id"], "task-001");
    EXPECT_EQ(j["mxrc"]["sequence_id"], "seq-123");
    EXPECT_EQ(j["mxrc"]["action_id"], "action-456");
    EXPECT_DOUBLE_EQ(j["mxrc"]["cycle_time_us"], 125.5);
}

TEST_F(StructuredLoggerTest, StructuredLogEventWithCustomLabels) {
    StructuredLogEvent event;
    event.timestamp = "2025-11-21T10:30:45.123Z";
    event.log_level = "info";
    event.log_logger = "test.logger";
    event.process_name = "mxrc_test";
    event.process_pid = 12345;
    event.thread_id = 67890;
    event.message = "Test message with labels";
    event.ecs_version = "8.11";

    // Add custom labels
    event.labels["environment"] = "test";
    event.labels["component"] = "rt_executive";
    event.labels["version"] = "1.0.0";

    std::string json_str = event.toJson();
    json j = json::parse(json_str);

    EXPECT_EQ(j["labels"]["environment"], "test");
    EXPECT_EQ(j["labels"]["component"], "rt_executive");
    EXPECT_EQ(j["labels"]["version"], "1.0.0");
}

TEST_F(StructuredLoggerTest, StructuredLogEventValidation) {
    StructuredLogEvent event;

    // Invalid: missing required fields
    EXPECT_FALSE(event.isValid());

    // Add required fields
    event.timestamp = "2025-11-21T10:30:45.123Z";
    event.log_level = "info";
    event.log_logger = "test.logger";
    event.message = "Test message";
    event.ecs_version = "8.11";

    EXPECT_TRUE(event.isValid());

    // Invalid trace_id (wrong length)
    event.trace_id = "short";
    EXPECT_FALSE(event.isValid());

    // Valid trace_id
    event.trace_id = "abcdef0123456789abcdef0123456789";
    EXPECT_TRUE(event.isValid());

    // Invalid span_id (wrong length)
    event.span_id = "short";
    EXPECT_FALSE(event.isValid());

    // Valid span_id
    event.span_id = "0123456789abcdef";
    EXPECT_TRUE(event.isValid());
}

// ============================================================================
// Async logging test
// ============================================================================

TEST_F(StructuredLoggerTest, AsyncLogging) {
    auto logger = createStructuredLogger(
        "async_test",
        test_log_file_,
        10 * 1024 * 1024,  // 10MB
        3,
        true,  // async
        8192   // queue size
    );

    ASSERT_NE(logger, nullptr);

    // Log multiple messages
    for (int i = 0; i < 10; ++i) {
        logger->log(spdlog::level::info,
                   "Async log message " + std::to_string(i),
                   {{"iteration", std::to_string(i)}});
    }

    // Flush to ensure all messages are written
    logger->flush();

    // Give async logger time to write
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Read and verify log file
    auto lines = readLogLines();
    EXPECT_EQ(lines.size(), 10);

    // Verify first log entry
    if (!lines.empty()) {
        json j = json::parse(lines[0]);
        EXPECT_EQ(j["log"]["level"], "info");
        EXPECT_EQ(j["message"], "Async log message 0");
        EXPECT_EQ(j["labels"]["iteration"], "0");
    }
}

TEST_F(StructuredLoggerTest, AsyncLoggingOverrunOldest) {
    // Create logger with small queue to test overrun behavior
    auto logger = createStructuredLogger(
        "overrun_test",
        test_log_file_,
        10 * 1024 * 1024,
        3,
        true,
        32  // Small queue size
    );

    ASSERT_NE(logger, nullptr);

    // Log many messages to trigger overrun
    for (int i = 0; i < 100; ++i) {
        logger->log(spdlog::level::info,
                   "Overrun test message " + std::to_string(i));
    }

    logger->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Verify that some logs were written (older ones may be dropped)
    auto lines = readLogLines();
    EXPECT_GT(lines.size(), 0);
}

// ============================================================================
// Trace context injection test
// ============================================================================

TEST_F(StructuredLoggerTest, TraceContextInjection) {
    auto logger = createStructuredLogger(
        "trace_test",
        test_log_file_,
        10 * 1024 * 1024,
        3,
        false  // sync for deterministic testing
    );

    ASSERT_NE(logger, nullptr);

    // Set trace context
    setThreadTraceContext(
        "abcdef0123456789abcdef0123456789",  // trace_id
        "0123456789abcdef"                   // span_id
    );

    // Log message
    logger->log(spdlog::level::info, "Message with trace context");
    logger->flush();

    // Clear trace context
    clearThreadTraceContext();

    // Log another message (no trace context)
    logger->log(spdlog::level::info, "Message without trace context");
    logger->flush();

    // Read and verify
    auto lines = readLogLines();
    ASSERT_GE(lines.size(), 2);

    // First message should have trace context
    json j1 = json::parse(lines[0]);
    EXPECT_EQ(j1["trace"]["id"], "abcdef0123456789abcdef0123456789");
    EXPECT_EQ(j1["span"]["id"], "0123456789abcdef");

    // Second message should not have trace context
    json j2 = json::parse(lines[1]);
    EXPECT_FALSE(j2.contains("trace"));
    EXPECT_FALSE(j2.contains("span"));
}

TEST_F(StructuredLoggerTest, TraceContextThreadLocal) {
    auto logger = createStructuredLogger(
        "trace_thread_test",
        test_log_file_,
        10 * 1024 * 1024,
        3,
        false
    );

    ASSERT_NE(logger, nullptr);

    std::atomic<bool> thread1_done{false};
    std::atomic<bool> thread2_done{false};

    // Thread 1: Set trace context and log
    std::thread t1([&]() {
        setThreadTraceContext(
            "11111111111111111111111111111111",
            "1111111111111111"
        );

        logger->log(spdlog::level::info, "Thread 1 message");
        logger->flush();

        clearThreadTraceContext();
        thread1_done = true;
    });

    // Thread 2: Different trace context
    std::thread t2([&]() {
        setThreadTraceContext(
            "22222222222222222222222222222222",
            "2222222222222222"
        );

        logger->log(spdlog::level::info, "Thread 2 message");
        logger->flush();

        clearThreadTraceContext();
        thread2_done = true;
    });

    t1.join();
    t2.join();

    EXPECT_TRUE(thread1_done);
    EXPECT_TRUE(thread2_done);

    // Verify that trace contexts were isolated per thread
    auto lines = readLogLines();
    ASSERT_GE(lines.size(), 2);

    bool found_thread1 = false;
    bool found_thread2 = false;

    for (const auto& line : lines) {
        json j = json::parse(line);

        if (j["message"] == "Thread 1 message") {
            EXPECT_EQ(j["trace"]["id"], "11111111111111111111111111111111");
            found_thread1 = true;
        }

        if (j["message"] == "Thread 2 message") {
            EXPECT_EQ(j["trace"]["id"], "22222222222222222222222222222222");
            found_thread2 = true;
        }
    }

    EXPECT_TRUE(found_thread1);
    EXPECT_TRUE(found_thread2);
}

// ============================================================================
// Custom labels test
// ============================================================================

TEST_F(StructuredLoggerTest, CustomLabels) {
    auto logger = createStructuredLogger(
        "labels_test",
        test_log_file_,
        10 * 1024 * 1024,
        3,
        false
    );

    ASSERT_NE(logger, nullptr);

    // Log with custom labels via context parameter
    std::map<std::string, std::string> labels;
    labels["environment"] = "production";
    labels["datacenter"] = "us-west-2";
    labels["version"] = "2.1.0";

    logger->log(spdlog::level::info, "Message with labels", labels);
    logger->flush();

    // Read and verify
    auto lines = readLogLines();
    ASSERT_GE(lines.size(), 1);

    json j = json::parse(lines[0]);
    EXPECT_EQ(j["labels"]["environment"], "production");
    EXPECT_EQ(j["labels"]["datacenter"], "us-west-2");
    EXPECT_EQ(j["labels"]["version"], "2.1.0");
}

// ============================================================================
// MXRC fields test
// ============================================================================

TEST_F(StructuredLoggerTest, MxrcCustomFields) {
    auto logger = createStructuredLogger(
        "mxrc_test",
        test_log_file_,
        10 * 1024 * 1024,
        3,
        false
    );

    ASSERT_NE(logger, nullptr);

    // Create event with MXRC fields
    StructuredLogEvent event;
    event.timestamp = getIso8601Timestamp();
    event.log_level = "info";
    event.log_logger = "mxrc.rt";
    event.process_name = "mxrc";
    event.process_pid = getpid();
    event.thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    event.message = "RT cycle completed";
    event.ecs_version = "8.11";

    event.mxrc_task_id = "rt_task_001";
    event.mxrc_sequence_id = "control_seq_01";
    event.mxrc_action_id = "move_action_123";
    event.mxrc_cycle_time_us = 250.75;

    logger->log(event);
    logger->flush();

    // Read and verify
    auto lines = readLogLines();
    ASSERT_GE(lines.size(), 1);

    json j = json::parse(lines[0]);
    EXPECT_EQ(j["mxrc"]["task_id"], "rt_task_001");
    EXPECT_EQ(j["mxrc"]["sequence_id"], "control_seq_01");
    EXPECT_EQ(j["mxrc"]["action_id"], "move_action_123");
    EXPECT_DOUBLE_EQ(j["mxrc"]["cycle_time_us"], 250.75);
}

// ============================================================================
// Log level test
// ============================================================================

TEST_F(StructuredLoggerTest, LogLevels) {
    auto logger = createStructuredLogger(
        "level_test",
        test_log_file_,
        10 * 1024 * 1024,
        3,
        false
    );

    ASSERT_NE(logger, nullptr);

    // Set level to warn
    logger->setLevel(spdlog::level::warn);
    EXPECT_EQ(logger->getLevel(), spdlog::level::warn);

    // Info should not be logged
    logger->log(spdlog::level::info, "Info message");
    logger->flush();

    // Warn should be logged
    logger->log(spdlog::level::warn, "Warning message");
    logger->flush();

    // Error should be logged
    logger->log(spdlog::level::err, "Error message");
    logger->flush();

    // Read and verify
    auto lines = readLogLines();
    EXPECT_EQ(lines.size(), 2);  // Only warn and error

    json j1 = json::parse(lines[0]);
    EXPECT_EQ(j1["log"]["level"], "warn");
    EXPECT_EQ(j1["message"], "Warning message");

    json j2 = json::parse(lines[1]);
    EXPECT_EQ(j2["log"]["level"], "error");
    EXPECT_EQ(j2["message"], "Error message");
}

// ============================================================================
// Timestamp format test
// ============================================================================

TEST_F(StructuredLoggerTest, TimestampFormat) {
    std::string ts = getIso8601Timestamp();

    // Should be in format: YYYY-MM-DDTHH:MM:SS.sssZ
    EXPECT_GT(ts.length(), 20);
    EXPECT_EQ(ts[4], '-');
    EXPECT_EQ(ts[7], '-');
    EXPECT_EQ(ts[10], 'T');
    EXPECT_EQ(ts[13], ':');
    EXPECT_EQ(ts[16], ':');
    EXPECT_EQ(ts.back(), 'Z');
}

// ============================================================================
// ECS compliance test
// ============================================================================

TEST_F(StructuredLoggerTest, EcsCompliance) {
    auto logger = createStructuredLogger(
        "ecs_test",
        test_log_file_,
        10 * 1024 * 1024,
        3,
        false
    );

    ASSERT_NE(logger, nullptr);

    logger->log(spdlog::level::info, "ECS compliance test");
    logger->flush();

    auto lines = readLogLines();
    ASSERT_GE(lines.size(), 1);

    json j = json::parse(lines[0]);

    // Required ECS fields
    EXPECT_TRUE(j.contains("@timestamp"));
    EXPECT_TRUE(j.contains("log"));
    EXPECT_TRUE(j["log"].contains("level"));
    EXPECT_TRUE(j["log"].contains("logger"));
    EXPECT_TRUE(j.contains("process"));
    EXPECT_TRUE(j["process"].contains("name"));
    EXPECT_TRUE(j["process"].contains("pid"));
    EXPECT_TRUE(j["process"].contains("thread"));
    EXPECT_TRUE(j["process"]["thread"].contains("id"));
    EXPECT_TRUE(j.contains("message"));
    EXPECT_TRUE(j.contains("ecs"));
    EXPECT_TRUE(j["ecs"].contains("version"));

    // ECS version should be 8.11
    EXPECT_EQ(j["ecs"]["version"], "8.11");
}

// ============================================================================
// Integration test: Complete logging workflow
// ============================================================================

TEST_F(StructuredLoggerTest, DISABLED_CompleteWorkflow) {
    auto logger = createStructuredLogger(
        "workflow_test",
        test_log_file_,
        10 * 1024 * 1024,
        3,
        false  // sync for deterministic testing
    );

    ASSERT_NE(logger, nullptr);

    // Scenario: RT task execution with tracing
    setThreadTraceContext(
        "workflow00000000000000000000001",
        "workflowspan0001"
    );

    // Log task start
    logger->log(spdlog::level::info,
               "RT task started",
               {{"task_name", "motion_control"}});

    // Log task execution with MXRC fields
    StructuredLogEvent exec_event;
    exec_event.timestamp = getIso8601Timestamp();
    exec_event.log_level = "info";
    exec_event.log_logger = "mxrc.rt";
    exec_event.process_name = "mxrc";
    exec_event.process_pid = getpid();
    exec_event.thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    exec_event.message = "RT cycle executed successfully";
    exec_event.ecs_version = "8.11";
    exec_event.mxrc_task_id = "motion_001";
    exec_event.mxrc_cycle_time_us = 199.5;
    exec_event.labels["status"] = "success";

    auto ctx = getThreadTraceContext();
    exec_event.trace_id = ctx.trace_id;
    exec_event.span_id = ctx.span_id;

    logger->log(exec_event);

    // Log task completion
    logger->log(spdlog::level::info,
               "RT task completed",
               {{"duration_us", "199.5"}});

    logger->flush();

    clearThreadTraceContext();

    // Verify complete workflow
    auto lines = readLogLines();
    ASSERT_GE(lines.size(), 3);

    // All logs should have the same trace ID
    for (const auto& line : lines) {
        json j = json::parse(line);
        EXPECT_EQ(j["trace"]["id"], "workflow00000000000000000000001");
        EXPECT_EQ(j["span"]["id"], "workflowspan0001");
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
