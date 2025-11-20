#pragma once

#include <atomic>
#include <cstdint>

namespace mxrc {
namespace core {
namespace rt {
namespace ipc {

// RT/Non-RT 프로세스 간 공유 메모리 데이터 구조
// POSIX shared memory를 통해 공유됨
struct alignas(64) SharedMemoryData {
    // RT → Non-RT 데이터 (10ms 주기로 갱신)
    struct alignas(64) RTToNonRT {
        int32_t robot_mode;           // 로봇 모드 (0=IDLE, 1=RUNNING, 2=ERROR)
        float position_x;             // X 위치 (mm)
        float position_y;             // Y 위치 (mm)
        float velocity;               // 속도 (mm/s)
        uint64_t timestamp_ns;        // 타임스탬프 (nanoseconds)
        uint32_t sequence;            // Sequence number (torn read 방지)
    } rt_to_nonrt;

    // Non-RT → RT 데이터 (100ms 주기로 갱신)
    struct alignas(64) NonRTToRT {
        float max_velocity;           // 최대 속도 제한 (mm/s)
        float pid_kp;                 // PID 비례 게인
        float pid_ki;                 // PID 적분 게인
        float pid_kd;                 // PID 미분 게인
        uint64_t timestamp_ns;        // 타임스탬프 (nanoseconds)
        uint32_t sequence;            // Sequence number (torn read 방지)
    } nonrt_to_rt;

    // Heartbeat (1ms/100ms 주기로 갱신)
    std::atomic<uint64_t> rt_heartbeat_ns;      // RT 프로세스 heartbeat
    std::atomic<uint64_t> nonrt_heartbeat_ns;   // Non-RT 프로세스 heartbeat

    // 상수
    static constexpr uint64_t HEARTBEAT_TIMEOUT_NS = 500'000'000ULL;  // 500ms
};

} // namespace ipc
} // namespace rt
} // namespace core
} // namespace mxrc
