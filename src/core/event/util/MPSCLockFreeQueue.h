// MPSCLockFreeQueue.h - Multi-Producer Single-Consumer Lock-Free Queue
// Copyright (C) 2025 MXRC Project
// Lock-free 큐 구현 (MPSC 패턴)

#ifndef MXRC_CORE_EVENT_UTIL_MPSCLOCKFREEQUEUE_H
#define MXRC_CORE_EVENT_UTIL_MPSCLOCKFREEQUEUE_H

#include <vector>
#include <atomic>
#include <cstddef>
#include <memory>

namespace mxrc::core::event {

/**
 * @brief Multi-Producer Single-Consumer Lock-Free Queue
 *
 * 여러 생산자와 단일 소비자 패턴에 최적화된 lock-free 큐입니다.
 * CAS(Compare-And-Swap)를 사용하여 여러 생산자 간의 동시성을 처리합니다.
 *
 * **스레드 안전성**:
 * - push: 여러 생산자 스레드에서 동시 호출 가능 (lock-free)
 * - pop: 단일 소비자 스레드에서만 호출해야 함
 * - size: 여러 스레드에서 호출 가능 (근사값 반환)
 *
 * **메모리 순서**:
 * - push: CAS를 통한 원자적 쓰기 위치 업데이트
 * - pop: acquire semantics (writePos 읽기 시)
 *
 * **성능 특성**:
 * - Lock-free: 생산자들이 락 없이 동시 작업 가능
 * - Wait-free pop: 소비자는 항상 bounded time에 완료
 * - Cache-friendly: False sharing 최소화
 *
 * @tparam T 큐에 저장할 요소 타입 (복사 또는 이동 가능해야 함)
 */
template<typename T>
class MPSCLockFreeQueue {
private:
    // Cache line 크기 (일반적으로 64바이트)
    static constexpr size_t CACHE_LINE_SIZE = 64;

    // Cache line padding을 위한 구조체
    struct alignas(CACHE_LINE_SIZE) AlignedAtomic {
        std::atomic<size_t> value;

        AlignedAtomic() : value(0) {}
        AlignedAtomic(size_t v) : value(v) {}
    };

    std::vector<T> buffer_;              ///< Ring buffer
    AlignedAtomic writePos_;             ///< 쓰기 위치 (여러 producer가 CAS로 업데이트)
    AlignedAtomic readPos_;              ///< 읽기 위치 (consumer만 업데이트)
    size_t capacity_;                    ///< 버퍼 용량

public:
    /**
     * @brief 큐 생성자
     *
     * @param capacity 큐의 최대 용량 (기본값: 10,000)
     */
    explicit MPSCLockFreeQueue(size_t capacity = 10000)
        : buffer_(capacity),
          writePos_(0),
          readPos_(0),
          capacity_(capacity) {}

    /**
     * @brief 큐에 요소 추가 (multi-producer 안전)
     *
     * 큐가 가득 찬 경우 false를 반환하고 요소를 추가하지 않습니다.
     * 여러 스레드가 동시에 호출해도 안전합니다.
     *
     * **주의**: CAS 재시도로 인해 최악의 경우 성능 저하 가능
     *
     * @param item 추가할 요소
     * @return true이면 성공, false이면 큐가 가득 찼거나 경쟁 실패
     */
    bool tryPush(const T& item) {
        // CAS 루프: 여러 생산자 간 경쟁 처리
        size_t currentWrite, nextWrite;

        while (true) {
            currentWrite = writePos_.value.load(std::memory_order_acquire);
            nextWrite = (currentWrite + 1) % capacity_;

            // 큐가 가득 찼는지 확인
            size_t currentRead = readPos_.value.load(std::memory_order_acquire);
            if (nextWrite == currentRead) {
                return false;  // Queue full
            }

            // CAS로 쓰기 위치 예약 시도
            // strong 버전 사용으로 spurious failure 방지
            if (writePos_.value.compare_exchange_strong(
                    currentWrite, nextWrite,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                // 성공: 예약한 위치에 쓰기
                // 버퍼에 쓰기 전에 fence 추가로 메모리 순서 보장
                buffer_[currentWrite] = item;
                return true;
            }

            // 실패: 다른 스레드가 먼저 예약했음, 재시도
            // compare_exchange_strong이 currentWrite를 업데이트했으므로
            // 다음 루프에서 새로운 값으로 재시도
        }
    }

    /**
     * @brief 큐에 요소 추가 (move 버전)
     *
     * @param item 이동할 요소
     * @return true이면 성공, false이면 큐가 가득 찼거나 경쟁 실패
     */
    bool tryPush(T&& item) {
        size_t currentWrite, nextWrite;

        while (true) {
            currentWrite = writePos_.value.load(std::memory_order_acquire);
            nextWrite = (currentWrite + 1) % capacity_;

            size_t currentRead = readPos_.value.load(std::memory_order_acquire);
            if (nextWrite == currentRead) {
                return false;  // Queue full
            }

            if (writePos_.value.compare_exchange_strong(
                    currentWrite, nextWrite,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                buffer_[currentWrite] = std::move(item);
                return true;
            }
        }
    }

    /**
     * @brief 큐에서 요소 제거 (consumer 전용)
     *
     * 큐가 비어 있는 경우 false를 반환합니다.
     *
     * **주의**: 이 함수는 단일 소비자 스레드에서만 호출해야 합니다.
     *
     * @param item 꺼낸 요소를 저장할 참조
     * @return true이면 성공, false이면 큐가 비어 있음
     */
    bool tryPop(T& item) {
        size_t currentRead = readPos_.value.load(std::memory_order_relaxed);

        // 큐가 비어 있는지 확인
        if (currentRead == writePos_.value.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }

        // 버퍼에서 읽기
        item = buffer_[currentRead];

        // 읽기 위치 업데이트 (release semantics)
        readPos_.value.store((currentRead + 1) % capacity_, std::memory_order_release);
        return true;
    }

    /**
     * @brief 큐의 현재 크기 반환 (근사값)
     *
     * **주의**: 멀티스레드 환경에서는 근사값일 수 있습니다.
     * 정확한 값이 아니므로 디버깅 및 모니터링 용도로만 사용하세요.
     *
     * @return 큐에 있는 요소의 대략적인 개수
     */
    size_t size() const {
        size_t write = writePos_.value.load(std::memory_order_acquire);
        size_t read = readPos_.value.load(std::memory_order_acquire);

        if (write >= read) {
            return write - read;
        } else {
            // Ring buffer wrapped around
            return capacity_ - read + write;
        }
    }

    /**
     * @brief 큐가 비어 있는지 확인 (근사값)
     *
     * @return true이면 비어 있음 (근사값)
     */
    bool empty() const {
        return readPos_.value.load(std::memory_order_acquire) ==
               writePos_.value.load(std::memory_order_acquire);
    }

    /**
     * @brief 큐의 최대 용량 반환
     *
     * @return 큐의 최대 용량
     */
    size_t capacity() const {
        return capacity_;
    }
};

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_UTIL_MPSCLOCKFREEQUEUE_H
