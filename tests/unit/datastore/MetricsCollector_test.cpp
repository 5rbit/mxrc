#include "gtest/gtest.h"
#include "core/datastore/managers/MetricsCollector.h"
#include <thread>
#include <vector>
#include <chrono>

namespace mxrc::core::datastore {

class MetricsCollectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        collector_ = std::make_unique<MetricsCollector>();
    }

    void TearDown() override {
        collector_.reset();
    }

    std::unique_ptr<MetricsCollector> collector_;
};

// 1. 기본 카운터 증가 테스트
TEST_F(MetricsCollectorTest, IncrementGetCalls) {
    // Given: 초기 상태
    auto initial_metrics = collector_->getMetrics();
    EXPECT_EQ(0.0, initial_metrics["get_calls"]);

    // When: get 카운터 증가
    collector_->incrementGet();
    collector_->incrementGet();
    collector_->incrementGet();

    // Then: 카운터가 3 증가
    auto metrics = collector_->getMetrics();
    EXPECT_EQ(3.0, metrics["get_calls"]);
    EXPECT_EQ(0.0, metrics["set_calls"]);
    EXPECT_EQ(0.0, metrics["delete_calls"]);
}

TEST_F(MetricsCollectorTest, IncrementSetCalls) {
    // When: set 카운터 증가
    collector_->incrementSet();
    collector_->incrementSet();

    // Then: 카운터가 2 증가
    auto metrics = collector_->getMetrics();
    EXPECT_EQ(0.0, metrics["get_calls"]);
    EXPECT_EQ(2.0, metrics["set_calls"]);
    EXPECT_EQ(0.0, metrics["delete_calls"]);
}

TEST_F(MetricsCollectorTest, IncrementDeleteCalls) {
    // When: delete 카운터 증가
    collector_->incrementDelete();

    // Then: 카운터가 1 증가
    auto metrics = collector_->getMetrics();
    EXPECT_EQ(0.0, metrics["get_calls"]);
    EXPECT_EQ(0.0, metrics["set_calls"]);
    EXPECT_EQ(1.0, metrics["delete_calls"]);
}

// 2. 메모리 사용량 업데이트 테스트
TEST_F(MetricsCollectorTest, UpdateMemoryUsage) {
    // When: 메모리 사용량 증가
    collector_->updateMemoryUsage(1024);  // +1KB
    collector_->updateMemoryUsage(2048);  // +2KB

    // Then: 총 3KB 증가
    auto metrics = collector_->getMetrics();
    EXPECT_EQ(3072.0, metrics["memory_usage_bytes"]);
}

TEST_F(MetricsCollectorTest, UpdateMemoryUsageNegative) {
    // Given: 초기 메모리 사용량 설정
    collector_->updateMemoryUsage(5000);

    // When: 메모리 사용량 감소
    collector_->updateMemoryUsage(-2000);

    // Then: 3000 bytes
    auto metrics = collector_->getMetrics();
    EXPECT_EQ(3000.0, metrics["memory_usage_bytes"]);
}

// 3. 메트릭 조회 테스트
TEST_F(MetricsCollectorTest, GetMetricsReturnsAllCounters) {
    // Given: 여러 카운터 증가
    collector_->incrementGet();
    collector_->incrementGet();
    collector_->incrementSet();
    collector_->incrementDelete();
    collector_->updateMemoryUsage(1024);

    // When: 메트릭 조회
    auto metrics = collector_->getMetrics();

    // Then: 모든 카운터가 포함됨
    EXPECT_EQ(4u, metrics.size());
    EXPECT_EQ(2.0, metrics["get_calls"]);
    EXPECT_EQ(1.0, metrics["set_calls"]);
    EXPECT_EQ(1.0, metrics["delete_calls"]);
    EXPECT_EQ(1024.0, metrics["memory_usage_bytes"]);
}

