#ifndef METRICS_COLLECTOR_H
#define METRICS_COLLECTOR_H

#include <atomic>
#include <map>
#include <string>
#include <cstdint>

namespace mxrc::core::datastore {

/**
 * @brief DataStore 성능 메트릭 수집 및 관리 클래스
 *
 * 책임:
 * - 연산 카운터 관리 (get, set, delete)
 * - 메모리 사용량 추적
 * - 메트릭 조회 및 리셋
 *
 * 특징:
 * - lock-free atomic 카운터 사용 (memory_order_relaxed)
 * - 멀티스레드 환경에서 안전한 동시 접근
 * - 성능 오버헤드 최소화
 *
 * 성능 목표:
 * - 카운터 증가: O(1) lock-free
 * - 메트릭 조회: O(1) atomic load
 * - 스레드 경합 없음
 */
class MetricsCollector {
public:
    /**
     * @brief 생성자 (모든 카운터를 0으로 초기화)
     */
    MetricsCollector() = default;

    /**
     * @brief 소멸자 (RAII 원칙)
     */
    ~MetricsCollector() = default;

    // 복사 및 이동 금지 (unique ownership)
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;
    MetricsCollector(MetricsCollector&&) = delete;
    MetricsCollector& operator=(MetricsCollector&&) = delete;

    /**
     * @brief get() 호출 횟수 증가
     *
     * 시간 복잡도: O(1)
     * 스레드 안전: lock-free atomic fetch_add
     * 메모리 순서: memory_order_relaxed (성능 최적화)
     */
    void incrementGet();

    /**
     * @brief set() 호출 횟수 증가
     *
     * 시간 복잡도: O(1)
     * 스레드 안전: lock-free atomic fetch_add
     */
    void incrementSet();

    /**
     * @brief poll() 호출 횟수 증가
     *
     * 시간 복잡도: O(1)
     * 스레드 안전: lock-free atomic fetch_add
     */
    void incrementPoll();

    /**
     * @brief delete/remove 호출 횟수 증가
     *
     * 시간 복잡도: O(1)
     * 스레드 안전: lock-free atomic fetch_add
     */
    void incrementDelete();

    /**
     * @brief 메모리 사용량 업데이트 (바이트 단위)
     * @param delta 메모리 증감량 (양수: 증가, 음수: 감소)
     *
     * 시간 복잡도: O(1)
     * 스레드 안전: lock-free atomic fetch_add
     *
     * @note signed int64_t를 사용하여 음수 delta 지원
     */
    void updateMemoryUsage(int64_t delta);

    /**
     * @brief 모든 메트릭 조회
     * @return 메트릭 이름 -> 값 맵
     *
     * 반환 키:
     * - "get_calls": get() 호출 횟수
     * - "set_calls": set() 호출 횟수
     * - "poll_calls": poll() 호출 횟수
     * - "delete_calls": delete/remove 호출 횟수
     * - "memory_usage_bytes": 현재 메모리 사용량 (바이트)
     *
     * 시간 복잡도: O(1) - 4개 atomic load
     * 스레드 안전: lock-free atomic load
     * 메모리 순서: memory_order_relaxed
     */
    std::map<std::string, double> getMetrics() const;

    /**
     * @brief 모든 메트릭 리셋 (0으로 초기화)
     *
     * 시간 복잡도: O(1) - 4개 atomic store
     * 스레드 안전: lock-free atomic store
     *
     * @note 테스트 또는 새로운 모니터링 세션 시작 시 사용
     */
    void resetMetrics();

private:
    /**
     * @brief get() 호출 카운터
     * - lock-free atomic 연산
     * - uint64_t로 오버플로우 방지 (2^64 - 1까지 카운트 가능)
     */
    std::atomic<uint64_t> get_calls_{0};

    /**
     * @brief set() 호출 카운터
     */
    std::atomic<uint64_t> set_calls_{0};

    /**
     * @brief poll() 호출 카운터
     */
    std::atomic<uint64_t> poll_calls_{0};

    /**
     * @brief delete/remove 호출 카운터
     */
    std::atomic<uint64_t> delete_calls_{0};

    /**
     * @brief 메모리 사용량 (바이트)
     * - int64_t로 음수 delta 처리 가능
     * - 내부적으로 uint64_t로 저장 (reinterpret_cast 사용)
     */
    std::atomic<int64_t> memory_usage_bytes_{0};
};

} // namespace mxrc::core::datastore

#endif // METRICS_COLLECTOR_H
