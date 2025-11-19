// LockFreeQueue.h - Single-Producer Single-Consumer Lock-Free Queue
// Copyright (C) 2025 MXRC Project
// Lock-free 큐 구현 (SPSC 패턴)

#ifndef MXRC_CORE_EVENT_UTIL_LOCKFREEQUEUE_H
#define MXRC_CORE_EVENT_UTIL_LOCKFREEQUEUE_H

#include <vector>
#include <atomic>
#include <cstddef>

namespace mxrc::core::event {

/**
 * @brief Single-Producer Single-Consumer Lock-Free Queue
 *
 * 단일 생산자-단일 소비자 패턴에 최적화된 lock-free 큐입니다.
 * CAS(Compare-And-Swap) 없이 atomic load/store만 사용하여 고성능을 달성합니다.
 *
 * **스레드 안전성**:
 * - push: 단일 생산자 스레드에서만 호출해야 함
 * - pop: 단일 소비자 스레드에서만 호출해야 함
 * - size: 여러 스레드에서 호출 가능 (근사값 반환)
 *
 * **메모리 순서**:
 * - push: release semantics (writePos 업데이트 시)
 * - pop: acquire semantics (writePos 읽기 시)
 *
 * @tparam T 큐에 저장할 요소 타입 (복사 가능해야 함)
 */
template<typename T>
class SPSCLockFreeQueue {
private:
    std::vector<T> buffer_;              ///< Ring buffer
    std::atomic<size_t> writePos_;       ///< 쓰기 위치 (producer가 업데이트)
    std::atomic<size_t> readPos_;        ///< 읽기 위치 (consumer가 업데이트)
    size_t capacity_;                    ///< 버퍼 용량

public:
    /**
     * @brief 큐 생성자
     *
     * @param capacity 큐의 최대 용량 (기본값: 10,000)
     */
    explicit SPSCLockFreeQueue(size_t capacity = 10000)
        : buffer_(capacity),
          writePos_(0),
          readPos_(0),
          capacity_(capacity) {}

    /**
     * @brief 큐에 요소 추가 (producer 전용)
     *
     * 큐가 가득 찬 경우 false를 반환하고 요소를 추가하지 않습니다.
     *
     * **주의**: 이 함수는 단일 생산자 스레드에서만 호출해야 합니다.
     *
     * @param item 추가할 요소
     * @return true이면 성공, false이면 큐가 가득 참
     */
    bool tryPush(const T& item) {
        size_t currentWrite = writePos_.load(std::memory_order_relaxed);
        size_t nextWrite = (currentWrite + 1) % capacity_;

        // 큐가 가득 찼는지 확인
        if (nextWrite == readPos_.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }

        // 버퍼에 쓰기
        buffer_[currentWrite] = item;

        // 쓰기 위치 업데이트 (release semantics로 가시성 보장)
        writePos_.store(nextWrite, std::memory_order_release);
        return true;
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
        size_t currentRead = readPos_.load(std::memory_order_relaxed);

        // 큐가 비어 있는지 확인
        if (currentRead == writePos_.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }

        // 버퍼에서 읽기
        item = buffer_[currentRead];

        // 읽기 위치 업데이트 (release semantics)
        readPos_.store((currentRead + 1) % capacity_, std::memory_order_release);
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
        size_t write = writePos_.load(std::memory_order_acquire);
        size_t read = readPos_.load(std::memory_order_acquire);

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
        return readPos_.load(std::memory_order_acquire) ==
               writePos_.load(std::memory_order_acquire);
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

#endif // MXRC_CORE_EVENT_UTIL_LOCKFREEQUEUE_H
