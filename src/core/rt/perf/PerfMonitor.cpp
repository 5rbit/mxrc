#include "PerfMonitor.h"
#include "core/config/ConfigLoader.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace mxrc {
namespace rt {
namespace perf {

PerfMonitor::PerfMonitor()
    : cycle_start_()
    , cycle_end_()
    , sample_index_(0)
    , total_cycles_(0)
    , deadline_misses_(0)
    , total_execution_time_us_(0)
    , min_latency_(std::numeric_limits<double>::max())
    , max_latency_(0.0)
    , sum_latency_(0.0)
    , sum_squared_latency_(0.0)
    , last_missed_deadline_(false) {
}

bool PerfMonitor::configure(const PerfMonitorConfig& config) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    config_ = config;

    // Initialize circular buffer
    latency_samples_.resize(config_.sample_buffer_size, 0.0);
    sample_index_ = 0;

    // Initialize histogram if enabled
    if (config_.enable_histogram) {
        histogram_.resize(config_.histogram_buckets, 0);
    }

    spdlog::info("PerfMonitor configured: process={}, cycle={}us, deadline={}us, buffer={}",
                 config_.process_name, config_.cycle_time_us, config_.deadline_us,
                 config_.sample_buffer_size);

    return true;
}

bool PerfMonitor::loadConfig(const std::string& config_path) {
    config::ConfigLoader loader;
    if (!loader.loadFromFile(config_path)) {
        spdlog::error("Failed to load PerfMonitor config from: {}", config_path);
        return false;
    }

    try {
        const auto& json = loader.getJson();

        PerfMonitorConfig config;
        config.process_name = json.value("process_name", "");
        config.cycle_time_us = json.value("cycle_time_us", 1000);
        config.deadline_us = json.value("deadline_us", 1000);
        config.enable_histogram = json.value("enable_histogram", true);
        config.histogram_buckets = json.value("histogram_buckets", 100);
        config.sample_buffer_size = json.value("sample_buffer_size", 10000);
        config.enable_tracing = json.value("enable_tracing", false);

        return configure(config);
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse PerfMonitor config: {}", e.what());
        return false;
    }
}

void PerfMonitor::startCycle() {
    cycle_start_ = Clock::now();
}

void PerfMonitor::endCycle() {
    cycle_end_ = Clock::now();

    // Calculate latency
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        cycle_end_ - cycle_start_);
    double latency_us = static_cast<double>(duration.count());

    std::lock_guard<std::mutex> lock(stats_mutex_);

    // Record sample
    recordSample(latency_us);

    // Update counters
    total_cycles_++;
    total_execution_time_us_ += static_cast<uint64_t>(latency_us);

    // Check deadline
    last_missed_deadline_ = (latency_us > config_.deadline_us);
    if (last_missed_deadline_) {
        deadline_misses_++;

        if (config_.enable_tracing) {
            spdlog::warn("Deadline miss: latency={:.2f}us > deadline={}us (cycle #{})",
                        latency_us, config_.deadline_us, total_cycles_);
        }
    }

    // Update histogram
    if (config_.enable_histogram) {
        updateHistogram(latency_us);
    }

    // Log periodic statistics
    if (config_.enable_tracing && total_cycles_ % 10000 == 0) {
        auto stats = getStats();
        spdlog::info("PerfMonitor stats @{}: avg={:.2f}us, p99={:.2f}us, jitter={:.2f}us, miss_rate={:.4f}%",
                    total_cycles_, stats.avg_latency, stats.p99_latency,
                    stats.jitter, stats.deadline_miss_rate);
    }
}

