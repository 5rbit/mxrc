#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace mxrc {
namespace core {
namespace rt {

// RT 프로세스 상태
enum class RTState : uint8_t {
    INIT = 0,      // 초기화
    READY,         // 준비 (스케줄 설정 완료)
    RUNNING,       // 실행 중
    PAUSED,        // 일시정지
    SAFE_MODE,     // 안전 모드 (Non-RT heartbeat 실패)
    ERROR,         // 오류 발생
    SHUTDOWN       // 종료
};

// 상태 전환 이벤트
enum class RTEvent : uint8_t {
    START = 0,         // 시작 요청
    PAUSE,             // 일시정지 요청
    RESUME,            // 재개 요청
    STOP,              // 중지 요청
    ERROR_OCCUR,       // 오류 발생
    SAFE_MODE_ENTER,   // 안전 모드 진입 (heartbeat 실패)
    SAFE_MODE_EXIT,    // 안전 모드 탈출 (heartbeat 복구)
    RESET              // 리셋
};

// 상태 전환 콜백
using StateTransitionCallback = std::function<void(RTState from, RTState to, RTEvent event)>;

// RT 상태 머신
class RTStateMachine {
public:
    RTStateMachine();
    ~RTStateMachine() = default;

    // 현재 상태 조회
    RTState getState() const { return current_state_; }

    // 상태 전환 처리
    // event: 발생한 이벤트
    // 반환: 성공 0, 실패 -1 (허용되지 않는 전환)
    int handleEvent(RTEvent event);

    // 상태 전환 콜백 등록
    void setTransitionCallback(StateTransitionCallback callback);

    // 상태 이름 문자열 변환
    static std::string stateToString(RTState state);
    static std::string eventToString(RTEvent event);

private:
    // 상태 전환 검증
    bool isValidTransition(RTState from, RTState to, RTEvent event) const;

    // 상태 전환 실행
    void transitionTo(RTState new_state, RTEvent event);

    RTState current_state_;
    StateTransitionCallback transition_callback_;
};

} // namespace rt
} // namespace core
} // namespace mxrc
