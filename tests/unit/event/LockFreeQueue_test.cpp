// LockFreeQueue_test.cpp - Lock-Free Queue 단위 테스트
// Copyright (C) 2025 MXRC Project

#include "gtest/gtest.h"
#include "util/LockFreeQueue.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace mxrc::core::event;

namespace mxrc::core::event {

/**
 * @brief LockFreeQueue 테스트 픽스처
 */
class LockFreeQueueTest : public ::testing::Test {
protected:
    static constexpr size_t DEFAULT_CAPACITY = 1000;

    void SetUp() override {
        queue_ = std::make_unique<SPSCLockFreeQueue<int>>(DEFAULT_CAPACITY);
    }

    void TearDown() override {
        queue_.reset();
    }

    std::unique_ptr<SPSCLockFreeQueue<int>> queue_;
};

// ===== T012: Single-threaded push/pop 테스트 =====

TEST_F(LockFreeQueueTest, PushAndPopSingleElement) {
    // Given: 빈 큐
    EXPECT_TRUE(queue_->empty());

    // When: 요소 하나 push
    bool pushed = queue_->tryPush(42);

    // Then: 성공적으로 push됨
    EXPECT_TRUE(pushed);
    EXPECT_FALSE(queue_->empty());
    EXPECT_EQ(queue_->size(), 1);

    // When: 요소 하나 pop
    int value;
    bool popped = queue_->tryPop(value);

    // Then: 성공적으로 pop되고 값이 일치
    EXPECT_TRUE(popped);
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(queue_->empty());
}

TEST_F(LockFreeQueueTest, PushAndPopMultipleElements) {
    // Given: 빈 큐
    constexpr int NUM_ELEMENTS = 100;

    // When: 여러 요소 push
    for (int i = 0; i < NUM_ELEMENTS; ++i) {
        EXPECT_TRUE(queue_->tryPush(i));
    }

    // Then: 크기가 올바름
    EXPECT_EQ(queue_->size(), NUM_ELEMENTS);

    // When: 모든 요소 pop
    for (int i = 0; i < NUM_ELEMENTS; ++i) {
        int value;
        EXPECT_TRUE(queue_->tryPop(value));
        EXPECT_EQ(value, i);
    }

    // Then: 큐가 비어 있음
    EXPECT_TRUE(queue_->empty());
}

TEST_F(LockFreeQueueTest, PopFromEmptyQueue) {
    // Given: 빈 큐
    EXPECT_TRUE(queue_->empty());

    // When: pop 시도
    int value;
    bool popped = queue_->tryPop(value);

    // Then: 실패
    EXPECT_FALSE(popped);
}

// ===== T013: Multi-threaded concurrency 테스트 =====

TEST_F(LockFreeQueueTest, ConcurrentPushAndPop) {
    // Given: Producer와 Consumer 스레드 준비
    constexpr int NUM_ITEMS = 10000;
    std::atomic<int> producedCount{0};
    std::atomic<int> consumedCount{0};
    std::atomic<bool> producerDone{false};

    // When: Producer 스레드에서 push
    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            while (!queue_->tryPush(i)) {
                std::this_thread::yield();  // 큐가 가득 찬 경우 대기
            }
            producedCount.fetch_add(1, std::memory_order_relaxed);
        }
        producerDone.store(true, std::memory_order_release);
    });

    // Consumer 스레드에서 pop
    std::thread consumer([&]() {
        int lastValue = -1;
        while (consumedCount.load(std::memory_order_relaxed) < NUM_ITEMS) {
            int value;
            if (queue_->tryPop(value)) {
                EXPECT_EQ(value, lastValue + 1);  // 순서 검증
                lastValue = value;
                consumedCount.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    // Then: 모든 항목이 생산되고 소비됨
    EXPECT_EQ(producedCount.load(), NUM_ITEMS);
    EXPECT_EQ(consumedCount.load(), NUM_ITEMS);
    EXPECT_TRUE(queue_->empty());
}

TEST_F(LockFreeQueueTest, StressTestHighThroughput) {
    // Given: 대용량 큐
    auto largeQueue = std::make_unique<SPSCLockFreeQueue<int>>(50000);
    constexpr int NUM_ITEMS = 100000;

    std::atomic<int> consumedCount{0};
    std::atomic<bool> producerDone{false};

    auto startTime = std::chrono::high_resolution_clock::now();

    // When: Producer가 빠르게 push
    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            while (!largeQueue->tryPush(i)) {
                std::this_thread::yield();
            }
        }
        producerDone.store(true, std::memory_order_release);
    });

    // Consumer가 빠르게 pop
    std::thread consumer([&]() {
        while (consumedCount.load(std::memory_order_relaxed) < NUM_ITEMS) {
            int value;
            if (largeQueue->tryPop(value)) {
                consumedCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    producer.join();
    consumer.join();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();

    // Then: 모든 항목이 처리됨
    EXPECT_EQ(consumedCount.load(), NUM_ITEMS);

    // 성능 로그 (참고용)
    double throughput = (NUM_ITEMS * 1000.0) / duration;  // ops/sec
    std::cout << "Throughput: " << throughput << " ops/sec ("
              << duration << " ms)" << std::endl;
}

// ===== T014: Queue capacity limits 테스트 =====

TEST_F(LockFreeQueueTest, PushToFullQueue) {
    // Given: 작은 큐 (capacity - 1개까지만 저장 가능)
    constexpr size_t SMALL_CAPACITY = 10;
    auto smallQueue = std::make_unique<SPSCLockFreeQueue<int>>(SMALL_CAPACITY);

    // When: capacity - 1개 요소 push
    for (size_t i = 0; i < SMALL_CAPACITY - 1; ++i) {
        EXPECT_TRUE(smallQueue->tryPush(static_cast<int>(i)));
    }

    // Then: 마지막 push는 실패 (큐가 가득 참)
    EXPECT_FALSE(smallQueue->tryPush(999));
}

TEST_F(LockFreeQueueTest, WrapAroundRingBuffer) {
    // Given: 작은 큐
    constexpr size_t CAPACITY = 20;
    auto smallQueue = std::make_unique<SPSCLockFreeQueue<int>>(CAPACITY);

    // When: capacity - 1개 push
    for (size_t i = 0; i < CAPACITY - 1; ++i) {
        EXPECT_TRUE(smallQueue->tryPush(static_cast<int>(i)));
    }

    // When: 절반 pop
    for (size_t i = 0; i < (CAPACITY - 1) / 2; ++i) {
        int value;
        EXPECT_TRUE(smallQueue->tryPop(value));
    }

    // When: 다시 push (ring buffer wrap around 발생)
    for (size_t i = 0; i < (CAPACITY - 1) / 2; ++i) {
        EXPECT_TRUE(smallQueue->tryPush(static_cast<int>(i + 100)));
    }

    // Then: 큐가 다시 가득 참
    EXPECT_FALSE(smallQueue->tryPush(999));
}

// ===== T015: Performance benchmark (>10,000 ops/sec) =====

TEST_F(LockFreeQueueTest, PerformanceBenchmarkSingleThreaded) {
    // Given: 대용량 큐
    constexpr int NUM_OPS = 100000;
    auto startTime = std::chrono::high_resolution_clock::now();

    // When: push/pop 반복
    for (int i = 0; i < NUM_OPS; ++i) {
        EXPECT_TRUE(queue_->tryPush(i));
        int value;
        EXPECT_TRUE(queue_->tryPop(value));
        EXPECT_EQ(value, i);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime).count();

    // Then: 처리량 확인
    double throughput = (NUM_OPS * 2 * 1000000.0) / duration;  // ops/sec (push+pop)
    std::cout << "Single-threaded throughput: " << throughput << " ops/sec" << std::endl;

    // 목표: >10,000 ops/sec (실제로는 훨씬 높아야 함)
    EXPECT_GT(throughput, 10000);
}

TEST_F(LockFreeQueueTest, PerformanceBenchmarkMultiThreaded) {
    // Given: 대용량 큐
    auto largeQueue = std::make_unique<SPSCLockFreeQueue<int>>(50000);
    constexpr int NUM_OPS = 100000;

    std::atomic<bool> producerDone{false};
    std::atomic<int> consumedCount{0};

    auto startTime = std::chrono::high_resolution_clock::now();

    // When: Multi-threaded push/pop
    std::thread producer([&]() {
        for (int i = 0; i < NUM_OPS; ++i) {
            while (!largeQueue->tryPush(i)) {
                std::this_thread::yield();
            }
        }
        producerDone.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
        while (consumedCount.load(std::memory_order_relaxed) < NUM_OPS) {
            int value;
            if (largeQueue->tryPop(value)) {
                consumedCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    producer.join();
    consumer.join();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime).count();

    // Then: 처리량 확인
    double throughput = (NUM_OPS * 1000000.0) / duration;  // ops/sec
    std::cout << "Multi-threaded throughput: " << throughput << " ops/sec" << std::endl;

    EXPECT_GT(throughput, 10000);  // 목표: >10,000 ops/sec
}

} // namespace mxrc::core::event
