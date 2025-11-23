#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace mxrc {
namespace rt {
namespace perf {

/**
 * @brief NUMA memory allocation policy
 *
 * Production readiness: Defines how memory is allocated across NUMA nodes.
 */
enum class MemoryPolicy {
    DEFAULT,        // System default policy
    BIND,           // Strict binding to specific node (fail if unavailable)
    PREFERRED,      // Prefer specific node, fallback to others
    INTERLEAVE,     // Interleave across multiple nodes
    LOCAL           // Allocate on current node (recommended for RT)
};

/**
 * @brief NUMA binding configuration
 *
 * Production readiness: Defines NUMA node binding for memory locality optimization.
 * Minimizes memory access latency by ensuring local NUMA access.
 *
 * Based on data-model.md: NUMABindingConfig
 */
struct NUMABindingConfig {
    std::string process_name;           // Process name
    int numa_node;                      // NUMA node ID to bind to
    MemoryPolicy memory_policy;         // Memory allocation policy
    bool strict_binding;                // Strict binding (fail on error)
    bool migrate_pages;                 // Migrate existing pages to NUMA node
    std::vector<int> cpu_cores_hint;    // Preferred CPU cores within NUMA node

    // Default values
    NUMABindingConfig()
        : process_name("")
        , numa_node(0)
        , memory_policy(MemoryPolicy::LOCAL)
        , strict_binding(true)
        , migrate_pages(false)
        , cpu_cores_hint() {}
};

/**
 * @brief NUMA binding manager
 *
 * Production readiness: Manages NUMA memory binding for RT processes.
 * Uses libnuma to set memory allocation policies.
 */
class NUMABinding {
public:
    NUMABinding() = default;
    ~NUMABinding() = default;

    // Non-copyable, non-movable
    NUMABinding(const NUMABinding&) = delete;
    NUMABinding& operator=(const NUMABinding&) = delete;

    /**
     * @brief Load configuration from JSON file
     *
     * @param config_path Path to numa_binding.json
     * @return true if successfully loaded
     */
    bool loadConfig(const std::string& config_path);

    /**
     * @brief Apply NUMA binding configuration
     *
     * Binds the calling process to configured NUMA node.
     *
     * @param config NUMA binding configuration
     * @return true if successfully applied
     */
    bool apply(const NUMABindingConfig& config);

    /**
     * @brief Verify NUMA binding is applied
     *
     * Checks if memory policy is correctly set.
     *
     * @param config NUMA binding configuration
     * @return true if binding is applied correctly
     */
    bool verifyBinding(const NUMABindingConfig& config) const;

    /**
     * @brief Get NUMA statistics
     *
     * Parses /proc/[pid]/numa_maps to get memory access statistics.
     *
     * @param pid Process ID (0 for current process)
     * @return NUMAStats NUMA access statistics
     */
    struct NUMAStats {
        uint64_t total_pages;
        uint64_t local_pages;
        uint64_t remote_pages;
        double local_access_percent;
    };
    NUMAStats getStats(pid_t pid = 0) const;

    /**
     * @brief Check if NUMA is available
     *
     * @return true if system has multiple NUMA nodes
     */
    static bool isAvailable();

    /**
     * @brief Get number of NUMA nodes
     *
     * @return int Number of NUMA nodes in system
     */
    static int getNumNodes();

private:
    NUMABindingConfig config_;

    /**
     * @brief Set NUMA memory policy
     *
     * @param node NUMA node ID
     * @param policy Memory policy
     * @return true if successful
     */
    bool setMemoryPolicy(int node, MemoryPolicy policy);

    /**
     * @brief Migrate pages to NUMA node
     *
     * @param node NUMA node ID
     * @return true if successful
     */
    bool migratePages(int node);

    /**
     * @brief Parse /proc/[pid]/numa_maps
     *
     * @param pid Process ID
     * @return NUMAStats Parsed statistics
     */
    NUMAStats parseNumaMaps(pid_t pid) const;
};

/**
 * @brief RAII guard for NUMA binding
 *
 * Production readiness: Automatic restoration of original NUMA policy.
 * Follows MXRC Constitution principle: RAII for resource management.
 */
class NUMABindingGuard {
public:
    /**
     * @brief Save current NUMA policy and apply new configuration
     *
     * @param binding NUMA binding manager
     * @param config Configuration to apply
     */
    NUMABindingGuard(NUMABinding& binding, const NUMABindingConfig& config);

    /**
     * @brief Restore original NUMA policy
     */
    ~NUMABindingGuard();

    // Non-copyable, movable
    NUMABindingGuard(const NUMABindingGuard&) = delete;
    NUMABindingGuard& operator=(const NUMABindingGuard&) = delete;
    NUMABindingGuard(NUMABindingGuard&&) = default;
    NUMABindingGuard& operator=(NUMABindingGuard&&) = default;

private:
    NUMABinding& binding_;
    int original_node_;
    MemoryPolicy original_policy_;
    bool restore_on_destroy_;
};

// Utility functions
inline const char* memoryPolicyToString(MemoryPolicy policy) {
    switch (policy) {
        case MemoryPolicy::DEFAULT: return "DEFAULT";
        case MemoryPolicy::BIND: return "BIND";
        case MemoryPolicy::PREFERRED: return "PREFERRED";
        case MemoryPolicy::INTERLEAVE: return "INTERLEAVE";
        case MemoryPolicy::LOCAL: return "LOCAL";
        default: return "UNKNOWN";
    }
}

} // namespace perf
} // namespace rt
} // namespace mxrc
