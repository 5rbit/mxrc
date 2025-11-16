// EventBus_test.cpp - EventBus 단위 테스트
// Copyright (C) 2025 MXRC Project

#include "gtest/gtest.h"
#include "core/EventBus.h"
#include "dto/EventBase.h"
#include "util/EventFilter.h"
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

using namespace mxrc::core::event;

namespace mxrc::core::event {

/**
 * @brief EventBus 테스트 픽스처
 */
class EventBusTest : public ::testing::Test {
protected:
    void SetUp() override {
        eventBus_ = std::make_unique<EventBus>(1000);
    }

    void TearDown() override {
        if (eventBus_ && eventBus_->isRunning()) {
            eventBus_->stop();
        }
        eventBus_.reset();
    }

    std::unique_ptr<EventBus> eventBus_;
};

// ===== T024: Basic publish/subscribe 테스트 =====

TEST_F(EventBusTest, PublishAndSubscribe) {
    // Given: EventBus 시작 및 구독 등록
    std::atomic<int> eventCount{0};
    std::shared_ptr<IEvent> receivedEvent;

    auto subId = eventBus_->subscribe(
        Filters::all(),
        [&](std::shared_ptr<IEvent> event) {
            receivedEvent = event;
            eventCount++;
        }
    );

    eventBus_->start();

    // When: 이벤트 발행
    auto event = std::make_shared<EventBase>(EventType::ACTION_STARTED, "test_action");
    bool published = eventBus_->publish(event);

    // 이벤트 처리 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Then: 이벤트가 발행되고 수신됨
    EXPECT_TRUE(published);
    EXPECT_EQ(eventCount.load(), 1);
    EXPECT_NE(receivedEvent, nullptr);
    EXPECT_EQ(receivedEvent->getType(), EventType::ACTION_STARTED);
    EXPECT_EQ(receivedEvent->getTargetId(), "test_action");

    // Cleanup
    eventBus_->unsubscribe(subId);
}

TEST_F(EventBusTest, PublishBeforeStart) {
    // Given: EventBus가 시작되지 않음
    EXPECT_FALSE(eventBus_->isRunning());

    // When: 이벤트 발행
    auto event = std::make_shared<EventBase>(EventType::ACTION_STARTED, "test");
    bool published = eventBus_->publish(event);

    // Then: 큐에 추가됨 (아직 처리되지 않음)
    EXPECT_TRUE(published);
}

TEST_F(EventBusTest, MultipleSubscribers) {
    // Given: 여러 구독자 등록
    std::atomic<int> count1{0}, count2{0}, count3{0};

    auto sub1 = eventBus_->subscribe(Filters::all(), [&](auto) { count1++; });
    auto sub2 = eventBus_->subscribe(Filters::all(), [&](auto) { count2++; });
    auto sub3 = eventBus_->subscribe(Filters::all(), [&](auto) { count3++; });

    eventBus_->start();

    // When: 이벤트 발행
    auto event = std::make_shared<EventBase>(EventType::ACTION_STARTED, "test");
    eventBus_->publish(event);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Then: 모든 구독자가 이벤트 수신
    EXPECT_EQ(count1.load(), 1);
    EXPECT_EQ(count2.load(), 1);
    EXPECT_EQ(count3.load(), 1);

    // Cleanup
    eventBus_->unsubscribe(sub1);
    eventBus_->unsubscribe(sub2);
    eventBus_->unsubscribe(sub3);
}

// ===== T025: Subscription registration/unregistration 테스트 =====

TEST_F(EventBusTest, UnsubscribeStopsEventDelivery) {
    // Given: 구독자 등록
    std::atomic<int> eventCount{0};

    auto subId = eventBus_->subscribe(
        Filters::all(),
        [&](auto) { eventCount++; }
    );

    eventBus_->start();

    // When: 이벤트 발행
    auto event1 = std::make_shared<EventBase>(EventType::ACTION_STARTED, "test1");
    eventBus_->publish(event1);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Then: 이벤트 수신됨
    EXPECT_EQ(eventCount.load(), 1);

    // When: 구독 해제
    bool unsubscribed = eventBus_->unsubscribe(subId);
    EXPECT_TRUE(unsubscribed);

    // When: 다시 이벤트 발행
    auto event2 = std::make_shared<EventBase>(EventType::ACTION_COMPLETED, "test2");
    eventBus_->publish(event2);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Then: 두 번째 이벤트는 수신되지 않음
    EXPECT_EQ(eventCount.load(), 1);
}

TEST_F(EventBusTest, UnsubscribeNonExistentSubscription) {
    // Given: 존재하지 않는 구독 ID
    std::string fakeSubId = "non_existent_sub_id";

    // When: 구독 해제 시도
    bool result = eventBus_->unsubscribe(fakeSubId);

    // Then: 실패
    EXPECT_FALSE(result);
}

// ===== T026: Type-based filtering 테스트 =====

TEST_F(EventBusTest, TypeBasedFiltering) {
    // Given: 타입별로 다른 구독자
    std::atomic<int> actionCount{0}, sequenceCount{0};

    auto actionSub = eventBus_->subscribe(
        Filters::byType(EventType::ACTION_STARTED),
        [&](auto) { actionCount++; }
    );

    auto sequenceSub = eventBus_->subscribe(
        Filters::byType(EventType::SEQUENCE_STARTED),
        [&](auto) { sequenceCount++; }
    );

    eventBus_->start();

    // When: 다양한 타입의 이벤트 발행
    eventBus_->publish(std::make_shared<EventBase>(EventType::ACTION_STARTED, "a1"));
    eventBus_->publish(std::make_shared<EventBase>(EventType::SEQUENCE_STARTED, "s1"));
    eventBus_->publish(std::make_shared<EventBase>(EventType::ACTION_COMPLETED, "a2"));
    eventBus_->publish(std::make_shared<EventBase>(EventType::SEQUENCE_STARTED, "s2"));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Then: 각 구독자가 해당 타입만 수신
    EXPECT_EQ(actionCount.load(), 1);   // ACTION_STARTED만 1개
    EXPECT_EQ(sequenceCount.load(), 2); // SEQUENCE_STARTED 2개

    // Cleanup
    eventBus_->unsubscribe(actionSub);
    eventBus_->unsubscribe(sequenceSub);
}

// ===== T027: Predicate-based filtering 테스트 =====

TEST_F(EventBusTest, PredicateBasedFiltering) {
    // Given: 복잡한 predicate 필터
    std::atomic<int> criticalCount{0};

    auto criticalFilter = [](const std::shared_ptr<IEvent>& event) {
        return event && event->getTargetId().find("critical_") == 0;
    };

    auto subId = eventBus_->subscribe(criticalFilter, [&](auto) { criticalCount++; });
    eventBus_->start();

    // When: 다양한 이벤트 발행
    eventBus_->publish(std::make_shared<EventBase>(EventType::ACTION_STARTED, "critical_action1"));
    eventBus_->publish(std::make_shared<EventBase>(EventType::ACTION_STARTED, "normal_action"));
    eventBus_->publish(std::make_shared<EventBase>(EventType::SEQUENCE_STARTED, "critical_seq1"));
    eventBus_->publish(std::make_shared<EventBase>(EventType::TASK_STARTED, "regular_task"));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Then: critical_로 시작하는 ID만 수신
    EXPECT_EQ(criticalCount.load(), 2);

    eventBus_->unsubscribe(subId);
}

// ===== T028: Subscriber exception isolation 테스트 =====

TEST_F(EventBusTest, SubscriberExceptionIsolation) {
    // Given: 예외를 던지는 구독자와 정상 구독자
    std::atomic<int> normalCount{0};

    auto badSub = eventBus_->subscribe(
        Filters::all(),
        [](auto) { throw std::runtime_error("Test exception"); }
    );

    auto goodSub = eventBus_->subscribe(
        Filters::all(),
        [&](auto) { normalCount++; }
    );

    eventBus_->start();

    // When: 이벤트 발행
    auto event = std::make_shared<EventBase>(EventType::ACTION_STARTED, "test");
    eventBus_->publish(event);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Then: 정상 구독자는 이벤트를 받음 (예외가 격리됨)
    EXPECT_EQ(normalCount.load(), 1);

    // 통계 확인 (실패한 콜백이 기록됨)
    EXPECT_GT(eventBus_->getStats().failedCallbacks.load(), 0);

    // Cleanup
    eventBus_->unsubscribe(badSub);
    eventBus_->unsubscribe(goodSub);
}

// ===== T029: Queue overflow handling 테스트 =====

TEST_F(EventBusTest, QueueOverflowHandling) {
    // Given: 작은 큐를 가진 EventBus
    auto smallBus = std::make_unique<EventBus>(10);

    // When: 큐 용량보다 많은 이벤트 발행 (EventBus 시작 전)
    int publishedCount = 0;
    for (int i = 0; i < 20; ++i) {
        auto event = std::make_shared<EventBase>(EventType::ACTION_STARTED, "action" + std::to_string(i));
        if (smallBus->publish(event)) {
            publishedCount++;
        }
    }

    // Then: 일부 이벤트는 드롭됨
    EXPECT_LT(publishedCount, 20);
    EXPECT_GT(smallBus->getStats().droppedEvents.load(), 0);

    std::cout << "Published: " << publishedCount << ", Dropped: "
              << smallBus->getStats().droppedEvents.load() << std::endl;
}

// ===== T030: Event statistics collection 테스트 =====

TEST_F(EventBusTest, StatisticsCollection) {
    // Given: EventBus 시작
    std::atomic<int> receivedCount{0};

    auto subId = eventBus_->subscribe(
        Filters::all(),
        [&](auto) { receivedCount++; }
    );

    eventBus_->start();

    // When: 여러 이벤트 발행
    constexpr int NUM_EVENTS = 10;
    for (int i = 0; i < NUM_EVENTS; ++i) {
        auto event = std::make_shared<EventBase>(EventType::ACTION_STARTED, "action" + std::to_string(i));
        eventBus_->publish(event);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Then: 통계가 정확함
    const auto& stats = eventBus_->getStats();

    EXPECT_EQ(stats.publishedEvents.load(), NUM_EVENTS);
    EXPECT_EQ(stats.processedEvents.load(), NUM_EVENTS);
    EXPECT_EQ(stats.droppedEvents.load(), 0);
    EXPECT_EQ(stats.activeSubscriptions.load(), 1);

    std::cout << "Stats - Published: " << stats.publishedEvents
              << ", Processed: " << stats.processedEvents
              << ", Active subs: " << stats.activeSubscriptions << std::endl;

    eventBus_->unsubscribe(subId);
}

TEST_F(EventBusTest, ResetStatistics) {
    // Given: 통계가 누적됨
    eventBus_->start();

    for (int i = 0; i < 5; ++i) {
        auto event = std::make_shared<EventBase>(EventType::ACTION_STARTED, "test");
        eventBus_->publish(event);
    }

    EXPECT_GT(eventBus_->getStats().publishedEvents.load(), 0);

    // When: 통계 리셋
    eventBus_->resetStats();

    // Then: 모든 통계가 0으로 초기화됨
    const auto& stats = eventBus_->getStats();
    EXPECT_EQ(stats.publishedEvents.load(), 0);
    EXPECT_EQ(stats.processedEvents.load(), 0);
    EXPECT_EQ(stats.droppedEvents.load(), 0);
}

// ===== Additional: Start/Stop behavior 테스트 =====

TEST_F(EventBusTest, StartStopBehavior) {
    // Given: EventBus가 정지 상태
    EXPECT_FALSE(eventBus_->isRunning());

    // When: 시작
    eventBus_->start();

    // Then: 실행 중
    EXPECT_TRUE(eventBus_->isRunning());

    // When: 정지
    eventBus_->stop();

    // Then: 정지됨
    EXPECT_FALSE(eventBus_->isRunning());
}

TEST_F(EventBusTest, DoubleStartIgnored) {
    // Given: EventBus가 실행 중
    eventBus_->start();
    EXPECT_TRUE(eventBus_->isRunning());

    // When: 다시 시작 시도
    eventBus_->start();  // 경고 로그가 출력되지만 크래시하지 않음

    // Then: 여전히 실행 중
    EXPECT_TRUE(eventBus_->isRunning());
}

TEST_F(EventBusTest, ProcessRemainingEventsOnStop) {
    // Given: 이벤트 발행 후 즉시 정지
    std::atomic<int> receivedCount{0};

    auto subId = eventBus_->subscribe(
        Filters::all(),
        [&](auto) { receivedCount++; }
    );

    eventBus_->start();

    // When: 여러 이벤트 발행
    for (int i = 0; i < 5; ++i) {
        auto event = std::make_shared<EventBase>(EventType::ACTION_STARTED, "action" + std::to_string(i));
        eventBus_->publish(event);
    }

    // 즉시 정지 (남은 이벤트는 stop()에서 처리함)
    eventBus_->stop();

    // Then: 모든 이벤트가 처리됨
    EXPECT_EQ(receivedCount.load(), 5);

    eventBus_->unsubscribe(subId);
}

} // namespace mxrc::core::event
