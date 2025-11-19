# 데이터 모델: 비동기 로깅 시스템 구조

**브랜치**: `018-async-logging` | **날짜**: 2025-11-19

이 문서는 비동기 로깅 시스템의 내부 데이터 구조 및 컴포넌트를 정의합니다.

## 개요

비동기 로깅 시스템은 **공개 API를 변경하지 않고** 내부 구현만 비동기로 전환합니다. 외부에서는 기존 spdlog API(`spdlog::info()`, `spdlog::error()` 등)를 그대로 사용합니다.

## 엔티티 (Entities)

### 1. AsyncLogger (비동기 로거)

**목적**: 애플리케이션 로그 메시지를 lock-free queue에 넣고 백그라운드 스레드로 전달

**타입**: `std::shared_ptr<spdlog::async_logger>`

**속성**:
```cpp
class AsyncLogger {
    std::string name;                        // "mxrc_logger"
    std::vector<spdlog::sink_ptr> sinks;     // 출력 대상 (콘솔 + 파일)
    spdlog::level level;                     // 로그 레벨 (debug)
    std::string pattern;                     // 포맷 패턴
    spdlog::level flush_level;               // flush_on(critical)
    error_handler_t error_handler;           // 에러 콜백
};
```

**관계**:
- 애플리케이션당 하나의 기본 로거 (`spdlog::default_logger()`)
- N개의 Sink 소유 (1:N)
- 1개의 ThreadPool 공유 (N:1)

**생명주기**:
```
[초기화] --initialize_async_logger()--> [활성]
                                           ↓
                                    spdlog::shutdown()
                                           ↓
                                        [정리됨]
```

**검증 규칙**:
- name은 고유해야 함
- sinks는 최소 1개 이상
- flush_level은 level 이상이어야 함

---

### 2. ThreadPool (백그라운드 스레드 풀)

**목적**: 로그 메시지를 실제 파일/콘솔에 쓰는 백그라운드 작업 처리

**타입**: `std::shared_ptr<spdlog::details::thread_pool>`

**속성**:
```cpp
class ThreadPool {
    size_t queue_size;            // 8192 (메시지 버퍼 크기)
    size_t n_threads;             // 1 (백그라운드 스레드 수)
    mpmc_queue<async_msg> queue;  // lock-free queue
    std::vector<std::thread> threads;  // 작업 스레드
    overflow_policy policy;       // block (큐 가득 차면 대기)
};
```

**관계**:
- 프로세스당 하나의 전역 thread pool (`spdlog::thread_pool()`)
- 여러 async_logger가 공유 (singleton 패턴)

**데이터 흐름**:
```
Application Thread          Background Thread
──────────────────          ─────────────────
logger->info(msg)
    ↓
queue.enqueue(msg)  ───→  queue.dequeue()
    ↓ (즉시 반환)           ↓
                        sink->log(msg)
                            ↓
                        fwrite() + fflush()
```

**성능 특성**:
- enqueue 시간: <1μs (lock-free)
- 처리량: >100,000 msg/sec
- CPU 사용률: <5% (백그라운드 스레드)

---

### 3. Sink (로그 출력 대상)

**목적**: 로그 메시지의 실제 출력 위치 (콘솔, 파일 등)

**타입**: `std::shared_ptr<spdlog::sinks::sink>`

#### 3.1. ConsoleSink (콘솔 출력)

```cpp
class ConsoleSink : public spdlog::sinks::sink {
    FILE* stream;           // stdout 또는 stderr
    bool colored;           // 색상 지원 여부
    std::mutex mutex;       // 스레드 안전성
};
```

**특징**:
- ANSI 색상 코드 지원 (stdout_color_sink_mt)
- 로그 레벨별 색상 (debug=회색, info=흰색, error=빨강, critical=빨강+볼드)

#### 3.2. FileSink (파일 출력)

```cpp
class FileSink : public spdlog::sinks::sink {
    std::string filename;    // "logs/mxrc.log"
    bool truncate;           // false (append 모드)
    std::ofstream file;      // 파일 핸들
    std::mutex mutex;        // 스레드 안전성
};
```

**특징**:
- append 모드로 열기 (기존 로그 보존)
- 자동 디렉토리 생성 (logs/ 없으면 생성)
- RAII 패턴으로 자동 닫기

**관계**:
- AsyncLogger가 여러 Sink 소유 (1:N)
- 각 Sink는 독립적으로 로그 레벨 필터링 가능

---

### 4. LogMessage (로그 메시지)

**목적**: 로그 큐에 저장되는 단위 메시지

**타입**: `spdlog::details::async_msg`

