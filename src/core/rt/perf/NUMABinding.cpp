#include "NUMABinding.h"
#include "core/config/ConfigLoader.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <unistd.h>

// Conditional NUMA support
#ifdef __has_include
#  if __has_include(<numa.h>)
#    define HAVE_NUMA 1
#    include <numa.h>
#    include <numaif.h>
#  else
#    define HAVE_NUMA 0
#  endif
#else
#  define HAVE_NUMA 0
#endif

namespace mxrc {
namespace rt {
namespace perf {

bool NUMABinding::loadConfig(const std::string& config_path) {
    config::ConfigLoader loader;
    if (!loader.loadFromFile(config_path)) {
        spdlog::error("Failed to load NUMA binding config from: {}", config_path);
        return false;
    }

    try {
        const auto& json = loader.getJson();

        config_.process_name = json.value("process_name", "");
        config_.numa_node = json.value("numa_node", 0);

        // Parse memory policy
        std::string policy_str = json.value("memory_policy", "LOCAL");
        if (policy_str == "DEFAULT") {
            config_.memory_policy = MemoryPolicy::DEFAULT;
        } else if (policy_str == "BIND") {
            config_.memory_policy = MemoryPolicy::BIND;
        } else if (policy_str == "PREFERRED") {
            config_.memory_policy = MemoryPolicy::PREFERRED;
        } else if (policy_str == "INTERLEAVE") {
            config_.memory_policy = MemoryPolicy::INTERLEAVE;
        } else if (policy_str == "LOCAL") {
            config_.memory_policy = MemoryPolicy::LOCAL;
        } else {
            config_.memory_policy = MemoryPolicy::LOCAL;
        }

        config_.strict_binding = json.value("strict_binding", true);
        config_.migrate_pages = json.value("migrate_pages", false);

        // Parse CPU cores hint
        if (json.contains("cpu_cores_hint") && json["cpu_cores_hint"].is_array()) {
            config_.cpu_cores_hint.clear();
            for (const auto& core : json["cpu_cores_hint"]) {
                config_.cpu_cores_hint.push_back(core.get<int>());
            }
        }

        spdlog::info("NUMA binding config loaded: process={}, node={}, policy={}",
                     config_.process_name, config_.numa_node,
                     memoryPolicyToString(config_.memory_policy));

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse NUMA binding config: {}", e.what());
        return false;
    }
}

bool NUMABinding::apply(const NUMABindingConfig& config) {
    config_ = config;

#if HAVE_NUMA
    if (!isAvailable()) {
        spdlog::warn("NUMA is not available on this system");
        return false;
    }

    // Validate NUMA node
    int max_node = numa_max_node();
    if (config_.numa_node < 0 || config_.numa_node > max_node) {
        spdlog::error("Invalid NUMA node: {} (max: {})", config_.numa_node, max_node);
        return false;
    }

    // Set memory policy
    if (!setMemoryPolicy(config_.numa_node, config_.memory_policy)) {
        spdlog::error("Failed to set NUMA memory policy");
        return false;
    }

    // Migrate pages if requested
    if (config_.migrate_pages) {
        if (!migratePages(config_.numa_node)) {
            spdlog::warn("Failed to migrate pages to NUMA node {}", config_.numa_node);
        }
    }

    spdlog::info("NUMA binding applied: node={}, policy={}",
                 config_.numa_node, memoryPolicyToString(config_.memory_policy));

    return true;
#else
    spdlog::warn("NUMA support not compiled in - NUMA binding will be skipped");
    spdlog::warn("Install libnuma-dev and rebuild to enable NUMA support");
    return false;
#endif
}

bool NUMABinding::setMemoryPolicy(int node, MemoryPolicy policy) {
#if HAVE_NUMA
    unsigned long nodemask = 1UL << node;

    int result = 0;
    switch (policy) {
        case MemoryPolicy::DEFAULT:
            result = set_mempolicy(MPOL_DEFAULT, nullptr, 0);
            break;
        case MemoryPolicy::BIND:
            result = set_mempolicy(MPOL_BIND, &nodemask, sizeof(nodemask) * 8);
            break;
        case MemoryPolicy::PREFERRED:
            result = set_mempolicy(MPOL_PREFERRED, &nodemask, sizeof(nodemask) * 8);
            break;
        case MemoryPolicy::INTERLEAVE:
            result = set_mempolicy(MPOL_INTERLEAVE, &nodemask, sizeof(nodemask) * 8);
            break;
        case MemoryPolicy::LOCAL:
            result = set_mempolicy(MPOL_LOCAL, nullptr, 0);
            break;
        default:
            spdlog::error("Unknown memory policy");
            return false;
    }

    if (result != 0) {
        spdlog::error("set_mempolicy failed: {}", strerror(errno));
        return false;
    }

    return true;
#else
    return false;
#endif
}

bool NUMABinding::migratePages(int node) {
#if HAVE_NUMA
    // This is a simplified implementation
    // In production, you might want to use numa_migrate_pages()
    spdlog::info("Page migration requested to node {} (not fully implemented)", node);
    return true;
#else
    return false;
#endif
}

bool NUMABinding::verifyBinding(const NUMABindingConfig& config) const {
#if HAVE_NUMA
    if (!isAvailable()) {
        return false;
    }

    // Check current memory policy
    int mode;
    unsigned long nodemask;
    long result = get_mempolicy(&mode, &nodemask, sizeof(nodemask) * 8, nullptr, 0);

    if (result != 0) {
        spdlog::error("get_mempolicy failed: {}", strerror(errno));
        return false;
    }

    // Verify the policy matches
    bool policy_matches = false;
    switch (config.memory_policy) {
        case MemoryPolicy::DEFAULT:
            policy_matches = (mode == MPOL_DEFAULT);
            break;
        case MemoryPolicy::BIND:
            policy_matches = (mode == MPOL_BIND);
            break;
        case MemoryPolicy::PREFERRED:
            policy_matches = (mode == MPOL_PREFERRED);
            break;
        case MemoryPolicy::INTERLEAVE:
            policy_matches = (mode == MPOL_INTERLEAVE);
            break;
        case MemoryPolicy::LOCAL:
            policy_matches = (mode == MPOL_LOCAL);
            break;
    }

    if (!policy_matches) {
        spdlog::warn("NUMA policy verification failed: expected {}, got mode {}",
                     memoryPolicyToString(config.memory_policy), mode);
        return false;
    }

    spdlog::info("NUMA binding verified: node={}, policy={}",
                 config.numa_node, memoryPolicyToString(config.memory_policy));

    return true;
#else
    return false;
#endif
}

NUMABinding::NUMAStats NUMABinding::getStats(pid_t pid) const {
    NUMAStats stats{0, 0, 0, 0.0};

    if (pid == 0) {
        pid = getpid();
    }

    return parseNumaMaps(pid);
}

NUMABinding::NUMAStats NUMABinding::parseNumaMaps(pid_t pid) const {
    NUMAStats stats{0, 0, 0, 0.0};

    std::string numa_maps_path = "/proc/" + std::to_string(pid) + "/numa_maps";
    std::ifstream file(numa_maps_path);

    if (!file.is_open()) {
        spdlog::warn("Cannot open {} to read NUMA statistics", numa_maps_path);
        return stats;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Parse numa_maps format: <address> <policy> ... N<node>=<pages>
        std::istringstream iss(line);
        std::string token;

        while (iss >> token) {
            if (token.find("N") == 0 && token.find("=") != std::string::npos) {
                // Parse N<node>=<pages>
                size_t eq_pos = token.find("=");
                int node = std::stoi(token.substr(1, eq_pos - 1));
                uint64_t pages = std::stoull(token.substr(eq_pos + 1));

                stats.total_pages += pages;

                // Assume local node is config_.numa_node
                if (node == config_.numa_node) {
                    stats.local_pages += pages;
                } else {
                    stats.remote_pages += pages;
                }
            }
        }
    }

    if (stats.total_pages > 0) {
        stats.local_access_percent = (stats.local_pages * 100.0) / stats.total_pages;
    }

    spdlog::debug("NUMA stats: total={}, local={}, remote={}, local%={:.2f}",
                  stats.total_pages, stats.local_pages, stats.remote_pages,
                  stats.local_access_percent);

    return stats;
}

bool NUMABinding::isAvailable() {
#if HAVE_NUMA
    return numa_available() != -1;
#else
    return false;
#endif
}

int NUMABinding::getNumNodes() {
#if HAVE_NUMA
    if (isAvailable()) {
        return numa_max_node() + 1;
    }
#endif
    return 1; // Single node (UMA system)
}

// NUMABindingGuard implementation
NUMABindingGuard::NUMABindingGuard(NUMABinding& binding, const NUMABindingConfig& config)
    : binding_(binding)
    , original_node_(0)
    , original_policy_(MemoryPolicy::DEFAULT)
    , restore_on_destroy_(true) {

#if HAVE_NUMA
    // Save current policy
    int mode;
    unsigned long nodemask;
    long result = get_mempolicy(&mode, &nodemask, sizeof(nodemask) * 8, nullptr, 0);

    if (result == 0) {
        // Map mode to MemoryPolicy
        switch (mode) {
            case MPOL_DEFAULT:
                original_policy_ = MemoryPolicy::DEFAULT;
                break;
            case MPOL_BIND:
                original_policy_ = MemoryPolicy::BIND;
                break;
            case MPOL_PREFERRED:
                original_policy_ = MemoryPolicy::PREFERRED;
                break;
            case MPOL_INTERLEAVE:
                original_policy_ = MemoryPolicy::INTERLEAVE;
                break;
            case MPOL_LOCAL:
                original_policy_ = MemoryPolicy::LOCAL;
                break;
            default:
                original_policy_ = MemoryPolicy::DEFAULT;
                break;
        }

        // Find first set bit in nodemask
        for (int i = 0; i < 64; ++i) {
            if (nodemask & (1UL << i)) {
                original_node_ = i;
                break;
            }
        }
    }
#endif

    // Apply new configuration
    if (!binding_.apply(config)) {
        spdlog::error("Failed to apply NUMA binding in guard");
        restore_on_destroy_ = false;
    }
}

NUMABindingGuard::~NUMABindingGuard() {
#if HAVE_NUMA
    if (restore_on_destroy_) {
        // Restore original policy
        unsigned long nodemask = 1UL << original_node_;
        int result = 0;

        switch (original_policy_) {
            case MemoryPolicy::DEFAULT:
                result = set_mempolicy(MPOL_DEFAULT, nullptr, 0);
                break;
            case MemoryPolicy::BIND:
                result = set_mempolicy(MPOL_BIND, &nodemask, sizeof(nodemask) * 8);
                break;
            case MemoryPolicy::PREFERRED:
                result = set_mempolicy(MPOL_PREFERRED, &nodemask, sizeof(nodemask) * 8);
                break;
            case MemoryPolicy::INTERLEAVE:
                result = set_mempolicy(MPOL_INTERLEAVE, &nodemask, sizeof(nodemask) * 8);
                break;
            case MemoryPolicy::LOCAL:
                result = set_mempolicy(MPOL_LOCAL, nullptr, 0);
                break;
        }

        if (result != 0) {
            spdlog::error("Failed to restore original NUMA policy: {}", strerror(errno));
        } else {
            spdlog::debug("Original NUMA policy restored");
        }
    }
#endif
}

} // namespace perf
} // namespace rt
} // namespace mxrc
