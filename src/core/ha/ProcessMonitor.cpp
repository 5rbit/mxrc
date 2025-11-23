// ProcessMonitor.cpp
// Copyright (C) 2025 MXRC Project

#include "ProcessMonitor.h"
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <fstream>
#include <sstream>

// systemd watchdog support
#ifdef __linux__
#  if __has_include(<systemd/sd-daemon.h>)
#    include <systemd/sd-daemon.h>
#    define HAS_SYSTEMD 1
#  else
#    define HAS_SYSTEMD 0
#  endif
#else
#  define HAS_SYSTEMD 0
#endif

namespace mxrc::ha {

ProcessMonitor::ProcessMonitor(const ProcessMonitorConfig& config,
                               std::shared_ptr<IFailoverManager> failover_manager)
    : config_(config)
    , failover_manager_(std::move(failover_manager))
    , consecutive_failures_(0) {

    // Initialize current status
    current_status_.process_name = config_.process_name;
    current_status_.pid = getpid();
    current_status_.status = HealthStatus::STOPPED;
    current_status_.last_heartbeat = std::chrono::system_clock::now();
    current_status_.response_time_ms = 0.0;
    current_status_.cpu_usage_percent = 0.0;
    current_status_.memory_usage_mb = 0;
    current_status_.deadline_miss_count = 0;
    current_status_.error_message = "";
    current_status_.restart_count = 0;

    last_health_check_ = std::chrono::system_clock::now();
}

ProcessMonitor::~ProcessMonitor() {
    stop();
}

bool ProcessMonitor::start() {
    if (running_) {
        spdlog::warn("ProcessMonitor already running for {}", config_.process_name);
        return false;
    }

    // Set up systemd watchdog if enabled
    if (config_.enable_systemd_watchdog) {
        if (!setupSystemdWatchdog()) {
            spdlog::warn("Failed to setup systemd watchdog, continuing without it");
        }
    }

    // Update status to STARTING
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_status_.status = HealthStatus::STARTING;
    }

    running_ = true;
    monitor_thread_ = std::thread(&ProcessMonitor::monitorLoop, this);

    spdlog::info("ProcessMonitor started for {} (PID: {})", config_.process_name, current_status_.pid);
    return true;
}

void ProcessMonitor::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Update status to STOPPING
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_status_.status = HealthStatus::STOPPING;
    }

    // Wait for monitor thread to finish
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }

    // Update status to STOPPED
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_status_.status = HealthStatus::STOPPED;
    }

    spdlog::info("ProcessMonitor stopped for {}", config_.process_name);
}

// T041: Periodic health check loop
void ProcessMonitor::monitorLoop() {
    spdlog::debug("ProcessMonitor loop started for {}", config_.process_name);

    while (running_) {
        auto loop_start = std::chrono::steady_clock::now();

        // T043: Update health status with current metrics
        updateHealthStatus();

        // Perform health check
        bool health_check_passed = performHealthCheck();

        if (health_check_passed) {
            consecutive_failures_ = 0;
        } else {
            consecutive_failures_++;
            spdlog::warn("Health check failed for {} (consecutive failures: {})",
                        config_.process_name, consecutive_failures_.load());

            // T044: Trigger failover if threshold exceeded
            if (failover_manager_) {
                const auto& policy = failover_manager_->getPolicy();
                if (consecutive_failures_ >= policy.failure_threshold) {
                    spdlog::error("Health check failure threshold exceeded for {}, triggering failover",
                                 config_.process_name);
                    failover_manager_->handleProcessFailure(config_.process_name);
                }
            }
        }

        // T042: Notify systemd watchdog
        if (config_.enable_systemd_watchdog && health_check_passed) {
            notifySystemdWatchdog();
        }

        // Sleep until next health check interval
        auto loop_end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(loop_end - loop_start);
        auto sleep_duration = std::chrono::milliseconds(config_.health_check_interval_ms) - elapsed;

        if (sleep_duration > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(sleep_duration);
        }
    }

    spdlog::debug("ProcessMonitor loop stopped for {}", config_.process_name);
}

// T043: Perform health check
bool ProcessMonitor::performHealthCheck() {
    auto check_start = std::chrono::steady_clock::now();

    // Simple liveness check: verify thread is responsive
    // In a real implementation, this could ping the RT executive or check shared memory
    bool is_alive = running_;

    auto check_end = std::chrono::steady_clock::now();
    auto response_time = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
        check_end - check_start);

    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_status_.response_time_ms = response_time.count();
    }

    // Check if response time exceeded timeout
    if (response_time.count() > config_.health_check_timeout_ms) {
        spdlog::warn("Health check timeout for {}: {:.2f}ms > {}ms",
                    config_.process_name, response_time.count(), config_.health_check_timeout_ms);
        return false;
    }

    last_health_check_ = std::chrono::system_clock::now();
    return is_alive;
}

// T043: Update health status based on current metrics
void ProcessMonitor::updateHealthStatus() {
    std::lock_guard<std::mutex> lock(status_mutex_);

    // Update metrics
    current_status_.cpu_usage_percent = current_cpu_usage_.load();
    current_status_.memory_usage_mb = current_memory_usage_.load();
    current_status_.deadline_miss_count = current_deadline_misses_.load();
    current_status_.last_heartbeat = std::chrono::system_clock::now();

    // Determine health status based on metrics
    current_status_.status = determineHealthStatus();
}

