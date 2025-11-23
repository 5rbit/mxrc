// RTMetrics.cpp
// Copyright (C) 2025 MXRC Project

#include "RTMetrics.h"

namespace mxrc::core::rt {

RTMetrics::RTMetrics(std::shared_ptr<monitoring::MetricsCollector> collector)
    : collector_(std::move(collector)) {

    // Minor cycle duration (0.1ms ~ 10ms buckets)
    cycle_duration_minor_ = collector_->getOrCreateHistogram(
        "rt_cycle_duration_seconds",
        {{"type", "minor"}},
        {0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.01},
        "RT minor cycle execution duration in seconds");

    // Major cycle duration (1ms ~ 1000ms buckets)
    cycle_duration_major_ = collector_->getOrCreateHistogram(
        "rt_cycle_duration_seconds",
        {{"type", "major"}},
        {0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0},
        "RT major cycle execution duration in seconds");

    // Cycle jitter (0.01ms ~ 10ms buckets)
    cycle_jitter_ = collector_->getOrCreateHistogram(
        "rt_cycle_jitter_seconds",
        {},
        {0.00001, 0.00005, 0.0001, 0.0005, 0.001, 0.005, 0.01},
        "RT cycle jitter (standard deviation) in seconds");

    // Deadline misses
    deadline_misses_ = collector_->getOrCreateCounter(
        "rt_deadline_misses_total",
        {},
        "Total number of RT deadline misses");

    // State Machine
    current_state_ = collector_->getOrCreateGauge(
        "rt_state",
        {},
        "Current RT state (0=INIT, 1=READY, 2=RUNNING, 3=SAFE_MODE, 4=SHUTDOWN)");

    state_transitions_ = collector_->getOrCreateCounter(
        "rt_state_transitions_total",
        {},
        "Total number of RT state transitions");

    safe_mode_entries_ = collector_->getOrCreateCounter(
        "rt_safe_mode_entries_total",
        {},
        "Total number of SAFE_MODE entries");

    // Heartbeat
    nonrt_heartbeat_alive_ = collector_->getOrCreateGauge(
        "rt_nonrt_heartbeat_alive",
        {},
        "Non-RT heartbeat status (0=lost, 1=alive)");

    nonrt_heartbeat_timeout_seconds_ = collector_->getOrCreateGauge(
        "rt_nonrt_heartbeat_timeout_seconds",
        {},
        "Non-RT heartbeat timeout in seconds");

    // Production readiness: NUMA metrics
    numa_local_pages_ = collector_->getOrCreateGauge(
        "rt_numa_local_pages",
        {},
        "Number of pages allocated on local NUMA node");

    numa_remote_pages_ = collector_->getOrCreateGauge(
        "rt_numa_remote_pages",
        {},
        "Number of pages allocated on remote NUMA nodes");

    numa_local_access_percent_ = collector_->getOrCreateGauge(
        "rt_numa_local_access_percent",
        {},
        "Percentage of memory accesses to local NUMA node");

    // Production readiness: Performance monitoring metrics
    perf_latency_ = collector_->getOrCreateHistogram(
        "rt_perf_latency_seconds",
        {},
        {0.00001, 0.00005, 0.0001, 0.0005, 0.001, 0.005, 0.01},
        "RT performance monitoring latency distribution in seconds");

    perf_p50_latency_ = collector_->getOrCreateGauge(
        "rt_perf_p50_latency_seconds",
        {},
        "RT performance P50 (median) latency in seconds");

    perf_p95_latency_ = collector_->getOrCreateGauge(
        "rt_perf_p95_latency_seconds",
        {},
        "RT performance P95 latency in seconds");

    perf_p99_latency_ = collector_->getOrCreateGauge(
        "rt_perf_p99_latency_seconds",
        {},
        "RT performance P99 latency in seconds");

    perf_jitter_ = collector_->getOrCreateGauge(
        "rt_perf_jitter_seconds",
        {},
        "RT performance jitter (standard deviation) in seconds");

    perf_deadline_misses_ = collector_->getOrCreateCounter(
        "rt_perf_deadline_misses_total",
        {},
        "Total number of performance deadline misses");

    perf_deadline_miss_rate_ = collector_->getOrCreateGauge(
        "rt_perf_deadline_miss_rate_percent",
        {},
        "Percentage of cycles that missed deadline");
}

void RTMetrics::recordMinorCycleDuration(double duration_seconds) {
    cycle_duration_minor_->observe(duration_seconds);
}

void RTMetrics::recordMajorCycleDuration(double duration_seconds) {
    cycle_duration_major_->observe(duration_seconds);
}

void RTMetrics::recordCycleJitter(double jitter_seconds) {
    cycle_jitter_->observe(jitter_seconds);
}

void RTMetrics::incrementDeadlineMisses() {
    deadline_misses_->increment();
}

void RTMetrics::updateState(RTState state) {
    current_state_->set(static_cast<double>(state));
}

void RTMetrics::incrementStateTransitions() {
    state_transitions_->increment();
}

void RTMetrics::incrementSafeModeEntries() {
    safe_mode_entries_->increment();
}

void RTMetrics::updateNonRTHeartbeatAlive(bool alive) {
    nonrt_heartbeat_alive_->set(alive ? 1.0 : 0.0);
}

void RTMetrics::updateNonRTHeartbeatTimeout(double timeout_seconds) {
    nonrt_heartbeat_timeout_seconds_->set(timeout_seconds);
}

void RTMetrics::incrementDataStoreWrites(const std::string& key) {
    collector_->incrementCounter("rt_datastore_writes_total", {{"key", key}});
}

void RTMetrics::incrementDataStoreReads(const std::string& key) {
    collector_->incrementCounter("rt_datastore_reads_total", {{"key", key}});
}

void RTMetrics::incrementDataStoreSeqlockRetries(const std::string& key) {
    collector_->incrementCounter("rt_datastore_seqlock_retries_total", {{"key", key}});
}

// Production readiness: NUMA monitoring methods

void RTMetrics::updateNUMAStats(uint64_t local_pages, uint64_t remote_pages, double local_access_percent) {
    numa_local_pages_->set(static_cast<double>(local_pages));
    numa_remote_pages_->set(static_cast<double>(remote_pages));
    numa_local_access_percent_->set(local_access_percent);
}

// Production readiness: Performance monitoring methods

void RTMetrics::recordPerfLatency(double latency_seconds) {
    perf_latency_->observe(latency_seconds);
}

void RTMetrics::updatePerfPercentiles(double p50_seconds, double p95_seconds, double p99_seconds) {
    perf_p50_latency_->set(p50_seconds);
    perf_p95_latency_->set(p95_seconds);
    perf_p99_latency_->set(p99_seconds);
}

void RTMetrics::updatePerfJitter(double jitter_seconds) {
    perf_jitter_->set(jitter_seconds);
}

void RTMetrics::incrementPerfDeadlineMisses() {
    perf_deadline_misses_->increment();
}

void RTMetrics::updatePerfDeadlineMissRate(double miss_rate_percent) {
    perf_deadline_miss_rate_->set(miss_rate_percent);
}

} // namespace mxrc::core::rt
