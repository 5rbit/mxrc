# API 계약: 비동기 로깅 시스템

**브랜치**: `018-async-logging` | **날짜**: 2025-11-19

이 문서는 비동기 로깅 시스템의 공개 API를 정의합니다.

## 네임스페이스

```cpp
namespace mxrc::core::logging {
    // 초기화 함수
    void initialize_async_logger();

    // 종료 함수 (선택적, spdlog::shutdown() 사용 권장)
    void shutdown_logger();

    // 시그널 핸들러 등록 (선택적)
    void register_signal_handlers();
}
```

---

## 1. 초기화 API

### `initialize_async_logger()`

**목적**: 비동기 로거를 초기화하고 전역 기본 로거로 설정

**시그니처**:
```cpp
void initialize_async_logger();
```

**전제조건**:
- main() 시작 직후 호출
- 한 번만 호출 (여러 번 호출 시 undefined behavior)

**후행조건**:
- `spdlog::default_logger()`가 비동기 로거로 설정됨
- 로그 파일 `logs/mxrc.log` 생성
- 백그라운드 스레드 시작됨
- 주기적 flush 스레드 시작됨

**예외**:
- `spdlog::spdlog_ex`: 로그 디렉토리 생성 실패 시
- `std::runtime_error`: thread pool 초기화 실패 시

**예제**:
```cpp
#include "core/logging/Log.h"

int main() {
    try {
        mxrc::core::logging::initialize_async_logger();
        spdlog::info("Application started");
    } catch (const std::exception& e) {
        std::cerr << "Logger initialization failed: " << e.what() << std::endl;
        return 1;
    }

    // ... 애플리케이션 로직 ...

    spdlog::shutdown();
    return 0;
}
```

**성능 계약**:
- 호출 시간 < 10ms
- 메모리 사용량 < 10MB

---

## 2. 로깅 API (spdlog 표준 API)

### `spdlog::info()`, `spdlog::error()`, etc.

**목적**: 로그 메시지 기록

**시그니처**:
```cpp
template<typename... Args>
void spdlog::info(fmt::format_string<Args...> fmt, Args&&... args);

template<typename... Args>
void spdlog::debug(fmt::format_string<Args...> fmt, Args&&... args);

template<typename... Args>
void spdlog::warn(fmt::format_string<Args...> fmt, Args&&... args);

template<typename... Args>
void spdlog::error(fmt::format_string<Args...> fmt, Args&&... args);

template<typename... Args>
void spdlog::critical(fmt::format_string<Args...> fmt, Args&&... args);
```

**전제조건**:
- `initialize_async_logger()` 호출됨
- (초기화 전에도 호출 가능, 이 경우 동기식 기본 로거 사용)

**후행조건**:
- 메시지가 lock-free queue에 추가됨
- 함수 즉시 반환 (I/O 대기 없음)
- CRITICAL 레벨은 즉시 플러시됨

**성능 계약**:
- 호출 시간 < 10μs (평균)
- 큐가 가득 차면 block (timeout 없음)

**예제**:
```cpp
// 기본 로깅
spdlog::info("Server started on port {}", port);

// 디버그 로그
spdlog::debug("Variable x = {}, y = {}", x, y);

// 경고
spdlog::warn("Low memory: {} MB remaining", free_memory);

// 에러 (즉시 플러시 아님)
spdlog::error("Failed to connect to {}", host);

// 크리티컬 (즉시 플러시됨)
spdlog::critical("Unrecoverable error: {}", error_msg);
```

---

## 3. 종료 API

### `spdlog::shutdown()`

**목적**: 모든 로거를 플러시하고 정리

**시그니처**:
```cpp
void spdlog::shutdown();
```

**전제조건**:
- 애플리케이션 종료 직전 호출
- 또는 signal handler에서 호출

**후행조건**:
- 큐에 남은 모든 메시지 처리됨
- 모든 파일 핸들 닫힘
- 백그라운드 스레드 종료됨

**성능 계약**:
- 호출 시간 < 1초 (큐 크기에 따라 변동)

**예제**:
```cpp
int main() {
    mxrc::core::logging::initialize_async_logger();

    // ... 애플리케이션 로직 ...

    spdlog::shutdown();  // 종료 전 필수
    return 0;
}
```

---

## 4. 시그널 핸들러 API (선택적)

### `register_signal_handlers()`

**목적**: 크래시 시그널 핸들러 등록

**시그니처**:
```cpp
void mxrc::core::logging::register_signal_handlers();
```

**전제조건**:
- `initialize_async_logger()` 호출됨
- POSIX 시스템 (Linux/Unix)

**후행조건**:
- SIGSEGV, SIGABRT, SIGTERM 핸들러 등록됨
- 크래시 시 백트레이스 기록 + 로그 플러시

**예제**:
```cpp
int main() {
    mxrc::core::logging::initialize_async_logger();
    mxrc::core::logging::register_signal_handlers();  // 선택적

    // ... 애플리케이션 로직 ...
}
```

**구현 (src/core/logging/SignalHandler.h)**:
```cpp
namespace mxrc::core::logging {

inline void signal_handler(int signal) {
    spdlog::critical("Signal {} received", signal);

    // 백트레이스 기록 (backward-cpp)
    #ifdef USE_BACKWARD_CPP
    backward::StackTrace st;
    st.load_here(32);
    backward::Printer p;
    p.print(st, stderr);
    #endif

    // 로그 플러시
    spdlog::shutdown();

    // 기본 핸들러로 전달
    std::signal(signal, SIG_DFL);
    std::raise(signal);
}

inline void register_signal_handlers() {
    std::signal(SIGSEGV, signal_handler);
    std::signal(SIGABRT, signal_handler);
    std::signal(SIGTERM, signal_handler);
}

}  // namespace mxrc::core::logging
```

