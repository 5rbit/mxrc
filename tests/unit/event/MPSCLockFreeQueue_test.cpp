// MPSCLockFreeQueue_test.cpp - MPSC Lock-Free Queue 단위 테스트
// Copyright (C) 2025 MXRC Project

#include "gtest/gtest.h"
#include "util/MPSCLockFreeQueue.h"
#include <thread>
#include <vector>
#include <atomic>

using namespace mxrc::core::event;

namespace mxrc::core::event {

class MPSCLockFreeQueueTest : public ::testing::Test {
protected:
    static constexpr size_t DEFAULT_CAPACITY = 1000;

    void SetUp() override {
        queue_ = std::make_unique<MPSCLockFreeQueue<int>>(DEFAULT_CAPACITY);
    }

    void TearDown() override {
        queue_.reset();
    }

    std::unique_ptr<MPSCLockFreeQueue<int>> queue_;
};

// ===== Basic single-threaded tests =====

TEST_F(MPSCLockFreeQueueTest, PushAndPopSingleElement) {
    // Given: Empty queue
    EXPECT_TRUE(queue_->empty());

    // When: Push one element
    bool pushed = queue_->tryPush(42);

    // Then: Successfully pushed
    EXPECT_TRUE(pushed);
    EXPECT_FALSE(queue_->empty());
    EXPECT_EQ(queue_->size(), 1);

    // When: Pop one element
    int value;
    bool popped = queue_->tryPop(value);

    // Then: Successfully popped with correct value
    EXPECT_TRUE(popped);
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(queue_->empty());
}

TEST_F(MPSCLockFreeQueueTest, MultipleProducersSimultaneousPush) {
    // Given: Multiple producers
    constexpr int NUM_PRODUCERS = 10;
    constexpr int ITEMS_PER_PRODUCER = 100;
    std::atomic<int> successCount{0};

    // When: Multiple threads push simultaneously
    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([this, p, &successCount]() {
            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                int value = p * ITEMS_PER_PRODUCER + i;
                if (queue_->tryPush(value)) {
                    successCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }

    // Then: All items should be pushed successfully (queue large enough)
    EXPECT_EQ(successCount.load(), NUM_PRODUCERS * ITEMS_PER_PRODUCER);

    // Verify we can pop all items
    int count = 0;
    int value;
    while (queue_->tryPop(value)) {
        count++;
    }
    EXPECT_EQ(count, NUM_PRODUCERS * ITEMS_PER_PRODUCER);
}

TEST_F(MPSCLockFreeQueueTest, MultipleProducersSingleConsumer) {
    // Given: Multiple producers and one consumer
    constexpr int NUM_PRODUCERS = 5;
    constexpr int ITEMS_PER_PRODUCER = 200;
    constexpr int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    std::atomic<int> producedCount{0};
    std::atomic<int> consumedCount{0};
    std::atomic<bool> producersDone{false};

    // When: Multiple producers push items
    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([this, p, &producedCount]() {
            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                int value = p * ITEMS_PER_PRODUCER + i;
                while (!queue_->tryPush(value)) {
                    std::this_thread::yield();
                }
                producedCount.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Single consumer pops items
    std::thread consumer([this, &consumedCount, &producersDone, TOTAL_ITEMS]() {
        while (consumedCount.load(std::memory_order_relaxed) < TOTAL_ITEMS) {
            int value;
            if (queue_->tryPop(value)) {
                consumedCount.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    });

    for (auto& t : producers) {
        t.join();
    }
    producersDone.store(true, std::memory_order_release);

    consumer.join();

    // Then: All items produced and consumed
    EXPECT_EQ(producedCount.load(), TOTAL_ITEMS);
    EXPECT_EQ(consumedCount.load(), TOTAL_ITEMS);
    EXPECT_TRUE(queue_->empty());
}

} // namespace mxrc::core::event
