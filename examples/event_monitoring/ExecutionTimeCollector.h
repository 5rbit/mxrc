// ExecutionTimeCollector.h - 실행 시간 메트릭 수집기
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_EXAMPLES_EVENT_MONITORING_EXECUTIONTIMECOLLECTOR_H
#define MXRC_EXAMPLES_EVENT_MONITORING_EXECUTIONTIMECOLLECTOR_H

#include "interfaces/IEventBus.h"
#include "dto/ActionEvents.h"
#include "dto/SequenceEvents.h"
#include "dto/TaskEvents.h"
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>

namespace mxrc::examples::event_monitoring {

/**
 * @brief 실행 시간 메트릭 수집기
 *
 * Action, Sequence, Task의 실행 시간을 수집하고 통계를 제공합니다.
 * EventBus 구독만으로 동작하며 핵심 코드 수정이 필요 없습니다.
 *
 * **사용 예시**:
 * @code
 * auto collector = std::make_shared<ExecutionTimeCollector>();
 * collector->subscribeToEventBus(eventBus);
 *
 * // Action/Sequence/Task 실행...
 *
 * // 메트릭 조회
 * auto avgTime = collector->getAverageExecutionTime("action1");
 * auto stats = collector->getStatistics("action1");
 * @endcode
 */
class ExecutionTimeCollector {
public:
    /**
     * @brief 메트릭 통계 구조체
     */
    struct Statistics {
        size_t count = 0;           ///< 실행 횟수
        long totalTime = 0;          ///< 총 실행 시간 (ms)
        long minTime = LONG_MAX;     ///< 최소 실행 시간 (ms)
        long maxTime = 0;            ///< 최대 실행 시간 (ms)
        double avgTime = 0.0;        ///< 평균 실행 시간 (ms)
    };

    ExecutionTimeCollector() = default;
    ~ExecutionTimeCollector();

    // 복사/이동 방지
    ExecutionTimeCollector(const ExecutionTimeCollector&) = delete;
    ExecutionTimeCollector& operator=(const ExecutionTimeCollector&) = delete;

    /**
     * @brief EventBus에 구독하여 이벤트 수집 시작
     *
     * @param eventBus 구독할 EventBus
     */
    void subscribeToEventBus(std::shared_ptr<mxrc::core::event::IEventBus> eventBus);

    /**
     * @brief EventBus 구독 해제
     */
    void unsubscribe();

    /**
     * @brief 특정 ID의 실행 시간 기록
     *
     * @param id Action/Sequence/Task ID
     * @param durationMs 실행 시간 (ms)
     */
    void recordExecutionTime(const std::string& id, long durationMs);

    /**
     * @brief 특정 ID의 메트릭 존재 여부 확인
     *
     * @param id Action/Sequence/Task ID
     * @return true if metrics exist
     */
    bool hasMetrics(const std::string& id) const;

    /**
     * @brief 특정 ID의 평균 실행 시간 조회
     *
     * @param id Action/Sequence/Task ID
     * @return 평균 실행 시간 (ms), 없으면 0
     */
    double getAverageExecutionTime(const std::string& id) const;

    /**
     * @brief 특정 ID의 통계 조회
     *
     * @param id Action/Sequence/Task ID
     * @return 통계 구조체
     */
    Statistics getStatistics(const std::string& id) const;

    /**
     * @brief 모든 ID 목록 조회
     *
     * @return ID 목록
     */
    std::vector<std::string> getAllIds() const;

    /**
     * @brief 수집된 모든 메트릭 삭제
     */
    void clear();

    /**
     * @brief 총 수집된 실행 횟수
     *
     * @return 총 실행 횟수
     */
    size_t getTotalExecutionCount() const;

private:
    void handleActionCompleted(std::shared_ptr<mxrc::core::event::IEvent> event);
    void handleSequenceCompleted(std::shared_ptr<mxrc::core::event::IEvent> event);
    void handleTaskCompleted(std::shared_ptr<mxrc::core::event::IEvent> event);

    mutable std::mutex mutex_;
    std::map<std::string, std::vector<long>> executionTimes_;
    std::shared_ptr<mxrc::core::event::IEventBus> eventBus_;
    std::vector<std::string> subscriptionIds_;
};

} // namespace mxrc::examples::event_monitoring

#endif // MXRC_EXAMPLES_EVENT_MONITORING_EXECUTIONTIMECOLLECTOR_H
