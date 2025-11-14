#pragma once

namespace mxrc::core::sequence {

enum class ActionStatus {
    PENDING,      // 대기 중
    RUNNING,      // 실행 중
    PAUSED,       // 일시정지
    COMPLETED,    // 완료
    FAILED,       // 실패
    CANCELLED,    // 취소됨
    TIMEOUT       // 타임아웃
};

enum class SequenceStatus {
    PENDING,      // 대기 중
    RUNNING,      // 실행 중
    PAUSED,       // 일시정지
    COMPLETED,    // 완료
    FAILED,       // 실패
    CANCELLED     // 취소됨
};

inline const char* toString(ActionStatus status) {
    switch (status) {
        case ActionStatus::PENDING: return "PENDING";
        case ActionStatus::RUNNING: return "RUNNING";
        case ActionStatus::PAUSED: return "PAUSED";
        case ActionStatus::COMPLETED: return "COMPLETED";
        case ActionStatus::FAILED: return "FAILED";
        case ActionStatus::CANCELLED: return "CANCELLED";
        case ActionStatus::TIMEOUT: return "TIMEOUT";
        default: return "UNKNOWN";
    }
}

inline const char* toString(SequenceStatus status) {
    switch (status) {
        case SequenceStatus::PENDING: return "PENDING";
        case SequenceStatus::RUNNING: return "RUNNING";
        case SequenceStatus::PAUSED: return "PAUSED";
        case SequenceStatus::COMPLETED: return "COMPLETED";
        case SequenceStatus::FAILED: return "FAILED";
        case SequenceStatus::CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

} // namespace mxrc::core::sequence