PerfStats PerfMonitor::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    PerfStats stats;

    if (total_cycles_ == 0) {
        return stats;
    }

    stats.min_latency = min_latency_;
    stats.max_latency = max_latency_;
    stats.avg_latency = sum_latency_ / total_cycles_;

    stats.p50_latency = calculatePercentile(0.50);
    stats.p95_latency = calculatePercentile(0.95);
    stats.p99_latency = calculatePercentile(0.99);

    stats.jitter = calculateJitter();
    stats.max_jitter = max_latency_ - stats.avg_latency;

    stats.total_cycles = total_cycles_;
    stats.deadline_misses = deadline_misses_;
    stats.deadline_miss_rate = (total_cycles_ > 0)
        ? (deadline_misses_ * 100.0) / total_cycles_
        : 0.0;

    stats.total_execution_time_us = total_execution_time_us_;
    stats.avg_execution_time_us = (total_cycles_ > 0)
        ? static_cast<double>(total_execution_time_us_) / total_cycles_
        : 0.0;

    return stats;
}

void PerfMonitor::reset() {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    sample_index_ = 0;
    total_cycles_ = 0;
    deadline_misses_ = 0;
    total_execution_time_us_ = 0;

    min_latency_ = std::numeric_limits<double>::max();
    max_latency_ = 0.0;
    sum_latency_ = 0.0;
    sum_squared_latency_ = 0.0;

    last_missed_deadline_ = false;

    std::fill(latency_samples_.begin(), latency_samples_.end(), 0.0);

    if (config_.enable_histogram) {
        std::fill(histogram_.begin(), histogram_.end(), 0);
    }

    spdlog::info("PerfMonitor statistics reset");
}

bool PerfMonitor::didMissDeadline() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_missed_deadline_;
}

std::vector<uint64_t> PerfMonitor::getHistogram() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return histogram_;
}

void PerfMonitor::recordSample(double latency_us) {
    // Update min/max
    if (latency_us < min_latency_) {
        min_latency_ = latency_us;
    }
    if (latency_us > max_latency_) {
        max_latency_ = latency_us;
    }

    // Update sum for average
    sum_latency_ += latency_us;
    sum_squared_latency_ += (latency_us * latency_us);

    // Store in circular buffer for percentile calculation
    latency_samples_[sample_index_] = latency_us;
    sample_index_ = (sample_index_ + 1) % latency_samples_.size();
}

double PerfMonitor::calculatePercentile(double percentile) const {
    if (total_cycles_ == 0) {
        return 0.0;
    }

    // Get valid samples
    size_t num_samples = std::min(total_cycles_, static_cast<uint64_t>(latency_samples_.size()));
    std::vector<double> sorted_samples(latency_samples_.begin(),
                                       latency_samples_.begin() + num_samples);

    // Sort samples
    std::sort(sorted_samples.begin(), sorted_samples.end());

    // Calculate percentile index
    size_t index = static_cast<size_t>(percentile * (sorted_samples.size() - 1));

    return sorted_samples[index];
}

void PerfMonitor::updateHistogram(double latency_us) {
    // Calculate bucket based on latency
    // Bucket 0: 0-deadline/buckets
    // Bucket 1: deadline/buckets - 2*deadline/buckets
    // ...
    // Last bucket: overflow (> deadline)

    double bucket_width = static_cast<double>(config_.deadline_us) / (config_.histogram_buckets - 1);
    size_t bucket = static_cast<size_t>(latency_us / bucket_width);

    // Cap at last bucket for overflow
    if (bucket >= histogram_.size()) {
        bucket = histogram_.size() - 1;
    }

    histogram_[bucket]++;
}

double PerfMonitor::calculateJitter() const {
    if (total_cycles_ <= 1) {
        return 0.0;
    }

    // Calculate variance: E[X^2] - (E[X])^2
    double mean = sum_latency_ / total_cycles_;
    double mean_squared = sum_squared_latency_ / total_cycles_;
    double variance = mean_squared - (mean * mean);

    // Standard deviation (jitter)
    return std::sqrt(std::max(0.0, variance));
}

// CycleGuard implementation
CycleGuard::CycleGuard(PerfMonitor& monitor)
    : monitor_(monitor)
    , ended_(false) {
    monitor_.startCycle();
}

CycleGuard::~CycleGuard() {
    if (!ended_) {
        monitor_.endCycle();
    }
}

} // namespace perf
} // namespace rt
} // namespace mxrc
