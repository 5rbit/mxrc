// RTMetrics.h - RT 프로세스 메트릭 관리
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_CORE_RT_RTMETRICS_H
#define MXRC_CORE_RT_RTMETRICS_H

#include "core/monitoring/MetricsCollector.h"
#include "RTStateMachine.h"
#include <memory>
#include <string>

namespace mxrc::core::rt {

/**
 * @brief RT 프로세스 메트릭 수집기
 *
 * RTExecutive, RTStateMachine, RTDataStore의 메트릭을 관리합니다.
 */
class RTMetrics {
private:
    std::shared_ptr<monitoring::MetricsCollector> collector_;

    // RT Cycle 메트릭
    std::shared_ptr<monitoring::Histogram> cycle_duration_minor_;
    std::shared_ptr<monitoring::Histogram> cycle_duration_major_;
    std::shared_ptr<monitoring::Histogram> cycle_jitter_;
    std::shared_ptr<monitoring::Counter> deadline_misses_;

    // State Machine 메트릭
    std::shared_ptr<monitoring::Gauge> current_state_;
    std::shared_ptr<monitoring::Counter> state_transitions_;
    std::shared_ptr<monitoring::Counter> safe_mode_entries_;

    // Heartbeat 메트릭
    std::shared_ptr<monitoring::Gauge> nonrt_heartbeat_alive_;
    std::shared_ptr<monitoring::Gauge> nonrt_heartbeat_timeout_seconds_;

    // DataStore 메트릭 (동적 생성)
    // key별로 writes_total, reads_total, seqlock_retries_total

    // Production readiness: NUMA metrics
    std::shared_ptr<monitoring::Gauge> numa_local_pages_;
    std::shared_ptr<monitoring::Gauge> numa_remote_pages_;
    std::shared_ptr<monitoring::Gauge> numa_local_access_percent_;

    // Production readiness: Performance monitoring metrics
    std::shared_ptr<monitoring::Histogram> perf_latency_;
    std::shared_ptr<monitoring::Gauge> perf_p50_latency_;
    std::shared_ptr<monitoring::Gauge> perf_p95_latency_;
    std::shared_ptr<monitoring::Gauge> perf_p99_latency_;
    std::shared_ptr<monitoring::Gauge> perf_jitter_;
    std::shared_ptr<monitoring::Counter> perf_deadline_misses_;
    std::shared_ptr<monitoring::Gauge> perf_deadline_miss_rate_;

public:
    /**
     * @brief RTMetrics 생성자
     *
     * @param collector 메트릭 수집기
     */
    explicit RTMetrics(std::shared_ptr<monitoring::MetricsCollector> collector);

    /**
     * @brief Minor cycle 실행 시간 기록
     *
     * @param duration_seconds 실행 시간 (초)
     */
    void recordMinorCycleDuration(double duration_seconds);

    /**
     * @brief Major cycle 실행 시간 기록
     *
     * @param duration_seconds 실행 시간 (초)
     */
    void recordMajorCycleDuration(double duration_seconds);

    /**
     * @brief Cycle jitter 기록
     *
     * @param jitter_seconds 지터 (초)
     */
    void recordCycleJitter(double jitter_seconds);

    /**
     * @brief Deadline miss 카운트 증가
     */
    void incrementDeadlineMisses();

    /**
     * @brief 현재 상태 업데이트
     *
     * @param state RT 상태
     */
    void updateState(RTState state);

    /**
     * @brief 상태 전이 카운트 증가
     */
    void incrementStateTransitions();

    /**
     * @brief SAFE_MODE 진입 카운트 증가
     */
    void incrementSafeModeEntries();

    /**
     * @brief Non-RT heartbeat 상태 업데이트
     *
     * @param alive true이면 연결, false이면 끊김
     */
    void updateNonRTHeartbeatAlive(bool alive);

    /**
     * @brief Non-RT heartbeat timeout 설정
     *
     * @param timeout_seconds timeout 시간 (초)
     */
    void updateNonRTHeartbeatTimeout(double timeout_seconds);

    /**
     * @brief DataStore 쓰기 카운트 증가
     *
     * @param key 데이터 키 이름
     */
    void incrementDataStoreWrites(const std::string& key);

    /**
     * @brief DataStore 읽기 카운트 증가
     *
     * @param key 데이터 키 이름
     */
    void incrementDataStoreReads(const std::string& key);

    /**
     * @brief DataStore Seqlock 재시도 카운트 증가
     *
     * @param key 데이터 키 이름
     */
    void incrementDataStoreSeqlockRetries(const std::string& key);

    /**
     * @brief 메트릭 수집기 반환
     *
     * @return 메트릭 수집기
     */
    std::shared_ptr<monitoring::MetricsCollector> getCollector() const {
        return collector_;
    }

    // Production readiness: NUMA monitoring methods

    /**
     * @brief Update NUMA memory statistics
     *
     * @param local_pages Number of pages allocated on local NUMA node
     * @param remote_pages Number of pages allocated on remote NUMA nodes
     * @param local_access_percent Percentage of local NUMA access
     */
    void updateNUMAStats(uint64_t local_pages, uint64_t remote_pages, double local_access_percent);

    // Production readiness: Performance monitoring methods

    /**
     * @brief Record performance latency sample
     *
     * @param latency_seconds Latency in seconds
     */
    void recordPerfLatency(double latency_seconds);

    /**
     * @brief Update performance percentile metrics
     *
     * @param p50_seconds P50 latency in seconds
     * @param p95_seconds P95 latency in seconds
     * @param p99_seconds P99 latency in seconds
     */
    void updatePerfPercentiles(double p50_seconds, double p95_seconds, double p99_seconds);

    /**
     * @brief Update performance jitter
     *
     * @param jitter_seconds Jitter (standard deviation) in seconds
     */
    void updatePerfJitter(double jitter_seconds);

    /**
     * @brief Increment performance deadline miss counter
     */
    void incrementPerfDeadlineMisses();

    /**
     * @brief Update performance deadline miss rate
     *
     * @param miss_rate_percent Deadline miss rate as percentage
     */
    void updatePerfDeadlineMissRate(double miss_rate_percent);
};

} // namespace mxrc::core::rt

#endif // MXRC_CORE_RT_RTMETRICS_H
