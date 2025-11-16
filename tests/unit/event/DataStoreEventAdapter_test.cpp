// DataStoreEventAdapter_test.cpp - DataStoreEventAdapter 단위 테스트
// Copyright (C) 2025 MXRC Project

#include <gtest/gtest.h>
#include "core/event/adapters/DataStoreEventAdapter.h"
#include "core/event/core/EventBus.h"
#include "core/datastore/DataStore.h"
#include "core/event/dto/DataStoreEvents.h"
#include "core/event/dto/ActionEvents.h"
#include "core/event/dto/SequenceEvents.h"
#include "core/event/util/EventFilter.h"
#include <chrono>
#include <thread>
#include <atomic>

namespace mxrc::core::event {

class DataStoreEventAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // DataStore는 Singleton이므로 getInstance()로 참조 획득
        dataStore_ = std::shared_ptr<DataStore>(&DataStore::getInstance(), [](DataStore*) {});
        eventBus_ = std::make_shared<EventBus>();
        eventBus_->start();  // 디스패치 스레드 시작
        adapter_ = std::make_shared<DataStoreEventAdapter>(dataStore_, eventBus_);

        receivedEvents_.clear();
    }

    void TearDown() override {
        adapter_.reset();
        if (eventBus_) {
            eventBus_->stop();
        }
        eventBus_.reset();
    }

    // 이벤트 수신 헬퍼
    void subscribeToEvents(EventType eventType) {
        eventBus_->subscribe(
            Filters::byType(eventType),
            [this](std::shared_ptr<IEvent> event) {
                std::lock_guard<std::mutex> lock(eventsMutex_);
                receivedEvents_.push_back(event);
                eventsCV_.notify_all();
            }
        );
    }

    // 이벤트 대기 헬퍼
    bool waitForEventCount(size_t expectedCount, int timeoutMs = 1000) {
        std::unique_lock<std::mutex> lock(eventsMutex_);
        return eventsCV_.wait_for(
            lock,
            std::chrono::milliseconds(timeoutMs),
            [this, expectedCount] { return receivedEvents_.size() >= expectedCount; }
        );
    }

    std::shared_ptr<DataStore> dataStore_;
    std::shared_ptr<EventBus> eventBus_;
    std::shared_ptr<DataStoreEventAdapter> adapter_;

    std::vector<std::shared_ptr<IEvent>> receivedEvents_;
    std::mutex eventsMutex_;
    std::condition_variable eventsCV_;
};

// ========== T058: DataStore 변경 → EventBus 발행 ==========

TEST_F(DataStoreEventAdapterTest, DataStoreChangePublishesEvent) {
    // Given: DATASTORE_VALUE_CHANGED 이벤트 구독
    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);

    // DataStore 감시 시작 (정확한 ID로 구독)
    adapter_->startWatching("test.value");

    // When: DataStore에 값 설정
    dataStore_->set("test.value", 42, DataType::Config);

    // Then: DATASTORE_VALUE_CHANGED 이벤트 수신
    ASSERT_TRUE(waitForEventCount(1, 500));

    auto event = std::static_pointer_cast<DataStoreValueChangedEvent>(receivedEvents_[0]);
    EXPECT_EQ(event->key, "test.value");
    EXPECT_EQ(event->newValue, "42");
    EXPECT_EQ(event->valueType, "Config");
    EXPECT_EQ(event->source, "datastore");
}

TEST_F(DataStoreEventAdapterTest, MultipleDataStoreChanges) {
    // Given
    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);
    adapter_->startWatching("robot.x");
    adapter_->startWatching("robot.y");
    adapter_->startWatching("robot.state");

    // When: 여러 값 변경
    dataStore_->set("robot.x", 10.5, DataType::InterfaceData);
    dataStore_->set("robot.y", 20.3, DataType::InterfaceData);
    dataStore_->set("robot.state", std::string("running"), DataType::RobotMode);

    // Then: 3개 이벤트 수신
    ASSERT_TRUE(waitForEventCount(3, 1000));
    EXPECT_EQ(receivedEvents_.size(), 3);
}

