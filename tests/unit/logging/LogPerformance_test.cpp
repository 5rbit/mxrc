#include "gtest/gtest.h"
#include "core/logging/Log.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>
#include <filesystem>

namespace mxrc::core::logging {

class LogPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 로그 디렉토리가 없으면 생성
        std::filesystem::create_directories("logs");

        // 기존 로그 파일 삭제
        if (std::filesystem::exists("logs/mxrc.log")) {
            std::filesystem::remove("logs/mxrc.log");
        }

        // 비동기 로거 초기화
        initialize_async_logger();
    }

    void TearDown() override {
        shutdown_logger();
        spdlog::drop_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

// T019: 10μs 지연 테스트 (10,000회 로그 평균)
TEST_F(LogPerformanceTest, TenMicrosecondLatency) {
    // Given
    const int N = 10000;

    // When
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < N; i++) {
        spdlog::info("Performance test message {}", i);
    }

    auto end = std::chrono::high_resolution_clock::now();

    // Then
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    double avg_latency = static_cast<double>(duration_us) / N;

    std::cout << "Average log call latency: " << avg_latency << " μs" << std::endl;
    std::cout << "Total duration: " << duration_us << " μs for " << N << " calls" << std::endl;

    // 평균 지연 < 10μs
    EXPECT_LT(avg_latency, 10.0);
}

// T020: 1000Hz 제어 루프 오버헤드 테스트 (1% 미만)
TEST_F(LogPerformanceTest, ControlLoopOverhead) {
    // Given
    const int loop_count = 1000;  // 1초 분량 (1000Hz)
    const int target_period_us = 1000;  // 1ms = 1000μs

    // 로깅 없는 기준 루프 측정
    auto baseline_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < loop_count; i++) {
        std::this_thread::sleep_for(std::chrono::microseconds(target_period_us));
    }
    auto baseline_end = std::chrono::high_resolution_clock::now();
    auto baseline_duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
        baseline_end - baseline_start).count();

    // 로깅 있는 루프 측정
    auto logging_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < loop_count; i++) {
        spdlog::debug("Control loop iteration {}", i);
        std::this_thread::sleep_for(std::chrono::microseconds(target_period_us));
    }
    auto logging_end = std::chrono::high_resolution_clock::now();
    auto logging_duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
        logging_end - logging_start).count();

    // Then
    double overhead_percent = 100.0 * (logging_duration_us - baseline_duration_us) / baseline_duration_us;

    std::cout << "Baseline duration: " << baseline_duration_us << " μs" << std::endl;
    std::cout << "Logging duration: " << logging_duration_us << " μs" << std::endl;
    std::cout << "Overhead: " << overhead_percent << "%" << std::endl;

    // 오버헤드 < 1%
    EXPECT_LT(overhead_percent, 1.0);

    // 평균 루프 주기 < 1.01ms (1010μs)
    double avg_period_us = static_cast<double>(logging_duration_us) / loop_count;
    std::cout << "Average loop period with logging: " << avg_period_us << " μs" << std::endl;
    EXPECT_LT(avg_period_us, 1010.0);
}

// T021: 초당 10,000 로그 처리량 테스트 (95% 유지)
TEST_F(LogPerformanceTest, ThroughputTest) {
    // Given
    const int total_messages = 10000;
    const int expected_duration_ms = 1000;  // 1초

    // When
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < total_messages; i++) {
        spdlog::info("Throughput test {}", i);
    }

    auto end = std::chrono::high_resolution_clock::now();

    // Then
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    // 메시지당 평균 시간
    double avg_time_per_msg_us = (duration_ms * 1000.0) / total_messages;

    std::cout << "Total duration: " << duration_ms << " ms" << std::endl;
    std::cout << "Average time per message: " << avg_time_per_msg_us << " μs" << std::endl;
    std::cout << "Messages per second: " << (total_messages * 1000 / duration_ms) << std::endl;

    // 처리량 95% 이상 유지 (10,000 msg/sec의 95% = 9,500 msg/sec)
    // 즉, 1초 이내에 10,000개 메시지 처리
    EXPECT_LT(duration_ms, expected_duration_ms);

    // 평균 메시지당 시간 < 100μs (10,000 msg/sec 달성을 위해)
    EXPECT_LT(avg_time_per_msg_us, 100.0);
}

// p95, p99 지연 측정 테스트
TEST_F(LogPerformanceTest, LatencyPercentiles) {
    // Given
    const int N = 10000;
    std::vector<double> latencies;
    latencies.reserve(N);

    // When
    for (int i = 0; i < N; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        spdlog::info("Latency test {}", i);
        auto end = std::chrono::high_resolution_clock::now();

        auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start).count();
        latencies.push_back(static_cast<double>(latency_us));
    }

    // Then
    std::sort(latencies.begin(), latencies.end());

    double p50 = latencies[N * 50 / 100];
    double p95 = latencies[N * 95 / 100];
    double p99 = latencies[N * 99 / 100];

    std::cout << "p50 latency: " << p50 << " μs" << std::endl;
    std::cout << "p95 latency: " << p95 << " μs" << std::endl;
    std::cout << "p99 latency: " << p99 << " μs" << std::endl;

    // API 계약: p95 < 20μs, p99 < 50μs
    EXPECT_LT(p95, 20.0);
    EXPECT_LT(p99, 50.0);
}

}  // namespace mxrc::core::logging
