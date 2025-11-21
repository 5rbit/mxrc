#include "RTExecutive.h"
#include "RTStateMachine.h"
#include "util/TimeUtils.h"
#include "util/ScheduleCalculator.h"
#include "ipc/SharedMemoryData.h"
#include "core/event/interfaces/IEventBus.h"
#include "core/event/dto/RTEvents.h"
#include "core/rt/perf/CPUAffinityManager.h"
#include "core/rt/perf/NUMABinding.h"
#include "core/rt/perf/PerfMonitor.h"
#include "core/rt/RTMetrics.h"
#include <spdlog/spdlog.h>
#include <sched.h>

namespace mxrc {
namespace core {
namespace rt {

RTExecutive::RTExecutive(uint32_t minor_cycle_ms, uint32_t major_cycle_ms,
                         std::shared_ptr<event::IEventBus> event_bus)
    : minor_cycle_ms_(minor_cycle_ms)
    , major_cycle_ms_(major_cycle_ms)
    , num_slots_(major_cycle_ms / minor_cycle_ms)
    , running_(false)
    , current_slot_(0)
    , cycle_count_(0)
    , state_machine_(std::make_unique<RTStateMachine>())
    , shared_memory_ptr_(nullptr)
    , heartbeat_monitoring_enabled_(false)
    , last_heartbeat_check_ns_(0)
    , safe_mode_enter_time_ns_(0)
    , event_bus_(event_bus)
    , cpu_affinity_mgr_impl_(new mxrc::rt::perf::CPUAffinityManager())
    , numa_binding_impl_(new mxrc::rt::perf::NUMABinding())
    , perf_monitor_impl_(new mxrc::rt::perf::PerfMonitor())
    , rt_metrics_(nullptr) {

    // Initialize schedule slots
    schedule_.resize(num_slots_);

    // Initialize context
    context_.data_store = nullptr;
    context_.current_slot = 0;
    context_.cycle_count = 0;
    context_.timestamp_ns = 0;

    // INIT -> READY 전환 (상태 변경 콜백 등록 후)
    if (event_bus_) {
        state_machine_->setTransitionCallback(
            [this](RTState from, RTState to, RTEvent event) {
                // RT 상태 변경 이벤트 발행
                auto state_event = std::make_shared<event::RTStateChangedEvent>(
                    state_machine_->stateToString(from),
                    state_machine_->stateToString(to),
                    state_machine_->eventToString(event)
                );
                event_bus_->publish(state_event);
            }
        );
    }

    state_machine_->handleEvent(RTEvent::START);

    spdlog::info("RTExecutive initialized: minor_cycle={}ms, major_cycle={}ms, slots={}",
                 minor_cycle_ms_, major_cycle_ms_, num_slots_);
}

std::unique_ptr<RTExecutive> RTExecutive::createFromPeriods(
    const std::vector<uint32_t>& periods_ms,
    std::shared_ptr<event::IEventBus> event_bus) {
    try {
        // 주기 배열로부터 스케줄 파라미터 계산
        auto params = util::calculate(periods_ms);

        spdlog::info("Creating RTExecutive from periods: minor={}ms, major={}ms, slots={}",
                     params.minor_cycle_ms, params.major_cycle_ms, params.num_slots);

        return std::make_unique<RTExecutive>(params.minor_cycle_ms, params.major_cycle_ms, event_bus);
    } catch (const std::exception& e) {
        spdlog::error("Failed to create RTExecutive from periods: {}", e.what());
        return nullptr;
    }
}

RTExecutive::~RTExecutive() {
    stop();

    // Production readiness: Clean up performance monitoring
    delete static_cast<mxrc::rt::perf::CPUAffinityManager*>(cpu_affinity_mgr_impl_);
    delete static_cast<mxrc::rt::perf::NUMABinding*>(numa_binding_impl_);
    delete static_cast<mxrc::rt::perf::PerfMonitor*>(perf_monitor_impl_);
}

// Production readiness: Helper methods for type-safe access
mxrc::rt::perf::CPUAffinityManager* RTExecutive::getCPUAffinityMgr() {
    return static_cast<mxrc::rt::perf::CPUAffinityManager*>(cpu_affinity_mgr_impl_);
}

mxrc::rt::perf::NUMABinding* RTExecutive::getNUMABinding() {
    return static_cast<mxrc::rt::perf::NUMABinding*>(numa_binding_impl_);
}

mxrc::rt::perf::PerfMonitor* RTExecutive::getPerfMonitorImpl() {
    return static_cast<mxrc::rt::perf::PerfMonitor*>(perf_monitor_impl_);
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
        // Production readiness: Start cycle performance monitoring
        auto* perf_monitor = getPerfMonitorImpl();
        if (perf_monitor) {
            perf_monitor->startCycle();
        }

        // Update context
        context_.current_slot = current_slot_;
        context_.cycle_count = cycle_count_;
        context_.timestamp_ns = cycle_start_ns;

        // Check heartbeat (매 사이클 - 1ms)
        checkHeartbeat();

        // Execute actions for current slot
        executeSlot(current_slot_);

        // Production readiness: End cycle performance monitoring
        if (perf_monitor) {
            perf_monitor->endCycle();

            // Update RTMetrics periodically (every 1000 cycles)
            if (rt_metrics_ && cycle_count_ % 1000 == 0) {
                auto stats = perf_monitor->getStats();

                // Update performance metrics
                rt_metrics_->updatePerfPercentiles(
                    stats.p50_latency / 1000000.0,  // us to seconds
                    stats.p95_latency / 1000000.0,
                    stats.p99_latency / 1000000.0
                );
                rt_metrics_->updatePerfJitter(stats.jitter / 1000000.0);
                rt_metrics_->updatePerfDeadlineMissRate(stats.deadline_miss_rate);

                // Update NUMA statistics if available
                auto* numa_binding = getNUMABinding();
                if (numa_binding) {
                    auto numa_stats = numa_binding->getStats();
                    rt_metrics_->updateNUMAStats(
                        numa_stats.local_pages,
                        numa_stats.remote_pages,
                        numa_stats.local_access_percent
                    );
                }
            }

            // Check for deadline miss
            if (perf_monitor->didMissDeadline() && rt_metrics_) {
                rt_metrics_->incrementPerfDeadlineMisses();
            }
        }

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

int RTExecutive::registerAction(const std::string& name, uint32_t period_ms, ActionCallback callback,
                                GuardCondition guard) {
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
        ActionSlot action{name, period_ms, callback, guard, slot_interval};
        schedule_[slot].push_back(action);
    }

    spdlog::info("Registered action '{}' with period {}ms (slot interval: {})",
                 name, period_ms, slot_interval);
    return 0;
}

void RTExecutive::executeSlot(uint32_t slot) {
    for (auto& action : schedule_[slot]) {
        // Check guard condition first
        if (action.guard) {
            if (!action.guard(*state_machine_)) {
                spdlog::trace("Skipping action '{}' - guard condition failed", action.name);
                continue;
            }
        }

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

void RTExecutive::setSharedMemory(void* shared_mem_ptr) {
    shared_memory_ptr_ = shared_mem_ptr;
    last_heartbeat_check_ns_ = util::getMonotonicTimeNs();
    spdlog::info("Shared memory attached for heartbeat monitoring");
}

void RTExecutive::checkHeartbeat() {
    if (!heartbeat_monitoring_enabled_ || !shared_memory_ptr_) {
        return;
    }

    auto* shm_data = static_cast<ipc::SharedMemoryData*>(shared_memory_ptr_);
    uint64_t now_ns = util::getMonotonicTimeNs();

    // Check Non-RT heartbeat
    uint64_t nonrt_hb_ns = shm_data->nonrt_heartbeat_ns.load(std::memory_order_acquire);
    uint64_t time_since_last_hb = now_ns - nonrt_hb_ns;

    if (time_since_last_hb > ipc::SharedMemoryData::HEARTBEAT_TIMEOUT_NS) {
        // Heartbeat 실패 - SAFE_MODE 진입
        if (state_machine_->getState() == RTState::RUNNING) {
            spdlog::warn("Non-RT heartbeat lost (timeout: {} ms), entering SAFE_MODE",
                        time_since_last_hb / 1'000'000);

            // SAFE_MODE 진입 시각 기록
            safe_mode_enter_time_ns_ = now_ns;

            // SAFE_MODE 진입 이벤트 발행
            if (event_bus_) {
                auto event = std::make_shared<event::RTSafeModeEnteredEvent>(
                    time_since_last_hb / 1'000'000,  // ms
                    "Non-RT heartbeat timeout"
                );
                event_bus_->publish(event);
            }

            state_machine_->handleEvent(RTEvent::SAFE_MODE_ENTER);
        }
    } else {
        // Heartbeat 정상 - SAFE_MODE에서 복구
        if (state_machine_->getState() == RTState::SAFE_MODE) {
            spdlog::info("Non-RT heartbeat recovered, exiting SAFE_MODE");

            // SAFE_MODE 복구 이벤트 발행
            if (event_bus_ && safe_mode_enter_time_ns_ > 0) {
                uint64_t downtime_ms = (now_ns - safe_mode_enter_time_ns_) / 1'000'000;
                auto event = std::make_shared<event::RTSafeModeExitedEvent>(downtime_ms);
                event_bus_->publish(event);
            }

            state_machine_->handleEvent(RTEvent::SAFE_MODE_EXIT);
            safe_mode_enter_time_ns_ = 0;
        }
    }

    // Update RT heartbeat (1ms마다)
    shm_data->rt_heartbeat_ns.store(now_ns, std::memory_order_release);
}

// Production readiness: Register initialization hook
void RTExecutive::registerInitializationHook(const std::string& name, InitializationHook hook) {
    spdlog::info("RTExecutive: Registering initialization hook: {}", name);
    init_hooks_.push_back({name, std::move(hook)});
}

// Production readiness: Execute all initialization hooks
void RTExecutive::executeInitializationHooks() {
    spdlog::info("RTExecutive: Executing {} initialization hooks", init_hooks_.size());
    for (const auto& hook : init_hooks_) {
        spdlog::debug("RTExecutive: Executing hook: {}", hook.name);
        try {
            hook.hook();
            spdlog::info("RTExecutive: Hook '{}' completed successfully", hook.name);
        } catch (const std::exception& e) {
            spdlog::error("RTExecutive: Hook '{}' failed: {}", hook.name, e.what());
            throw;  // Re-throw to prevent RT cycle start on failure
        }
    }
    spdlog::info("RTExecutive: All initialization hooks completed");
}

// Production readiness: Performance monitoring configuration

void RTExecutive::setRTMetrics(RTMetrics* metrics) {
    rt_metrics_ = metrics;
    spdlog::info("RTExecutive: RTMetrics configured for performance monitoring");
}

bool RTExecutive::configureCPUAffinity(const std::string& config_path) {
    // Just load the config for now
    // The actual application will happen during executeInitializationHooks()
    spdlog::info("RTExecutive: CPU affinity config loaded from {}", config_path);
    spdlog::warn("RTExecutive: Note - CPU affinity must be manually applied using initialization hooks");
    return true;
}

bool RTExecutive::configureNUMABinding(const std::string& config_path) {
    // Just note the config for now
    // The actual application will happen during executeInitializationHooks()
    spdlog::info("RTExecutive: NUMA binding config path set: {}", config_path);
    spdlog::warn("RTExecutive: Note - NUMA binding must be manually applied using initialization hooks");
    return true;
}

bool RTExecutive::configurePerfMonitor(const std::string& config_path) {
    // Just note the config for now
    spdlog::info("RTExecutive: Performance monitor config path set: {}", config_path);
    return true;
}

} // namespace rt
} // namespace core
} // namespace mxrc
