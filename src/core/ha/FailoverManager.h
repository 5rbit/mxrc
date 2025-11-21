#pragma once

#include <string>
#include <cstdint>

namespace mxrc {
namespace ha {

/**
 * @brief Failover policy configuration
 *
 * Production readiness: Defines failover behavior policy.
 * Used by ProcessMonitor and FailoverManager for process monitoring and recovery.
 *
 * Based on data-model.md: FailoverPolicy
 */
struct FailoverPolicy {
    std::string process_name;                // Target process name
    uint32_t health_check_interval_ms;       // Health check interval (ms)
    uint32_t health_check_timeout_ms;        // Health check timeout (ms)
    uint32_t failure_threshold;              // Number of failures to trigger failover
    uint32_t restart_delay_ms;               // Delay before restart (ms)
    uint32_t max_restart_count;              // Max restart count within time window
    uint32_t restart_window_sec;             // Time window for restart count (seconds)
    bool enable_state_recovery;              // Whether to recover from checkpoint
    uint32_t checkpoint_interval_sec;        // Checkpoint creation interval (seconds)
    bool enable_leader_election;             // Enable leader election in distributed env

    // Default constructor with recommended defaults
    FailoverPolicy()
        : health_check_interval_ms(1000)
        , health_check_timeout_ms(500)
        , failure_threshold(3)
        , restart_delay_ms(100)
        , max_restart_count(5)
        , restart_window_sec(60)
        , enable_state_recovery(true)
        , checkpoint_interval_sec(60)
        , enable_leader_election(false) {}

    /**
     * @brief Validate policy configuration
     *
     * @return true if policy is valid, false otherwise
     */
    bool isValid() const {
        // Validation rules from data-model.md
        if (health_check_timeout_ms >= health_check_interval_ms) {
            return false;  // Timeout must be shorter than interval
        }
        if (failure_threshold < 1) {
            return false;  // At least 1 failure required
        }
        if (max_restart_count == 0) {
            return false;  // At least 1 restart must be allowed
        }
        if (enable_state_recovery && checkpoint_interval_sec == 0) {
            return false;  // Checkpoint interval must be > 0 if recovery enabled
        }
        return true;
    }
};

/**
 * @brief Failover manager interface
 *
 * Interface for implementing failover management.
 * Follows MXRC Constitution principle: Interface-based design (I-prefix).
 */
class IFailoverManager {
public:
    virtual ~IFailoverManager() = default;

    /**
     * @brief Start failover monitoring
     *
     * Begins monitoring process health and managing failover.
     *
     * @return true if successfully started, false otherwise
     */
    virtual bool start() = 0;

    /**
     * @brief Stop failover monitoring
     *
     * Stops monitoring and cleanup resources.
     */
    virtual void stop() = 0;

    /**
     * @brief Handle process failure
     *
     * Called when process health check fails.
     * Triggers restart or failover based on policy.
     *
     * @param process_name Name of failed process
     */
    virtual void handleProcessFailure(const std::string& process_name) = 0;

    /**
     * @brief Trigger process restart
     *
     * Initiates process restart with optional state recovery.
     *
     * @param process_name Name of process to restart
     * @param recover_state Whether to recover from checkpoint
     * @return true if restart triggered, false otherwise
     */
    virtual bool triggerRestart(const std::string& process_name, bool recover_state = true) = 0;

    /**
     * @brief Check if restart is allowed
     *
     * Checks if restart count is within allowed limit.
     *
     * @param process_name Name of process
     * @return true if restart allowed, false otherwise
     */
    virtual bool canRestart(const std::string& process_name) const = 0;

    /**
     * @brief Get current restart count
     *
     * Returns number of restarts within current time window.
     *
     * @param process_name Name of process
     * @return uint32_t Current restart count
     */
    virtual uint32_t getRestartCount(const std::string& process_name) const = 0;

    /**
     * @brief Reset restart count
     *
     * Resets restart count for a process.
     *
     * @param process_name Name of process
     */
    virtual void resetRestartCount(const std::string& process_name) = 0;

    /**
     * @brief Load failover policy from JSON file
     *
     * @param config_path Path to failover policy JSON file
     * @return true if successfully loaded, false otherwise
     */
    virtual bool loadPolicy(const std::string& config_path) = 0;

    /**
     * @brief Get current failover policy
     *
     * @return const FailoverPolicy& Current policy configuration
     */
    virtual const FailoverPolicy& getPolicy() const = 0;
};

} // namespace ha
} // namespace mxrc
