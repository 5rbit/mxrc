# Quickstart: RT/Non-RT 아키텍처 개선

**Date**: 2025-11-21
**Relevant Plan**: [plan.md](plan.md)

이 문서는 새로 도입된 IPC 및 로깅 아키텍처의 핵심 컴포넌트를 사용하는 방법을 간략하게 설명합니다.

## 1. 전제 조건

- `Boost` 라이브러리가 시스템에 설치되어 있고, `CMake`가 이를 찾을 수 있어야 합니다.
- Non-RT 프로세스가 시작될 때 공유 메모리 파일(`shm_log_buffer`)을 생성하고 초기화해야 합니다.

## 2. RT 프로세스에서 로그 기록하기

RT 프로세스의 모든 로그는 `ShmSink`를 사용하는 `spdlog` 로거를 통해 기록되어야 합니다. 이는 로그 메시지를 공유 메모리 링 버퍼로 보냅니다.

```cpp
// 1. main 함수 등에서 로거 설정
#include "spdlog/spdlog.h"
#include "src/core/logging/sinks/shm_sink.h" // (구현 예정)

void setup_rt_logger() {
    // 공유 메모리 버퍼에 접근하는 sink 생성
    auto shm_sink = std::make_shared<mxrc::core::logging::ShmSink>();
    
    // 이 sink를 사용하는 로거 생성
    auto rt_logger = std::make_shared<spdlog::logger>("rt_logger", shm_sink);
    
    // 기본 로거로 등록
    spdlog::set_default_logger(rt_logger);
}

// 2. 애플리케이션 코드에서 사용
#include "spdlog/spdlog.h"

void some_real_time_function() {
    SPDLOG_INFO("Real-time task ID {} completed in {} us", 123, 45);
    // 이 로그는 파일 I/O 없이 공유 메모리로 직접 전송됩니다.
}
```

## 3. Non-RT 프로세스에서 로그 소비하기

Non-RT 프로세스는 별도의 스레드에서 `LogConsumer`를 실행하여 공유 메모리에서 로그를 읽고, 파일에 쓰는 표준 `spdlog` 로거로 전달해야 합니다.

```cpp
#include "src/core/nonrt/log_consumer.h" // (구현 예정)
#include <thread>

void run_log_consumer() {
    // 파일에 쓰는 표준 로거 설정
    // setup_file_logger(); 
    
    mxrc::core::nonrt::LogConsumer consumer;
    
    // 별도 스레드에서 로그 소비 루프 실행
    std::thread consumer_thread([&consumer]() {
        consumer.run(); 
    });
    
    consumer_thread.detach();
}
```

## 4. IPC 큐로 Heartbeat 보내기

RT 프로세스는 주기적으로 `IIpcQueue`를 통해 Heartbeat 메시지를 보냅니다.

```cpp
#include "src/core/ipc/IIpcQueue.h" // 인터페이스
#include <memory>

// 큐는 사전에 초기화되어야 함
std::unique_ptr<mxrc::core::ipc::IIpcQueue> ipc_queue; 

void send_heartbeat() {
    mxrc::core::ipc::IpcMessage msg;
    msg.type = mxrc::core::ipc::IpcMessage::MessageType::HEARTBEAT;
    
    if (!ipc_queue->push(msg)) {
        // 큐가 꽉 찼을 때의 예외 처리
    }
}
```
