#include "EtherCATLogger.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace mxrc {
namespace ethercat {

EtherCATLogger::EtherCATLogger()
    : cycle_start_ns_(0) {
}

void EtherCATLogger::cycleStart() {
    cycle_start_ns_ = getNowNs();
}

void EtherCATLogger::cycleEnd() {
    uint64_t now_ns = getNowNs();
    uint64_t latency_ns = now_ns - cycle_start_ns_;

    // 통계 업데이트
    stats_.total_cycles++;
    stats_.frame_count++;

    // Latency 통계
    stats_.latency_ns = (stats_.latency_ns * (stats_.total_cycles - 1) + latency_ns) / stats_.total_cycles;

    if (latency_ns > stats_.max_latency_ns) {
        stats_.max_latency_ns = latency_ns;
    }
    if (latency_ns < stats_.min_latency_ns) {
        stats_.min_latency_ns = latency_ns;
    }

    // 에러율 계산
    uint64_t total_errors = stats_.send_errors + stats_.receive_errors;
    stats_.error_rate = static_cast<double>(total_errors) / static_cast<double>(stats_.total_cycles);
}

void EtherCATLogger::logSendError() {
    stats_.send_errors++;
    spdlog::error("EtherCAT send 에러 (총 {}건)", stats_.send_errors);
}

void EtherCATLogger::logReceiveError() {
    stats_.receive_errors++;
    spdlog::error("EtherCAT receive 에러 (총 {}건)", stats_.receive_errors);
}

void EtherCATLogger::resetStatistics() {
    stats_ = EtherCATStatistics();
    spdlog::info("EtherCAT 통계 리셋");
}

void EtherCATLogger::printStatistics() const {
    spdlog::info("=== EtherCAT 통계 ===");
    spdlog::info("Total Cycles: {}", stats_.total_cycles);
    spdlog::info("Frame Count: {}", stats_.frame_count);
    spdlog::info("Send Errors: {}", stats_.send_errors);
    spdlog::info("Receive Errors: {}", stats_.receive_errors);
    spdlog::info("Error Rate: {:.4f}%", stats_.error_rate * 100.0);
    spdlog::info("Avg Latency: {} ns ({:.3f} us)", stats_.latency_ns, stats_.latency_ns / 1000.0);
    spdlog::info("Max Latency: {} ns ({:.3f} us)", stats_.max_latency_ns, stats_.max_latency_ns / 1000.0);
    spdlog::info("Min Latency: {} ns ({:.3f} us)",
                 stats_.min_latency_ns == UINT64_MAX ? 0 : stats_.min_latency_ns,
                 stats_.min_latency_ns == UINT64_MAX ? 0.0 : stats_.min_latency_ns / 1000.0);
}

uint64_t EtherCATLogger::getNowNs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

} // namespace ethercat
} // namespace mxrc
