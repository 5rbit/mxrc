// monitoring_extension_test.cpp - 모니터링 확장 통합 테스트
// Copyright (C) 2025 MXRC Project

#include "gtest/gtest.h"
#include "event_monitoring/ExecutionTimeCollector.h"
#include "event_monitoring/StateTransitionLogger.h"

#include "core/EventBus.h"
#include "core/ActionExecutor.h"
#include "core/SequenceEngine.h"
#include "core/TaskExecutor.h"
#include "core/ActionFactory.h"
#include "core/SequenceRegistry.h"
#include "core/TaskRegistry.h"
#include "impl/DelayAction.h"
#include "dto/SequenceDefinition.h"
#include "dto/TaskDefinition.h"
#include "util/ExecutionContext.h"

#include <thread>
#include <chrono>

using namespace mxrc::core::event;
using namespace mxrc::core::action;
using namespace mxrc::core::sequence;
using namespace mxrc::core::task;
using namespace mxrc::examples::event_monitoring;

namespace mxrc::integration::event {

/**
 * @brief 모니터링 확장 테스트 픽스처
 */
class MonitoringExtensionTest : public ::testing::Test {
protected:
    void SetUp() override {
        eventBus_ = std::make_shared<EventBus>(10000);
        eventBus_->start();

        // Create Action infrastructure
        actionFactory_ = std::make_shared<ActionFactory>();
        actionExecutor_ = std::make_shared<ActionExecutor>(eventBus_);

        // Register DelayAction
        actionFactory_->registerFactory("Delay", [](const std::string& id, const auto& params) {
            long duration = 100;
            auto it = params.find("duration");
            if (it != params.end()) {
                duration = std::stol(it->second);
            }
            return std::make_shared<DelayAction>(id, duration);
        });

        context_ = std::make_unique<ExecutionContext>();
    }

    void TearDown() override {
        if (eventBus_ && eventBus_->isRunning()) {
            eventBus_->stop();
        }
    }

