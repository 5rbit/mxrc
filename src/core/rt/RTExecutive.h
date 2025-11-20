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
namespace rt {

// Forward declarations
class RTDataStore;
class RTStateMachine;

// 실시간 주기 실행기
// SCHED_FIFO 우선순위와 절대 시간 기반 대기로 jitter 최소화
class RTExecutive {
public:
    using ActionCallback = std::function<void(RTContext&)>;

    // minor_cycle_ms: 최소 주기 (ms)
    // major_cycle_ms: 전체 프레임 크기 (ms)
    RTExecutive(uint32_t minor_cycle_ms, uint32_t major_cycle_ms);

    // 주기 배열로부터 동적 초기화
    // periods_ms: 등록할 action들의 주기 배열
    // 반환: 성공 0, 실패 -1
    static RTExecutive* createFromPeriods(const std::vector<uint32_t>& periods_ms);

    ~RTExecutive();

    // 실시간 주기 실행 시작
    // 반환: 성공 0, 실패 -1
    int run();

    // 실행 중지
    void stop();

    // 주기적으로 실행할 action 등록
    // period_ms: 실행 주기 (ms), minor_cycle의 배수여야 함
    // 반환: 성공 0, 실패 -1
    int registerAction(const std::string& name, uint32_t period_ms, ActionCallback callback);

    // RTDataStore 설정
    void setDataStore(RTDataStore* data_store);

    // 스케줄 파라미터 조회
    uint32_t getMinorCycleMs() const { return minor_cycle_ms_; }
    uint32_t getMajorCycleMs() const { return major_cycle_ms_; }
    uint32_t getNumSlots() const { return num_slots_; }

private:
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

    // Action storage
    struct ActionSlot {
        std::string name;
        uint32_t period_ms;
        ActionCallback callback;
        uint32_t next_slot;
    };
    std::vector<std::vector<ActionSlot>> schedule_;
};

} // namespace rt
} // namespace core
} // namespace mxrc
