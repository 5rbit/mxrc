// StateTransitionLogger.h - 상태 전환 로거
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_EXAMPLES_EVENT_MONITORING_STATETRANSITIONLOGGER_H
#define MXRC_EXAMPLES_EVENT_MONITORING_STATETRANSITIONLOGGER_H

#include "interfaces/IEventBus.h"
#include "dto/EventType.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <fstream>

namespace mxrc::examples::event_monitoring {

/**
 * @brief 상태 전환 로거
 *
 * Action, Sequence, Task의 모든 상태 전환을 로깅합니다.
 * 파일 또는 메모리에 로그를 기록할 수 있습니다.
 *
 * **사용 예시**:
 * @code
 * auto logger = std::make_shared<StateTransitionLogger>();
 * logger->setLogToFile("state_transitions.log");
 * logger->subscribeToEventBus(eventBus);
 *
 * // Action/Sequence/Task 실행...
 *
 * // 로그 조회
 * auto logs = logger->getLogs();
 * logger->printLogs();
 * @endcode
 */
class StateTransitionLogger {
public:
    /**
     * @brief 상태 전환 로그 엔트리
     */
    struct LogEntry {
        std::chrono::system_clock::time_point timestamp;
        std::string eventType;      ///< 이벤트 타입
        std::string entityId;       ///< Action/Sequence/Task ID
        std::string entityType;     ///< "Action", "Sequence", "Task"
        std::string message;        ///< 추가 메시지
    };

    StateTransitionLogger();
    ~StateTransitionLogger();

    // 복사/이동 방지
    StateTransitionLogger(const StateTransitionLogger&) = delete;
    StateTransitionLogger& operator=(const StateTransitionLogger&) = delete;

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
     * @brief 파일 로깅 활성화
     *
     * @param filename 로그 파일 경로
     * @param append true이면 기존 파일에 추가, false이면 덮어쓰기
     */
    void setLogToFile(const std::string& filename, bool append = false);

    /**
     * @brief 파일 로깅 비활성화
     */
    void disableFileLogging();

    /**
     * @brief 메모리 로깅 활성화/비활성화
     *
     * @param enable true이면 메모리에 로그 저장
     */
    void setLogToMemory(bool enable);

    /**
     * @brief 수집된 로그 조회
     *
     * @return 로그 엔트리 목록
     */
    std::vector<LogEntry> getLogs() const;

    /**
     * @brief 로그 개수 조회
     *
     * @return 로그 개수
     */
    size_t getLogCount() const;

    /**
     * @brief 메모리 로그 삭제
     */
    void clearLogs();

    /**
     * @brief 로그를 콘솔에 출력
     */
    void printLogs() const;

    /**
     * @brief 특정 엔티티의 로그만 조회
     *
     * @param entityId Action/Sequence/Task ID
     * @return 해당 엔티티의 로그 목록
     */
    std::vector<LogEntry> getLogsForEntity(const std::string& entityId) const;

private:
    void handleEvent(std::shared_ptr<mxrc::core::event::IEvent> event);
    void logEntry(const LogEntry& entry);
    std::string eventTypeToString(mxrc::core::event::EventType type) const;
    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) const;

    mutable std::mutex mutex_;
    std::vector<LogEntry> logs_;
    std::shared_ptr<mxrc::core::event::IEventBus> eventBus_;
    std::vector<std::string> subscriptionIds_;

    bool logToMemory_ = true;
    bool logToFile_ = false;
    std::ofstream logFile_;
    std::string logFilename_;
};

} // namespace mxrc::examples::event_monitoring

#endif // MXRC_EXAMPLES_EVENT_MONITORING_STATETRANSITIONLOGGER_H
