# 빠른 시작: 비동기 로깅 시스템 구현 가이드

**브랜치**: `018-async-logging` | **날짜**: 2025-11-19

이 문서는 개발자가 비동기 로깅 시스템을 구현하기 위한 실용적인 가이드입니다.

## 📋 전제 조건

- C++17 이상 컴파일러 (GCC 7.0+, Clang 5.0+)
- spdlog (프로젝트에 이미 포함됨)
- Google Test
- backward-cpp (선택적, 백트레이스 생성용)

## 🚀 구현 순서

### 1단계: Log.h 헤더 파일 작성

**목표**: 비동기 로거 초기화 함수 구현

#### 1.1 파일 생성 (src/core/logging/Log.h)

```cpp
#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

namespace mxrc::core::logging {

// 주기적 flush를 위한 전역 변수
inline std::atomic<bool> g_flush_thread_running{false};
inline std::thread g_flush_thread;

inline void initialize_async_logger() {
    // 1. Sink 생성
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "logs/mxrc.log",
        true  // truncate=false (append 모드)
    );
    file_sink->set_level(spdlog::level::debug);

    // 2. Thread pool 초기화 (큐 크기 8192, 스레드 1개)
    spdlog::init_thread_pool(8192, 1);

    // 3. Async logger 생성
    std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
    auto logger = std::make_shared<spdlog::async_logger>(
        "mxrc_logger",
        sinks.begin(),
        sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block  // 큐 가득 차면 대기
    );

    // 4. 로그 레벨 및 패턴 설정
    logger->set_level(spdlog::level::debug);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

    // 5. CRITICAL 로그 즉시 플러시 정책
    logger->flush_on(spdlog::level::critical);

    // 6. 에러 핸들러 설정 (로거 오류 시 stderr 출력, 애플리케이션 계속)
    logger->set_error_handler([](const std::string& msg) {
        std::cerr << "Logger error: " << msg << std::endl;
    });

    // 7. 기본 로거 설정
    spdlog::set_default_logger(logger);

    // 8. 주기적 flush 스레드 시작
    g_flush_thread_running.store(true);
    g_flush_thread = std::thread([logger]() {
        while (g_flush_thread_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            if (g_flush_thread_running.load()) {
                logger->flush();
            }
        }
    });

    spdlog::info("Async logger initialized");
}

inline void shutdown_logger() {
    // 1. flush 스레드 정지
    g_flush_thread_running.store(false);
    if (g_flush_thread.joinable()) {
        g_flush_thread.join();
    }

    // 2. 남은 로그 플러시 및 정리
    spdlog::shutdown();
}

}  // namespace mxrc::core::logging
```

#### 1.2 main.cpp 통합

```cpp
// src/main.cpp

#include "core/logging/Log.h"

int main() {
    // 로거 초기화 (main 시작 직후)
    mxrc::core::logging::initialize_async_logger();

    spdlog::info("MXRC application started");

    try {
        // ... 애플리케이션 로직 ...

    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        mxrc::core::logging::shutdown_logger();
        return 1;
    }

    // 정상 종료 시 로거 정리
    spdlog::info("MXRC application finished");
    mxrc::core::logging::shutdown_logger();

    return 0;
}
```

#### 1.3 테스트 작성

```cpp
// tests/unit/logging/AsyncLogger_test.cpp

#include "gtest/gtest.h"
#include "core/logging/Log.h"
#include <fstream>
#include <filesystem>

namespace mxrc::core::logging {

class AsyncLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 로그 파일 정리
        std::filesystem::remove("logs/mxrc.log");
    }

    void TearDown() override {
        shutdown_logger();
    }
};

TEST_F(AsyncLoggerTest, Initialization) {
    ASSERT_NO_THROW(initialize_async_logger());
}

TEST_F(AsyncLoggerTest, BasicLogging) {
    initialize_async_logger();

    spdlog::info("Test info message");
    spdlog::debug("Test debug message");
    spdlog::warn("Test warning message");

    // 로그가 파일에 기록되도록 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    shutdown_logger();

    // 로그 파일 확인
    std::ifstream log_file("logs/mxrc.log");
    ASSERT_TRUE(log_file.is_open());

    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("Test info message"), std::string::npos);
}

TEST_F(AsyncLoggerTest, CriticalFlushImmediate) {
    initialize_async_logger();

    auto before = std::filesystem::last_write_time("logs/mxrc.log");

    spdlog::critical("Critical message");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto after = std::filesystem::last_write_time("logs/mxrc.log");

    EXPECT_GT(after, before);  // 파일이 즉시 업데이트됨
}

}  // namespace mxrc::core::logging
```

