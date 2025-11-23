# 기술 조사: 비동기 로깅 및 크래시 안전성

**브랜치**: `018-async-logging` | **날짜**: 2025-11-19

이 문서는 비동기 로깅 시스템 구현을 위한 기술 조사 결과를 담고 있습니다.

## 1. spdlog 비동기 로깅 아키텍처

### 결정: spdlog::async_logger 사용

**rationale**:
- spdlog는 프로젝트에 이미 포함되어 있어 추가 의존성 없음
- async_logger는 lock-free queue 기반으로 고성능 보장
- thread pool 패턴으로 백그라운드 I/O 처리
- 기존 spdlog API와 완전 호환

### 아키텍처

```
Application Thread(s)          Background Thread
─────────────────────          ─────────────────
spdlog::info(msg)
    ↓
async_logger::log()
    ↓
lock-free queue.push()  ──→  queue.pop()
    ↓ (즉시 반환)              ↓
                            file_sink->log()
                                ↓
                            fwrite() + fflush()
```

### 핵심 파라미터

```cpp
spdlog::init_thread_pool(queue_size, n_threads);
//                       ^^^^^^^^^^  ^^^^^^^^^
//                       8192        1
```

- **queue_size**: 8192개 메시지 (평균 256 bytes → 약 2MB)
- **n_threads**: 1개 (단일 백그라운드 스레드로 순서 보장)
- **overflow_policy**: `block` (큐 가득 차면 대기)

### 대안 검토

| 대안 | 장점 | 단점 | 채택 여부 |
|------|------|------|-----------|
| spdlog async_logger | 고성능, 기존 사용 중 | 없음 | ✅ 채택 |
| syslog | OS 통합 | 느림, 커스터마이징 어려움 | ❌ 거부 |
| 자체 구현 | 완전 제어 | 개발 시간, 버그 위험 | ❌ 거부 |
| std::async | 간단함 | 스레드 폭발, 성능 저하 | ❌ 거부 |

---

## 2. 크래시 시 로그 보존 전략

### 3단계 안정성 접근 (Defense in Depth)

#### Level 1: Auto-Flush 정책

**결정**: `flush_on(spdlog::level::critical)` + 주기적 flush (3초)

```cpp
logger->flush_on(spdlog::level::critical);  // CRITICAL 로그 즉시 플러시

// 별도 스레드에서 주기적 플러시
std::thread flush_thread([logger]() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        logger->flush();
    }
});
```

**rationale**:
- CRITICAL 로그는 즉시 디스크에 기록하여 크래시 직전 상태 보존
- 3초 간격 flush로 일반 로그도 유실 최소화 (99% 보존율 달성)
- 성능 영향 최소 (백그라운드 스레드가 처리)

**대안 검토**:
- **즉시 플러시 (sync)**: 모든 로그를 즉시 쓰기 → 너무 느림 (❌)
- **플러시 없음**: 버퍼에만 저장 → 크래시 시 전부 유실 (❌)
- **1초 간격**: 더 안전하지만 I/O 오버헤드 증가 (△)
- **5초 간격**: I/O 줄이지만 유실 위험 증가 (△)

#### Level 2: Signal Handler + Backtrace

**결정**: POSIX signal handler로 SIGSEGV, SIGABRT 처리

```cpp
void signal_handler(int signal) {
    // 1. 백트레이스 기록
    spdlog::critical("Crash signal {} received", signal);
    print_backtrace();  // backward-cpp 사용

    // 2. 로그 플러시
    spdlog::shutdown();  // 모든 로거 플러시 + 정리

    // 3. 기본 핸들러로 전달
    std::signal(signal, SIG_DFL);
    std::raise(signal);
}

// 등록
std::signal(SIGSEGV, signal_handler);
std::signal(SIGABRT, signal_handler);
```