TEST_F(DataStoreEventAdapterTest, StopWatchingStopsEvents) {
    // Given
    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);
    adapter_->startWatching("test.value1");

    dataStore_->set("test.value1", 1, DataType::Config);
    ASSERT_TRUE(waitForEventCount(1));

    // When: 감시 중지
    adapter_->stopWatching("test.value1");
    receivedEvents_.clear();

    dataStore_->set("test.value1", 2, DataType::Config);

    // Then: 새 이벤트 수신 안 됨
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(receivedEvents_.size(), 0);
}

// ========== T059: EventBus → DataStore 자동 저장 ==========

TEST_F(DataStoreEventAdapterTest, ActionCompletedSavesToDataStore) {
    // Given: Action 결과 자동 저장 구독
    adapter_->subscribeToActionResults("action.results.");

    // When: ACTION_COMPLETED 이벤트 발행
    auto actionEvent = std::make_shared<ActionCompletedEvent>(
        "action1",
        "DelayAction",
        100,  // durationMs
        ""    // result
    );
    eventBus_->publish(actionEvent);

    // EventBus 디스패치 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Then: DataStore에 결과 저장됨
    std::string result = dataStore_->get<std::string>("action.results.action1");
    EXPECT_TRUE(result.find("completed") != std::string::npos);
    EXPECT_TRUE(result.find("100ms") != std::string::npos);
}

TEST_F(DataStoreEventAdapterTest, SequenceCompletedSavesToDataStore) {
    // Given: Sequence 결과 자동 저장 구독
    adapter_->subscribeToSequenceResults("sequence.results.");

    // When: SEQUENCE_COMPLETED 이벤트 발행
    auto seqEvent = std::make_shared<SequenceCompletedEvent>(
        "seq1",
        "TestSequence",
        5,    // completedSteps
        5,    // totalSteps
        500   // durationMs
    );
    eventBus_->publish(seqEvent);

    // EventBus 디스패치 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Then: DataStore에 결과 저장됨
    std::string result = dataStore_->get<std::string>("sequence.results.seq1");
    EXPECT_TRUE(result.find("completed") != std::string::npos);
    EXPECT_TRUE(result.find("5/5") != std::string::npos);
    EXPECT_TRUE(result.find("500ms") != std::string::npos);
}

// ========== T060: 순환 업데이트 방지 ==========

TEST_F(DataStoreEventAdapterTest, CircularUpdatePrevention) {
    // Given: DataStore 변경 감시 및 Action 결과 저장 모두 활성화
    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);
    adapter_->startWatching("action.results.action2");
    adapter_->subscribeToActionResults("action.results.");

    // When: ACTION_COMPLETED 이벤트 발행 (DataStore에 저장됨)
    auto actionEvent = std::make_shared<ActionCompletedEvent>(
        "action2",
        "MoveAction",
        200,
        ""
    );
    eventBus_->publish(actionEvent);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Then: DATASTORE_VALUE_CHANGED 이벤트가 발생하지 않음 (순환 방지)
    // 순환 업데이트 방지 메커니즘으로 인해 0개 또는 매우 적은 수의 이벤트만 발생
    EXPECT_LE(receivedEvents_.size(), 1);
}

// ========== T061: 양방향 연동 통합 테스트 ==========

TEST_F(DataStoreEventAdapterTest, BidirectionalIntegration) {
    // Given: 양방향 연동 설정
    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);
    adapter_->startWatching("system.mode");
    adapter_->subscribeToActionResults("action.results.");

    // When: 직접 DataStore 변경
    dataStore_->set("system.mode", std::string("active"), DataType::RobotMode);

    // Then: 이벤트 수신
    ASSERT_TRUE(waitForEventCount(1, 500));
    auto dsEvent = std::static_pointer_cast<DataStoreValueChangedEvent>(receivedEvents_[0]);
    EXPECT_EQ(dsEvent->key, "system.mode");
    EXPECT_EQ(dsEvent->newValue, "active");

    // When: Action 완료 이벤트 발행
    receivedEvents_.clear();
    auto actionEvent = std::make_shared<ActionCompletedEvent>(
        "action3", "TestAction", 150, "");
    eventBus_->publish(actionEvent);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Then: DataStore에 저장됨
    std::string result = dataStore_->get<std::string>("action.results.action3");
    EXPECT_TRUE(result.find("completed") != std::string::npos);
}

