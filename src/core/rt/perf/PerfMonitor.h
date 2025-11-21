#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <cstdint>
#include <mutex>

namespace mxrc {
namespace rt {
namespace perf {

/**
 * @brief Performance statistics
 *
 * Production readiness: Metrics for RT performance monitoring.
 */
struct PerfStats {
    // Latency metrics (in microseconds)
    double min_latency;
    double max_latency;
    double avg_latency;
    double p50_latency;
    double p95_latency;
    double p99_latency;

    // Jitter metrics (in microseconds)
    double jitter;              // Standard deviation of latency
    double max_jitter;          // Maximum deviation from expected

    // Deadline tracking
    uint64_t total_cycles;      // Total execution cycles
    uint64_t deadline_misses;   // Number of deadline violations
    double deadline_miss_rate;  // Percentage of deadline misses

    // Execution time statistics
    uint64_t total_execution_time_us;  // Total execution time
    double avg_execution_time_us;      // Average per cycle

    PerfStats()
        : min_latency(0.0)
        , max_latency(0.0)
        , avg_latency(0.0)
        , p50_latency(0.0)
        , p95_latency(0.0)
        , p99_latency(0.0)
        , jitter(0.0)
        , max_jitter(0.0)
        , total_cycles(0)
        , deadline_misses(0)
        , deadline_miss_rate(0.0)
        , total_execution_time_us(0)
        , avg_execution_time_us(0.0) {}
};

/**
 * @brief Performance Monitor Configuration
 *
 * Production readiness: Configuration for RT performance monitoring.
 */
struct PerfMonitorConfig {
    std::string process_name;       // Process name for identification
    uint64_t cycle_time_us;         // Expected cycle time in microseconds
    uint64_t deadline_us;           // Deadline for each cycle
    bool enable_histogram;          // Enable latency histogram collection
    uint32_t histogram_buckets;     // Number of histogram buckets
    uint32_t sample_buffer_size;    // Number of samples to keep for statistics
    bool enable_tracing;            // Enable detailed tracing

    PerfMonitorConfig()
        : process_name("")
        , cycle_time_us(1000)       // Default 1ms cycle
        , deadline_us(1000)         // Default 1ms deadline
        , enable_histogram(true)
        , histogram_buckets(100)
        , sample_buffer_size(10000)
        , enable_tracing(false) {}
};

/**
 * @brief Performance Monitor
 *
 * Production readiness: Monitors RT performance metrics including jitter,
 * deadline misses, and execution time statistics.
 *
 * Usage:
 *   PerfMonitor monitor;
 *   monitor.configure(config);
 *
 *   while (running) {
 *       monitor.startCycle();
 *       // ... do work ...
 *       monitor.endCycle();
 *   }
 *
 *   auto stats = monitor.getStats();
 */
class PerfMonitor {
public:
    PerfMonitor();
    ~PerfMonitor() = default;

    // Non-copyable, non-movable
    PerfMonitor(const PerfMonitor&) = delete;
    PerfMonitor& operator=(const PerfMonitor&) = delete;

    /**
     * @brief Configure performance monitor
     *
     * @param config Performance monitor configuration
     * @return true if successfully configured
     */
    bool configure(const PerfMonitorConfig& config);

    /**
     * @brief Load configuration from JSON file
     *
     * @param config_path Path to perf_monitor.json
     * @return true if successfully loaded
     */
    bool loadConfig(const std::string& config_path);

    /**
     * @brief Mark the start of a cycle
     *
     * Records timestamp for cycle start.
     */
    void startCycle();

    /**
     * @brief Mark the end of a cycle
     *
     * Records timestamp, calculates latency, checks deadline.
     */
    void endCycle();

    /**
     * @brief Get current performance statistics
     *
     * @return PerfStats Current statistics
     */
    PerfStats getStats() const;

    /**
     * @brief Reset all statistics
     *
     * Clears all collected data and resets counters.
     */
    void reset();

    /**
     * @brief Check if last cycle missed deadline
     *
     * @return true if last cycle exceeded deadline
     */
    bool didMissDeadline() const;

    /**
     * @brief Get latency histogram
     *
     * Returns histogram of latency distribution.
     *
     * @return std::vector<uint64_t> Histogram buckets
     */
    std::vector<uint64_t> getHistogram() const;

private:
    PerfMonitorConfig config_;

    // Timing
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint cycle_start_;
    TimePoint cycle_end_;

    // Statistics tracking
    mutable std::mutex stats_mutex_;
    std::vector<double> latency_samples_;  // Circular buffer of recent samples
    size_t sample_index_;                   // Current position in circular buffer

    uint64_t total_cycles_;
    uint64_t deadline_misses_;
    uint64_t total_execution_time_us_;

    double min_latency_;
    double max_latency_;
    double sum_latency_;
    double sum_squared_latency_;  // For jitter calculation

    bool last_missed_deadline_;

    // Histogram
    std::vector<uint64_t> histogram_;

    /**
     * @brief Record a latency sample
     *
     * @param latency_us Latency in microseconds
     */
    void recordSample(double latency_us);

    /**
     * @brief Calculate percentile from samples
     *
     * @param percentile Percentile value (0.0-1.0)
     * @return double Percentile value in microseconds
     */
    double calculatePercentile(double percentile) const;

    /**
     * @brief Update histogram with latency sample
     *
     * @param latency_us Latency in microseconds
     */
    void updateHistogram(double latency_us);

    /**
     * @brief Calculate standard deviation (jitter)
     *
     * @return double Standard deviation in microseconds
     */
    double calculateJitter() const;
};

/**
 * @brief RAII guard for cycle timing
 *
 * Production readiness: Automatic cycle start/end tracking.
 * Ensures cycles are properly measured even if exceptions occur.
 */
class CycleGuard {
public:
    /**
     * @brief Start cycle timing
     *
     * @param monitor Performance monitor
     */
    explicit CycleGuard(PerfMonitor& monitor);

    /**
     * @brief End cycle timing
     */
    ~CycleGuard();

    // Non-copyable, movable
    CycleGuard(const CycleGuard&) = delete;
    CycleGuard& operator=(const CycleGuard&) = delete;
    CycleGuard(CycleGuard&&) = default;
    CycleGuard& operator=(CycleGuard&&) = default;

private:
    PerfMonitor& monitor_;
    bool ended_;
};

} // namespace perf
} // namespace rt
} // namespace mxrc
