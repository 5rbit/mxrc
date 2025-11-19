#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <thread>
#include <atomic>

namespace mxrc::core::logging {

// 전역 플러시 스레드 제어
static std::atomic<bool> g_flush_thread_running{false};
static std::thread g_flush_thread;

/**
 * 비동기 로거 초기화
 *
 * 목적: spdlog async_logger를 생성하고 전역 기본 로거로 설정
 *
 * 전제조건:
 * - main() 시작 직후 한 번만 호출
 * - 여러 번 호출 시 undefined behavior
 *
 * 후행조건:
 * - spdlog::default_logger()가 비동기 로거로 설정됨
 * - 로그 파일 logs/mxrc.log 생성
 * - 백그라운드 스레드 시작됨
 * - 주기적 flush 스레드 시작됨 (3초 간격)
 *
 * 예외:
 * - spdlog::spdlog_ex: 로그 디렉토리 생성 실패 시
 * - std::runtime_error: thread pool 초기화 실패 시
 *
 * 성능 계약:
 * - 호출 시간 < 10ms
 * - 메모리 사용량 < 10MB
 */
inline void initialize_async_logger() {
    try {
        // spdlog thread pool 초기화 (큐 크기 8192, 스레드 1개)
        spdlog::init_thread_pool(8192, 1);

        // 콘솔 sink 생성
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        // 파일 sink 생성 (logs/mxrc.log)
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/mxrc.log", true);

        // async_logger 생성
        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        auto async_logger = std::make_shared<spdlog::async_logger>(
            "mxrc_logger",
            sinks.begin(),
            sinks.end(),
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block
        );

        // 로그 패턴 설정
        async_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

        // 로그 레벨 설정 (DEBUG 이상 출력)
        async_logger->set_level(spdlog::level::debug);

        // CRITICAL 레벨 로그는 즉시 플러시
        async_logger->flush_on(spdlog::level::critical);

        // 에러 핸들러 설정
        async_logger->set_error_handler([](const std::string& msg) {
            std::cerr << "Logger error: " << msg << std::endl;
        });

        // 기본 로거로 설정
        spdlog::set_default_logger(async_logger);

        // 주기적 flush 스레드 시작 (3초 간격)
        g_flush_thread_running = true;
        g_flush_thread = std::thread([]() {
            while (g_flush_thread_running.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(3));
                if (g_flush_thread_running.load()) {
                    spdlog::default_logger()->flush();
                }
            }
        });

        spdlog::info("Async logger initialized successfully");

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        throw;
    }
}

/**
 * 로거 종료
 *
 * 목적: 모든 로거를 플러시하고 정리
 *
 * 전제조건:
 * - 애플리케이션 종료 직전 호출
 *
 * 후행조건:
 * - 큐에 남은 모든 메시지 처리됨
 * - 모든 파일 핸들 닫힘
 * - 백그라운드 스레드 종료됨
 * - 주기적 flush 스레드 종료됨
 *
 * 성능 계약:
 * - 호출 시간 < 1초 (큐 크기에 따라 변동)
 */
inline void shutdown_logger() {
    // 주기적 flush 스레드 종료
    if (g_flush_thread_running.load()) {
        g_flush_thread_running = false;
        if (g_flush_thread.joinable()) {
            g_flush_thread.join();
        }
    }

    // spdlog 종료 (모든 로거 플러시 및 스레드 풀 정리)
    spdlog::shutdown();
}

}  // namespace mxrc::core::logging