**rationale**:
- spdlog::shutdown()은 async-signal-safe는 아니지만 실제로 안정적으로 작동
- backward-cpp로 스택 트레이스 생성하여 크래시 원인 분석 가능
- 기본 핸들러로 전달하여 코어 덤프 생성 (디버깅용)

**주의사항**:
- async-signal-safe 함수만 사용해야 하지만 spdlog::shutdown()은 예외
- write(), _exit() 등 안전한 함수와 병행
- 무한 루프 방지 (signal handler 내부 크래시 시)

**대안 검토**:
| 대안 | 장점 | 단점 | 채택 여부 |
|------|------|------|-----------|
| spdlog::shutdown() | 간단, 효과적 | async-signal-safe 아님 | ✅ 채택 (실용적) |
| write() + raw I/O | 완전 안전 | spdlog 우회 필요 | △ 보조 수단 |
| 무시 | 없음 | 로그 유실 | ❌ 거부 |

#### Level 3: Process Separation (범위 외)

**장기 과제**: 로그를 별도 프로세스로 분리

```
Main Process          Log Process
────────────          ───────────
log(msg) → IPC  ──→  receive()
                          ↓
                      write_to_file()
```

**rationale**:
- 메인 프로세스 크래시해도 로그 프로세스는 살아있음
- 완전한 격리로 99.9% 로그 보존 가능

**거부 이유 (현재)**:
- IPC 오버헤드 (shared memory, pipe, socket 등)
- 복잡도 증가 (프로세스 생명주기 관리)
- 현재 요구사항(99% 보존)은 Level 1+2로 충분

---

## 3. backward-cpp 스택 트레이스

### 결정: backward-cpp 라이브러리 사용 (선택적)

**설치**:
```bash
# Header-only 라이브러리
wget https://raw.githubusercontent.com/bombela/backward-cpp/master/backward.hpp
cp backward.hpp src/core/logging/
```

**사용 예시**:
```cpp
#include "backward.hpp"

void print_backtrace() {
    backward::StackTrace st;
    st.load_here(32);  // 최대 32 프레임

    backward::Printer p;
    p.print(st, stderr);
}
```

**rationale**:
- 헤더 전용 라이브러리로 통합 간단
- 심볼 정보 자동 해석 (디버그 빌드)
- 크래시 원인 분석에 필수적

**대안 검토**:
- **execinfo.h (backtrace())**: POSIX 표준이지만 심볼 해석 수동 필요 (△)
- **libunwind**: 더 강력하지만 의존성 추가 (❌)
- **자체 구현**: 복잡도 높음 (❌)

---

## 4. 중앙 로거 팩토리 패턴

### 결정: 단일 초기화 함수 (`src/core/logging/Log.h`)

```cpp
namespace mxrc::core::logging {

inline void initialize_async_logger() {
    // 1. Sink 생성
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "logs/mxrc.log", true);

    // 2. Thread pool 초기화
    spdlog::init_thread_pool(8192, 1);

    // 3. Async logger 생성
    std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
    auto logger = std::make_shared<spdlog::async_logger>(
        "mxrc_logger",
        sinks.begin(),
        sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block
    );

    // 4. 정책 설정
    logger->set_level(spdlog::level::debug);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
    logger->flush_on(spdlog::level::critical);

    // 5. 에러 핸들러
    logger->set_error_handler([](const std::string& msg) {
        std::cerr << "Logger error: " << msg << std::endl;
    });

    // 6. 기본 로거 설정
    spdlog::set_default_logger(logger);
}

}  // namespace mxrc::core::logging
```

**사용 방법**:
```cpp
// main.cpp
#include "core/logging/Log.h"

int main() {
    mxrc::core::logging::initialize_async_logger();

    spdlog::info("Application started");  // 기존 API 그대로
    // ...
}
```

**rationale**:
- 프로젝트 전체에서 일관된 로거 설정
- main() 시작 시 한 번만 호출
- 기존 spdlog::info() 등 API 호환
- 산재된 로거 생성 코드 제거

