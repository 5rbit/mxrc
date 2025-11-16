// TriggerManager 단위 테스트
// Phase 3B-3: 이벤트 기반 실행 테스트

#include "gtest/gtest.h"
#include "core/task/core/TriggerManager.h"
#include "core/action/util/ExecutionContext.h"
#include <atomic>

namespace mxrc::core::task {

using namespace mxrc::core::action;

/**
 * @brief TriggerManager 테스트
 */
class TriggerManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = std::make_unique<TriggerManager>();
    }

    void TearDown() override {
        manager->clear();
        manager.reset();
    }

    std::unique_ptr<TriggerManager> manager;
};

/**
 * @brief 트리거 등록 및 실행 기본 테스트
 */
TEST_F(TriggerManagerTest, BasicTriggerRegistrationAndFire) {
    std::atomic<int> count{0};

    auto callback = [&count](const std::string& data, ExecutionContext& ctx) {
        count++;
    };

    manager->registerTrigger("task1", "event1", callback);

    EXPECT_TRUE(manager->hasTrigger("task1", "event1"));

    // 이벤트 발생
    manager->fireEvent("event1");

    EXPECT_EQ(count, 1);
    EXPECT_EQ(manager->getTriggerCount("task1"), 1);
}

/**
 * @brief 여러 이벤트 등록 테스트
 */
TEST_F(TriggerManagerTest, MultipleEventRegistration) {
    std::atomic<int> count1{0};
    std::atomic<int> count2{0};

    manager->registerTrigger("task1", "event1",
        [&count1](const std::string& data, ExecutionContext& ctx) { count1++; });

    manager->registerTrigger("task2", "event2",
        [&count2](const std::string& data, ExecutionContext& ctx) { count2++; });

    EXPECT_TRUE(manager->hasTrigger("task1", "event1"));
    EXPECT_TRUE(manager->hasTrigger("task2", "event2"));

    manager->fireEvent("event1");
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 0);

    manager->fireEvent("event2");
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);
}

/**
 * @brief 한 이벤트에 여러 Task 등록 테스트
 */
TEST_F(TriggerManagerTest, MultipleTasksOnSameEvent) {
    std::atomic<int> count1{0};
    std::atomic<int> count2{0};

    manager->registerTrigger("task1", "event1",
        [&count1](const std::string& data, ExecutionContext& ctx) { count1++; });

    manager->registerTrigger("task2", "event1",
        [&count2](const std::string& data, ExecutionContext& ctx) { count2++; });

    // 한 이벤트 발생으로 두 Task 모두 실행
    manager->fireEvent("event1");

    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);
    EXPECT_EQ(manager->getTriggerCount("task1"), 1);
    EXPECT_EQ(manager->getTriggerCount("task2"), 1);
}

/**
 * @brief 이벤트 데이터 전달 테스트
 */
TEST_F(TriggerManagerTest, EventDataPassing) {
    std::string receivedData;

    auto callback = [&receivedData](const std::string& data, ExecutionContext& ctx) {
        receivedData = data;
        // Context에서도 확인 가능
        auto eventData = ctx.getVariable("event_data");
        EXPECT_TRUE(eventData.has_value());
    };

    manager->registerTrigger("task1", "event1", callback);

    manager->fireEvent("event1", "test_data");

    EXPECT_EQ(receivedData, "test_data");
}

/**
 * @brief 트리거 해제 테스트
 */
TEST_F(TriggerManagerTest, UnregisterTrigger) {
    std::atomic<int> count{0};

    auto callback = [&count](const std::string& data, ExecutionContext& ctx) {
        count++;
    };

    manager->registerTrigger("task1", "event1", callback);

    manager->fireEvent("event1");
    EXPECT_EQ(count, 1);

    // 트리거 해제
    manager->unregisterTrigger("task1", "event1");
    EXPECT_FALSE(manager->hasTrigger("task1", "event1"));

    manager->fireEvent("event1");
    EXPECT_EQ(count, 1);  // 더 이상 증가하지 않음
}

/**
 * @brief 모든 이벤트에서 트리거 해제 테스트
 */
TEST_F(TriggerManagerTest, UnregisterAllTriggers) {
    std::atomic<int> count{0};

    auto callback = [&count](const std::string& data, ExecutionContext& ctx) {
        count++;
    };

    manager->registerTrigger("task1", "event1", callback);
    manager->registerTrigger("task1", "event2", callback);

    manager->fireEvent("event1");
    manager->fireEvent("event2");
    EXPECT_EQ(count, 2);

    // 모든 이벤트에서 해제
    manager->unregisterTrigger("task1");

    manager->fireEvent("event1");
    manager->fireEvent("event2");
    EXPECT_EQ(count, 2);  // 더 이상 증가하지 않음
}

