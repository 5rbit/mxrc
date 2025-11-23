#pragma once

#include <cstdint>
#include <string>

namespace mxrc {
namespace ethercat {

// EtherCAT 통신 통계
struct EtherCATStatistics {
    uint64_t total_cycles;          // 총 cycle 수
    uint64_t send_errors;           // 전송 에러 수
    uint64_t receive_errors;        // 수신 에러 수
    uint64_t frame_count;           // 총 프레임 수
    double error_rate;              // 에러율 (0.0 ~ 1.0)
    uint64_t latency_ns;            // 평균 latency (nanoseconds)
    uint64_t max_latency_ns;        // 최대 latency
    uint64_t min_latency_ns;        // 최소 latency

    EtherCATStatistics()
        : total_cycles(0)
        , send_errors(0)
        , receive_errors(0)
        , frame_count(0)
        , error_rate(0.0)
        , latency_ns(0)
        , max_latency_ns(0)
        , min_latency_ns(UINT64_MAX) {}
};

// EtherCAT Logger 유틸리티
// 통신 통계 수집 및 로깅
class EtherCATLogger {
public:
    EtherCATLogger();
    ~EtherCATLogger() = default;

    // Cycle 시작 (latency 측정용)
    void cycleStart();

    // Cycle 종료 (latency 계산 및 통계 업데이트)
    void cycleEnd();

    // 에러 기록
    void logSendError();
    void logReceiveError();

    // 통계 조회
    const EtherCATStatistics& getStatistics() const { return stats_; }

    // 통계 리셋
    void resetStatistics();

    // 통계 출력 (주기적 로깅용)
    void printStatistics() const;

private:
    EtherCATStatistics stats_;
    uint64_t cycle_start_ns_;

    // 현재 시간 조회 (nanoseconds)
    uint64_t getNowNs() const;
};

} // namespace ethercat
} // namespace mxrc