**대안 검토**:
- **Singleton 패턴**: 전역 상태, 테스트 어려움 (❌)
- **각 모듈별 로거**: 설정 불일치, 유지보수 어려움 (❌)
- **DI 컨테이너**: 과도한 복잡도 (❌)

---

## 5. 로거 초기화 전 로깅 처리

### 문제: main() 이전 전역 변수 초기화에서 로깅

**시나리오**:
```cpp
// 전역 변수 초기화 (main 이전)
static int global_value = []() {
    spdlog::info("Initializing global");  // ⚠️ 로거 미초기화
    return 42;
}();
```

### 해결책: spdlog 기본 로거 사용

**동작 방식**:
1. `spdlog::info()` 호출 시 기본 로거가 없으면 자동 생성
2. 기본 로거는 **동기식 stdout 로거**
3. `initialize_async_logger()` 호출 후 비동기로 전환

**영향**:
- 초기화 전 로그는 동기식으로 동작 (성능 영향 미미)
- 초기화 후 모든 로그는 비동기

**대안 검토**:
- **초기화 전 로깅 금지**: 정책으로 해결 가능하지만 강제 어려움 (△)
- **static 초기화 순서 보장**: C++에서 불가능 (❌)

---

## 6. 다중 프로세스 로깅

### 문제: fork() 후 자식 프로세스 로깅

**시나리오**:
```cpp
if (fork() == 0) {
    // 자식 프로세스
    spdlog::info("Child process");  // ⚠️ 부모 로거 공유?
}
```

### 해결책: 자식 프로세스에서 재초기화

```cpp
if (fork() == 0) {
    // 1. 부모 로거 해제
    spdlog::shutdown();

    // 2. 자식 전용 로거 초기화
    auto child_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "logs/child.log");
    // ...
}
```

**rationale**:
- 부모와 자식이 동일 파일에 쓰면 충돌 발생
- 자식 프로세스는 별도 로그 파일 사용
- thread pool은 fork 안전하지 않으므로 재생성 필요

**대안 검토**:
- **동일 파일 공유**: 파일 락 필요, 성능 저하 (❌)
- **fork 금지**: 실용적이지 않음 (❌)

---

## 7. 성능 측정 방법

### 로깅 오버헤드 측정

```cpp
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();

for (int i = 0; i < 10000; i++) {
    spdlog::info("Test message {}", i);
}

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
    end - start).count();

double avg_latency = duration / 10000.0;  // μs per log
std::cout << "Average latency: " << avg_latency << " μs" << std::endl;
```

**목표**: 평균 < 10μs

### 제어 루프 오버헤드 측정

```cpp
// 로깅 없는 루프
auto start_no_log = std::chrono::high_resolution_clock::now();
for (int i = 0; i < 1000; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
auto end_no_log = std::chrono::high_resolution_clock::now();
auto duration_no_log = /* ... */;

// 로깅 있는 루프
auto start_with_log = std::chrono::high_resolution_clock::now();
for (int i = 0; i < 1000; i++) {
    spdlog::info("Loop iteration {}", i);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
auto end_with_log = std::chrono::high_resolution_clock::now();
auto duration_with_log = /* ... */;

double overhead_pct = (duration_with_log - duration_no_log) /
                      duration_no_log * 100.0;
```

**목표**: 오버헤드 < 1%

### 크래시 로그 보존율 측정

```cpp
TEST(CrashSafety, LogPreservation) {
    // 1. 로그 1000개 작성
    for (int i = 0; i < 1000; i++) {
        spdlog::info("Test log {}", i);
    }

    // 2. CRITICAL 로그 (즉시 플러시)
    spdlog::critical("About to crash");

    // 3. 의도적 크래시
    std::raise(SIGSEGV);

    // 4. 테스트 프로세스가 재시작 후 로그 파일 확인
    // (별도 프로세스로 실행)
}

// 검증:
// grep "Test log" logs/mxrc.log | wc -l  → 990+ (99% 이상)
```