    std::shared_ptr<EventBus> eventBus_;
    std::shared_ptr<ActionFactory> actionFactory_;
    std::shared_ptr<ActionExecutor> actionExecutor_;
    std::unique_ptr<ExecutionContext> context_;
};

// ===== T069: Custom Metric Collector 추가 테스트 =====

TEST_F(MonitoringExtensionTest, AddCustomMetricCollector) {
    // Given: ExecutionTimeCollector를 EventBus에만 구독 (핵심 코드 수정 없음)
    auto collector = std::make_shared<ExecutionTimeCollector>();
    collector->subscribeToEventBus(eventBus_);

    // When: Action 실행
    auto action1 = std::make_shared<DelayAction>("test1", 100);
    auto action2 = std::make_shared<DelayAction>("test2", 150);
    auto action3 = std::make_shared<DelayAction>("test3", 200);

    actionExecutor_->executeAsync(action1, *context_);
    actionExecutor_->waitForCompletion("test1");

    actionExecutor_->executeAsync(action2, *context_);
    actionExecutor_->waitForCompletion("test2");

    actionExecutor_->executeAsync(action3, *context_);
    actionExecutor_->waitForCompletion("test3");

    // 이벤트 처리 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Then: 메트릭이 수집되어야 함
    EXPECT_EQ(collector->getTotalExecutionCount(), 3);
    EXPECT_TRUE(collector->hasMetrics("test1"));
    EXPECT_TRUE(collector->hasMetrics("test2"));
    EXPECT_TRUE(collector->hasMetrics("test3"));

    // 통계 검증
    auto stats1 = collector->getStatistics("test1");
    EXPECT_EQ(stats1.count, 1);
    EXPECT_GE(stats1.avgTime, 90);  // 최소 90ms (약간의 오차 허용)

    auto stats2 = collector->getStatistics("test2");
    EXPECT_GE(stats2.avgTime, 140);

    auto stats3 = collector->getStatistics("test3");
    EXPECT_GE(stats3.avgTime, 190);

    // 모든 ID 확인
    auto ids = collector->getAllIds();
    EXPECT_EQ(ids.size(), 3);
}

// ===== T070: External Logging System 통합 테스트 =====

TEST_F(MonitoringExtensionTest, ExternalLoggingSystemIntegration) {
    // Given: StateTransitionLogger를 구독 (메모리 로깅)
    auto logger = std::make_shared<StateTransitionLogger>();
    logger->setLogToMemory(true);
    logger->subscribeToEventBus(eventBus_);

    // When: Sequence 실행
    auto sequenceEngine = std::make_shared<SequenceEngine>(actionFactory_, actionExecutor_, eventBus_);
    auto sequenceRegistry = std::make_shared<SequenceRegistry>();

    SequenceDefinition seqDef("log_seq", "Logging Test");
    seqDef.addStep(ActionStep("step1", "Delay").addParameter("duration", "50"));
    seqDef.addStep(ActionStep("step2", "Delay").addParameter("duration", "50"));
    sequenceRegistry->registerDefinition(seqDef);

    auto result = sequenceEngine->execute(seqDef, *context_);

    // 이벤트 처리 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Then: 로그가 수집되어야 함
    auto logs = logger->getLogs();
    EXPECT_GT(logs.size(), 0);

    // Sequence 관련 로그 확인
    auto seqLogs = logger->getLogsForEntity("log_seq");
    EXPECT_GT(seqLogs.size(), 0);

    // 최소한 SEQUENCE_STARTED와 SEQUENCE_COMPLETED 이벤트가 있어야 함
    bool hasStarted = false;
    bool hasCompleted = false;
    for (const auto& log : seqLogs) {
        if (log.eventType == "SEQUENCE_STARTED") hasStarted = true;
        if (log.eventType == "SEQUENCE_COMPLETED") hasCompleted = true;
    }

    EXPECT_TRUE(hasStarted);
    EXPECT_TRUE(hasCompleted);
}

// ===== T071: Multiple Subscribers Independence 테스트 =====

TEST_F(MonitoringExtensionTest, MultipleSubscribersIndependence) {
    // Given: 세 개의 독립적인 구독자
    auto collector1 = std::make_shared<ExecutionTimeCollector>();
    auto collector2 = std::make_shared<ExecutionTimeCollector>();
    auto logger = std::make_shared<StateTransitionLogger>();

    collector1->subscribeToEventBus(eventBus_);
    collector2->subscribeToEventBus(eventBus_);
    logger->subscribeToEventBus(eventBus_);

    // When: Action 실행
    auto action = std::make_shared<DelayAction>("multi_sub_test", 100);

    actionExecutor_->executeAsync(action, *context_);
    actionExecutor_->waitForCompletion("multi_sub_test");

    // 이벤트 처리 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Then: 모든 구독자가 독립적으로 이벤트를 받아야 함
    EXPECT_TRUE(collector1->hasMetrics("multi_sub_test"));
    EXPECT_TRUE(collector2->hasMetrics("multi_sub_test"));
    EXPECT_GT(logger->getLogCount(), 0);

    // 한 구독자를 해제해도 다른 구독자는 영향 없음
    collector1->unsubscribe();

    auto action2 = std::make_shared<DelayAction>("multi_sub_test2", 100);
    actionExecutor_->executeAsync(action2, *context_);
    actionExecutor_->waitForCompletion("multi_sub_test2");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // collector1은 새 이벤트를 받지 못함
    EXPECT_FALSE(collector1->hasMetrics("multi_sub_test2"));

    // collector2와 logger는 여전히 이벤트를 받음
    EXPECT_TRUE(collector2->hasMetrics("multi_sub_test2"));
    auto logs = logger->getLogsForEntity("multi_sub_test2");
    EXPECT_GT(logs.size(), 0);
}

// ===== 추가 테스트: 핵심 코드 무수정 검증 =====

TEST_F(MonitoringExtensionTest, NoCoreCodeModificationRequired) {
    // Given: 기존 시스템을 먼저 생성 (EventBus 없이도 동작)
    auto executorNoEvents = std::make_shared<ActionExecutor>(nullptr);  // EventBus 없음

    // When: Action 실행 (EventBus 없어도 정상 동작)
    auto action = std::make_shared<DelayAction>("no_event", 50);
    executorNoEvents->executeAsync(action, *context_);
    executorNoEvents->waitForCompletion("no_event");

    // Then: 정상 실행됨 (이벤트는 발행되지 않음)
    EXPECT_EQ(action->getStatus(), ActionStatus::COMPLETED);

    // 이제 EventBus와 모니터를 추가
    auto collector = std::make_shared<ExecutionTimeCollector>();
    collector->subscribeToEventBus(eventBus_);

    // 동일한 코드 패턴으로 실행 (핵심 로직 변경 없음)
    auto action2 = std::make_shared<DelayAction>("with_event", 50);
    actionExecutor_->executeAsync(action2, *context_);
    actionExecutor_->waitForCompletion("with_event");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // EventBus가 있으면 메트릭 수집됨
    EXPECT_TRUE(collector->hasMetrics("with_event"));
}

// ===== 추가 테스트: 메트릭 정확성 검증 =====

TEST_F(MonitoringExtensionTest, MetricAccuracy) {
    // Given
    auto collector = std::make_shared<ExecutionTimeCollector>();
    collector->subscribeToEventBus(eventBus_);

    // When: 동일한 Action을 여러 번 실행
    constexpr int NUM_RUNS = 5;
    for (int i = 0; i < NUM_RUNS; i++) {
        auto action = std::make_shared<DelayAction>("repeated_action", 100);
        actionExecutor_->executeAsync(action, *context_);
        actionExecutor_->waitForCompletion("repeated_action");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Then: 통계가 정확해야 함
    auto stats = collector->getStatistics("repeated_action");
    EXPECT_EQ(stats.count, NUM_RUNS);
    EXPECT_GE(stats.avgTime, 90);  // 평균 최소 90ms
    EXPECT_LE(stats.avgTime, 200); // 평균 최대 200ms (여유)
    EXPECT_GE(stats.minTime, 90);
    EXPECT_LE(stats.maxTime, 200);
    EXPECT_EQ(stats.totalTime, stats.count * stats.avgTime);
}

// ===== 추가 테스트: 파일 로깅 검증 =====

TEST_F(MonitoringExtensionTest, FileLoggingWorks) {
    // Given
    const std::string logFile = "/tmp/mxrc_test_log.txt";
    auto logger = std::make_shared<StateTransitionLogger>();
    logger->setLogToFile(logFile);
    logger->subscribeToEventBus(eventBus_);

    // When
    auto action = std::make_shared<DelayAction>("file_log_test", 50);
    actionExecutor_->executeAsync(action, *context_);
    actionExecutor_->waitForCompletion("file_log_test");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Then: 파일이 생성되고 내용이 있어야 함
    std::ifstream file(logFile);
    ASSERT_TRUE(file.is_open());

    std::string line;
    int lineCount = 0;
    while (std::getline(file, line)) {
        lineCount++;
    }
    file.close();

    EXPECT_GT(lineCount, 0);  // 최소 1줄 이상

    // 정리
    std::remove(logFile.c_str());
}

} // namespace mxrc::integration::event
