#pragma once

#include <cstdint>

namespace mxrc {
namespace core {
namespace rt {
namespace util {

// 실시간 스케줄링 우선순위 설정
// policy: SCHED_FIFO, SCHED_RR 등
// priority: 1-99 (높을수록 우선순위 높음)
int setPriority(int policy, int priority);

// 현재 스레드를 특정 CPU 코어에 고정
// core_id: CPU 코어 번호 (0부터 시작)
int pinToCPU(int core_id);

// 모든 메모리 페이지 잠금 (페이징 방지)
int lockMemory();

// 단조 시간 (나노초)
uint64_t getMonotonicTimeNs();

// 절대 시간까지 대기
// wakeup_time_ns: 깨어날 절대 시간 (나노초)
int waitUntilAbsoluteTime(uint64_t wakeup_time_ns);

} // namespace util
} // namespace rt
} // namespace core
} // namespace mxrc
