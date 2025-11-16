// SubscriptionManager_test.cpp - SubscriptionManager 단위 테스트
// Copyright (C) 2025 MXRC Project

#include "gtest/gtest.h"
#include "core/SubscriptionManager.h"
#include "dto/EventBase.h"
#include "util/EventFilter.h"
#include <thread>
#include <vector>
#include <atomic>

using namespace mxrc::core::event;

namespace mxrc::core::event {

/**
 * @brief SubscriptionManager 테스트 픽스처
 */
class SubscriptionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<SubscriptionManager>();
    }

    void TearDown() override {
        manager_.reset();
    }

    std::unique_ptr<SubscriptionManager> manager_;
};

// ===== T018: Subscription add/remove 테스트 =====

TEST_F(SubscriptionManagerTest, AddSubscription) {
    // Given: 빈 SubscriptionManager
    EXPECT_EQ(manager_->getSubscriptionCount(), 0);

    // When: 구독 추가
    auto filter = [](const std::shared_ptr<IEvent>&) { return true; };
    auto callback = [](std::shared_ptr<IEvent>) {};

    std::string subId = manager_->addSubscription(filter, callback);

    // Then: 구독이 추가됨
    EXPECT_FALSE(subId.empty());
    EXPECT_EQ(manager_->getSubscriptionCount(), 1);
}

TEST_F(SubscriptionManagerTest, RemoveSubscription) {
    // Given: 구독이 하나 있음
    auto filter = [](const std::shared_ptr<IEvent>&) { return true; };
    auto callback = [](std::shared_ptr<IEvent>) {};
    std::string subId = manager_->addSubscription(filter, callback);

    EXPECT_EQ(manager_->getSubscriptionCount(), 1);

    // When: 구독 제거
    bool removed = manager_->removeSubscription(subId);

    // Then: 성공적으로 제거됨
    EXPECT_TRUE(removed);
    EXPECT_EQ(manager_->getSubscriptionCount(), 0);
}

TEST_F(SubscriptionManagerTest, RemoveNonExistentSubscription) {
    // Given: 빈 SubscriptionManager
    EXPECT_EQ(manager_->getSubscriptionCount(), 0);

    // When: 존재하지 않는 구독 제거 시도
    bool removed = manager_->removeSubscription("non_existent_id");

    // Then: 실패
    EXPECT_FALSE(removed);
}

TEST_F(SubscriptionManagerTest, AddMultipleSubscriptions) {
    // Given: 빈 SubscriptionManager
    std::vector<std::string> subIds;

    // When: 여러 구독 추가
    for (int i = 0; i < 10; ++i) {
        auto filter = [](const std::shared_ptr<IEvent>&) { return true; };
        auto callback = [](std::shared_ptr<IEvent>) {};
        subIds.push_back(manager_->addSubscription(filter, callback));
    }

    // Then: 모든 구독이 추가됨
    EXPECT_EQ(manager_->getSubscriptionCount(), 10);

    // When: 모든 구독 제거
    for (const auto& subId : subIds) {
        EXPECT_TRUE(manager_->removeSubscription(subId));
    }

    // Then: 모두 제거됨
    EXPECT_EQ(manager_->getSubscriptionCount(), 0);
}

// ===== T019: Getting subscribers by event type 테스트 =====

TEST_F(SubscriptionManagerTest, GetAllSubscriptions) {
    // Given: 여러 구독이 등록됨
    std::atomic<int> callCount{0};

    auto callback1 = [&callCount](std::shared_ptr<IEvent>) { callCount++; };
    auto callback2 = [&callCount](std::shared_ptr<IEvent>) { callCount++; };

    manager_->addSubscription(Filters::all(), callback1);
    manager_->addSubscription(Filters::all(), callback2);

    // When: 모든 구독 조회
    auto subscriptions = manager_->getAllSubscriptions();

    // Then: 2개의 구독이 반환됨
    EXPECT_EQ(subscriptions.size(), 2);

    // 콜백 실행 테스트
    auto testEvent = std::make_shared<EventBase>(EventType::ACTION_STARTED, "test_action");
    for (const auto& sub : subscriptions) {
        if (sub.filter(testEvent)) {
            sub.callback(testEvent);
        }
    }

    EXPECT_EQ(callCount.load(), 2);
}

