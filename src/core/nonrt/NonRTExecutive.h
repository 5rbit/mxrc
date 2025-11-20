#pragma once

#include <atomic>
#include <memory>
#include <cstdint>
#include <string>
#include <thread>
#include <functional>

// Forward declarations (global namespace)
class DataStore;

namespace mxrc {
namespace core {
namespace event {
class EventBus;
}
namespace task {
class TaskExecutor;
}
namespace action {
class ActionExecutor;
}
namespace sequence {
class SequenceEngine;
}

namespace rt {
namespace ipc {
class SharedMemoryRegion;
struct SharedMemoryData;
}
}

namespace nonrt {

// Non-RT 프로세스 실행기
// TaskExecutor를 래핑하고 RT 프로세스와 공유 메모리를 통해 통신
class NonRTExecutive {
public:
    // shm_name: 공유 메모리 이름 (예: "/mxrc_shm")
    // datastore: DataStore 인스턴스
    // event_bus: EventBus 인스턴스
    NonRTExecutive(const std::string& shm_name,
                   std::shared_ptr<::DataStore> datastore,
                   std::shared_ptr<event::EventBus> event_bus);
    ~NonRTExecutive();

    // 초기화 (공유 메모리 연결)
    // 반환: 성공 0, 실패 -1
    int init();

    // Non-RT 실행 시작
    // 반환: 성공 0, 실패 -1
    int run();

    // 실행 중지
    void stop();

    // TaskExecutor 조회
    task::TaskExecutor* getTaskExecutor() { return task_executor_.get(); }

    // 실행 중 여부
    bool isRunning() const { return running_; }

private:
    // Heartbeat 갱신 스레드
    void heartbeatThread();

    // RT 상태 동기화 스레드 (100ms 주기)
    void syncThread();

    // RT 상태를 DataStore에 반영
    void syncRTStatus();

    // 설정
    std::string shm_name_;

    // 의존성
    std::shared_ptr<::DataStore> datastore_;
    std::shared_ptr<event::EventBus> event_bus_;

    // TaskExecutor 인프라
    std::shared_ptr<task::TaskExecutor> task_executor_;
    std::shared_ptr<action::ActionExecutor> action_executor_;
    std::shared_ptr<sequence::SequenceEngine> sequence_engine_;

    // 공유 메모리
    std::unique_ptr<rt::ipc::SharedMemoryRegion> shm_region_;
    rt::ipc::SharedMemoryData* shm_data_;

    // 스레드
    std::thread heartbeat_thread_;
    std::thread sync_thread_;
    std::atomic<bool> running_;
};

} // namespace nonrt
} // namespace core
} // namespace mxrc
