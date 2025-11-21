#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace mxrc {
namespace rt {
namespace perf {

/**
 * @brief CPU isolation mode
 *
 * Production readiness: Defines how CPU cores are isolated from OS scheduler.
 */
enum class IsolationMode {
    NONE,           // No isolation
    ISOLCPUS,       // Kernel boot parameter isolcpus
    CGROUPS,        // cgroups cpuset
    HYBRID          // isolcpus + cgroups (recommended)
};

/**
 * @brief Scheduling policy
 *
 * Production readiness: Real-time scheduling policies for RT threads.
 */
enum class SchedPolicy {
    OTHER,      // Normal scheduling
    FIFO,       // RT FIFO (recommended for RT)
    RR,         // RT Round-Robin
    DEADLINE    // Deadline scheduling
};

/**
 * @brief CPU affinity configuration
 *
 * Production readiness: Defines CPU core allocation policy for RT processes.
 * Ensures predictable real-time performance by pinning threads to dedicated cores.
 *
 * Based on data-model.md: CPUAffinityConfig
 */
struct CPUAffinityConfig {
    std::string process_name;           // Process name (e.g., "rt_process")
    std::string thread_name;            // Thread name (optional, e.g., "main")
    std::vector<int> cpu_cores;         // CPU core IDs to bind to
    IsolationMode isolation_mode;       // CPU isolation mode
    bool is_exclusive;                  // Exclusive allocation (block other processes)
    int priority;                       // Thread priority (1-99 for SCHED_FIFO/RR)
    SchedPolicy policy;                 // Scheduling policy

    // Default values
    CPUAffinityConfig()
        : process_name("")
        , thread_name("")
        , cpu_cores()
        , isolation_mode(IsolationMode::NONE)
        , is_exclusive(true)
        , priority(80)
        , policy(SchedPolicy::FIFO) {}
};

/**
 * @brief CPU Affinity Manager
 *
 * Production readiness: Manages CPU core affinity for RT processes.
 * Uses pthread_setaffinity_np to pin threads to specific CPU cores.
 */
class CPUAffinityManager {
public:
    CPUAffinityManager() = default;
    ~CPUAffinityManager() = default;

    // Non-copyable, non-movable
    CPUAffinityManager(const CPUAffinityManager&) = delete;
    CPUAffinityManager& operator=(const CPUAffinityManager&) = delete;

    /**
     * @brief Load configuration from JSON file
     *
     * @param config_path Path to cpu_affinity.json
     * @return true if successfully loaded
     */
    bool loadConfig(const std::string& config_path);

    /**
     * @brief Apply CPU affinity configuration
     *
     * Pins the calling thread to configured CPU cores.
     *
     * @param config CPU affinity configuration
     * @return true if successfully applied
     */
    bool apply(const CPUAffinityConfig& config);

    /**
     * @brief Verify CPU isolation is configured
     *
     * Checks if isolcpus/cgroups are properly set up.
     *
     * @param config CPU affinity configuration
     * @return true if isolation is configured correctly
     */
    bool verifyIsolation(const CPUAffinityConfig& config) const;

    /**
     * @brief Get current CPU affinity
     *
     * @return std::vector<int> List of CPU cores current thread is bound to
     */
    std::vector<int> getCurrentAffinity() const;

private:
    CPUAffinityConfig config_;

    /**
     * @brief Set CPU affinity for current thread
     *
     * @param cpu_cores CPU core IDs
     * @return true if successful
     */
    bool setCPUAffinity(const std::vector<int>& cpu_cores);

    /**
     * @brief Set scheduling policy and priority
     *
     * @param policy Scheduling policy
     * @param priority Thread priority (1-99)
     * @return true if successful
     */
    bool setSchedulingPolicy(SchedPolicy policy, int priority);

    /**
     * @brief Check if isolcpus is configured
     *
     * @param cpu_cores CPU cores to check
     * @return true if cores are isolated via isolcpus
     */
    bool checkIsolcpus(const std::vector<int>& cpu_cores) const;

    /**
     * @brief Check if cgroups cpuset is configured
     *
     * @param cpu_cores CPU cores to check
     * @return true if cores are isolated via cgroups
     */
    bool checkCgroups(const std::vector<int>& cpu_cores) const;
};

/**
 * @brief RAII guard for CPU affinity
 *
 * Production readiness: Automatic restoration of original CPU affinity.
 * Follows MXRC Constitution principle: RAII for resource management.
 */
class CPUAffinityGuard {
public:
    /**
     * @brief Save current affinity and apply new configuration
     *
     * @param manager CPU affinity manager
     * @param config Configuration to apply
     */
    CPUAffinityGuard(CPUAffinityManager& manager, const CPUAffinityConfig& config);

    /**
     * @brief Restore original affinity
     */
    ~CPUAffinityGuard();

    // Non-copyable, movable
    CPUAffinityGuard(const CPUAffinityGuard&) = delete;
    CPUAffinityGuard& operator=(const CPUAffinityGuard&) = delete;
    CPUAffinityGuard(CPUAffinityGuard&&) = default;
    CPUAffinityGuard& operator=(CPUAffinityGuard&&) = default;

private:
    CPUAffinityManager& manager_;
    std::vector<int> original_affinity_;
    bool restore_on_destroy_;
};

// Utility functions
inline const char* isolationModeToString(IsolationMode mode) {
    switch (mode) {
        case IsolationMode::NONE: return "NONE";
        case IsolationMode::ISOLCPUS: return "ISOLCPUS";
        case IsolationMode::CGROUPS: return "CGROUPS";
        case IsolationMode::HYBRID: return "HYBRID";
        default: return "UNKNOWN";
    }
}

inline const char* schedPolicyToString(SchedPolicy policy) {
    switch (policy) {
        case SchedPolicy::OTHER: return "SCHED_OTHER";
        case SchedPolicy::FIFO: return "SCHED_FIFO";
        case SchedPolicy::RR: return "SCHED_RR";
        case SchedPolicy::DEADLINE: return "SCHED_DEADLINE";
        default: return "UNKNOWN";
    }
}

} // namespace perf
} // namespace rt
} // namespace mxrc
