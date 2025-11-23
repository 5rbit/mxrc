#pragma once

#include "HealthCheck.h"
#include "FailoverManager.h"
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>

namespace mxrc {
namespace ha {

/**
 * @brief Process monitor configuration
 *
 * Configuration for ProcessMonitor health checking behavior.
 */
struct ProcessMonitorConfig {
    std::string process_name;                   // Process name to monitor
    uint32_t health_check_interval_ms;          // Health check interval (ms)
    uint32_t health_check_timeout_ms;           // Health check timeout (ms)
    uint32_t cpu_threshold_percent;             // CPU usage threshold for DEGRADED
    uint64_t memory_threshold_mb;               // Memory usage threshold for DEGRADED
    uint32_t deadline_miss_threshold;           // Deadline miss count threshold
    bool enable_systemd_watchdog;               // Enable systemd watchdog integration

    // Default values
    ProcessMonitorConfig()
        : health_check_interval_ms(1000)
        , health_check_timeout_ms(500)
        , cpu_threshold_percent(90)
        , memory_threshold_mb(2048)
        , deadline_miss_threshold(100)
        , enable_systemd_watchdog(false) {}
};

/**
 * @brief Process monitoring class
 *
 * Production readiness: Monitors process health and triggers failover.
 * Implements IHealthCheck interface and integrates with systemd watchdog.
 *
 * T041-T044: Process monitoring implementation
 */
class ProcessMonitor : public IHealthCheck {
private:
    ProcessMonitorConfig config_;
    std::shared_ptr<IFailoverManager> failover_manager_;

    ProcessHealthStatus current_status_;
    mutable std::mutex status_mutex_;

    std::atomic<bool> running_{false};
    std::thread monitor_thread_;

    // Metrics tracking
    std::atomic<double> current_cpu_usage_{0.0};
    std::atomic<uint64_t> current_memory_usage_{0};
    std::atomic<uint64_t> current_deadline_misses_{0};

    // Health check tracking
    std::atomic<uint32_t> consecutive_failures_{0};
    std::chrono::system_clock::time_point last_health_check_;

    // Monitoring loop
    void monitorLoop();

    // Health check logic
    bool performHealthCheck();
    void updateHealthStatus();
    HealthStatus determineHealthStatus() const;

    // systemd watchdog integration
    void notifySystemdWatchdog();
    bool setupSystemdWatchdog();

    // System metrics collection
    double getCpuUsage() const;
    uint64_t getMemoryUsage() const;

public:
    /**
     * @brief ProcessMonitor constructor
     *
     * @param config Monitor configuration
     * @param failover_manager Optional failover manager for triggering restarts
     */
    explicit ProcessMonitor(const ProcessMonitorConfig& config,
                           std::shared_ptr<IFailoverManager> failover_manager = nullptr);

    /**
     * @brief Destructor
     */
    ~ProcessMonitor();

    // Prevent copying
    ProcessMonitor(const ProcessMonitor&) = delete;
    ProcessMonitor& operator=(const ProcessMonitor&) = delete;

    /**
     * @brief Start monitoring
     *
     * @return true if successfully started, false otherwise
     */
    bool start();

    /**
     * @brief Stop monitoring
     */
    void stop();

    /**
     * @brief Check if monitor is running
     *
     * @return true if running, false otherwise
     */
    bool isRunning() const { return running_; }

    // IHealthCheck interface implementation
    ProcessHealthStatus getHealthStatus() const override;
    bool isHealthy() const override;
    bool isReady() const override;
    bool isAlive() const override;
    void updateStatus(double cpu_usage, uint64_t memory_usage, uint64_t deadline_miss_count) override;
    void recordHeartbeat() override;
    void setError(const std::string& error_message) override;
};

} // namespace ha
} // namespace mxrc
