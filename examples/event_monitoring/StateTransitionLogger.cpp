// StateTransitionLogger.cpp - 상태 전환 로거 구현
// Copyright (C) 2025 MXRC Project

#include "StateTransitionLogger.h"
#include "dto/ActionEvents.h"
#include "dto/SequenceEvents.h"
#include "dto/TaskEvents.h"
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace mxrc::core::event;

namespace mxrc::examples::event_monitoring {

StateTransitionLogger::StateTransitionLogger() = default;

StateTransitionLogger::~StateTransitionLogger() {
    unsubscribe();
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void StateTransitionLogger::subscribeToEventBus(std::shared_ptr<IEventBus> eventBus) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (eventBus_) {
        unsubscribe();
    }

    eventBus_ = eventBus;

    if (!eventBus_) {
        return;
    }

    // 모든 이벤트 구독 (필터 없음)
    auto subId = eventBus_->subscribe(
        [](auto e) { return true; },  // 모든 이벤트
        [this](std::shared_ptr<IEvent> event) {
            handleEvent(event);
        });
    subscriptionIds_.push_back(subId);
}

void StateTransitionLogger::unsubscribe() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (eventBus_) {
        for (const auto& subId : subscriptionIds_) {
            eventBus_->unsubscribe(subId);
        }
        subscriptionIds_.clear();
        eventBus_.reset();
    }
}

void StateTransitionLogger::setLogToFile(const std::string& filename, bool append) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (logFile_.is_open()) {
        logFile_.close();
    }

    logFilename_ = filename;
    auto mode = append ? (std::ios::out | std::ios::app) : std::ios::out;
    logFile_.open(filename, mode);

    logToFile_ = logFile_.is_open();
}

void StateTransitionLogger::disableFileLogging() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (logFile_.is_open()) {
        logFile_.close();
    }
    logToFile_ = false;
}

void StateTransitionLogger::setLogToMemory(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    logToMemory_ = enable;
}

std::vector<StateTransitionLogger::LogEntry> StateTransitionLogger::getLogs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return logs_;
}

size_t StateTransitionLogger::getLogCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return logs_.size();
}

void StateTransitionLogger::clearLogs() {
    std::lock_guard<std::mutex> lock(mutex_);
    logs_.clear();
}

void StateTransitionLogger::printLogs() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::cout << "\n===== State Transition Logs (" << logs_.size() << " entries) =====\n";
    for (const auto& log : logs_) {
        std::cout << formatTimestamp(log.timestamp) << " | "
                  << std::setw(12) << log.entityType << " | "
                  << std::setw(20) << log.entityId << " | "
                  << std::setw(25) << log.eventType << " | "
                  << log.message << "\n";
    }
    std::cout << "==========================================================\n";
}

std::vector<StateTransitionLogger::LogEntry> StateTransitionLogger::getLogsForEntity(
    const std::string& entityId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<LogEntry> result;
    for (const auto& log : logs_) {
        if (log.entityId == entityId) {
            result.push_back(log);
        }
    }
    return result;
}

