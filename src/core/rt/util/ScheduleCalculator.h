#pragma once

#include <cstdint>
#include <vector>

namespace mxrc {
namespace core {
namespace rt {
namespace util {

// 주기 배열에서 계산된 스케줄 파라미터
struct ScheduleParams {
    uint32_t minor_cycle_ms;  // 최소 주기 (모든 주기의 GCD)
    uint32_t major_cycle_ms;  // 전체 프레임 (모든 주기의 LCM)
    uint32_t num_slots;       // 슬롯 개수 (major / minor)
};

// 주기 배열로부터 스케줄 파라미터 계산
// GCD (minor cycle), LCM (major cycle) 계산 및 검증
// 예외: 주기가 유효하지 않거나 LCM이 MAX를 초과하면 invalid_argument
ScheduleParams calculate(const std::vector<uint32_t>& periods_ms);

// 최대공약수
uint32_t gcd(uint32_t a, uint32_t b);

// 최소공배수
uint32_t lcm(uint32_t a, uint32_t b);

// 최대 허용 major cycle (ms)
constexpr uint32_t MAX_MAJOR_CYCLE_MS = 1000;

} // namespace util
} // namespace rt
} // namespace core
} // namespace mxrc
