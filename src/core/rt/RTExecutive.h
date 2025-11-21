#pragma once

#include "RTContext.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <memory>

namespace mxrc {
namespace core {

namespace event {
class IEventBus;
}

namespace rt {

// Forward declarations
class RTDataStore;
class RTStateMachine;
class RTMetrics;
enum class RTState : uint8_t;

} // namespace rt
} // namespace core
} // namespace mxrc

// Forward declarations for mxrc::rt::perf
namespace mxrc {
namespace rt {
namespace perf {
class CPUAffinityManager;
class NUMABinding;
class PerfMonitor;
} // namespace perf
} // namespace rt
} // namespace mxrc

namespace mxrc {
namespace core {
namespace rt {

// 실시간 주기 실행기
// SCHED_FIFO 우선순위와 절대 시간 기반 대기로 jitter 최소화
class RTExecutive {
public:
    using ActionCallback = std::function<void(RTContext&)>;
    using GuardCondition = std::function<bool(const RTStateMachine&)>;
    using InitializationHook = std::function<void()>;  // Production readiness: init hook

    // minor_cycle_ms: 최소 주기 (ms)
    // major_cycle_ms: 전체 프레임 크기 (ms)
    // event_bus: EventBus (optional, nullptr이면 이벤트 발행하지 않음)
    RTExecutive(uint32_t minor_cycle_ms, uint32_t major_cycle_ms,
                std::shared_ptr<event::IEventBus> event_bus = nullptr);

    // 주기 배열로부터 동적 초기화
    // periods_ms: 등록할 action들의 주기 배열
    // 반환: 성공 0, 실패 -1
    static std::unique_ptr<RTExecutive> createFromPeriods(const std::vector<uint32_t>& periods_ms,
                                                           std::shared_ptr<event::IEventBus> event_bus = nullptr);

    ~RTExecutive();

    // 실시간 주기 실행 시작
    // 반환: 성공 0, 실패 -1
    int run();

    // 실행 중지
    void stop();

    // 주기적으로 실행할 action 등록
    // period_ms: 실행 주기 (ms), minor_cycle의 배수여야 함
    // guard: 실행 전 검증 조건 (optional, nullptr이면 항상 실행)
    // 반환: 성공 0, 실패 -1
    int registerAction(const std::string& name, uint32_t period_ms, ActionCallback callback,
                       GuardCondition guard = nullptr);

    // RTDataStore 설정
    void setDataStore(RTDataStore* data_store);

    // 스케줄 파라미터 조회
    uint32_t getMinorCycleMs() const { return minor_cycle_ms_; }
    uint32_t getMajorCycleMs() const { return major_cycle_ms_; }
    uint32_t getNumSlots() const { return num_slots_; }

    // 상태 머신 조회
    RTStateMachine* getStateMachine() { return state_machine_.get(); }
    const RTStateMachine* getStateMachine() const { return state_machine_.get(); }

    // Shared memory 설정 (heartbeat monitoring용)
    void setSharedMemory(void* shared_mem_ptr);

    // Heartbeat monitoring 활성화/비활성화
    void enableHeartbeatMonitoring(bool enable) { heartbeat_monitoring_enabled_ = enable; }

    // Production readiness: Register initialization hooks
    // Called before RT cycle starts, for CPU affinity/NUMA setup
    void registerInitializationHook(const std::string& name, InitializationHook hook);

    // Production readiness: Execute all initialization hooks
    // Called at the beginning of run() before RT loop
    void executeInitializationHooks();

    // Production readiness: Performance monitoring setup
    /**
     * @brief Set RTMetrics for performance monitoring
     * @param metrics RTMetrics instance (must outlive RTExecutive)
     */
    void setRTMetrics(RTMetrics* metrics);

    /**
     * @brief Configure CPU affinity from JSON file
     * @param config_path Path to cpu_affinity.json
     * @return true if successfully configured
     */
    bool configureCPUAffinity(const std::string& config_path);

    /**
     * @brief Configure NUMA binding from JSON file
     * @param config_path Path to numa_binding.json
     * @return true if successfully configured
     */
    bool configureNUMABinding(const std::string& config_path);

    /**
     * @brief Configure performance monitor from JSON file
     * @param config_path Path to perf_monitor.json
     * @return true if successfully configured
     */
    bool configurePerfMonitor(const std::string& config_path);

    /**
     * @brief Get performance monitor instance
     * @return PerfMonitor pointer (may be nullptr if not configured)
     */
    mxrc::rt::perf::PerfMonitor* getPerfMonitor() { return getPerfMonitorImpl(); }

private:
    // Non-RT Heartbeat 체크 및 SAFE_MODE 전환
    void checkHeartbeat();
    // 현재 슬롯의 모든 action 실행
    void executeSlot(uint32_t slot);

    // 다음 주기까지 대기
    int waitUntilNextCycle(uint64_t cycle_start_ns, uint64_t cycle_duration_ns);

    // Configuration
    uint32_t minor_cycle_ms_;
    uint32_t major_cycle_ms_;
    uint32_t num_slots_;

    // Runtime state
    std::atomic<bool> running_;
    uint32_t current_slot_;
    uint64_t cycle_count_;

    // RTContext for action callbacks
    RTContext context_;

    // State machine
    std::unique_ptr<RTStateMachine> state_machine_;

    // Shared memory for heartbeat monitoring
    void* shared_memory_ptr_;
    bool heartbeat_monitoring_enabled_;
    uint64_t last_heartbeat_check_ns_;
    uint64_t safe_mode_enter_time_ns_;  // SAFE_MODE 진입 시각

    // EventBus for publishing state change events
    std::shared_ptr<event::IEventBus> event_bus_;

    // Action storage
    struct ActionSlot {
        std::string name;
        uint32_t period_ms;
        ActionCallback callback;
        GuardCondition guard;  // Guard condition (nullptr = always execute)
        uint32_t next_slot;
    };
    std::vector<std::vector<ActionSlot>> schedule_;

    // Production readiness: Initialization hooks
    struct InitHook {
        std::string name;
        InitializationHook hook;
    };
    std::vector<InitHook> init_hooks_;

    // Production readiness: Performance monitoring
    // Using void* to avoid exposing implementation details in header
    void* cpu_affinity_mgr_impl_;
    void* numa_binding_impl_;
    void* perf_monitor_impl_;
    RTMetrics* rt_metrics_;  // Non-owning pointer, managed by caller

    // Helper methods for type-safe access
    mxrc::rt::perf::CPUAffinityManager* getCPUAffinityMgr();
    mxrc::rt::perf::NUMABinding* getNUMABinding();
    mxrc::rt::perf::PerfMonitor* getPerfMonitorImpl();
};

} // namespace rt
} // namespace core
} // namespace mxrc