void StateTransitionLogger::handleEvent(std::shared_ptr<IEvent> event) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.eventType = eventTypeToString(event->getType());

    // 이벤트 타입별로 정보 추출
    switch (event->getType()) {
        // Action Events
        case EventType::ACTION_STARTED: {
            auto e = std::static_pointer_cast<ActionStartedEvent>(event);
            entry.entityId = e->actionId;
            entry.entityType = "Action";
            entry.message = "Type: " + e->actionType;
            break;
        }
        case EventType::ACTION_COMPLETED: {
            auto e = std::static_pointer_cast<ActionCompletedEvent>(event);
            entry.entityId = e->actionId;
            entry.entityType = "Action";
            entry.message = "Duration: " + std::to_string(e->durationMs) + "ms";
            break;
        }
        case EventType::ACTION_FAILED: {
            auto e = std::static_pointer_cast<ActionFailedEvent>(event);
            entry.entityId = e->actionId;
            entry.entityType = "Action";
            entry.message = "Error: " + e->errorMessage;
            break;
        }
        case EventType::ACTION_CANCELLED: {
            auto e = std::static_pointer_cast<ActionCancelledEvent>(event);
            entry.entityId = e->actionId;
            entry.entityType = "Action";
            entry.message = "Cancelled";
            break;
        }

        // Sequence Events
        case EventType::SEQUENCE_STARTED: {
            auto e = std::static_pointer_cast<SequenceStartedEvent>(event);
            entry.entityId = e->sequenceId;
            entry.entityType = "Sequence";
            entry.message = "Steps: " + std::to_string(e->totalSteps);
            break;
        }
        case EventType::SEQUENCE_STEP_STARTED: {
            auto e = std::static_pointer_cast<SequenceStepStartedEvent>(event);
            entry.entityId = e->sequenceId;
            entry.entityType = "Sequence";
            entry.message = "Step: " + e->stepId + " (" + std::to_string(e->stepIndex + 1) + "/" + std::to_string(e->totalSteps) + ")";
            break;
        }
        case EventType::SEQUENCE_COMPLETED: {
            auto e = std::static_pointer_cast<SequenceCompletedEvent>(event);
            entry.entityId = e->sequenceId;
            entry.entityType = "Sequence";
            entry.message = "Duration: " + std::to_string(e->durationMs) + "ms";
            break;
        }
        case EventType::SEQUENCE_FAILED: {
            auto e = std::static_pointer_cast<SequenceFailedEvent>(event);
            entry.entityId = e->sequenceId;
            entry.entityType = "Sequence";
            entry.message = "Failed at step: " + std::to_string(e->failedStepIndex);
            break;
        }

        // Task Events
        case EventType::TASK_STARTED: {
            auto e = std::static_pointer_cast<TaskStartedEvent>(event);
            entry.entityId = e->taskId;
            entry.entityType = "Task";
            entry.message = "Mode: " + e->executionMode;
            break;
        }
        case EventType::TASK_COMPLETED: {
            auto e = std::static_pointer_cast<TaskCompletedEvent>(event);
            entry.entityId = e->taskId;
            entry.entityType = "Task";
            entry.message = "Completed";
            break;
        }
        case EventType::TASK_FAILED: {
            auto e = std::static_pointer_cast<TaskFailedEvent>(event);
            entry.entityId = e->taskId;
            entry.entityType = "Task";
            entry.message = "Error: " + e->errorMessage;
            break;
        }

        default:
            entry.entityId = event->getEventId();
            entry.entityType = "Unknown";
            entry.message = "";
            break;
    }

    logEntry(entry);
}

void StateTransitionLogger::logEntry(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 메모리에 저장
    if (logToMemory_) {
        logs_.push_back(entry);
    }

    // 파일에 저장
    if (logToFile_ && logFile_.is_open()) {
        logFile_ << formatTimestamp(entry.timestamp) << " | "
                 << entry.entityType << " | "
                 << entry.entityId << " | "
                 << entry.eventType << " | "
                 << entry.message << "\n";
        logFile_.flush();
    }
}

std::string StateTransitionLogger::eventTypeToString(EventType type) const {
    switch (type) {
        case EventType::ACTION_STARTED: return "ACTION_STARTED";
        case EventType::ACTION_COMPLETED: return "ACTION_COMPLETED";
        case EventType::ACTION_FAILED: return "ACTION_FAILED";
        case EventType::ACTION_CANCELLED: return "ACTION_CANCELLED";
        case EventType::ACTION_TIMEOUT: return "ACTION_TIMEOUT";

        case EventType::SEQUENCE_STARTED: return "SEQUENCE_STARTED";
        case EventType::SEQUENCE_STEP_STARTED: return "SEQUENCE_STEP_STARTED";
        case EventType::SEQUENCE_STEP_COMPLETED: return "SEQUENCE_STEP_COMPLETED";
        case EventType::SEQUENCE_COMPLETED: return "SEQUENCE_COMPLETED";
        case EventType::SEQUENCE_FAILED: return "SEQUENCE_FAILED";
        case EventType::SEQUENCE_CANCELLED: return "SEQUENCE_CANCELLED";
        case EventType::SEQUENCE_PAUSED: return "SEQUENCE_PAUSED";
        case EventType::SEQUENCE_RESUMED: return "SEQUENCE_RESUMED";

        case EventType::TASK_STARTED: return "TASK_STARTED";
        case EventType::TASK_COMPLETED: return "TASK_COMPLETED";
        case EventType::TASK_FAILED: return "TASK_FAILED";
        case EventType::TASK_CANCELLED: return "TASK_CANCELLED";
        case EventType::TASK_SCHEDULED: return "TASK_SCHEDULED";

        case EventType::DATASTORE_VALUE_CHANGED: return "DATASTORE_VALUE_CHANGED";

        default: return "UNKNOWN";
    }
}

std::string StateTransitionLogger::formatTimestamp(
    const std::chrono::system_clock::time_point& tp) const {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return ss.str();
}

} // namespace mxrc::examples::event_monitoring
