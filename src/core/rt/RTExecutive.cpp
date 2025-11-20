#include "RTExecutive.h"
#include "util/TimeUtils.h"
#include <spdlog/spdlog.h>
#include <sched.h>

namespace mxrc {
namespace core {
namespace rt {

RTExecutive::RTExecutive(uint32_t minor_cycle_ms, uint32_t major_cycle_ms)
    : minor_cycle_ms_(minor_cycle_ms)
    , major_cycle_ms_(major_cycle_ms)
    , num_slots_(major_cycle_ms / minor_cycle_ms)
    , running_(false)
    , current_slot_(0) {

    // Initialize schedule slots
    schedule_.resize(num_slots_);

    spdlog::info("RTExecutive initialized: minor_cycle={}ms, major_cycle={}ms, slots={}",
                 minor_cycle_ms_, major_cycle_ms_, num_slots_);
}

RTExecutive::~RTExecutive() {
    stop();
}

int RTExecutive::run() {
    spdlog::info("RTExecutive starting...");

    // Set RT priority
    if (util::setPriority(SCHED_FIFO, 90) != 0) {
        spdlog::error("Failed to set RT priority. May need CAP_SYS_NICE capability.");
        // Continue anyway for testing
    }

    // Pin to CPU core 1
    if (util::pinToCPU(1) != 0) {
        spdlog::warn("Failed to pin to CPU core. Performance may be affected.");
    }

    // Lock memory to prevent paging
    if (util::lockMemory() != 0) {
        spdlog::warn("Failed to lock memory. May need CAP_IPC_LOCK capability.");
    }

    running_ = true;
    current_slot_ = 0;

    uint64_t cycle_duration_ns = minor_cycle_ms_ * 1'000'000ULL;
    uint64_t cycle_start_ns = util::getMonotonicTimeNs();

    // Main cyclic executive loop
    while (running_) {
        // Execute actions for current slot
        executeSlot(current_slot_);

        // Move to next slot
        current_slot_ = (current_slot_ + 1) % num_slots_;

        // Wait until next cycle
        uint64_t next_cycle_ns = cycle_start_ns + cycle_duration_ns;
        waitUntilNextCycle(cycle_start_ns, cycle_duration_ns);
        cycle_start_ns = next_cycle_ns;
    }

    spdlog::info("RTExecutive stopped");
    return 0;
}

void RTExecutive::stop() {
    if (running_) {
        spdlog::info("RTExecutive stopping...");
        running_ = false;
    }
}

int RTExecutive::registerAction(const std::string& name, uint32_t period_ms, ActionCallback callback) {
    // Validate period is a multiple of minor cycle
    if (period_ms % minor_cycle_ms_ != 0) {
        spdlog::error("Action period {}ms is not a multiple of minor cycle {}ms",
                      period_ms, minor_cycle_ms_);
        return -1;
    }

    // Calculate slot interval
    uint32_t slot_interval = period_ms / minor_cycle_ms_;

    // Add action to first slot, and calculate next execution slots
    ActionSlot action{name, period_ms, callback, slot_interval};
    schedule_[0].push_back(action);

    spdlog::info("Registered action '{}' with period {}ms (slot interval: {})",
                 name, period_ms, slot_interval);
    return 0;
}

void RTExecutive::executeSlot(uint32_t slot) {
    for (auto& action : schedule_[slot]) {
        // TODO: Execute action with context
        // For now, just log
        spdlog::trace("Executing action '{}'", action.name);
    }
}

int RTExecutive::waitUntilNextCycle(uint64_t cycle_start_ns, uint64_t cycle_duration_ns) {
    uint64_t wakeup_time_ns = cycle_start_ns + cycle_duration_ns;
    return util::waitUntilAbsoluteTime(wakeup_time_ns);
}

} // namespace rt
} // namespace core
} // namespace mxrc
