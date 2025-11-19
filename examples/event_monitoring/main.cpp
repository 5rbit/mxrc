// main.cpp - Event Monitoring Example
// Copyright (C) 2025 MXRC Project
//
// 이 예제는 기존 코드 수정 없이 EventBus 구독만으로
// 새로운 모니터링 컴포넌트를 추가하는 방법을 보여줍니다.

#include "ExecutionTimeCollector.h"
#include "StateTransitionLogger.h"

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

#include <iostream>
#include <memory>
#include <thread>

using namespace mxrc::core::event;
using namespace mxrc::core::action;
using namespace mxrc::core::sequence;
using namespace mxrc::core::task;
using namespace mxrc::examples::event_monitoring;

/**
 * @brief 예제 1: ExecutionTimeCollector 사용
 *
 * Action 실행 시간을 자동으로 수집하고 통계를 제공합니다.
 */
void example1_ExecutionTimeCollector() {
    std::cout << "\n===== Example 1: ExecutionTimeCollector =====\n";

    // 1. EventBus 생성 및 시작
    auto eventBus = std::make_shared<EventBus>(1000);
    eventBus->start();

    // 2. ExecutionTimeCollector 생성 및 구독
    //    핵심: 기존 코드 수정 없이 EventBus 구독만으로 동작!
    auto collector = std::make_shared<ExecutionTimeCollector>();
    collector->subscribeToEventBus(eventBus);

    // 3. 기존 코드 그대로 사용 (ActionExecutor)
    auto executor = std::make_shared<ActionExecutor>(eventBus);
    ExecutionContext context;

    // 4. 여러 Action 실행
    std::cout << "Executing actions...\n";
    for (int i = 0; i < 5; i++) {
        auto action = std::make_shared<DelayAction>("action" + std::to_string(i), 50 + i * 10);
        executor->executeAsync(action, context);
        executor->waitForCompletion("action" + std::to_string(i));
    }

    // 작은 지연으로 이벤트 처리 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 5. 수집된 메트릭 조회
    std::cout << "\nCollected Metrics:\n";
    std::cout << "Total executions: " << collector->getTotalExecutionCount() << "\n";

    for (const auto& id : collector->getAllIds()) {
        auto stats = collector->getStatistics(id);
        std::cout << "  " << id << ": "
                  << "count=" << stats.count << ", "
                  << "avg=" << stats.avgTime << "ms, "
                  << "min=" << stats.minTime << "ms, "
                  << "max=" << stats.maxTime << "ms\n";
    }

    // 정리
    eventBus->stop();
}

/**
 * @brief 예제 2: StateTransitionLogger 사용
 *
 * 모든 상태 전환을 로깅합니다.
 */
void example2_StateTransitionLogger() {
    std::cout << "\n===== Example 2: StateTransitionLogger =====\n";

    // 1. EventBus 생성 및 시작
    auto eventBus = std::make_shared<EventBus>(1000);
    eventBus->start();

    // 2. StateTransitionLogger 생성 및 구독
    auto logger = std::make_shared<StateTransitionLogger>();
    logger->setLogToFile("state_transitions.log");  // 파일 로깅 활성화
    logger->subscribeToEventBus(eventBus);

    // 3. 기존 코드 그대로 사용
    auto executor = std::make_shared<ActionExecutor>(eventBus);
    ExecutionContext context;

    // 4. Action 실행
    std::cout << "Executing actions with state logging...\n";
    auto action1 = std::make_shared<DelayAction>("log_action1", 100);
    auto action2 = std::make_shared<DelayAction>("log_action2", 150);

    executor->executeAsync(action1, context);
    executor->waitForCompletion("log_action1");

    executor->executeAsync(action2, context);
    executor->waitForCompletion("log_action2");

    // 작은 지연으로 이벤트 처리 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 5. 로그 출력
    logger->printLogs();

    std::cout << "\nLog file 'state_transitions.log' created.\n";

    // 정리
    eventBus->stop();
}

/**
 * @brief 예제 3: 여러 모니터링 컴포넌트 동시 사용
 *
 * ExecutionTimeCollector와 StateTransitionLogger를 동시에 사용합니다.
 * 두 컴포넌트는 서로 독립적으로 동작합니다.
 */
void example3_MultipleMonitors() {
    std::cout << "\n===== Example 3: Multiple Monitors =====\n";

    // 1. EventBus 생성 및 시작
    auto eventBus = std::make_shared<EventBus>(1000);
    eventBus->start();

    // 2. 두 개의 모니터링 컴포넌트 생성
    auto collector = std::make_shared<ExecutionTimeCollector>();
    auto logger = std::make_shared<StateTransitionLogger>();

    collector->subscribeToEventBus(eventBus);
    logger->subscribeToEventBus(eventBus);

    // 3. Sequence 실행 (더 복잡한 시나리오)
    auto sequenceEngine = std::make_shared<SequenceEngine>(eventBus);
    auto sequenceRegistry = std::make_shared<SequenceRegistry>();
    auto actionFactory = std::make_shared<ActionFactory>();

    // Action 팩토리 등록
    actionFactory->registerFactory("Delay", [](const std::string& id, const auto& params) {
        long duration = 100;
        auto it = params.find("duration");
        if (it != params.end()) {
            duration = std::stol(it->second);
        }
        return std::make_shared<DelayAction>(id, duration);
    });

    // Sequence 정의
    SequenceDefinition seqDef("multi_seq", "Multiple Actions");
    seqDef.addStep(ActionStep("step1", "Delay").addParameter("duration", "80"));
    seqDef.addStep(ActionStep("step2", "Delay").addParameter("duration", "120"));
    seqDef.addStep(ActionStep("step3", "Delay").addParameter("duration", "100"));
    sequenceRegistry->registerDefinition(seqDef);

    // Sequence 실행
    std::cout << "Executing sequence with multiple monitors...\n";
    ExecutionContext context;
    sequenceEngine->startSequence("multi_seq", sequenceRegistry, actionFactory, context);
    sequenceEngine->waitForCompletion("multi_seq");

    // 작은 지연으로 이벤트 처리 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 4. 두 모니터의 결과 출력
    std::cout << "\n--- ExecutionTimeCollector Results ---\n";
    std::cout << "Total executions: " << collector->getTotalExecutionCount() << "\n";

    std::cout << "\n--- StateTransitionLogger Results ---\n";
    std::cout << "Total log entries: " << logger->getLogCount() << "\n";
    logger->printLogs();

    // 정리
    eventBus->stop();
}

/**
 * @brief 메인 함수
 */
int main() {
    std::cout << "===== Event Monitoring Examples =====\n";
    std::cout << "Demonstrating extensibility without core code modification\n";

    try {
        // 예제 1: 실행 시간 수집
        example1_ExecutionTimeCollector();

        // 예제 2: 상태 전환 로깅
        example2_StateTransitionLogger();

        // 예제 3: 여러 모니터 동시 사용
        example3_MultipleMonitors();

        std::cout << "\n===== All Examples Completed Successfully =====\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