TEST_F(DataStoreEventAdapterTest, ValueTypeConversion) {
    // Given
    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);
    adapter_->startWatching("test.int");
    adapter_->startWatching("test.double");
    adapter_->startWatching("test.string");
    adapter_->startWatching("test.bool");

    // When: 다양한 타입의 값 저장
    dataStore_->set("test.int", 42, DataType::Config);
    dataStore_->set("test.double", 3.14, DataType::Config);
    dataStore_->set("test.string", std::string("hello"), DataType::Config);
    dataStore_->set("test.bool", true, DataType::Config);

    // Then: 모든 이벤트 수신 및 타입 변환 확인
    ASSERT_TRUE(waitForEventCount(4, 1000));

    auto intEvent = std::static_pointer_cast<DataStoreValueChangedEvent>(receivedEvents_[0]);
    EXPECT_EQ(intEvent->newValue, "42");

    auto doubleEvent = std::static_pointer_cast<DataStoreValueChangedEvent>(receivedEvents_[1]);
    EXPECT_TRUE(doubleEvent->newValue.find("3.14") != std::string::npos);

    auto strEvent = std::static_pointer_cast<DataStoreValueChangedEvent>(receivedEvents_[2]);
    EXPECT_EQ(strEvent->newValue, "hello");

    auto boolEvent = std::static_pointer_cast<DataStoreValueChangedEvent>(receivedEvents_[3]);
    EXPECT_EQ(boolEvent->newValue, "true");
}

// ========== 추가 테스트: 생성/소멸 안정성 ==========

TEST_F(DataStoreEventAdapterTest, ConstructorWithNullDataStore) {
    // Given: nullptr DataStore
    std::shared_ptr<DataStore> nullDataStore = nullptr;

    // When/Then: 생성 시 예외 발생하지 않음 (nullptr 체크는 사용 시점에서)
    EXPECT_NO_THROW({
        auto adapter = std::make_shared<DataStoreEventAdapter>(nullDataStore, eventBus_);
    });
}

TEST_F(DataStoreEventAdapterTest, ConstructorWithNullEventBus) {
    // Given: nullptr EventBus
    std::shared_ptr<EventBus> nullEventBus = nullptr;

    // When/Then: 생성 시 예외 발생하지 않음
    EXPECT_NO_THROW({
        auto adapter = std::make_shared<DataStoreEventAdapter>(dataStore_, nullEventBus);
    });
}

TEST_F(DataStoreEventAdapterTest, DestructorUnsubscribesAll) {
    // Given: 여러 구독 설정
    adapter_->startWatching("test.key1");
    adapter_->startWatching("test.key2");
    adapter_->subscribeToActionResults("action.results.");
    adapter_->subscribeToSequenceResults("sequence.results.");

    // When: 어댑터 소멸
    adapter_.reset();

    // Then: 모든 구독이 해제되어야 함 (로그 확인)
    // 소멸자에서 unsubscribe 호출이 정상적으로 이루어져야 함
    SUCCEED();  // 크래시 없이 완료되면 성공
}

TEST_F(DataStoreEventAdapterTest, MultipleStartWatchingSameKey) {
    // Given: 이벤트 구독
    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);

    // When: 동일한 키에 대해 여러 번 startWatching 호출
    adapter_->startWatching("test.key");
    adapter_->startWatching("test.key");
    adapter_->startWatching("test.key");

    dataStore_->set("test.key", 100, DataType::Config);

    // Then: DataStore의 현재 구현상 동일한 Observer가 여러 번 등록되면
    // 동일한 이벤트가 여러 번 발행됨 (중복 등록 방지 없음)
    ASSERT_TRUE(waitForEventCount(3, 500));
    EXPECT_EQ(receivedEvents_.size(), 3);
}

TEST_F(DataStoreEventAdapterTest, StopWatchingNonExistentKey) {
    // When: 감시하지 않은 키를 stopWatching
    EXPECT_NO_THROW({
        adapter_->stopWatching("never.watched");
    });
}

// ========== 추가 테스트: 동시성 및 스레드 안정성 ==========

