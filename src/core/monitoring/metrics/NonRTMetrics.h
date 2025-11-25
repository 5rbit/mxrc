#pragma once

#include <atomic>
#include <cstdint>

namespace mxrc::core::monitoring {

/**
 * @brief Non-real-time process metrics for Prometheus export
 *
 * Feature 019 - US5: Monitoring & Observability
 *
 * These metrics track the performance and health of the Non-RT process:
 * - EventBus queue depth and throughput
 * - Task execution statistics
 * - Sequence/Action completion rates
 * - System resource usage
 *
 * All metrics are thread-safe (atomic) for concurrent access from
 * multiple non-RT threads.
 *
 * Prometheus Naming Convention:
 * - mxrc_nonrt_eventbus_queue_depth (gauge)
 * - mxrc_nonrt_events_processed_total (counter)
 * - mxrc_nonrt_tasks_completed_total (counter)
 */
struct NonRTMetrics {
    // ========================================================================
    // EventBus Metrics
    // ========================================================================

    /**
     * @brief Current EventBus queue depth
     *
     * Prometheus: mxrc_nonrt_eventbus_queue_depth (gauge)
     */
    std::atomic<uint64_t> eventbus_queue_depth{0};

    /**
     * @brief EventBus peak queue depth
     *
     * Prometheus: mxrc_nonrt_eventbus_queue_depth_max (gauge)
     */
    std::atomic<uint64_t> eventbus_queue_depth_max{0};

    /**
     * @brief Total events published
     *
     * Prometheus: mxrc_nonrt_events_published_total (counter)
     */
    std::atomic<uint64_t> events_published_total{0};

    /**
     * @brief Total events processed (dispatched to subscribers)
     *
     * Prometheus: mxrc_nonrt_events_processed_total (counter)
     */
    std::atomic<uint64_t> events_processed_total{0};

    /**
     * @brief Events dropped due to backpressure
     *
     * Prometheus: mxrc_nonrt_events_dropped_total{priority="LOW|NORMAL|HIGH"} (counter)
     */
    std::atomic<uint64_t> events_dropped_total{0};

    /**
     * @brief Events expired due to TTL
     *
     * Prometheus: mxrc_nonrt_events_expired_total (counter)
     */
    std::atomic<uint64_t> events_expired_total{0};

    /**
     * @brief Events coalesced (merged)
     *
     * Prometheus: mxrc_nonrt_events_coalesced_total (counter)
     */
    std::atomic<uint64_t> events_coalesced_total{0};

    /**
     * @brief Average event processing latency (microseconds)
     *
     * Prometheus: mxrc_nonrt_event_latency_microseconds (gauge)
     */
    std::atomic<uint64_t> event_latency_avg_us{0};

    // ========================================================================
    // Task Execution Metrics
    // ========================================================================

    /**
     * @brief Total tasks started
     *
     * Prometheus: mxrc_nonrt_tasks_started_total (counter)
     */
    std::atomic<uint64_t> tasks_started_total{0};

    /**
     * @brief Total tasks completed successfully
     *
     * Prometheus: mxrc_nonrt_tasks_completed_total (counter)
     */
    std::atomic<uint64_t> tasks_completed_total{0};

    /**
     * @brief Total tasks failed
     *
     * Prometheus: mxrc_nonrt_tasks_failed_total (counter)
     */
    std::atomic<uint64_t> tasks_failed_total{0};

    /**
     * @brief Currently running tasks
     *
     * Prometheus: mxrc_nonrt_tasks_running (gauge)
     */
    std::atomic<uint32_t> tasks_running{0};

    /**
     * @brief Average task execution time (milliseconds)
     *
     * Prometheus: mxrc_nonrt_task_duration_milliseconds (gauge)
     */
    std::atomic<uint64_t> task_duration_avg_ms{0};

    // ========================================================================
    // Sequence & Action Metrics
    // ========================================================================

    /**
     * @brief Total sequences started
     *
     * Prometheus: mxrc_nonrt_sequences_started_total (counter)
     */
    std::atomic<uint64_t> sequences_started_total{0};

    /**
     * @brief Total sequences completed
     *
     * Prometheus: mxrc_nonrt_sequences_completed_total (counter)
     */
    std::atomic<uint64_t> sequences_completed_total{0};

    /**
     * @brief Total actions executed
     *
     * Prometheus: mxrc_nonrt_actions_executed_total (counter)
     */
    std::atomic<uint64_t> actions_executed_total{0};

    /**
     * @brief Total action retries
     *
     * Prometheus: mxrc_nonrt_action_retries_total (counter)
     */
    std::atomic<uint64_t> action_retries_total{0};