---

### 2단계: 시그널 핸들러 구현 (선택적)

**목표**: 크래시 시 로그 보존 및 백트레이스 기록

#### 2.1 파일 생성 (src/core/logging/SignalHandler.h)

```cpp
#pragma once

#include <spdlog/spdlog.h>
#include <csignal>
#include <cstdlib>

// backward-cpp 사용 시 (선택적)
#ifdef USE_BACKWARD_CPP
#include "backward.hpp"
#endif

namespace mxrc::core::logging {

inline void signal_handler(int signal) {
    // 1. 백트레이스 기록
    spdlog::critical("Signal {} received, generating backtrace...", signal);

#ifdef USE_BACKWARD_CPP
    backward::StackTrace st;
    st.load_here(32);  // 최대 32 프레임

    backward::Printer p;
    p.print(st, stderr);

    // 백트레이스를 로그에도 기록
    std::ostringstream oss;
    p.print(st, oss);
    spdlog::critical("Backtrace:\n{}", oss.str());
#else
    spdlog::critical("Backtrace not available (compile with -DUSE_BACKWARD_CPP)");
#endif

    // 2. 로그 플러시 (모든 큐 비우기)
    spdlog::critical("Flushing logs before exit...");
    spdlog::shutdown();

    // 3. 기본 핸들러로 전달 (코어 덤프 생성)
    std::signal(signal, SIG_DFL);
    std::raise(signal);
}

inline void register_signal_handlers() {
    std::signal(SIGSEGV, signal_handler);  // 세그멘테이션 폴트
    std::signal(SIGABRT, signal_handler);  // abort() 호출
    std::signal(SIGTERM, signal_handler);  // 종료 시그널 (선택적)

    spdlog::info("Signal handlers registered (SIGSEGV, SIGABRT, SIGTERM)");
}

}  // namespace mxrc::core::logging
```

#### 2.2 main.cpp 통합

```cpp
// src/main.cpp

#include "core/logging/Log.h"
#include "core/logging/SignalHandler.h"

int main() {
    mxrc::core::logging::initialize_async_logger();

    // 시그널 핸들러 등록 (선택적)
    mxrc::core::logging::register_signal_handlers();

    spdlog::info("MXRC application started");

    // ... 애플리케이션 로직 ...

    mxrc::core::logging::shutdown_logger();
    return 0;
}
```

#### 2.3 테스트 작성

```cpp
// tests/unit/logging/SignalHandler_test.cpp

#include "gtest/gtest.h"
#include "core/logging/Log.h"
#include "core/logging/SignalHandler.h"
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

namespace mxrc::core::logging {

TEST(SignalHandlerTest, SIGSEGVLogsBeforeCrash) {
    pid_t pid = fork();

    if (pid == 0) {
        // 자식 프로세스: 크래시 유도
        initialize_async_logger();
        register_signal_handlers();

        spdlog::critical("About to crash with SIGSEGV");

        // 의도적 세그멘테이션 폴트
        int* nullptr_ptr = nullptr;
        *nullptr_ptr = 42;

        // 여기 도달하지 않음
        std::exit(1);

    } else {
        // 부모 프로세스: 자식 종료 대기
        int status;
        waitpid(pid, &status, 0);

        // 자식이 시그널로 종료되었는지 확인
        EXPECT_TRUE(WIFSIGNALED(status));
        EXPECT_EQ(WTERMSIG(status), SIGSEGV);

        // 로그 파일 확인
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::ifstream log_file("logs/mxrc.log");
        std::string content((std::istreambuf_iterator<char>(log_file)),
                            std::istreambuf_iterator<char>());

        EXPECT_NE(content.find("About to crash"), std::string::npos);
        EXPECT_NE(content.find("Signal 11 received"), std::string::npos);
    }
}

}  // namespace mxrc::core::logging
```