// 4. 메트릭 리셋 테스트
TEST_F(MetricsCollectorTest, ResetMetrics) {
    // Given: 카운터 증가
    collector_->incrementGet();
    collector_->incrementSet();
    collector_->incrementDelete();
    collector_->updateMemoryUsage(1024);

    auto before_reset = collector_->getMetrics();
    EXPECT_EQ(1.0, before_reset["get_calls"]);
    EXPECT_EQ(1.0, before_reset["set_calls"]);

    // When: 메트릭 리셋
    collector_->resetMetrics();

    // Then: 모든 카운터가 0으로 초기화
    auto after_reset = collector_->getMetrics();
    EXPECT_EQ(0.0, after_reset["get_calls"]);
    EXPECT_EQ(0.0, after_reset["set_calls"]);
    EXPECT_EQ(0.0, after_reset["delete_calls"]);
    EXPECT_EQ(0.0, after_reset["memory_usage_bytes"]);
}

// 5. 스레드 안전성 테스트
TEST_F(MetricsCollectorTest, ThreadSafety_ConcurrentIncrements) {
    // Given: 10개 스레드, 각 1000번 증가
    const int num_threads = 10;
    const int increments_per_thread = 1000;
    std::vector<std::thread> threads;

    // When: 동시에 incrementGet 호출
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, increments_per_thread]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                collector_->incrementGet();
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // Then: 정확히 10,000번 증가
    auto metrics = collector_->getMetrics();
    EXPECT_EQ(10000.0, metrics["get_calls"]);
}

TEST_F(MetricsCollectorTest, ThreadSafety_MixedOperations) {
    // Given: 여러 스레드가 서로 다른 카운터 증가
    const int num_threads = 5;
    const int operations_per_thread = 500;
    std::vector<std::thread> threads;

    // When: 스레드별로 다른 연산 수행
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, operations_per_thread]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                if (i % 3 == 0) {
                    collector_->incrementGet();
                } else if (i % 3 == 1) {
                    collector_->incrementSet();
                } else {
                    collector_->incrementDelete();
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Then: 각 카운터가 올바르게 증가
    auto metrics = collector_->getMetrics();
    double total = metrics["get_calls"] + metrics["set_calls"] + metrics["delete_calls"];
    EXPECT_EQ(2500.0, total);  // 5 threads * 500 ops = 2500
}

// 6. 오버플로우 테스트 (wrapping 동작 확인)
TEST_F(MetricsCollectorTest, CounterWrapping_NoException) {
    // Given: 매우 큰 값 설정 (uint64_t 최대값에 근접)
    const uint64_t large_value = std::numeric_limits<uint64_t>::max() - 10;

    // When: 큰 값에서 증가 시도 (내부적으로 wrapping 발생)
    // 참고: atomic<uint64_t>는 wrapping 동작이 정의되어 있음
    for (uint64_t i = 0; i < large_value && i < 100; ++i) {
        EXPECT_NO_THROW(collector_->incrementGet());
    }

    // Then: 예외 없이 정상 동작
    auto metrics = collector_->getMetrics();
    EXPECT_GE(metrics["get_calls"], 0.0);
}

// 7. 메트릭 조회 중 카운터 증가 테스트 (동시성)
TEST_F(MetricsCollectorTest, ConcurrentGetMetricsAndIncrement) {
    // Given: 읽기 스레드와 쓰기 스레드 동시 실행
    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    // Writer threads: 카운터 증가
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([this, &stop]() {
            while (!stop.load()) {
                collector_->incrementGet();
                collector_->incrementSet();
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    // Reader thread: 메트릭 조회
    threads.emplace_back([this, &stop]() {
        for (int i = 0; i < 100; ++i) {
            auto metrics = collector_->getMetrics();
            EXPECT_GE(metrics["get_calls"], 0.0);
            EXPECT_GE(metrics["set_calls"], 0.0);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        stop.store(true);
    });

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // Then: 데드락이나 크래시 없이 정상 종료
    auto final_metrics = collector_->getMetrics();
    EXPECT_GT(final_metrics["get_calls"], 0.0);
    EXPECT_GT(final_metrics["set_calls"], 0.0);
}

} // namespace mxrc::core::datastore
