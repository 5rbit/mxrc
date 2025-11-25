#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace mxrc::core::monitoring {

/**
 * @brief Real-time process metrics for Prometheus export
 *
 * Feature 019 - US5: Monitoring & Observability
 *
 * These metrics track the performance and health of the RT process:
 * - Cycle timing (WCET, average, jitter)
 * - Deadline misses
 * - Fieldbus communication statistics
 * - CPU utilization
 *
 * All metrics are thread-safe (atomic) and suitable for lock-free access
 * from the RT thread.
 *
 * Prometheus Naming Convention:
 * - mxrc_rt_cycle_time_microseconds{quantile="p50|p99|max"}
 * - mxrc_rt_deadline_misses_total
 * - mxrc_rt_fieldbus_errors_total
 * - mxrc_rt_cpu_utilization_percent
 */
struct RTMetrics {
    // ========================================================================
    // Cycle Timing Metrics
    // ========================================================================

    /**
     * @brief Current cycle time in microseconds
     *
     * Prometheus: mxrc_rt_cycle_time_microseconds (gauge)
     */
    std::atomic<uint64_t> cycle_time_us{0};

    /**
     * @brief Minimum cycle time observed (microseconds)
     *
     * Prometheus: mxrc_rt_cycle_time_min_microseconds (gauge)
     */
    std::atomic<uint64_t> cycle_time_min_us{UINT64_MAX};

    /**
     * @brief Maximum cycle time observed (microseconds)
     *
     * Prometheus: mxrc_rt_cycle_time_max_microseconds (gauge)
     */
    std::atomic<uint64_t> cycle_time_max_us{0};

    /**
     * @brief Average cycle time (exponential moving average)
     *
     * Prometheus: mxrc_rt_cycle_time_avg_microseconds (gauge)
     */
    std::atomic<uint64_t> cycle_time_avg_us{0};

    /**
     * @brief Cycle time jitter (standard deviation)
     *
     * Prometheus: mxrc_rt_cycle_jitter_microseconds (gauge)
     */
    std::atomic<uint64_t> cycle_jitter_us{0};

    // ========================================================================
    // Deadline & Error Metrics
    // ========================================================================

    /**
     * @brief Total number of deadline misses
     *
     * Prometheus: mxrc_rt_deadline_misses_total (counter)
     */
    std::atomic<uint64_t> deadline_misses_total{0};

    /**
     * @brief Consecutive deadline misses (for HA trigger)
     *
     * Prometheus: mxrc_rt_deadline_misses_consecutive (gauge)
     */
    std::atomic<uint32_t> deadline_misses_consecutive{0};

    /**
     * @brief Total cycles executed
     *
     * Prometheus: mxrc_rt_cycles_total (counter)
     */
    std::atomic<uint64_t> cycles_total{0};

    // ========================================================================
    // Fieldbus Metrics
    // ========================================================================

    /**
     * @brief Total fieldbus communication errors
     *
     * Prometheus: mxrc_rt_fieldbus_errors_total (counter)
     */
    std::atomic<uint64_t> fieldbus_errors_total{0};

    /**
     * @brief Fieldbus working counter errors (EtherCAT specific)
     *
     * Prometheus: mxrc_rt_fieldbus_wkc_errors_total (counter)
     */
    std::atomic<uint64_t> fieldbus_wkc_errors_total{0};

    /**
     * @brief Total bytes sent over fieldbus
     *
     * Prometheus: mxrc_rt_fieldbus_bytes_sent_total (counter)
     */
    std::atomic<uint64_t> fieldbus_bytes_sent{0};

    /**
     * @brief Total bytes received over fieldbus
     *
     * Prometheus: mxrc_rt_fieldbus_bytes_received_total (counter)
     */
    std::atomic<uint64_t> fieldbus_bytes_received{0};

    // ========================================================================
    // CPU & Resource Metrics
    // ========================================================================

    /**
     * @brief CPU utilization percentage (0-100)
     *
     * Prometheus: mxrc_rt_cpu_utilization_percent (gauge)
     */
    std::atomic<double> cpu_utilization_percent{0.0};

    /**
     * @brief Memory usage in bytes
     *
     * Prometheus: mxrc_rt_memory_usage_bytes (gauge)
     */
    std::atomic<uint64_t> memory_usage_bytes{0};

    /**
     * @brief RT thread priority
     *
     * Prometheus: mxrc_rt_thread_priority (gauge)
     */
    std::atomic<int32_t> thread_priority{0};

    // ========================================================================
    // DataStore Metrics
    // ========================================================================

    /**
     * @brief Total DataStore get operations
     *
     * Prometheus: mxrc_rt_datastore_gets_total (counter)
     */
    std::atomic<uint64_t> datastore_gets_total{0};

    /**
     * @brief Total DataStore set operations
     *
     * Prometheus: mxrc_rt_datastore_sets_total (counter)
     */
    std::atomic<uint64_t> datastore_sets_total{0};

    /**
     * @brief DataStore cache hit rate (percentage)
     *
     * Prometheus: mxrc_rt_datastore_hit_rate_percent (gauge)
     */
    std::atomic<double> datastore_hit_rate_percent{0.0};

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /**
     * @brief Update cycle time statistics
     *
     * @param cycle_time_us New cycle time measurement
     */
    void updateCycleTime(uint64_t new_cycle_time_us) {
        cycle_time_us.store(new_cycle_time_us, std::memory_order_relaxed);

        // Update min
        uint64_t current_min = cycle_time_min_us.load(std::memory_order_relaxed);
        while (new_cycle_time_us < current_min) {
            if (cycle_time_min_us.compare_exchange_weak(
                    current_min, new_cycle_time_us,
                    std::memory_order_relaxed, std::memory_order_relaxed)) {
                break;
            }
        }

        // Update max
        uint64_t current_max = cycle_time_max_us.load(std::memory_order_relaxed);
        while (new_cycle_time_us > current_max) {
            if (cycle_time_max_us.compare_exchange_weak(
                    current_max, new_cycle_time_us,
                    std::memory_order_relaxed, std::memory_order_relaxed)) {
                break;
            }
        }

        // Update exponential moving average (alpha = 0.1)
        uint64_t current_avg = cycle_time_avg_us.load(std::memory_order_relaxed);
        uint64_t new_avg = static_cast<uint64_t>(
            0.9 * current_avg + 0.1 * new_cycle_time_us);
        cycle_time_avg_us.store(new_avg, std::memory_order_relaxed);
    }

    /**
     * @brief Increment deadline miss counter
     */
    void recordDeadlineMiss() {
        deadline_misses_total.fetch_add(1, std::memory_order_relaxed);
        deadline_misses_consecutive.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Reset consecutive deadline miss counter (on successful cycle)
     */
    void resetConsecutiveMisses() {
        deadline_misses_consecutive.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Reset all metrics (for testing)
     */
    void reset() {
        cycle_time_us = 0;
        cycle_time_min_us = UINT64_MAX;
        cycle_time_max_us = 0;
        cycle_time_avg_us = 0;
        cycle_jitter_us = 0;
        deadline_misses_total = 0;
        deadline_misses_consecutive = 0;
        cycles_total = 0;
        fieldbus_errors_total = 0;
        fieldbus_wkc_errors_total = 0;
        fieldbus_bytes_sent = 0;
        fieldbus_bytes_received = 0;
        cpu_utilization_percent = 0.0;
        memory_usage_bytes = 0;
        thread_priority = 0;
        datastore_gets_total = 0;
        datastore_sets_total = 0;
        datastore_hit_rate_percent = 0.0;
    }
};

} // namespace mxrc::core::monitoring
