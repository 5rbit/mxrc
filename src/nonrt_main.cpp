#include "core/nonrt/NonRTExecutive.h"
#include "core/datastore/DataStore.h"
#include "core/event/core/EventBus.h"
#include <spdlog/spdlog.h>
#include <csignal>
#include <atomic>

using namespace mxrc;
using namespace mxrc::core;

// Global flag for signal handling
std::atomic<bool> g_shutdown{false};

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        spdlog::info("Received signal {}, shutting down...", signal);
        g_shutdown = true;
    }
}

int main(int argc, char** argv) {
    // 로깅 초기화
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

    spdlog::info("========================================");
    spdlog::info("  MXRC Non-RT Process");
    spdlog::info("========================================");

    // 시그널 핸들러 등록
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // 공유 메모리 이름 (RT 프로세스와 동일)
    const std::string shm_name = "/mxrc_shm";

    // DataStore 생성
    auto datastore = DataStore::create();
    if (!datastore) {
        spdlog::error("Failed to create DataStore");
        return 1;
    }

    // EventBus 생성
    auto event_bus = std::make_shared<event::EventBus>();

    // NonRTExecutive 생성
    auto executive = std::make_unique<nonrt::NonRTExecutive>(
        shm_name, datastore, event_bus);

    // 초기화
    if (executive->init() != 0) {
        spdlog::error("Failed to initialize NonRTExecutive");
        return 1;
    }

    spdlog::info("NonRTExecutive initialized successfully");

    // 백그라운드 스레드에서 실행
    std::thread exec_thread([&]() {
        executive->run();
    });

    // 메인 스레드는 shutdown 대기
    spdlog::info("Non-RT process running. Press Ctrl+C to stop.");
    while (!g_shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 종료
    spdlog::info("Stopping Non-RT process...");
    executive->stop();

    if (exec_thread.joinable()) {
        exec_thread.join();
    }

    spdlog::info("Non-RT process stopped successfully");
    return 0;
}
