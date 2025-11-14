#ifndef MXRC_CORE_SEQUENCE_SEQUENCE_STATUS_H
#define MXRC_CORE_SEQUENCE_SEQUENCE_STATUS_H

#include <string>

namespace mxrc::core::sequence {

/**
 * @brief Sequence 실행 상태
 */
enum class SequenceStatus {
    PENDING,        // 실행 대기 중
    RUNNING,        // 실행 중
    COMPLETED,      // 정상 완료
    FAILED,         // 실패
    CANCELLED,      // 취소됨
    PAUSED,         // 일시정지
    TIMEOUT         // 타임아웃
};

/**
 * @brief SequenceStatus를 문자열로 변환
 */
inline std::string sequenceStatusToString(SequenceStatus status) {
    switch (status) {
        case SequenceStatus::PENDING:    return "PENDING";
        case SequenceStatus::RUNNING:    return "RUNNING";
        case SequenceStatus::COMPLETED:  return "COMPLETED";
        case SequenceStatus::FAILED:     return "FAILED";
        case SequenceStatus::CANCELLED:  return "CANCELLED";
        case SequenceStatus::PAUSED:     return "PAUSED";
        case SequenceStatus::TIMEOUT:    return "TIMEOUT";
        default:                         return "UNKNOWN";
    }
}

} // namespace mxrc::core::sequence

#endif // MXRC_CORE_SEQUENCE_SEQUENCE_STATUS_H