TEST_F(SubscriptionManagerTest, FilterBasedSelection) {
    // Given: 타입별로 다른 필터를 가진 구독들
    std::atomic<int> actionCallCount{0};
    std::atomic<int> sequenceCallCount{0};

    auto actionCallback = [&actionCallCount](std::shared_ptr<IEvent>) { actionCallCount++; };
    auto sequenceCallback = [&sequenceCallCount](std::shared_ptr<IEvent>) { sequenceCallCount++; };

    manager_->addSubscription(Filters::byType(EventType::ACTION_STARTED), actionCallback);
    manager_->addSubscription(Filters::byType(EventType::SEQUENCE_STARTED), sequenceCallback);

    // When: 이벤트 발행 (필터 테스트)
    auto actionEvent = std::make_shared<EventBase>(EventType::ACTION_STARTED, "action1");
    auto sequenceEvent = std::make_shared<EventBase>(EventType::SEQUENCE_STARTED, "seq1");

    auto subscriptions = manager_->getAllSubscriptions();

    for (const auto& sub : subscriptions) {
        if (sub.filter(actionEvent)) {
            sub.callback(actionEvent);
        }
        if (sub.filter(sequenceEvent)) {
            sub.callback(sequenceEvent);
        }
    }

    // Then: 각 콜백이 해당 타입만 수신
    EXPECT_EQ(actionCallCount.load(), 1);
    EXPECT_EQ(sequenceCallCount.load(), 1);
}

// ===== T020: Thread safety 테스트 =====

TEST_F(SubscriptionManagerTest, ConcurrentAddSubscriptions) {
    // Given: 여러 스레드가 동시에 구독 추가
    constexpr int NUM_THREADS = 10;
    constexpr int SUBS_PER_THREAD = 100;

    std::vector<std::thread> threads;
    std::vector<std::vector<std::string>> subIdsByThread(NUM_THREADS);

    // When: 동시에 구독 추가
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t, &subIdsByThread]() {
            for (int i = 0; i < SUBS_PER_THREAD; ++i) {
                auto filter = [](const std::shared_ptr<IEvent>&) { return true; };
                auto callback = [](std::shared_ptr<IEvent>) {};
                std::string subId = manager_->addSubscription(filter, callback);
                subIdsByThread[t].push_back(subId);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Then: 모든 구독이 추가됨 (데이터 레이스 없음)
    EXPECT_EQ(manager_->getSubscriptionCount(), NUM_THREADS * SUBS_PER_THREAD);

    // 모든 구독 ID가 고유함
    std::set<std::string> allSubIds;
    for (const auto& subIds : subIdsByThread) {
        for (const auto& subId : subIds) {
            allSubIds.insert(subId);
        }
    }
    EXPECT_EQ(allSubIds.size(), NUM_THREADS * SUBS_PER_THREAD);
}

TEST_F(SubscriptionManagerTest, ConcurrentAddAndRemove) {
    // Given: 여러 스레드가 동시에 추가/제거
    constexpr int NUM_OPERATIONS = 1000;
    std::atomic<int> addCount{0};
    std::atomic<int> removeCount{0};

    std::vector<std::string> subIds;
    std::mutex subIdsMutex;

    // When: 동시에 추가
    std::thread adder([&]() {
        for (int i = 0; i < NUM_OPERATIONS; ++i) {
            auto filter = [](const std::shared_ptr<IEvent>&) { return true; };
            auto callback = [](std::shared_ptr<IEvent>) {};
            std::string subId = manager_->addSubscription(filter, callback);

            {
                std::lock_guard<std::mutex> lock(subIdsMutex);
                subIds.push_back(subId);
            }
            addCount++;
        }
    });

    // 동시에 제거
    std::thread remover([&]() {
        for (int i = 0; i < NUM_OPERATIONS; ++i) {
            std::string subId;
            {
                std::lock_guard<std::mutex> lock(subIdsMutex);
                if (!subIds.empty()) {
                    subId = subIds.back();
                    subIds.pop_back();
                }
            }

            if (!subId.empty() && manager_->removeSubscription(subId)) {
                removeCount++;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });

    adder.join();
    remover.join();

    // Then: 추가/제거가 안전하게 수행됨 (데이터 레이스 없음)
    EXPECT_EQ(addCount.load(), NUM_OPERATIONS);
    // removeCount는 addCount보다 작거나 같음 (타이밍에 따라)
    EXPECT_LE(removeCount.load(), addCount.load());

    std::cout << "Added: " << addCount << ", Removed: " << removeCount << std::endl;
}

TEST_F(SubscriptionManagerTest, ClearAllSubscriptions) {
    // Given: 여러 구독이 등록됨
    for (int i = 0; i < 10; ++i) {
        auto filter = [](const std::shared_ptr<IEvent>&) { return true; };
        auto callback = [](std::shared_ptr<IEvent>) {};
        manager_->addSubscription(filter, callback);
    }

    EXPECT_EQ(manager_->getSubscriptionCount(), 10);

    // When: 모든 구독 제거
    manager_->clear();

    // Then: 모두 제거됨
    EXPECT_EQ(manager_->getSubscriptionCount(), 0);
}

} // namespace mxrc::core::event