**속성**:
```cpp
struct AsyncMsg {
    std::string logger_name;        // 로거 이름
    spdlog::level level;            // 로그 레벨
    spdlog::source_loc source;      // 소스 위치 (파일:라인)
    std::chrono::system_clock::time_point time;  // 타임스탬프
    std::string payload;            // 포맷된 메시지
    size_t thread_id;               // 스레드 ID
};
```

**메모리 사용량**:
- 평균 메시지 크기: 256 bytes
- 큐 크기 8192 → 총 2MB

**검증 규칙**:
- payload는 비어있을 수 없음
- time은 과거 시간이어야 함

---

### 5. SignalHandler (시그널 핸들러)

**목적**: 크래시 시그널(SIGSEGV, SIGABRT)을 처리하여 로그 플러시

**타입**: 함수 포인터 (`void (*)(int)`)

**속성**:
```cpp
void signal_handler(int signal) {
    // 1. 백트레이스 기록
    print_backtrace();

    // 2. 로그 플러시
    spdlog::shutdown();

    // 3. 기본 핸들러로 전달
    std::signal(signal, SIG_DFL);
    std::raise(signal);
}
```

**등록**:
```cpp
std::signal(SIGSEGV, signal_handler);
std::signal(SIGABRT, signal_handler);
std::signal(SIGTERM, signal_handler);  // 선택적
```

**관계**:
- 전역 함수 (프로세스당 1개)
- AsyncLogger와 독립적 (spdlog::shutdown() 호출만)

**주의사항**:
- async-signal-safe 함수만 사용해야 하지만 spdlog::shutdown()은 예외 (실용적 선택)
- 무한 재귀 방지 (signal handler 내부 크래시 시)

---

### 6. FlushThread (주기적 플러시 스레드)

**목적**: 3초마다 로그 버퍼를 디스크에 플러시

**타입**: `std::thread`

**구현**:
```cpp
std::thread flush_thread([logger]() {
    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        logger->flush();
    }
});
```

**관계**:
- AsyncLogger와 N:1 (여러 로거가 독립적으로 flush 스레드 가질 수 있음, 단 MXRC는 단일 로거)
- 애플리케이션 종료 시 running=false로 정리

**생명주기**:
```
[초기화] --start_flush_thread()--> [실행 중]
                                      ↓
                              running=false
                                      ↓
                               flush_thread.join()
                                      ↓
                                   [종료됨]
```

---

## 데이터 흐름 (Data Flow)

### 1. 로그 메시지 작성 흐름

```
사용자 코드: spdlog::info("Value: {}", x)
    ↓
[1] AsyncLogger::log(level=info, fmt="Value: {}", args...)
    ↓
[2] 메시지 포맷팅 (fmt 라이브러리)
    ↓
[3] AsyncMsg 생성 (logger_name, level, time, payload)
    ↓
[4] ThreadPool::queue.enqueue(msg)  // lock-free, <1μs
    ↓
[5] 즉시 반환 (사용자 코드 계속 진행)

────────────────────────────────────────────────────────

백그라운드 스레드 (병렬 실행):
[6] ThreadPool::queue.dequeue()  // 메시지 대기
    ↓
[7] AsyncMsg msg = queue.pop()
    ↓
[8] for (auto& sink : logger->sinks())
    ↓
[9] sink->log(msg)
    ↓
[10] fwrite(formatted_msg, file)
    ↓
[11] (flush_level 이상이면) fflush(file)
```

**타이밍**:
- [1]~[5]: <10μs (사용자 스레드 블로킹)
- [6]~[11]: 백그라운드 (사용자 코드와 병렬)

### 2. CRITICAL 로그 플러시 흐름

```
spdlog::critical("Error occurred")
    ↓
[1]~[5] 동일
    ↓
[6]~[10] 동일
    ↓
[11] fflush(file)  // ✅ 즉시 디스크 쓰기
```

**보장**:
- CRITICAL 로그는 100ms 이내 파일에 기록
- 크래시 시 유실 없음

### 3. 크래시 시 로그 보존 흐름

```
정상 실행 중...
    ↓
SIGSEGV 발생 (예: nullptr dereference)
    ↓
[1] OS가 signal_handler(SIGSEGV) 호출
    ↓
[2] print_backtrace()  // backward-cpp
    ↓
[3] spdlog::shutdown()  // 모든 로거 플러시 + 정리
    ↓
[4] 큐에 남은 모든 메시지 처리
    ↓
[5] 모든 sink->flush()
    ↓
[6] 스레드 풀 종료
    ↓
[7] std::signal(SIGSEGV, SIG_DFL)  // 기본 핸들러 복원
    ↓
[8] std::raise(SIGSEGV)  // 코어 덤프 생성
```

**결과**:
- 크래시 3초 이내 로그 99% 보존
- 백트레이스 포함