// T043: Determine health status based on metrics and thresholds
HealthStatus ProcessMonitor::determineHealthStatus() const {
    // If already in terminal states, don't change
    if (current_status_.status == HealthStatus::STARTING ||
        current_status_.status == HealthStatus::STOPPING ||
        current_status_.status == HealthStatus::STOPPED) {
        return current_status_.status;
    }

    // If there's an error message, mark as UNHEALTHY
    if (!current_status_.error_message.empty()) {
        return HealthStatus::UNHEALTHY;
    }

    // Check for degraded conditions
    bool cpu_high = current_status_.cpu_usage_percent > config_.cpu_threshold_percent;
    bool memory_high = current_status_.memory_usage_mb > config_.memory_threshold_mb;
    bool deadline_misses_high = current_status_.deadline_miss_count > config_.deadline_miss_threshold;

    if (cpu_high || memory_high || deadline_misses_high) {
        spdlog::debug("Process {} DEGRADED: CPU={:.1f}% (threshold={}%), Memory={}MB (threshold={}MB), Deadline misses={} (threshold={})",
                     config_.process_name,
                     current_status_.cpu_usage_percent, config_.cpu_threshold_percent,
                     current_status_.memory_usage_mb, config_.memory_threshold_mb,
                     current_status_.deadline_miss_count, config_.deadline_miss_threshold);
        return HealthStatus::DEGRADED;
    }

    return HealthStatus::HEALTHY;
}

// T042: systemd watchdog integration
void ProcessMonitor::notifySystemdWatchdog() {
#if HAS_SYSTEMD
    int ret = sd_notify(0, "WATCHDOG=1");
    if (ret < 0) {
        spdlog::warn("Failed to notify systemd watchdog: {}", strerror(-ret));
    } else {
        spdlog::trace("systemd watchdog notified for {}", config_.process_name);
    }
#else
    // systemd not available
    spdlog::trace("systemd watchdog not available on this platform");
#endif
}

// T042: Setup systemd watchdog
bool ProcessMonitor::setupSystemdWatchdog() {
#if HAS_SYSTEMD
    uint64_t usec = 0;
    int ret = sd_watchdog_enabled(0, &usec);

    if (ret < 0) {
        spdlog::error("Failed to check systemd watchdog: {}", strerror(-ret));
        return false;
    }

    if (ret == 0) {
        spdlog::info("systemd watchdog not enabled in service file");
        return false;
    }

    spdlog::info("systemd watchdog enabled: timeout={}us", usec);

    // Notify systemd that we're ready
    sd_notify(0, "READY=1");

    return true;
#else
    spdlog::warn("systemd not available on this platform");
    return false;
#endif
}

// System metrics collection (simplified implementations)
double ProcessMonitor::getCpuUsage() const {
    // TODO: Implement actual CPU usage calculation from /proc/stat
    // For now, return current tracked value
    return current_cpu_usage_.load();
}

uint64_t ProcessMonitor::getMemoryUsage() const {
    // Read memory usage from /proc/self/status
#ifdef __linux__
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.find("VmRSS:") == 0) {
            std::istringstream iss(line);
            std::string label;
            uint64_t value_kb;
            iss >> label >> value_kb;
            return value_kb / 1024;  // Convert KB to MB
        }
    }
#endif
    return current_memory_usage_.load();
}

// IHealthCheck interface implementation

ProcessHealthStatus ProcessMonitor::getHealthStatus() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return current_status_;
}

bool ProcessMonitor::isHealthy() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return current_status_.status == HealthStatus::HEALTHY;
}

bool ProcessMonitor::isReady() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    // Ready if HEALTHY or DEGRADED (can still accept requests)
    return current_status_.status == HealthStatus::HEALTHY ||
           current_status_.status == HealthStatus::DEGRADED;
}

bool ProcessMonitor::isAlive() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    // Alive if not STOPPED or STOPPING
    return current_status_.status != HealthStatus::STOPPED &&
           current_status_.status != HealthStatus::STOPPING;
}

void ProcessMonitor::updateStatus(double cpu_usage, uint64_t memory_usage, uint64_t deadline_miss_count) {
    current_cpu_usage_ = cpu_usage;
    current_memory_usage_ = memory_usage;
    current_deadline_misses_ = deadline_miss_count;
}

void ProcessMonitor::recordHeartbeat() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    current_status_.last_heartbeat = std::chrono::system_clock::now();

    // If we were STARTING, transition to HEALTHY on first heartbeat
    if (current_status_.status == HealthStatus::STARTING) {
        current_status_.status = HealthStatus::HEALTHY;
        spdlog::info("Process {} transitioned to HEALTHY", config_.process_name);
    }
}

void ProcessMonitor::setError(const std::string& error_message) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    current_status_.error_message = error_message;
    current_status_.status = HealthStatus::UNHEALTHY;
    spdlog::error("Process {} marked as UNHEALTHY: {}", config_.process_name, error_message);
}

} // namespace mxrc::ha
