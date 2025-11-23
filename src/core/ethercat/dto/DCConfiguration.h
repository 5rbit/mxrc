#pragma once

#include <cstdint>

namespace mxrc {
namespace ethercat {

// Distributed Clock (DC) 설정
struct DCConfiguration {
    bool enable;                    // DC 동기화 활성화 여부
    uint16_t reference_slave;       // 기준 clock slave (보통 첫 번째 장치)
    uint32_t sync0_cycle_time;      // SYNC0 주기 (nanoseconds)
    int32_t sync0_shift_time;       // SYNC0 shift (nanoseconds)
    uint32_t sync1_cycle_time;      // SYNC1 주기 (nanoseconds, 0 = 비활성화)

    DCConfiguration()
        : enable(false)
        , reference_slave(0)
        , sync0_cycle_time(10000000)    // 기본 10ms
        , sync0_shift_time(0)
        , sync1_cycle_time(0) {}
};

} // namespace ethercat
} // namespace mxrc
