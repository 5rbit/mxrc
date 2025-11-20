#include "RTExecutive.h"
#include "RTStateMachine.h"
#include "util/TimeUtils.h"
#include "util/ScheduleCalculator.h"
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
    , current_slot_(0)
    , cycle_count_(0)
    , state_machine_(std::make_unique<RTStateMachine>()) {

    // Initialize schedule slots
    schedule_.resize(num_slots_);

    // Initialize context
    context_.data_store = nullptr;
    context_.current_slot = 0;
    context_.cycle_count = 0;
    context_.timestamp_ns = 0;

    // INIT -> READY 전환
    state_machine_->handleEvent(RTEvent::START);

    spdlog::info("RTExecutive initialized: minor_cycle={}ms, major_cycle={}ms, slots={}",
                 minor_cycle_ms_, major_cycle_ms_, num_slots_);
}

RTExecutive* RTExecutive::createFromPeriods(const std::vector<uint32_t>& periods_ms) {
    try {
        // 주기 배열로부터 스케줄 파라미터 계산
        auto params = util::calculate(periods_ms);

        spdlog::info("Creating RTExecutive from periods: minor={}ms, major={}ms, slots={}",
                     params.minor_cycle_ms, params.major_cycle_ms, params.num_slots);

        return new RTExecutive(params.minor_cycle_ms, params.major_cycle_ms);
    } catch (const std::exception& e) {
        spdlog::error("Failed to create RTExecutive from periods: {}", e.what());
        return nullptr;
    }
}

RTExecutive::~RTExecutive() {
    stop();
}

int RTExecutive::run() {
    spdlog::info("RTExecutive starting...");

    // READY -> RUNNING 전환
    if (state_machine_->getState() == RTState::READY) {
        state_machine_->handleEvent(RTEvent::START);
    }

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
    cycle_count_ = 0;

    uint64_t cycle_duration_ns = minor_cycle_ms_ * 1'000'000ULL;
    uint64_t cycle_start_ns = util::getMonotonicTimeNs();

    // Main cyclic executive loop
    while (running_) {
        // Update context
        context_.current_slot = current_slot_;
        context_.cycle_count = cycle_count_;
        context_.timestamp_ns = cycle_start_ns;

        // Execute actions for current slot
        executeSlot(current_slot_);

        // Move to next slot
        current_slot_ = (current_slot_ + 1) % num_slots_;
        cycle_count_++;

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

        // RUNNING/PAUSED -> SHUTDOWN 전환
        if (state_machine_->getState() == RTState::RUNNING ||
            state_machine_->getState() == RTState::PAUSED) {
            state_machine_->handleEvent(RTEvent::STOP);
        }
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

    // Add action to all appropriate slots
    // Action은 slot_interval마다 실행되어야 함
    for (uint32_t slot = 0; slot < num_slots_; slot += slot_interval) {
        ActionSlot action{name, period_ms, callback, slot_interval};
        schedule_[slot].push_back(action);
    }

    spdlog::info("Registered action '{}' with period {}ms (slot interval: {})",
                 name, period_ms, slot_interval);
    return 0;
}

void RTExecutive::executeSlot(uint32_t slot) {
    for (auto& action : schedule_[slot]) {
        spdlog::trace("Executing action '{}'", action.name);

        // Execute action with context
        if (action.callback) {
            action.callback(context_);
        }
    }
}

void RTExecutive::setDataStore(RTDataStore* data_store) {
    context_.data_store = data_store;
    spdlog::info("RTDataStore attached to RTExecutive");
}

int RTExecutive::waitUntilNextCycle(uint64_t cycle_start_ns, uint64_t cycle_duration_ns) {
    uint64_t wakeup_time_ns = cycle_start_ns + cycle_duration_ns;
    return util::waitUntilAbsoluteTime(wakeup_time_ns);
}

} // namespace rt
} // namespace core
} // namespace mxrc
