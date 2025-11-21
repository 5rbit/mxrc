#include "CPUAffinityManager.h"
#include "core/config/ConfigLoader.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

namespace mxrc {
namespace rt {
namespace perf {

bool CPUAffinityManager::loadConfig(const std::string& config_path) {
    config::ConfigLoader loader;
    if (!loader.loadFromFile(config_path)) {
        spdlog::error("Failed to load CPU affinity config from: {}", config_path);
        return false;
    }

    try {
        const auto& json = loader.getJson();

        config_.process_name = json.value("process_name", "");
        config_.thread_name = json.value("thread_name", "");

        // Parse CPU cores array
        if (json.contains("cpu_cores") && json["cpu_cores"].is_array()) {
            config_.cpu_cores.clear();
            for (const auto& core : json["cpu_cores"]) {
                config_.cpu_cores.push_back(core.get<int>());
            }
        }

        // Parse isolation mode
        std::string mode_str = json.value("isolation_mode", "NONE");
        if (mode_str == "ISOLCPUS") {
            config_.isolation_mode = IsolationMode::ISOLCPUS;
        } else if (mode_str == "CGROUPS") {
            config_.isolation_mode = IsolationMode::CGROUPS;
        } else if (mode_str == "HYBRID") {
            config_.isolation_mode = IsolationMode::HYBRID;
        } else {
            config_.isolation_mode = IsolationMode::NONE;
        }

        config_.is_exclusive = json.value("is_exclusive", true);
        config_.priority = json.value("priority", 80);

        // Parse scheduling policy
        std::string policy_str = json.value("policy", "SCHED_FIFO");
        if (policy_str == "SCHED_OTHER") {
            config_.policy = SchedPolicy::OTHER;
        } else if (policy_str == "SCHED_FIFO") {
            config_.policy = SchedPolicy::FIFO;
        } else if (policy_str == "SCHED_RR") {
            config_.policy = SchedPolicy::RR;
        } else if (policy_str == "SCHED_DEADLINE") {
            config_.policy = SchedPolicy::DEADLINE;
        } else {
            config_.policy = SchedPolicy::FIFO;
        }

        spdlog::info("CPU affinity config loaded: process={}, cores={}, mode={}, priority={}",
                     config_.process_name, config_.cpu_cores.size(),
                     isolationModeToString(config_.isolation_mode), config_.priority);

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse CPU affinity config: {}", e.what());
        return false;
    }
}

bool CPUAffinityManager::apply(const CPUAffinityConfig& config) {
    config_ = config;

    // Validate configuration
    if (config_.cpu_cores.empty()) {
        spdlog::error("CPU affinity config has no CPU cores specified");
        return false;
    }

    // Set CPU affinity
    if (!setCPUAffinity(config_.cpu_cores)) {
        spdlog::error("Failed to set CPU affinity");
        return false;
    }

    // Set scheduling policy and priority
    if (!setSchedulingPolicy(config_.policy, config_.priority)) {
        spdlog::error("Failed to set scheduling policy");
        return false;
    }

    // Verify isolation if required
    if (config_.isolation_mode != IsolationMode::NONE) {
        if (!verifyIsolation(config_)) {
            spdlog::warn("CPU isolation verification failed - cores may not be properly isolated");
        }
    }

    spdlog::info("CPU affinity applied: cores=[{}], policy={}, priority={}",
                 [&]() {
                     std::string cores_str;
                     for (size_t i = 0; i < config_.cpu_cores.size(); ++i) {
                         if (i > 0) cores_str += ",";
                         cores_str += std::to_string(config_.cpu_cores[i]);
                     }
                     return cores_str;
                 }(),
                 schedPolicyToString(config_.policy), config_.priority);

    return true;
}

bool CPUAffinityManager::setCPUAffinity(const std::vector<int>& cpu_cores) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    for (int core : cpu_cores) {
        CPU_SET(core, &cpuset);
    }

    pthread_t current_thread = pthread_self();
    int result = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);

    if (result != 0) {
        spdlog::error("pthread_setaffinity_np failed: {}", strerror(result));
        return false;
    }