    // ========================================================================
    // DataStore Metrics (Non-RT access)
    // ========================================================================

    /**
     * @brief Total DataStore get operations
     *
     * Prometheus: mxrc_nonrt_datastore_gets_total (counter)
     */
    std::atomic<uint64_t> datastore_gets_total{0};

    /**
     * @brief Total DataStore set operations
     *
     * Prometheus: mxrc_nonrt_datastore_sets_total (counter)
     */
    std::atomic<uint64_t> datastore_sets_total{0};

    /**
     * @brief DataStore access latency (microseconds)
     *
     * Prometheus: mxrc_nonrt_datastore_latency_microseconds (gauge)
     */
    std::atomic<uint64_t> datastore_latency_avg_us{0};

    // ========================================================================
    // System Resource Metrics
    // ========================================================================

    /**
     * @brief CPU utilization percentage (0-100)
     *
     * Prometheus: mxrc_nonrt_cpu_utilization_percent (gauge)
     */
    std::atomic<double> cpu_utilization_percent{0.0};

    /**
     * @brief Memory usage in bytes
     *
     * Prometheus: mxrc_nonrt_memory_usage_bytes (gauge)
     */
    std::atomic<uint64_t> memory_usage_bytes{0};

    /**
     * @brief Thread pool active threads
     *
     * Prometheus: mxrc_nonrt_threads_active (gauge)
     */
    std::atomic<uint32_t> threads_active{0};

    // ========================================================================
    // Logging Metrics
    // ========================================================================

    /**
     * @brief Total log messages written
     *
     * Prometheus: mxrc_nonrt_logs_written_total{level="DEBUG|INFO|WARN|ERROR"} (counter)
     */
    std::atomic<uint64_t> logs_written_total{0};

    /**
     * @brief Log queue depth
     *
     * Prometheus: mxrc_nonrt_log_queue_depth (gauge)
     */
    std::atomic<uint64_t> log_queue_depth{0};

    /**
     * @brief Logs dropped due to queue overflow
     *
     * Prometheus: mxrc_nonrt_logs_dropped_total (counter)
     */
    std::atomic<uint64_t> logs_dropped_total{0};

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /**
     * @brief Update EventBus queue depth
     *
     * @param depth New queue depth
     */
    void updateQueueDepth(uint64_t depth) {
        eventbus_queue_depth.store(depth, std::memory_order_relaxed);

        // Update peak
        uint64_t current_max = eventbus_queue_depth_max.load(std::memory_order_relaxed);
        while (depth > current_max) {
            if (eventbus_queue_depth_max.compare_exchange_weak(
                    current_max, depth,
                    std::memory_order_relaxed, std::memory_order_relaxed)) {
                break;
            }
        }
    }

    /**
     * @brief Record task completion
     *
     * @param success Whether task succeeded
     * @param duration_ms Task execution time
     */
    void recordTaskCompletion(bool success, uint64_t duration_ms) {
        if (success) {
            tasks_completed_total.fetch_add(1, std::memory_order_relaxed);
        } else {
            tasks_failed_total.fetch_add(1, std::memory_order_relaxed);
        }

        // Update average duration (exponential moving average)
        uint64_t current_avg = task_duration_avg_ms.load(std::memory_order_relaxed);
        uint64_t new_avg = static_cast<uint64_t>(
            0.9 * current_avg + 0.1 * duration_ms);
        task_duration_avg_ms.store(new_avg, std::memory_order_relaxed);
    }

    /**
     * @brief Reset all metrics (for testing)
     */
    void reset() {
        eventbus_queue_depth = 0;
        eventbus_queue_depth_max = 0;
        events_published_total = 0;
        events_processed_total = 0;
        events_dropped_total = 0;
        events_expired_total = 0;
        events_coalesced_total = 0;
        event_latency_avg_us = 0;
        tasks_started_total = 0;
        tasks_completed_total = 0;
        tasks_failed_total = 0;
        tasks_running = 0;
        task_duration_avg_ms = 0;
        sequences_started_total = 0;
        sequences_completed_total = 0;
        actions_executed_total = 0;
        action_retries_total = 0;
        datastore_gets_total = 0;
        datastore_sets_total = 0;
        datastore_latency_avg_us = 0;
        cpu_utilization_percent = 0.0;
        memory_usage_bytes = 0;
        threads_active = 0;
        logs_written_total = 0;
        log_queue_depth = 0;
        logs_dropped_total = 0;
    }
};

} // namespace mxrc::core::monitoring
