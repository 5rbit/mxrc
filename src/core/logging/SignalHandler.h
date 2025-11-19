#pragma once

#include <spdlog/spdlog.h>
#include <csignal>
#include <cstdlib>

namespace mxrc::core::logging {

/**
 * 시그널 핸들러
 *
 * 목적: 치명적 시그널 발생 시 로그 플러시 및 백트레이스 기록
 *
 * 처리 시그널:
 * - SIGSEGV: Segmentation fault
 * - SIGABRT: Abort signal
 * - SIGTERM: Termination signal
 *
 * 동작:
 * 1. CRITICAL 로그 기록 (시그널 번호)
 * 2. 백트레이스 기록 (backward-cpp 사용 시)
 * 3. spdlog::shutdown() 호출하여 큐 플러시
 * 4. 기본 핸들러로 복원 및 시그널 재발생
 */
inline void signal_handler(int signal) {
    // CRITICAL 로그 기록 (즉시 플러시됨)
    spdlog::critical("Signal {} received", signal);

    // 백트레이스 기록 (backward-cpp 사용 시)
    #ifdef USE_BACKWARD_CPP
    #include "backward.hpp"
    backward::StackTrace st;
    st.load_here(32);
    backward::Printer p;
    p.print(st, stderr);
    #endif

    // 로그 플러시 (모든 큐 메시지 파일에 기록)
    spdlog::shutdown();

    // 기본 핸들러로 복원
    std::signal(signal, SIG_DFL);

    // 시그널 재발생 (코어 덤프 생성 등)
    std::raise(signal);
}

/**
 * 시그널 핸들러 등록
 *
 * 목적: 치명적 시그널 발생 시 로그 보존을 위한 핸들러 설정
 *
 * 전제조건:
 * - initialize_async_logger() 호출 완료
 * - POSIX 시스템 (Linux/Unix)
 *
 * 후행조건:
 * - SIGSEGV, SIGABRT, SIGTERM 핸들러 등록됨
 * - 크래시 시 백트레이스 기록 + 로그 플러시
 */
inline void register_signal_handlers() {
    std::signal(SIGSEGV, signal_handler);
    std::signal(SIGABRT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    spdlog::info("Signal handlers registered (SIGSEGV, SIGABRT, SIGTERM)");
}

}  // namespace mxrc::core::logging