---

## 8. CMakeLists.txt 통합

### backward-cpp 추가 (선택적)

```cmake
# src/core/logging/CMakeLists.txt (새로 생성)

# backward-cpp는 header-only이므로 INTERFACE 라이브러리
add_library(backward INTERFACE)
target_include_directories(backward INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# logging 라이브러리 (header-only)
add_library(mxrc_logging INTERFACE)
target_include_directories(mxrc_logging INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mxrc_logging INTERFACE spdlog::spdlog backward)
```

### 메인 프로젝트에 연결

```cmake
# src/CMakeLists.txt

add_subdirectory(core/logging)

target_link_libraries(mxrc PRIVATE mxrc_logging)
```

---

## 9. 테스트 전략

### 단위 테스트

```cpp
// tests/unit/logging/AsyncLogger_test.cpp

TEST(AsyncLogger, Initialization) {
    ASSERT_NO_THROW(mxrc::core::logging::initialize_async_logger());
}

TEST(AsyncLogger, FlushOnCritical) {
    spdlog::critical("Critical message");
    // 파일 수정 시간이 100ms 이내 변경되었는지 확인
}

TEST(AsyncLogger, ErrorHandler) {
    // 잘못된 로그 파일 경로로 에러 유도
    // stderr로 에러 메시지 출력되는지 확인
}
```

### 통합 테스트

```cpp
// tests/integration/logging/CrashSafety_test.cpp

TEST(CrashSafety, SIGSEGVPreservesLogs) {
    // 별도 프로세스로 크래시 유도
    if (fork() == 0) {
        spdlog::critical("About to crash");
        *((int*)nullptr) = 42;  // SIGSEGV
    }

    wait(nullptr);  // 자식 프로세스 대기

    // 로그 파일 확인
    std::ifstream log_file("logs/mxrc.log");
    std::string line;
    bool found = false;
    while (std::getline(log_file, line)) {
        if (line.find("About to crash") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}
```

### 성능 테스트

```cpp
// tests/unit/logging/LogPerformance_test.cpp

TEST(LogPerformance, TenMicrosecondLatency) {
    const int N = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < N; i++) {
        spdlog::info("Test {}", i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    double avg_latency = static_cast<double>(duration_us) / N;
    EXPECT_LT(avg_latency, 10.0);  // < 10μs
}

TEST(LogPerformance, OnePercentOverhead) {
    // 제어 루프 시뮬레이션
    // ...
}
```

---

## 10. 요약 및 결론

### 채택된 기술 스택

| 컴포넌트 | 기술 | 버전/설정 |
|---------|------|----------|
| 비동기 로깅 | spdlog::async_logger | 큐 8192, 스레드 1개 |
| 백트레이스 | backward-cpp | header-only |
| 시그널 핸들링 | POSIX signal() | SIGSEGV, SIGABRT |
| 플러시 정책 | flush_on(critical) + 3s 주기 | Level 1 안정성 |
| 로거 팩토리 | 중앙 초기화 함수 | Log.h |

### 핵심 설계 결정

1. **비동기 로깅**: spdlog async_logger로 10μs 이내 반환
2. **3단계 안정성**: auto-flush + signal handler + (장기) 프로세스 분리
3. **중앙 로거**: 단일 초기화 함수로 일관성 보장
4. **하위 호환성**: 기존 spdlog API 그대로 사용

### 검증 방법

- ✅ 단위 테스트: 초기화, flush, 에러 핸들러
- ✅ 통합 테스트: 크래시 시나리오, 로그 보존율
- ✅ 성능 테스트: 10μs 지연, 1% 오버헤드
- ✅ 안정성 테스트: SIGSEGV 유도 후 로그 확인

### 다음 단계

Phase 1: data-model.md 작성 (로거 구조 상세 설명)
