#include "TimeUtils.h"
#include <spdlog/spdlog.h>
#include <sched.h>
#include <pthread.h>
#include <sys/mman.h>
#include <time.h>
#include <errno.h>
#include <cstring>

namespace mxrc {
namespace core {
namespace rt {
namespace util {

int setPriority(int policy, int priority) {
    struct sched_param param;
    param.sched_priority = priority;

    if (sched_setscheduler(0, policy, &param) == -1) {
        spdlog::error("Failed to set scheduling policy: {} (errno: {})",
                      std::strerror(errno), errno);
        return -1;
    }

    spdlog::info("Set scheduling policy to {} with priority {}",
                 policy == SCHED_FIFO ? "SCHED_FIFO" : "SCHED_OTHER", priority);
    return 0;
}

int pinToCPU(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
        spdlog::error("Failed to pin thread to CPU {}: {} (errno: {})",
                      core_id, std::strerror(errno), errno);
        return -1;
    }

    spdlog::info("Pinned thread to CPU core {}", core_id);
    return 0;
}

int lockMemory() {
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        spdlog::error("Failed to lock memory: {} (errno: {})",
                      std::strerror(errno), errno);
        return -1;
    }

    spdlog::info("Locked all memory pages to prevent paging");
    return 0;
}

uint64_t getMonotonicTimeNs() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        spdlog::error("Failed to get monotonic time: {} (errno: {})",
                      std::strerror(errno), errno);
        return 0;
    }

    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
           static_cast<uint64_t>(ts.tv_nsec);
}

int waitUntilAbsoluteTime(uint64_t wakeup_time_ns) {
    struct timespec ts;
    ts.tv_sec = wakeup_time_ns / 1'000'000'000ULL;
    ts.tv_nsec = wakeup_time_ns % 1'000'000'000ULL;

    if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr) != 0) {
        if (errno != EINTR) {  // Interrupted by signal is acceptable
            spdlog::error("Failed to sleep until absolute time: {} (errno: {})",
                          std::strerror(errno), errno);
            return -1;
        }
    }

    return 0;
}

} // namespace util
} // namespace rt
} // namespace core
} // namespace mxrc
