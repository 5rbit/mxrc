// RTExecutiveEventBusTest.cpp - RTExecutive EventBus 통합 테스트
// Copyright (C) 2025 MXRC Project

#include <gtest/gtest.h>
#include "core/rt/RTExecutive.h"
#include "core/rt/RTStateMachine.h"
#include "core/event/core/EventBus.h"
#include "core/event/dto/RTEvents.h"
#include "core/event/dto/EventType.h"
#include "core/event/util/EventFilter.h"
#include "core/rt/ipc/SharedMemory.h"
#include "core/rt/ipc/SharedMemoryData.h"
#include "core/rt/util/TimeUtils.h"
#include <memory>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>

using namespace mxrc::core;
using namespace mxrc::core::rt;
using namespace mxrc::core::event;

class RTExecutiveEventBusTest : public ::testing::Test {
protected:
    void SetUp() override {
        event_bus_ = std::make_shared<EventBus>();
        captured_events_.clear();
        event_count_.store(0);
    }

    void TearDown() override {
        if (event_bus_) {
            event_bus_->stop();
        }
    }

    void waitForEvents(size_t expected_count, int timeout_ms = 1000) {
        auto start = std::chrono::steady_clock::now();
        while (event_count_.load() < expected_count) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeout_ms) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    std::shared_ptr<EventBus> event_bus_;
    std::vector<std::shared_ptr<IEvent>> captured_events_;
    std::atomic<size_t> event_count_{0};
};

// 1. RT 상태 변경 이벤트 발행 테스트
TEST_F(RTExecutiveEventBusTest, PublishesStateChangedEvents) {
    // EventBus 시작
    event_bus_->start();

    // RTStateChangedEvent 구독
    event_bus_->subscribe(
        Filters::byType(EventType::RT_STATE_CHANGED),
        [this](std::shared_ptr<IEvent> event) {
            captured_events_.push_back(event);
            event_count_.fetch_add(1);
        }
    );

    // RTExecutive 생성 (INIT -> READY 전환 발생)
    RTExecutive executive(10, 100, event_bus_);

    // 이벤트 대기 (INIT -> READY)
    waitForEvents(1);

    // 검증
    ASSERT_EQ(event_count_.load(), 1);
    ASSERT_EQ(captured_events_.size(), 1);

    auto state_event = std::dynamic_pointer_cast<RTStateChangedEvent>(captured_events_[0]);
    ASSERT_NE(state_event, nullptr);
    EXPECT_EQ(state_event->getFromState(), "INIT");
    EXPECT_EQ(state_event->getToState(), "READY");
    EXPECT_EQ(state_event->getTriggerEvent(), "START");
    EXPECT_EQ(state_event->getTargetId(), "rt_executive");
}

// 2. EventBus가 nullptr인 경우 이벤트 발행하지 않음
TEST_F(RTExecutiveEventBusTest, NoEventsWhenEventBusIsNull) {
    // EventBus 없이 RTExecutive 생성
    RTExecutive executive(10, 100, nullptr);

    // 상태 전환 발생 (INIT -> READY)
    // 이벤트는 발행되지 않아야 함 (크래시가 발생하지 않음을 확인)

    SUCCEED();
}

// 3. SAFE_MODE 진입 이벤트 발행 테스트
TEST_F(RTExecutiveEventBusTest, PublishesSafeModeEnteredEvent) {
    // EventBus 시작
    event_bus_->start();

    // RTSafeModeEnteredEvent 구독
    event_bus_->subscribe(
        Filters::byType(EventType::RT_SAFE_MODE_ENTERED),
        [this](std::shared_ptr<IEvent> event) {
            captured_events_.push_back(event);
            event_count_.fetch_add(1);
        }
    );

    // RTExecutive 생성
    RTExecutive executive(1, 10, event_bus_);  // 1ms minor cycle

    // 공유 메모리 생성
    const std::string shm_name = "/mxrc_test_safemode_enter";
    ipc::SharedMemoryRegion shm_region;
    ASSERT_EQ(shm_region.create(shm_name, sizeof(ipc::SharedMemoryData)), 0);

    auto* shm_data = static_cast<ipc::SharedMemoryData*>(shm_region.getPtr());
    new (shm_data) ipc::SharedMemoryData();

    // RTExecutive에 공유 메모리 연결
    executive.setSharedMemory(shm_data);
    executive.enableHeartbeatMonitoring(true);

    // Non-RT heartbeat를 과거 시각으로 설정 (timeout 유발)
    uint64_t now_ns = util::getMonotonicTimeNs();
    uint64_t old_time = now_ns - (ipc::SharedMemoryData::HEARTBEAT_TIMEOUT_NS + 1'000'000);
    shm_data->nonrt_heartbeat_ns.store(old_time, std::memory_order_release);

    // RT 실행 시작 (별도 스레드)
    std::thread rt_thread([&executive]() {
        executive.run();
    });

    // SAFE_MODE 진입 이벤트 대기
    waitForEvents(1, 2000);

    // 정지
    executive.stop();
    rt_thread.join();

    // 공유 메모리 정리
    shm_region.unlink(shm_name);

    // 검증
    ASSERT_GE(event_count_.load(), 1);

    bool found_safemode_event = false;
    for (const auto& event : captured_events_) {
        auto safemode_event = std::dynamic_pointer_cast<RTSafeModeEnteredEvent>(event);
        if (safemode_event) {
            found_safemode_event = true;
            EXPECT_GT(safemode_event->getTimeoutMs(), 0);
            EXPECT_EQ(safemode_event->getReason(), "Non-RT heartbeat timeout");
            EXPECT_EQ(safemode_event->getTargetId(), "rt_executive");
            break;
        }
    }
    EXPECT_TRUE(found_safemode_event);
}

