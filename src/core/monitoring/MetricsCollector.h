// MetricsCollector.h - 메트릭 수집 및 Prometheus 내보내기
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_CORE_MONITORING_METRICSCOLLECTOR_H
#define MXRC_CORE_MONITORING_METRICSCOLLECTOR_H

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>

namespace mxrc::core::monitoring {

/**
 * @brief 메트릭 타입
 */
enum class MetricType {
    COUNTER,   ///< 단조 증가 카운터
    GAUGE,     ///< 임의의 값을 가질 수 있는 게이지
    HISTOGRAM  ///< 값의 분포를 추적하는 히스토그램
};

/**
 * @brief 메트릭 라벨
 */
using Labels = std::map<std::string, std::string>;

/**
 * @brief Counter 메트릭
 */
class Counter {
private:
    std::atomic<uint64_t> value_{0};

public:
    void increment(uint64_t delta = 1) {
        value_.fetch_add(delta, std::memory_order_relaxed);
    }

    uint64_t get() const {
        return value_.load(std::memory_order_relaxed);
    }

    void reset() {
        value_.store(0, std::memory_order_relaxed);
    }
};

/**
 * @brief Gauge 메트릭
 */
class Gauge {
private:
    std::atomic<double> value_{0.0};

public:
    void set(double value) {
        // atomic<double>은 없으므로 비트 단위로 저장
        auto bits = *reinterpret_cast<uint64_t*>(&value);
        reinterpret_cast<std::atomic<uint64_t>*>(&value_)->store(bits, std::memory_order_relaxed);
    }

    double get() const {
        auto bits = reinterpret_cast<const std::atomic<uint64_t>*>(&value_)->load(std::memory_order_relaxed);
        return *reinterpret_cast<const double*>(&bits);
    }

    void increment(double delta = 1.0) {
        set(get() + delta);
    }

    void decrement(double delta = 1.0) {
        set(get() - delta);
    }
};

/**
 * @brief Histogram 메트릭
 *
 * 간단한 히스토그램 구현 (sum, count, buckets)
 */
class Histogram {
private:
    mutable std::mutex mutex_;
    double sum_{0.0};
    uint64_t count_{0};
    std::vector<double> buckets_;  // bucket 경계값
    std::vector<uint64_t> bucket_counts_;

public:
    Histogram(const std::vector<double>& buckets);

    void observe(double value);

    double sum() const;
    uint64_t count() const;
    const std::vector<double>& buckets() const { return buckets_; }
    std::vector<uint64_t> bucketCounts() const;
};

/**
 * @brief 메트릭 레지스트리
 *
 * Thread-safe한 메트릭 수집 및 관리
 */
class MetricsCollector {
private:
    mutable std::mutex mutex_;

    // 메트릭 저장소
    std::map<std::string, std::map<Labels, std::shared_ptr<Counter>>> counters_;
    std::map<std::string, std::map<Labels, std::shared_ptr<Gauge>>> gauges_;
    std::map<std::string, std::map<Labels, std::shared_ptr<Histogram>>> histograms_;

    // 메트릭 도움말 텍스트
    std::map<std::string, std::string> help_texts_;

    std::string labelsToString(const Labels& labels) const;

public:
    MetricsCollector() = default;
    ~MetricsCollector() = default;

    // 복사 금지
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    /**
     * @brief Counter 가져오기 또는 생성
     */
    std::shared_ptr<Counter> getOrCreateCounter(
        const std::string& name,
        const Labels& labels = {},
        const std::string& help = "");

    /**
     * @brief Gauge 가져오기 또는 생성
     */
    std::shared_ptr<Gauge> getOrCreateGauge(
        const std::string& name,
        const Labels& labels = {},
        const std::string& help = "");

    /**
     * @brief Histogram 가져오기 또는 생성
     */
    std::shared_ptr<Histogram> getOrCreateHistogram(
        const std::string& name,
        const Labels& labels = {},
        const std::vector<double>& buckets = {0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0, 5.0, 10.0},
        const std::string& help = "");

    /**
     * @brief Counter 증가 (편의 메서드)
     */
    void incrementCounter(
        const std::string& name,
        const Labels& labels = {},
        uint64_t delta = 1);

    /**
     * @brief Gauge 설정 (편의 메서드)
     */
    void setGauge(
        const std::string& name,
        double value,
        const Labels& labels = {});

    /**
     * @brief Histogram 관찰 (편의 메서드)
     */
    void observeHistogram(
        const std::string& name,
        double value,
        const Labels& labels = {});

    /**
     * @brief Prometheus 포맷으로 내보내기
     *
     * @return Prometheus 텍스트 포맷 메트릭
     */
    std::string exportPrometheus() const;

    /**
     * @brief 모든 메트릭 초기화
     */
    void reset();
};

/**
 * @brief RAII 타이머 - Histogram에 실행 시간 자동 기록
 */
class ScopedTimer {
private:
    std::shared_ptr<Histogram> histogram_;
    std::chrono::steady_clock::time_point start_;

public:
    explicit ScopedTimer(std::shared_ptr<Histogram> histogram)
        : histogram_(std::move(histogram))
        , start_(std::chrono::steady_clock::now()) {}

    ~ScopedTimer() {
        if (histogram_) {
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration<double>(end - start_).count();
            histogram_->observe(duration);
        }
    }

    // 복사 금지
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
};

} // namespace mxrc::core::monitoring

#endif // MXRC_CORE_MONITORING_METRICSCOLLECTOR_H
