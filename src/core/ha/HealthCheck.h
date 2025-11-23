#pragma once

#include <string>
#include <chrono>
#include <memory>

namespace mxrc {
namespace ha {

/**
 * @brief Health status enumeration
 *
 * Represents the current health state of a process.
 */
enum class HealthStatus {
    HEALTHY,        // 정상 동작 중
    DEGRADED,       // 성능 저하 (deadline miss 증가, 응답 지연)
    UNHEALTHY,      // 비정상 (응답 없음, critical error)
    STARTING,       // 시작 중
    STOPPING,       // 종료 중
    STOPPED         // 종료됨
};

/**
 * @brief Process health status data
 *
 * Contains current health information about a process.
 * Used by FailoverManager to make failover decisions.
 */
struct ProcessHealthStatus {
    std::string process_name;
    pid_t pid;
    HealthStatus status;
    std::chrono::system_clock::time_point last_heartbeat;
    double response_time_ms;
    double cpu_usage_percent;
    uint64_t memory_usage_mb;
    uint64_t deadline_miss_count;
    std::string error_message;
    uint32_t restart_count;
};

/**
 * @brief Health check interface
 *
 * Interface for implementing health check providers.
 * Follows MXRC Constitution principle: Interface-based design (I-prefix).
 */
class IHealthCheck {
public:
    virtual ~IHealthCheck() = default;

    /**
     * @brief Get current process health status
     *
     * @return ProcessHealthStatus Current health information
     */
    virtual ProcessHealthStatus getHealthStatus() const = 0;

    /**
     * @brief Check if process is healthy
     *
     * @return true if status is HEALTHY, false otherwise
     */
    virtual bool isHealthy() const = 0;

    /**
     * @brief Check if process is ready to accept requests
     *
     * Used for Kubernetes readinessProbe.
     *
     * @return true if all subsystems initialized, false otherwise
     */
    virtual bool isReady() const = 0;

    /**
     * @brief Check if process is alive
     *
     * Used for Kubernetes livenessProbe.
     *
     * @return true if process is running, false otherwise
     */
    virtual bool isAlive() const = 0;

    /**
     * @brief Update health status with latest metrics
     *
     * Called periodically by ProcessMonitor.
     *
     * @param cpu_usage Current CPU usage percentage
     * @param memory_usage Current memory usage in MB
     * @param deadline_miss_count Total deadline misses
     */
    virtual void updateStatus(double cpu_usage, uint64_t memory_usage, uint64_t deadline_miss_count) = 0;

    /**
     * @brief Record heartbeat timestamp
     *
     * Called by systemd watchdog or custom monitor.
     */
    virtual void recordHeartbeat() = 0;

    /**
     * @brief Set error status with message
     *
     * @param error_message Description of the error
     */
    virtual void setError(const std::string& error_message) = 0;
};

/**
 * @brief Convert HealthStatus to string
 */
inline std::string healthStatusToString(HealthStatus status) {
    switch (status) {
        case HealthStatus::HEALTHY: return "HEALTHY";
        case HealthStatus::DEGRADED: return "DEGRADED";
        case HealthStatus::UNHEALTHY: return "UNHEALTHY";
        case HealthStatus::STARTING: return "STARTING";
        case HealthStatus::STOPPING: return "STOPPING";
        case HealthStatus::STOPPED: return "STOPPED";
        default: return "UNKNOWN";
    }
}

} // namespace ha
} // namespace mxrc
