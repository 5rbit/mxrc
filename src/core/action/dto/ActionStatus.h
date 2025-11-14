#ifndef MXRC_CORE_ACTION_ACTION_STATUS_H
#define MXRC_CORE_ACTION_ACTION_STATUS_H

#include <string>

namespace mxrc::core::action {

/**
 * @brief Action 실행 상태
 *
 * Action의 생명주기 동안 거치는 상태들을 정의합니다.
 */
enum class ActionStatus {
    PENDING,    // 대기 중: Action이 생성되었지만 아직 실행되지 않음
    RUNNING,    // 실행 중: Action이 현재 실행 중
    COMPLETED,  // 완료됨: Action이 성공적으로 완료됨
    FAILED,     // 실패: Action 실행 중 오류 발생
    CANCELLED,  // 취소됨: Action이 사용자에 의해 취소됨
    TIMEOUT     // 시간 초과: Action이 제한 시간 내에 완료되지 못함
};

/**
 * @brief ActionStatus를 문자열로 변환
 */
inline std::string actionStatusToString(ActionStatus status) {
    switch (status) {
        case ActionStatus::PENDING:   return "PENDING";
        case ActionStatus::RUNNING:   return "RUNNING";
        case ActionStatus::COMPLETED: return "COMPLETED";
        case ActionStatus::FAILED:    return "FAILED";
        case ActionStatus::CANCELLED: return "CANCELLED";
        case ActionStatus::TIMEOUT:   return "TIMEOUT";
        default:                      return "UNKNOWN";
    }
}

} // namespace mxrc::core::action

#endif // MXRC_CORE_ACTION_ACTION_STATUS_H