---

### 3단계: backward-cpp 통합 (선택적)

**목표**: 크래시 시 읽기 쉬운 백트레이스 생성

#### 3.1 backward.hpp 다운로드

```bash
cd src/core/logging
wget https://raw.githubusercontent.com/bombela/backward-cpp/master/backward.hpp
```

#### 3.2 CMakeLists.txt 수정

```cmake
# src/core/logging/CMakeLists.txt (새로 생성)

# backward-cpp는 header-only
add_library(backward INTERFACE)
target_include_directories(backward INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# USE_BACKWARD_CPP 정의
target_compile_definitions(backward INTERFACE USE_BACKWARD_CPP)

# logging 라이브러리 (header-only)
add_library(mxrc_logging INTERFACE)
target_include_directories(mxrc_logging INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mxrc_logging INTERFACE spdlog::spdlog)

# backward 사용 시 연결 (선택적)
option(USE_BACKWARD "Enable backward-cpp for backtraces" ON)
if(USE_BACKWARD)
    target_link_libraries(mxrc_logging INTERFACE backward)
endif()
```

```cmake
# src/CMakeLists.txt

# logging 디렉토리 추가
add_subdirectory(core/logging)

# mxrc 타겟에 연결
target_link_libraries(mxrc PRIVATE mxrc_logging)
```

---

## 🧪 테스트 전략

### 1. 단위 테스트 (Unit Tests)

```bash
# 빌드 디렉토리에서
cd build
./run_tests --gtest_filter=AsyncLogger*
```

**검증 항목**:
- 로거 초기화
- 기본 로깅 (info, debug, warn, error, critical)
- CRITICAL 로그 즉시 플러시
- 에러 핸들러 동작

### 2. 통합 테스트 (Integration Tests)

```bash
./run_tests --gtest_filter=SignalHandler*
```

**검증 항목**:
- SIGSEGV 시 로그 보존
- SIGABRT 시 로그 보존
- 백트레이스 생성 (backward-cpp 사용 시)

### 3. 성능 테스트 (Performance Tests)

```cpp
// tests/unit/logging/LogPerformance_test.cpp

TEST(LogPerformance, TenMicrosecondLatency) {
    mxrc::core::logging::initialize_async_logger();

    const int N = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < N; i++) {
        spdlog::info("Test message {}", i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    double avg_latency = static_cast<double>(duration_us) / N;

    spdlog::info("Average latency: {} μs", avg_latency);
    EXPECT_LT(avg_latency, 10.0);  // < 10μs

    mxrc::core::logging::shutdown_logger();
}

TEST(LogPerformance, ControlLoopOverhead) {
    // 로깅 없는 루프
    auto start_no_log = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto end_no_log = std::chrono::high_resolution_clock::now();
    auto duration_no_log = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_no_log - start_no_log).count();

    // 로깅 있는 루프
    mxrc::core::logging::initialize_async_logger();

    auto start_with_log = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        spdlog::debug("Loop iteration {}", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto end_with_log = std::chrono::high_resolution_clock::now();
    auto duration_with_log = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_with_log - start_with_log).count();

    mxrc::core::logging::shutdown_logger();

    double overhead_pct = static_cast<double>(duration_with_log - duration_no_log) /
                          duration_no_log * 100.0;

    spdlog::info("Overhead: {}%", overhead_pct);
    EXPECT_LT(overhead_pct, 1.0);  // < 1% 오버헤드
}
```

### 4. 메모리 검증 (Memory Validation)