// 4. SAFE_MODE 복구 이벤트 발행 테스트
TEST_F(RTExecutiveEventBusTest, PublishesSafeModeExitedEvent) {
    // EventBus 시작
    event_bus_->start();

    // RTSafeModeEnteredEvent와 RTSafeModeExitedEvent 구독
    event_bus_->subscribe(
        Filters::byType(EventType::RT_SAFE_MODE_ENTERED),
        [this](std::shared_ptr<IEvent> event) {
            captured_events_.push_back(event);
            event_count_.fetch_add(1);
        }
    );
    event_bus_->subscribe(
        Filters::byType(EventType::RT_SAFE_MODE_EXITED),
        [this](std::shared_ptr<IEvent> event) {
            captured_events_.push_back(event);
            event_count_.fetch_add(1);
        }
    );

    // RTExecutive 생성
    RTExecutive executive(1, 10, event_bus_);  // 1ms minor cycle

    // 공유 메모리 생성
    const std::string shm_name = "/mxrc_test_safemode_exit";
    ipc::SharedMemoryRegion shm_region;
    ASSERT_EQ(shm_region.create(shm_name, sizeof(ipc::SharedMemoryData)), 0);

    auto* shm_data = static_cast<ipc::SharedMemoryData*>(shm_region.getPtr());
    new (shm_data) ipc::SharedMemoryData();

    // RTExecutive에 공유 메모리 연결
    executive.setSharedMemory(shm_data);
    executive.enableHeartbeatMonitoring(true);

    // Non-RT heartbeat를 과거 시각으로 설정 (timeout 유발)
    uint64_t now_ns = util::getMonotonicTimeNs();
    uint64_t old_time = now_ns - (ipc::SharedMemoryData::HEARTBEAT_TIMEOUT_NS + 1'000'000);
    shm_data->nonrt_heartbeat_ns.store(old_time, std::memory_order_release);

    // RT 실행 시작 (별도 스레드)
    std::atomic<bool> rt_running{true};
    std::thread rt_thread([&executive, &rt_running]() {
        executive.run();
        rt_running = false;
    });

    // SAFE_MODE 진입 대기
    waitForEvents(1, 2000);

    // Heartbeat 복구 (현재 시각으로 갱신)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    now_ns = util::getMonotonicTimeNs();
    shm_data->nonrt_heartbeat_ns.store(now_ns, std::memory_order_release);

    // SAFE_MODE 복구 이벤트 대기
    waitForEvents(2, 2000);

    // 정지
    executive.stop();
    rt_thread.join();

    // 공유 메모리 정리
    shm_region.unlink(shm_name);

    // 검증
    ASSERT_GE(event_count_.load(), 2);

    bool found_exit_event = false;
    for (const auto& event : captured_events_) {
        auto exit_event = std::dynamic_pointer_cast<RTSafeModeExitedEvent>(event);
        if (exit_event) {
            found_exit_event = true;
            EXPECT_GT(exit_event->getDowntimeMs(), 0);
            EXPECT_EQ(exit_event->getTargetId(), "rt_executive");
            break;
        }
    }
    EXPECT_TRUE(found_exit_event);
}

// 5. 이벤트 데이터 정확성 테스트
TEST_F(RTExecutiveEventBusTest, EventsContainCorrectData) {
    // EventBus 시작
    event_bus_->start();

    // RTStateChangedEvent 구독
    event_bus_->subscribe(
        Filters::byType(EventType::RT_STATE_CHANGED),
        [this](std::shared_ptr<IEvent> event) {
            captured_events_.push_back(event);
            event_count_.fetch_add(1);
        }
    );

    // RTExecutive 생성
    RTExecutive executive(10, 100, event_bus_);

    // 이벤트 대기
    waitForEvents(1);

    // 검증
    ASSERT_EQ(captured_events_.size(), 1);

    auto state_event = std::dynamic_pointer_cast<RTStateChangedEvent>(captured_events_[0]);
    ASSERT_NE(state_event, nullptr);

    // 이벤트 기본 속성 검증
    EXPECT_FALSE(state_event->getEventId().empty());
    EXPECT_EQ(state_event->getType(), EventType::RT_STATE_CHANGED);
    EXPECT_EQ(state_event->getTypeName(), "RT_STATE_CHANGED");
    EXPECT_EQ(state_event->getTargetId(), "rt_executive");

    // 타임스탬프가 유효한지 확인
    auto timestamp = state_event->getTimestamp();
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp);
    EXPECT_LT(diff.count(), 10);  // 10초 이내
}