### 4. 주기적 플러시 흐름

```
[FlushThread]
while (running) {
    sleep(3s)
    ↓
    logger->flush()
    ↓
    큐에 남은 메시지 모두 처리
    ↓
    모든 sink->flush()
    ↓
    fflush(file)
}
```

**효과**:
- 최대 3초 이내 로그가 디스크에 기록됨
- 크래시 시 유실 최소화

---

## 상태 다이어그램

### AsyncLogger 상태 전이

```
┌─────────────┐
│ Uninitialized│
└──────┬──────┘
       │ initialize_async_logger()
       ↓
┌─────────────┐
│   Active    │ ←──┐
└──────┬──────┘    │
       │           │ log(), flush()
       │           │
       │ spdlog::shutdown()
       ↓
┌─────────────┐
│  Shutdown   │
└─────────────┘
```

### LogMessage 상태 전이

```
┌─────────────┐
│   Created   │ (메모리 할당)
└──────┬──────┘
       │ queue.enqueue()
       ↓
┌─────────────┐
│   Queued    │ (lock-free queue)
└──────┬──────┘
       │ queue.dequeue()
       ↓
┌─────────────┐
│ Processing  │ (백그라운드 스레드)
└──────┬──────┘
       │ sink->log()
       ↓
┌─────────────┐
│   Written   │ (파일/콘솔 출력)
└──────┬──────┘
       │ 소멸자
       ↓
┌─────────────┐
│  Destroyed  │ (메모리 해제)
└─────────────┘
```

---

## 메모리 레이아웃

### 주요 컴포넌트 메모리 사용량

| 컴포넌트 | 크기 | 수량 | 총 메모리 |
|---------|------|------|----------|
| AsyncLogger | ~1KB | 1 | 1KB |
| ThreadPool | ~8KB | 1 | 8KB |
| Lock-free queue | ~2MB | 1 | 2MB |
| ConsoleSink | ~1KB | 1 | 1KB |
| FileSink | ~1KB | 1 | 1KB |
| FlushThread | ~8MB (스택) | 1 | 8MB |
| **총계** | | | **~10MB** |

**메모리 최적화**:
- Lock-free queue는 고정 크기 (오버플로우 시 block)
- LogMessage는 포맷 후 바로 삭제 (누적 안 됨)
- shared_ptr로 자동 메모리 관리

---

## 스레드 안전성

### 락 전략

| 컴포넌트 | 락 타입 | 용도 |
|---------|---------|------|
| AsyncLogger | lock-free | log() 호출 시 |
| ThreadPool::queue | lock-free | enqueue/dequeue |
| ConsoleSink | std::mutex | stdout 쓰기 |
| FileSink | std::mutex | 파일 쓰기 |
| spdlog::shutdown() | 전역 뮤텍스 | 종료 시 |

**데드락 방지**:
- 각 락은 독립적 (중첩 사용 없음)
- lock-free queue로 락 경합 최소화
- RAII 패턴으로 자동 해제 (lock_guard, shared_ptr)

---

## 성능 특성

### 시간 복잡도

| 연산 | 동기식 (기존) | 비동기식 (개선) |
|------|--------------|----------------|
| log() 호출 | O(1) + I/O (수 ms) | O(1) + enqueue (<1μs) |
| flush() | O(N) + I/O | O(N) + I/O (백그라운드) |
| shutdown() | O(N) + I/O | O(N) + I/O (큐 비우기) |

### 예상 성능 개선

- **로깅 오버헤드**: 1000μs → 10μs (100배 개선)
- **제어 루프 지연**: 5% → 1% (5배 개선)
- **처리량**: 1,000 msg/sec → 100,000 msg/sec (100배 개선)

---

## 구현 세부사항

### 초기화 순서

```cpp
// src/core/logging/Log.h

namespace mxrc::core::logging {

inline void initialize_async_logger() {
    // 1. Sink 생성
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "logs/mxrc.log", true);

    // 2. ThreadPool 초기화
    spdlog::init_thread_pool(8192, 1);

    // 3. AsyncLogger 생성
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

    // 7. FlushThread 시작
    start_flush_thread(logger);
}

}  // namespace mxrc::core::logging
```

### 종료 순서

```cpp
// 애플리케이션 종료 시 (main() 끝 또는 signal handler)

// 1. FlushThread 정지
stop_flush_thread();

// 2. 남은 로그 플러시
spdlog::shutdown();  // 모든 큐 비우기 + flush

// 3. 자동 정리 (RAII)
// - AsyncLogger 소멸
// - ThreadPool 소멸
// - Sink들 소멸
```

---

## 다음 단계

Phase 1 계속: quickstart.md 작성 (개발자 실무 가이드)
