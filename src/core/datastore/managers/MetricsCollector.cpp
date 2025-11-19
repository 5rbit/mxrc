#include "MetricsCollector.h"

namespace mxrc::core::datastore {

void MetricsCollector::incrementGet() {
    // lock-free atomic 증가 (memory_order_relaxed)
    // 순서 보장 불필요 (단순 카운터 증가)
    get_calls_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsCollector::incrementSet() {
    set_calls_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsCollector::incrementPoll() {
    poll_calls_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsCollector::incrementDelete() {
    delete_calls_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsCollector::updateMemoryUsage(int64_t delta) {
    // signed delta를 atomic fetch_add로 처리
    // 음수 delta도 정상 동작 (2의 보수 연산)
    memory_usage_bytes_.fetch_add(delta, std::memory_order_relaxed);
}

std::map<std::string, double> MetricsCollector::getMetrics() const {
    // lock-free atomic load (memory_order_relaxed)
    // 순서 보장 불필요 (스냅샷 읽기)
    std::map<std::string, double> result;
    result["get_calls"] = static_cast<double>(get_calls_.load(std::memory_order_relaxed));
    result["set_calls"] = static_cast<double>(set_calls_.load(std::memory_order_relaxed));
    result["poll_calls"] = static_cast<double>(poll_calls_.load(std::memory_order_relaxed));
    result["delete_calls"] = static_cast<double>(delete_calls_.load(std::memory_order_relaxed));
    result["memory_usage_bytes"] = static_cast<double>(memory_usage_bytes_.load(std::memory_order_relaxed));
    return result;
}

void MetricsCollector::resetMetrics() {
    // lock-free atomic store (memory_order_relaxed)
    // 모든 카운터를 0으로 초기화
    get_calls_.store(0, std::memory_order_relaxed);
    set_calls_.store(0, std::memory_order_relaxed);
    poll_calls_.store(0, std::memory_order_relaxed);
    delete_calls_.store(0, std::memory_order_relaxed);
    memory_usage_bytes_.store(0, std::memory_order_relaxed);
}

} // namespace mxrc::core::datastore
