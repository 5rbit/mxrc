// EventStats.h - 이벤트 통계 추적
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_CORE_EVENT_UTIL_EVENTSTATS_H
#define MXRC_CORE_EVENT_UTIL_EVENTSTATS_H

#include <atomic>
#include <cstddef>

namespace mxrc::core::event {

/**
 * @brief 이벤트 시스템 통계 구조체
 *
 * EventBus의 성능 및 상태를 모니터링하기 위한 통계 카운터입니다.
 * 모든 필드는 atomic으로 스레드 안전합니다.
 */
struct EventStats {
    /// 발행된 총 이벤트 수
    std::atomic<uint64_t> publishedEvents{0};

    /// 성공적으로 처리된 이벤트 수
    std::atomic<uint64_t> processedEvents{0};

    /// 큐 오버플로우로 드롭된 이벤트 수
    std::atomic<uint64_t> droppedEvents{0};

    /// 구독자 콜백 실행 중 발생한 예외 수
    std::atomic<uint64_t> failedCallbacks{0};

    /// 현재 활성 구독자 수
    std::atomic<size_t> activeSubscriptions{0};

    /**
     * @brief 모든 통계 초기화
     */
    void reset() {
        publishedEvents.store(0, std::memory_order_relaxed);
        processedEvents.store(0, std::memory_order_relaxed);
        droppedEvents.store(0, std::memory_order_relaxed);
        failedCallbacks.store(0, std::memory_order_relaxed);
        activeSubscriptions.store(0, std::memory_order_relaxed);
    }
};

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_UTIL_EVENTSTATS_H
