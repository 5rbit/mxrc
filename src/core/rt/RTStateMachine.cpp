#include "RTStateMachine.h"
#include <spdlog/spdlog.h>

namespace mxrc {
namespace core {
namespace rt {

RTStateMachine::RTStateMachine()
    : current_state_(RTState::INIT), transition_callback_(nullptr) {
    spdlog::info("RTStateMachine initialized in INIT state");
}

int RTStateMachine::handleEvent(RTEvent event) {
    RTState next_state = current_state_;

    // 상태 전환 결정
    switch (current_state_) {
        case RTState::INIT:
            if (event == RTEvent::START) {
                next_state = RTState::READY;
            } else if (event == RTEvent::ERROR_OCCUR) {
                next_state = RTState::ERROR;
            }
            break;

        case RTState::READY:
            if (event == RTEvent::START) {
                next_state = RTState::RUNNING;
            } else if (event == RTEvent::ERROR_OCCUR) {
                next_state = RTState::ERROR;
            } else if (event == RTEvent::STOP) {
                next_state = RTState::SHUTDOWN;
            }
            break;

        case RTState::RUNNING:
            if (event == RTEvent::PAUSE) {
                next_state = RTState::PAUSED;
            } else if (event == RTEvent::STOP) {
                next_state = RTState::SHUTDOWN;
            } else if (event == RTEvent::ERROR_OCCUR) {
                next_state = RTState::ERROR;
            }
            break;

        case RTState::PAUSED:
            if (event == RTEvent::RESUME) {
                next_state = RTState::RUNNING;
            } else if (event == RTEvent::STOP) {
                next_state = RTState::SHUTDOWN;
            } else if (event == RTEvent::ERROR_OCCUR) {
                next_state = RTState::ERROR;
            }
            break;

        case RTState::ERROR:
            if (event == RTEvent::RESET) {
                next_state = RTState::INIT;
            } else if (event == RTEvent::STOP) {
                next_state = RTState::SHUTDOWN;
            }
            break;

        case RTState::SHUTDOWN:
            // 종료 상태에서는 전환 불가
            break;
    }

    // 상태가 변경되지 않으면 실패
    if (next_state == current_state_) {
        spdlog::warn("Invalid state transition: {} -> {} (event: {})",
                     stateToString(current_state_),
                     stateToString(next_state),
                     eventToString(event));
        return -1;
    }

    // 상태 전환 실행
    transitionTo(next_state, event);
    return 0;
}

void RTStateMachine::setTransitionCallback(StateTransitionCallback callback) {
    transition_callback_ = callback;
}

bool RTStateMachine::isValidTransition(RTState from, RTState to, RTEvent event) const {
    // handleEvent에서 이미 검증했으므로 항상 true
    return true;
}

void RTStateMachine::transitionTo(RTState new_state, RTEvent event) {
    RTState old_state = current_state_;
    current_state_ = new_state;

    spdlog::info("State transition: {} -> {} (event: {})",
                 stateToString(old_state),
                 stateToString(new_state),
                 eventToString(event));

    // 콜백 호출
    if (transition_callback_) {
        transition_callback_(old_state, new_state, event);
    }
}

std::string RTStateMachine::stateToString(RTState state) {
    switch (state) {
        case RTState::INIT: return "INIT";
        case RTState::READY: return "READY";
        case RTState::RUNNING: return "RUNNING";
        case RTState::PAUSED: return "PAUSED";
        case RTState::ERROR: return "ERROR";
        case RTState::SHUTDOWN: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

std::string RTStateMachine::eventToString(RTEvent event) {
    switch (event) {
        case RTEvent::START: return "START";
        case RTEvent::PAUSE: return "PAUSE";
        case RTEvent::RESUME: return "RESUME";
        case RTEvent::STOP: return "STOP";
        case RTEvent::ERROR_OCCUR: return "ERROR_OCCUR";
        case RTEvent::RESET: return "RESET";
        default: return "UNKNOWN";
    }
}

} // namespace rt
} // namespace core
} // namespace mxrc
