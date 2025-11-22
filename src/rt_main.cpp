#include "core/rt/RTExecutive.h"
#include "core/rt/RTDataStore.h"
#include "core/rt/ipc/SharedMemory.h"
#include "core/rt/ipc/SharedMemoryData.h"
#include "core/event/core/EventBus.h"
#include <spdlog/spdlog.h>
#include <systemd/sd-daemon.h>
#include <csignal>
#include <atomic>
#include <memory>

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
    spdlog::info("  MXRC RT (Real-Time) Process");
    spdlog::info("========================================");

    // 시그널 핸들러 등록
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // 공유 메모리 이름 (Non-RT 프로세스와 동일)
    const std::string shm_name = "/mxrc_shm";

    // EventBus 생성
    auto event_bus = std::make_shared<event::EventBus>();

    // RTExecutive 생성 (1ms minor cycle, 10ms major cycle)
    auto executive = std::make_unique<core::rt::RTExecutive>(1, 10, event_bus);

    // 공유 메모리 생성
    core::rt::ipc::SharedMemoryRegion shm_region;
    if (shm_region.create(shm_name, sizeof(core::rt::ipc::SharedMemoryData)) != 0) {
        spdlog::error("Failed to create shared memory: {}", shm_name);
        return 1;
    }

    auto* shm_data = static_cast<core::rt::ipc::SharedMemoryData*>(shm_region.getPtr());
    if (!shm_data) {
        spdlog::error("Invalid shared memory pointer");
        return 1;
    }

    // SharedMemoryData 초기화
    new (shm_data) core::rt::ipc::SharedMemoryData();

    // RTExecutive에 공유 메모리 연결
    executive->setSharedMemory(shm_data);
    executive->enableHeartbeatMonitoring(true);

    spdlog::info("RT Executive initialized successfully");

    // Feature 022 P1: Notify systemd that RT is READY (shared memory created)
    // Non-RT process can now safely connect via retry logic
    int notify_result = sd_notify(0, "READY=1\nSTATUS=RT shared memory ready");
    if (notify_result > 0) {
        spdlog::info("systemd notified: RT process ready (shared memory available)");
    } else if (notify_result == 0) {
        spdlog::debug("systemd notification not sent (not running under systemd)");
    } else {
        spdlog::warn("systemd notification failed: {}", strerror(-notify_result));
    }

    // EventBus 시작
    event_bus->start();

    // RT 실행 시작 (백그라운드 스레드)
    std::thread exec_thread([&]() {
        executive->run();
    });

    // 메인 스레드는 shutdown 대기
    spdlog::info("RT process running. Press Ctrl+C to stop.");
    while (!g_shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 종료
    spdlog::info("Stopping RT process...");
    executive->stop();

    if (exec_thread.joinable()) {
        exec_thread.join();
    }

    // EventBus 정지
    event_bus->stop();

    // 공유 메모리 정리
    shm_region.unlink(shm_name);

    spdlog::info("RT process stopped successfully");
    return 0;
}