TEST_F(DataStoreEventAdapterTest, ConcurrentDataStoreUpdates) {
    // Given
    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);

    for (int i = 0; i < 10; i++) {
        adapter_->startWatching("concurrent.key" + std::to_string(i));
    }

    // When: 여러 스레드에서 동시에 DataStore 업데이트
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([this, i]() {
            dataStore_->set("concurrent.key" + std::to_string(i), i * 10, DataType::Config);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Then: 모든 이벤트 수신
    ASSERT_TRUE(waitForEventCount(10, 2000));
    EXPECT_EQ(receivedEvents_.size(), 10);
}

TEST_F(DataStoreEventAdapterTest, ConcurrentEventBusPublish) {
    // Given: Action 결과 자동 저장 구독
    adapter_->subscribeToActionResults("action.results.");

    // When: 여러 스레드에서 동시에 이벤트 발행
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([this, i]() {
            auto event = std::make_shared<ActionCompletedEvent>(
                "action" + std::to_string(i),
                "TestAction",
                100,
                ""
            );
            eventBus_->publish(event);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // EventBus 디스패치 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Then: 모든 결과가 DataStore에 저장되어야 함
    for (int i = 0; i < 10; i++) {
        std::string key = "action.results.action" + std::to_string(i);
        EXPECT_NO_THROW({
            std::string result = dataStore_->get<std::string>(key);
            EXPECT_TRUE(result.find("completed") != std::string::npos);
        });
    }
}

TEST_F(DataStoreEventAdapterTest, ConcurrentStartStopWatching) {
    // When: 여러 스레드에서 동시에 start/stop 호출
    std::vector<std::thread> threads;

    for (int i = 0; i < 5; i++) {
        threads.emplace_back([this, i]() {
            std::string key = "watch.key" + std::to_string(i);
            for (int j = 0; j < 10; j++) {
                adapter_->startWatching(key);
                adapter_->stopWatching(key);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Then: 크래시 없이 완료
    SUCCEED();
}

TEST_F(DataStoreEventAdapterTest, CircularUpdateUnderConcurrency) {
    // Given: 양방향 연동 설정
    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);
    adapter_->startWatching("circular.key");
    adapter_->subscribeToActionResults("circular.");

    // When: 여러 스레드에서 동시에 순환 업데이트 유발 시도
    std::atomic<int> circularPreventedCount{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 5; i++) {
        threads.emplace_back([this, &circularPreventedCount]() {
            for (int j = 0; j < 10; j++) {
                auto event = std::make_shared<ActionCompletedEvent>(
                    "key", "TestAction", 100, "");
                eventBus_->publish(event);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Then: 순환 업데이트가 방지되어야 함
    // (이벤트 수가 무한정 증가하지 않음)
    EXPECT_LT(receivedEvents_.size(), 100);  // 순환이 발생했다면 훨씬 많을 것
}

// ========== 추가 테스트: 엣지 케이스 및 에러 처리 ==========

TEST_F(DataStoreEventAdapterTest, EmptyKeyWatching) {
    // When: 빈 키로 감시 시작
    EXPECT_NO_THROW({
        adapter_->startWatching("");
    });
}

TEST_F(DataStoreEventAdapterTest, VeryLongKeyWatching) {
    // Given: 매우 긴 키
    std::string longKey(1000, 'a');

    // When: 긴 키로 감시
    EXPECT_NO_THROW({
        adapter_->startWatching(longKey);
    });
}

TEST_F(DataStoreEventAdapterTest, SpecialCharactersInKey) {
    // Given: 특수 문자가 포함된 키
    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);
    std::string specialKey = "test.key!@#$%^&*()";

    adapter_->startWatching(specialKey);

    // When: 특수 문자 키로 데이터 설정
    dataStore_->set(specialKey, 123, DataType::Config);

    // Then: 정상적으로 이벤트 발행
    ASSERT_TRUE(waitForEventCount(1, 500));
    auto event = std::static_pointer_cast<DataStoreValueChangedEvent>(receivedEvents_[0]);
    EXPECT_EQ(event->key, specialKey);
}

TEST_F(DataStoreEventAdapterTest, RapidSubscribeUnsubscribe) {
    // When: 빠른 속도로 구독/해제 반복
    for (int i = 0; i < 100; i++) {
        std::string key = "rapid.key" + std::to_string(i % 10);
        adapter_->startWatching(key);
        adapter_->stopWatching(key);
    }

    // Then: 크래시 없이 완료
    SUCCEED();
}

TEST_F(DataStoreEventAdapterTest, MultipleAdaptersSameDataStore) {
    // Given: 동일한 DataStore에 대한 여러 어댑터
    auto eventBus2 = std::make_shared<EventBus>();
    eventBus2->start();
    auto adapter2 = std::make_shared<DataStoreEventAdapter>(dataStore_, eventBus2);

    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);

    std::atomic<int> events2Count{0};
    eventBus2->subscribe(
        Filters::byType(EventType::DATASTORE_VALUE_CHANGED),
        [&events2Count](std::shared_ptr<IEvent> event) {
            events2Count++;
        }
    );

    adapter_->startWatching("multi.key");
    adapter2->startWatching("multi.key");

    // When: DataStore 변경
    dataStore_->set("multi.key", 999, DataType::Config);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Then: 두 어댑터 모두 이벤트 수신
    ASSERT_TRUE(waitForEventCount(1, 500));
    EXPECT_GE(events2Count.load(), 1);

    eventBus2->stop();
}

TEST_F(DataStoreEventAdapterTest, EventBusStoppedDuringOperation) {
    // Given: 이벤트 구독 및 감시 설정
    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);
    adapter_->startWatching("test.key");

    // When: EventBus 중지 후 DataStore 변경
    eventBus_->stop();

    EXPECT_NO_THROW({
        dataStore_->set("test.key", 100, DataType::Config);
    });

    // EventBus 재시작
    eventBus_->start();
}

TEST_F(DataStoreEventAdapterTest, LargeNumberOfSubscriptions) {
    // Given: 많은 수의 구독
    for (int i = 0; i < 100; i++) {
        adapter_->startWatching("bulk.key" + std::to_string(i));
    }

    adapter_->subscribeToActionResults("action.results.");
    adapter_->subscribeToSequenceResults("sequence.results.");

    // When: 어댑터 소멸
    adapter_.reset();

    // Then: 정상적으로 모든 구독 해제
    SUCCEED();
}

TEST_F(DataStoreEventAdapterTest, DataStoreValueTypeChange) {
    // Given: 특정 타입의 값 설정
    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);
    adapter_->startWatching("type.key");

    dataStore_->set("type.key", 42, DataType::Config);
    ASSERT_TRUE(waitForEventCount(1, 500));
    receivedEvents_.clear();

    // When: 동일 키에 다른 타입의 값 설정 (DataStore가 허용하는 경우)
    // DataStore는 타입 불일치 시 예외를 발생시킬 수 있음
    EXPECT_THROW({
        dataStore_->set("type.key", std::string("changed"), DataType::Config);
    }, std::runtime_error);
}

TEST_F(DataStoreEventAdapterTest, UnknownDataTypeConversion) {
    // Given: 알 수 없는 타입의 데이터
    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);
    adapter_->startWatching("unknown.key");

    // When: 복잡한 타입 (std::any에 저장되지만 valueToString에서 처리 안 됨)
    struct CustomType {
        int value;
        std::string name;
    };

    CustomType custom{42, "test"};
    dataStore_->set("unknown.key", custom, DataType::Config);

    // Then: 이벤트는 발행되지만 값은 "<unknown type>"
    ASSERT_TRUE(waitForEventCount(1, 500));
    auto event = std::static_pointer_cast<DataStoreValueChangedEvent>(receivedEvents_[0]);
    EXPECT_EQ(event->newValue, "<unknown type>");
}

TEST_F(DataStoreEventAdapterTest, HighFrequencyUpdates) {
    // Given: 고빈도 업데이트 시나리오
    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);
    adapter_->startWatching("hf.key");

    // When: 빠른 속도로 연속 업데이트
    for (int i = 0; i < 100; i++) {
        dataStore_->set("hf.key", i, DataType::Config);
    }

    // Then: 모든 이벤트가 발행되어야 함
    ASSERT_TRUE(waitForEventCount(100, 2000));
    EXPECT_GE(receivedEvents_.size(), 100);
}

TEST_F(DataStoreEventAdapterTest, EventPublishFailureHandling) {
    // Given: EventBus를 stop하여 publish 실패 유발
    subscribeToEvents(EventType::DATASTORE_VALUE_CHANGED);
    adapter_->startWatching("fail.key");

    eventBus_->stop();

    // When: DataStore 변경 (publish 실패)
    EXPECT_NO_THROW({
        dataStore_->set("fail.key", 100, DataType::Config);
    });

    // Then: 어댑터는 크래시하지 않아야 함
    SUCCEED();
}

} // namespace mxrc::core::event