```bash
# Valgrind로 메모리 누수 검사
valgrind --leak-check=full --show-leak-kinds=all \
    ./run_tests --gtest_filter=AsyncLogger*

# AddressSanitizer (빌드 시 -fsanitize=address)
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address"
make
./run_tests --gtest_filter=AsyncLogger*
```

---

## 📊 성능 측정 방법

### 로깅 호출 지연 측정

```cpp
auto start = std::chrono::high_resolution_clock::now();

spdlog::info("Test message");

auto end = std::chrono::high_resolution_clock::now();
auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
    end - start).count();

std::cout << "Latency: " << latency_us << " μs" << std::endl;
```

**목표**: 평균 < 10μs

### 로그 보존율 측정

```cpp
// 1. 로그 1000개 작성
for (int i = 0; i < 1000; i++) {
    spdlog::info("Test log {}", i);
}

// 2. CRITICAL 로그
spdlog::critical("About to crash");

// 3. 의도적 크래시
std::raise(SIGSEGV);

// 4. 별도 프로세스에서 로그 파일 확인
// grep "Test log" logs/mxrc.log | wc -l  → 990+ (99% 이상)
```

---

## 🐛 디버깅 팁

### 1. 로그 파일 위치 확인

```bash
ls -lh logs/mxrc.log
tail -f logs/mxrc.log  # 실시간 로그 확인
```

### 2. 로그 레벨 변경

```cpp
// 런타임에 로그 레벨 변경
spdlog::set_level(spdlog::level::trace);  // 모든 로그
spdlog::set_level(spdlog::level::info);   // info 이상만
```

### 3. 백그라운드 스레드 확인

```bash
# gdb로 스레드 상태 확인
gdb ./mxrc
(gdb) run
^C
(gdb) info threads
(gdb) thread apply all bt
```

### 4. 큐 오버플로우 확인

```cpp
// 로거 에러 핸들러에서 확인
logger->set_error_handler([](const std::string& msg) {
    std::cerr << "Logger error: " << msg << std::endl;
    // msg에 "queue overflow" 포함 시 큐 크기 증가 필요
});
```

---

## ✅ 체크리스트

### 구현 전:
- [ ] issue #006 읽고 문제 이해
- [ ] research.md와 data-model.md 검토
- [ ] 기존 spdlog 사용 코드 파악

### 구현 중:
- [ ] Log.h 구현 및 테스트
- [ ] SignalHandler.h 구현 및 테스트 (선택적)
- [ ] backward-cpp 통합 (선택적)
- [ ] 모든 단위 테스트 통과
- [ ] main.cpp 통합

### 구현 후:
- [ ] 성능 테스트 통과 (10μs 지연, 1% 오버헤드)
- [ ] 크래시 안전성 테스트 통과 (99% 로그 보존)
- [ ] Valgrind 메모리 누수 없음
- [ ] 기존 테스트 100% 통과 (하위 호환성)
- [ ] 코드 리뷰 (CLAUDE.md 규칙 준수)

---

## 📚 참고 자료

- [research.md](research.md) - 기술 조사 결과
- [data-model.md](data-model.md) - 데이터 모델 상세
- [spec.md](spec.md) - 기능 사양서
- [contracts/logging-api.md](contracts/logging-api.md) - API 계약
- [issue/006](../../issue/006-spdlog-async-refactor.md) - 원인 분석

## 🤝 도움 받기

- 질문: CLAUDE.md의 코드 작성 가이드 참조
- 버그 리포트: issue/ 디렉토리에 새 이슈 생성
- 성능 이슈: spdlog 디버그 로그 활성화하여 병목 지점 식별

---

## 🎯 다음 단계

1. ✅ research.md 완료
2. ✅ data-model.md 완료
3. ✅ contracts/logging-api.md 완료
4. ✅ quickstart.md 완료 (현재)
5. ⏳ `/tasks` 명령으로 tasks.md 생성
6. ⏳ 실제 구현 시작