---

## 5. 에러 처리 계약

### 로거 자체 에러

**에러 핸들러**:
```cpp
logger->set_error_handler([](const std::string& msg) {
    std::cerr << "Logger error: " << msg << std::endl;
    // 애플리케이션은 계속 진행
});
```

**발생 가능한 에러**:
- 파일 쓰기 실패 (디스크 풀, 권한 없음)
- 큐 오버플로우 (매우 드묾, block 정책 사용 중)

**계약**:
- 에러 발생 시 stderr로 출력
- 애플리케이션 중단하지 않음
- 로그 유실 최소화

---

## 6. 성능 계약

### 로깅 호출 성능

| 항목 | 보장 | 측정 방법 |
|------|------|----------|
| 평균 호출 시간 | < 10μs | 고해상도 타이머 |
| p95 호출 시간 | < 20μs | 10,000회 측정 |
| p99 호출 시간 | < 50μs | 10,000회 측정 |

### 제어 루프 오버헤드

| 항목 | 보장 | 측정 방법 |
|------|------|----------|
| 1000Hz 루프 지연 | < 1% | 루프 주기 비교 |
| 처리량 영향 | < 5% | 로깅 전후 비교 |

### 로그 보존율

| 항목 | 보장 | 측정 방법 |
|------|------|----------|
| 정상 종료 시 | 100% | 로그 파일 확인 |
| 크래시 시 (3초 이내) | 99% | SIGSEGV 유도 후 확인 |
| CRITICAL 로그 | 100% | 즉시 플러시 확인 |

---

## 7. 스레드 안전성 계약

### 보장

- 모든 로깅 함수는 thread-safe
- 여러 스레드에서 동시 호출 가능
- lock-free queue로 경합 최소화

### 제한

- `initialize_async_logger()`는 한 번만 호출 (thread-safe 아님)
- `spdlog::shutdown()`은 모든 로그 완료 후 호출 (thread-safe하지만 사용 주의)

---

## 8. 메모리 계약

### 메모리 사용량

| 항목 | 보장 | 비고 |
|------|------|------|
| 로거 초기화 | < 10MB | 큐 포함 |
| 로그 메시지당 | ~256 bytes | 평균 |
| 총 메모리 | < 20MB | 정상 동작 시 |

### 메모리 누수

- RAII 패턴으로 자동 정리
- Valgrind로 검증 필수
- spdlog::shutdown() 호출 시 모든 메모리 해제

---

## 9. 호환성 계약

### API 호환성

- spdlog 표준 API 100% 호환
- 기존 코드 수정 없이 작동
- Semantic Versioning 준수 (패치 버전 증가)

### 플랫폼 호환성

- Linux Ubuntu 24.04 LTS (주 플랫폼)
- POSIX 시스템 (signal handler 사용 시)
- C++17 이상 필수 (C++20 권장)

---

## 10. 테스트 계약

### 단위 테스트

```cpp
// tests/unit/logging/AsyncLogger_test.cpp

TEST(AsyncLogger, Initialization) {
    ASSERT_NO_THROW(mxrc::core::logging::initialize_async_logger());
}

TEST(AsyncLogger, BasicLogging) {
    spdlog::info("Test message");
    // 큐에 추가되었는지 확인
}

TEST(AsyncLogger, CriticalFlush) {
    spdlog::critical("Critical message");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // 파일에 기록되었는지 확인
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
    EXPECT_LT(avg_latency, 10.0);
}
```

### 크래시 안전성 테스트

```cpp
// tests/integration/logging/CrashSafety_test.cpp

TEST(CrashSafety, SIGSEGVPreservesLogs) {
    if (fork() == 0) {
        spdlog::critical("About to crash");
        std::raise(SIGSEGV);
    }

    wait(nullptr);

    // 로그 파일에 "About to crash" 확인
}
```

---

## 11. 버전 관리 계약

### 현재 버전

- **API 버전**: 1.0.0
- **spdlog 버전**: 1.x (프로젝트 의존성)
- **C++ 표준**: C++17 이상

### 변경 사항

- 비호환 변경: 없음 (기존 API 유지)
- 새로운 기능: 비동기 로깅, 크래시 안전성
- 버그 수정: 동기식 로깅으로 인한 성능 저하 해결

---

## 12. 사용 예시

### 최소 사용 예시

```cpp
#include "core/logging/Log.h"

int main() {
    mxrc::core::logging::initialize_async_logger();

    spdlog::info("Hello, async logging!");

    spdlog::shutdown();
    return 0;
}
```

### 완전한 사용 예시

```cpp
#include "core/logging/Log.h"
#include "core/logging/SignalHandler.h"

int main() {
    try {
        // 1. 로거 초기화
        mxrc::core::logging::initialize_async_logger();

        // 2. 시그널 핸들러 등록 (선택적)
        mxrc::core::logging::register_signal_handlers();

        // 3. 로깅 사용
        spdlog::info("Application started");

        // 4. 제어 루프 (1000Hz)
        for (int i = 0; i < 1000; i++) {
            spdlog::debug("Loop iteration {}", i);
            // ... 제어 로직 ...
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        spdlog::info("Application finished");

    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return 1;
    }

    // 5. 종료
    spdlog::shutdown();
    return 0;
}
```

---

## 참고 자료

- spdlog 공식 문서: https://github.com/gabime/spdlog
- backward-cpp: https://github.com/bombela/backward-cpp
- research.md: 기술 조사 결과
- data-model.md: 내부 데이터 모델
