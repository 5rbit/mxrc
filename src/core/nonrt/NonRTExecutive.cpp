#include "NonRTExecutive.h"
#include "core/datastore/DataStore.h"
#include "core/event/core/EventBus.h"
#include "core/task/core/TaskExecutor.h"
#include "core/action/core/ActionExecutor.h"
#include "core/action/core/ActionFactory.h"
#include "core/sequence/core/SequenceEngine.h"
#include "core/rt/ipc/SharedMemory.h"
#include "core/rt/ipc/SharedMemoryData.h"
#include "core/rt/util/TimeUtils.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

// DataStore is already included above, but we need to ensure template instantiation
namespace mxrc {
// Forward declare to ensure visibility
} // namespace mxrc

namespace mxrc {
namespace core {
namespace nonrt {

NonRTExecutive::NonRTExecutive(const std::string& shm_name,
                               std::shared_ptr<::DataStore> datastore,
                               std::shared_ptr<event::EventBus> event_bus)
    : shm_name_(shm_name)
    , datastore_(datastore)
    , event_bus_(event_bus)
    , shm_data_(nullptr)
    , running_(false) {

    // TaskExecutor 인프라 생성
    auto action_factory = std::make_shared<action::ActionFactory>();
    action_executor_ = std::make_shared<action::ActionExecutor>(event_bus_);

    sequence_engine_ = std::make_shared<sequence::SequenceEngine>(
        action_factory, action_executor_, event_bus_);

    task_executor_ = std::make_shared<task::TaskExecutor>(
        action_factory, action_executor_, sequence_engine_, event_bus_);

    spdlog::info("NonRTExecutive created");
}

NonRTExecutive::~NonRTExecutive() {
    stop();
}

int NonRTExecutive::init() {
    // 공유 메모리 연결 (Feature 022: P1 Retry Logic)
    // RT 프로세스가 먼저 시작하고 공유 메모리를 생성할 때까지 대기
    shm_region_ = std::make_unique<rt::ipc::SharedMemoryRegion>();

    const int MAX_RETRIES = 50;          // 5 seconds (100ms × 50)
    const int RETRY_INTERVAL_MS = 100;   // 100ms fixed interval

    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        if (shm_region_->open(shm_name_) == 0) {
            // 공유 메모리 연결 성공
            shm_data_ = static_cast<rt::ipc::SharedMemoryData*>(shm_region_->getPtr());
            if (!shm_data_) {
                spdlog::error("Invalid shared memory pointer");
                return -1;
            }

            // 초기 heartbeat 설정
            uint64_t now_ns = rt::util::getMonotonicTimeNs();
            shm_data_->nonrt_heartbeat_ns.store(now_ns, std::memory_order_release);

            spdlog::info("NonRTExecutive initialized: shm={} (attempt {})", shm_name_, attempt + 1);
            return 0;
        }

        // 연결 실패 - 재시도
        if (attempt < MAX_RETRIES - 1) {
            spdlog::debug("Waiting for RT shared memory... (attempt {}/{})", attempt + 1, MAX_RETRIES);
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_INTERVAL_MS));
        }
    }

    // 최대 재시도 횟수 초과
    spdlog::error("Failed to connect to shared memory after {} attempts ({} seconds)",
                  MAX_RETRIES, MAX_RETRIES * RETRY_INTERVAL_MS / 1000);
    return -1;
}

int NonRTExecutive::run() {
    if (running_) {
        spdlog::warn("NonRTExecutive already running");
        return -1;
    }

    spdlog::info("NonRTExecutive starting...");

    // EventBus 시작
    event_bus_->start();

    running_ = true;

    // Heartbeat 스레드 시작 (100ms 주기)
    heartbeat_thread_ = std::thread(&NonRTExecutive::heartbeatThread, this);

    // RT 상태 동기화 스레드 시작 (100ms 주기)
    sync_thread_ = std::thread(&NonRTExecutive::syncThread, this);

    spdlog::info("NonRTExecutive started");

    // 메인 루프 (이벤트 처리 등)
    while (running_) {
        // Non-RT 작업 처리
        // TaskExecutor는 별도로 execute() 호출하여 사용
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}

void NonRTExecutive::stop() {
    if (!running_) {
        return;
    }

    spdlog::info("NonRTExecutive stopping...");
    running_ = false;

    // 스레드 종료 대기
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    if (sync_thread_.joinable()) {
        sync_thread_.join();
    }

    // EventBus 정지
    event_bus_->stop();

    // 공유 메모리 해제
    shm_region_.reset();
    shm_data_ = nullptr;

    spdlog::info("NonRTExecutive stopped");
}

void NonRTExecutive::heartbeatThread() {
    spdlog::info("Heartbeat thread started");

    while (running_) {
        if (shm_data_) {
            uint64_t now_ns = rt::util::getMonotonicTimeNs();
            shm_data_->nonrt_heartbeat_ns.store(now_ns, std::memory_order_release);
        }

        // 100ms 주기
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    spdlog::info("Heartbeat thread stopped");
}

void NonRTExecutive::syncThread() {
    spdlog::info("Sync thread started");

    while (running_) {
        syncRTStatus();

        // 100ms 주기
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    spdlog::info("Sync thread stopped");
}

void NonRTExecutive::syncRTStatus() {
    if (!shm_data_ || !datastore_) {
        return;
    }

    // RT → Non-RT 데이터 읽기 (sequence number로 torn read 방지)
    uint32_t seq_before = shm_data_->rt_to_nonrt.sequence;

    int32_t robot_mode = shm_data_->rt_to_nonrt.robot_mode;
    float position_x = shm_data_->rt_to_nonrt.position_x;
    float position_y = shm_data_->rt_to_nonrt.position_y;
    float velocity = shm_data_->rt_to_nonrt.velocity;
    uint64_t timestamp_ns = shm_data_->rt_to_nonrt.timestamp_ns;

    uint32_t seq_after = shm_data_->rt_to_nonrt.sequence;

    // Torn read 검증
    if (seq_before != seq_after) {
        spdlog::trace("Torn read detected, skipping sync (seq: {} vs {})", seq_before, seq_after);
        return;
    }

    // DataStore에 반영
    try {
        datastore_->set("rt.robot_mode", robot_mode, DataType::RobotMode);
        datastore_->set("rt.position_x", position_x, DataType::RobotMode);
        datastore_->set("rt.position_y", position_y, DataType::RobotMode);
        datastore_->set("rt.velocity", velocity, DataType::RobotMode);
        datastore_->set("rt.timestamp_ns", timestamp_ns, DataType::RobotMode);

        spdlog::trace("RT status synced: mode={}, pos=({:.2f},{:.2f}), vel={:.2f}",
                     robot_mode, position_x, position_y, velocity);
    } catch (const std::exception& e) {
        spdlog::error("Failed to sync RT status to DataStore: {}", e.what());
    }
}

} // namespace nonrt
} // namespace core
} // namespace mxrc