/**
 * @brief 트리거 실행 횟수 추적 테스트
 */
TEST_F(TriggerManagerTest, TriggerCountTracking) {
    auto callback = [](const std::string& data, ExecutionContext& ctx) {};

    manager->registerTrigger("task1", "event1", callback);

    EXPECT_EQ(manager->getTriggerCount("task1"), 0);

    manager->fireEvent("event1");
    EXPECT_EQ(manager->getTriggerCount("task1"), 1);

    manager->fireEvent("event1");
    EXPECT_EQ(manager->getTriggerCount("task1"), 2);

    manager->fireEvent("event1");
    EXPECT_EQ(manager->getTriggerCount("task1"), 3);
}

/**
 * @brief 등록되지 않은 이벤트 발생 테스트
 */
TEST_F(TriggerManagerTest, FireNonRegisteredEvent) {
    // 등록되지 않은 이벤트 발생 (오류 없이 처리)
    manager->fireEvent("non_existent_event");

    // 정상 종료 확인
    EXPECT_TRUE(true);
}

/**
 * @brief 콜백 예외 처리 테스트
 */
TEST_F(TriggerManagerTest, CallbackExceptionHandling) {
    std::atomic<int> count1{0};
    std::atomic<int> count2{0};

    // 첫 번째 콜백은 예외 발생
    manager->registerTrigger("task1", "event1",
        [&count1](const std::string& data, ExecutionContext& ctx) {
            count1++;
            throw std::runtime_error("Test exception");
        });

    // 두 번째 콜백은 정상 실행
    manager->registerTrigger("task2", "event1",
        [&count2](const std::string& data, ExecutionContext& ctx) {
            count2++;
        });

    manager->fireEvent("event1");

    // 첫 번째 콜백 예외 발생해도 두 번째 콜백은 실행됨
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);
}

/**
 * @brief clear 테스트
 */
TEST_F(TriggerManagerTest, ClearAllTriggers) {
    manager->registerTrigger("task1", "event1",
        [](const std::string& data, ExecutionContext& ctx) {});

    manager->registerTrigger("task2", "event2",
        [](const std::string& data, ExecutionContext& ctx) {});

    EXPECT_TRUE(manager->hasTrigger("task1", "event1"));
    EXPECT_TRUE(manager->hasTrigger("task2", "event2"));

    manager->clear();

    EXPECT_FALSE(manager->hasTrigger("task1", "event1"));
    EXPECT_FALSE(manager->hasTrigger("task2", "event2"));
}

/**
 * @brief ExecutionContext 공유 테스트
 */
TEST_F(TriggerManagerTest, ExecutionContextSharing) {
    std::atomic<int> sum{0};

    auto callback1 = [](const std::string& data, ExecutionContext& ctx) {
        ctx.setVariable("value1", 10);
    };

    auto callback2 = [&sum](const std::string& data, ExecutionContext& ctx) {
        auto value1 = ctx.getVariable("value1");
        if (value1.has_value()) {
            sum += std::any_cast<int>(value1.value());
        }
    };

    manager->registerTrigger("task1", "event1", callback1);
    manager->registerTrigger("task2", "event1", callback2);

    manager->fireEvent("event1");

    // task1이 먼저 실행되고 task2가 그 값을 읽을 수 있음
    EXPECT_EQ(sum, 10);
}

/**
 * @brief 복잡한 이벤트 시나리오 테스트
 */
TEST_F(TriggerManagerTest, ComplexEventScenario) {
    std::atomic<int> sensorTriggerCount{0};
    std::atomic<int> alarmTriggerCount{0};

    // 센서 이벤트
    manager->registerTrigger("sensor_task", "sensor_reading",
        [&sensorTriggerCount](const std::string& data, ExecutionContext& ctx) {
            sensorTriggerCount++;
            // 센서 값이 임계값 초과 시 알람 이벤트 발생 (이 테스트에서는 단순 증가)
        });

    // 알람 이벤트
    manager->registerTrigger("alarm_task", "alarm_triggered",
        [&alarmTriggerCount](const std::string& data, ExecutionContext& ctx) {
            alarmTriggerCount++;
        });

    // 센서 읽기 3회
    manager->fireEvent("sensor_reading", "value:50");
    manager->fireEvent("sensor_reading", "value:75");
    manager->fireEvent("sensor_reading", "value:100");

    EXPECT_EQ(sensorTriggerCount, 3);

    // 알람 트리거
    manager->fireEvent("alarm_triggered", "high_temperature");

    EXPECT_EQ(alarmTriggerCount, 1);
}

} // namespace mxrc::core::task