    return true;
}

bool CPUAffinityManager::setSchedulingPolicy(SchedPolicy policy, int priority) {
    int sched_policy;

    switch (policy) {
        case SchedPolicy::OTHER:
            sched_policy = SCHED_OTHER;
            priority = 0; // SCHED_OTHER doesn't use priority
            break;
        case SchedPolicy::FIFO:
            sched_policy = SCHED_FIFO;
            break;
        case SchedPolicy::RR:
            sched_policy = SCHED_RR;
            break;
        case SchedPolicy::DEADLINE:
            #ifdef SCHED_DEADLINE
            sched_policy = SCHED_DEADLINE;
            #else
            spdlog::warn("SCHED_DEADLINE not supported, falling back to SCHED_FIFO");
            sched_policy = SCHED_FIFO;
            #endif
            break;
        default:
            sched_policy = SCHED_FIFO;
            break;
    }

    struct sched_param param;
    param.sched_priority = priority;

    int result = pthread_setschedparam(pthread_self(), sched_policy, &param);

    if (result != 0) {
        spdlog::error("pthread_setschedparam failed: {} (may need CAP_SYS_NICE capability)",
                      strerror(result));
        return false;
    }

    return true;
}

std::vector<int> CPUAffinityManager::getCurrentAffinity() const {
    std::vector<int> cores;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    pthread_t current_thread = pthread_self();
    int result = pthread_getaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);

    if (result != 0) {
        spdlog::error("pthread_getaffinity_np failed: {}", strerror(result));
        return cores;
    }

    for (int i = 0; i < CPU_SETSIZE; ++i) {
        if (CPU_ISSET(i, &cpuset)) {
            cores.push_back(i);
        }
    }

    return cores;
}

bool CPUAffinityManager::verifyIsolation(const CPUAffinityConfig& config) const {
    bool verified = false;

    switch (config.isolation_mode) {
        case IsolationMode::ISOLCPUS:
            verified = checkIsolcpus(config.cpu_cores);
            break;
        case IsolationMode::CGROUPS:
            verified = checkCgroups(config.cpu_cores);
            break;
        case IsolationMode::HYBRID:
            verified = checkIsolcpus(config.cpu_cores) && checkCgroups(config.cpu_cores);
            break;
        default:
            verified = true; // No isolation required
            break;
    }

    return verified;
}

bool CPUAffinityManager::checkIsolcpus(const std::vector<int>& cpu_cores) const {
    // Check /proc/cmdline for isolcpus parameter
    std::ifstream cmdline("/proc/cmdline");
    if (!cmdline.is_open()) {
        spdlog::warn("Cannot open /proc/cmdline to verify isolcpus");
        return false;
    }

    std::string line;
    std::getline(cmdline, line);

    if (line.find("isolcpus=") == std::string::npos) {
        spdlog::warn("isolcpus parameter not found in kernel boot parameters");
        return false;
    }

    // TODO: Parse isolcpus value and verify it includes our cores
    // For now, just check if isolcpus is present
    spdlog::info("isolcpus parameter found in kernel boot parameters");
    return true;
}

bool CPUAffinityManager::checkCgroups(const std::vector<int>& cpu_cores) const {
    // Check if cgroups cpuset is configured
    std::ifstream cpuset("/sys/fs/cgroup/cpuset/cpuset.cpus");
    if (!cpuset.is_open()) {
        spdlog::warn("Cannot open /sys/fs/cgroup/cpuset/cpuset.cpus to verify cgroups");
        return false;
    }

    std::string line;
    std::getline(cpuset, line);

    if (line.empty()) {
        spdlog::warn("cgroups cpuset.cpus is empty");
        return false;
    }

    // TODO: Parse cpuset.cpus value and verify our cores are isolated
    // For now, just check if cpuset.cpus exists
    spdlog::info("cgroups cpuset found");
    return true;
}

// CPUAffinityGuard implementation
CPUAffinityGuard::CPUAffinityGuard(CPUAffinityManager& manager, const CPUAffinityConfig& config)
    : manager_(manager)
    , restore_on_destroy_(true) {

    // Save current affinity
    original_affinity_ = manager_.getCurrentAffinity();

    // Apply new configuration
    if (!manager_.apply(config)) {
        spdlog::error("Failed to apply CPU affinity in guard");
        restore_on_destroy_ = false;
    }
}

CPUAffinityGuard::~CPUAffinityGuard() {
    if (restore_on_destroy_ && !original_affinity_.empty()) {
        // Restore original affinity
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);

        for (int core : original_affinity_) {
            CPU_SET(core, &cpuset);
        }

        pthread_t current_thread = pthread_self();
        int result = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);

        if (result != 0) {
            spdlog::error("Failed to restore original CPU affinity: {}", strerror(result));
        } else {
            spdlog::debug("Original CPU affinity restored");
        }
    }
}

} // namespace perf
} // namespace rt
} // namespace mxrc
