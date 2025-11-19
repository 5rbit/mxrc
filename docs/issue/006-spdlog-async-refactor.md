## 🔴 이슈 #006: spdlog 동기(Synchronous) 사용으로 인한 성능 저하 위험

**날짜**: 2025-11-19
**심각도**: Medium
**브랜치**: `feature/async-logging`
**상태**: 📝 제안됨 (Proposed)
**관련 커밋**: N/A

---

### 1. 문제 증상

`spdlog`를 사용한 로깅 호출이 현재 동기(Synchronous) 방식으로 이루어지고 있습니다. 이로 인해 성능에 민감한 코드 경로(예: 실시간 제어 루프, 고속 데이터 처리)에서 로깅 작업이 발생할 경우, 파일 I/O 대기로 인해 스레드가 일시적으로 블로킹(Blocking)될 수 있습니다.

이 문제는 평상시에는 드러나지 않을 수 있으나, 시스템 부하가 높거나 디스크 활동이 많아질 때 예기치 않은 지연(Latency)을 유발하여 전체 시스템의 반응성과 안정성을 저해할 수 있는 잠재적 위험 요소입니다.

### 2. 근본 원인 분석

코드베이스 분석 결과(`docs/research/002-logging-datastore-analysis.md` 참조), `spdlog` 로거가 중앙화된 비동기 설정 없이, 코드베이스 여러 부분에서 각각 동기식 싱크(예: `stdout_color_sink_mt`)로 초기화되고 있음을 확인했습니다.

**문제 코드 예시 (`tests/integration/sequence/SequenceIntegration_test.cpp`)**:
```cpp
// 테스트 코드 등에서 흔히 발견되는 동기식 로거 설정
auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
console_sink->set_level(spdlog::level::trace);
auto logger = std::make_shared<spdlog::logger>("test_logger", console_sink);
spdlog::register_logger(logger);
```
위 코드는 로그 메시지를 즉시 콘솔에 출력하며, 이 과정에서 발생하는 I/O 작업은 호출 스레드를 블로킹합니다.

---

### 3. 해결 방안

`spdlog`가 제공하는 비동기 로깅 기능을 활용하여 로깅 호출과 실제 I/O 작업을 분리합니다. 이를 통해 로깅으로 인한 성능 영향을 최소화합니다.

#### 3.1. 비동기 로거 팩토리 구현

`spdlog::async_factory`를 사용하여 로거를 생성하는 중앙화된 유틸리티를 구현합니다. 이 팩토리는 내부적으로 큐(Queue)를 사용하여 로그 메시지를 백그라운드 스레드로 전달하고, 백그라운드 스레드가 실제 파일 쓰기/콘솔 출력을 담당하게 합니다.

**개선 코드 예시 (`src/core/logging/Log.h`)**:
```cpp
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace mxrc::core::logging {

inline void initialize_async_logger() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/mxrc.log", true);
    
    // 비동기 로거 생성
    spdlog::init_thread_pool(8192, 1); // 8192는 큐 크기, 1은 스레드 수
    std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
    auto logger = std::make_shared<spdlog::async_logger>(
        "mxrc_logger", 
        sinks.begin(), 
        sinks.end(), 
        spdlog::thread_pool(), 
        spdlog::async_overflow_policy::block
    );
    
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
}

} // namespace mxrc::core::logging
```

#### 3.2. 중앙화된 로거 설정

위에서 정의한 `initialize_async_logger()` 함수를 애플리케이션의 시작점(`src/main.cpp`)에서 단 한 번 호출하여, 프로젝트 전반에서 사용될 기본 로거를 설정합니다.

#### 3.3. 기존 코드 리팩토링

코드베이스에 산재된 모든 개별적인 `spdlog::register_logger` 또는 로거 생성 코드를 제거하고, 대신 `spdlog::get("mxrc_logger")`를 사용하거나 `spdlog::info(...)` 등의 전역 함수를 통해 중앙화된 로거를 사용하도록 수정합니다.

---

### 4. 로거 안정성 강화 방안 ("좀비 모드")

애플리케이션에 예기치 않은 오류(크래시, 세그멘테이션 폴트 등)가 발생하더라도, 로거는 마지막 순간까지 시스템의 상태를 기록할 수 있는 "좀비"와 같은 생명력을 가져야 합니다. 이를 위해 다음과 같은 다층적 안정성 강화 방안을 제안합니다.

#### 4.1. 레벨 1: 자동 Flush 및 자체 오류 처리 강화

비동기 로거의 기본 안정성을 확보하는 단계입니다.

- **위험 수준(Critical) 로그 즉시 Flush**: `critical` 레벨 이상의 로그가 기록될 때, 버퍼에 남아있지 않고 즉시 파일에 쓰도록 설정합니다. 이를 통해 치명적인 오류 발생 직전의 로그가 유실되는 것을 방지합니다.
  ```cpp
  // 초기화 시 추가
  logger->flush_on(spdlog::level::critical);
  ```

- **주기적 Flush 설정**: 일정 시간 간격으로 로그 버퍼를 자동으로 Flush하여, 예기치 않은 종료 시에도 최근 로그의 유실을 최소화합니다.
  ```cpp
  // 초기화 시 추가
  spdlog::flush_every(std::chrono::seconds(3));
  ```

- **로거 자체 오류 핸들러**: `spdlog`가 파일에 로그를 쓸 수 없는 등 자체적인 오류가 발생했을 때 애플리케이션 전체가 중단되지 않도록 별도의 오류 처리 로직을 등록합니다.
  ```cpp
  // 초기화 시 추가
  spdlog::set_error_handler([](const std::string& msg) {
      // 콘솔에 직접 에러를 출력하거나, 비상용 로그 파일에 기록
      fprintf(stderr, "SPDLOG CRITICAL ERROR: %s\n", msg.c_str());
  });
  ```

#### 4.2. 레벨 2: 시그널 핸들링을 통한 최종 로그 확보

`SIGSEGV`, `SIGABRT` 치명적인 시그널이 발생했을 때, 프로세스가 종료되기 직전 마지막 로그를 남기는 단계입니다.

- **안전한 시그널 핸들러 등록**: 시그널 핸들러 내에서는 비동기-시그널-안전(async-signal-safe) 함수만 사용해야 하므로, 직접적인 로깅은 위험합니다. 대신, 핸들러는 `spdlog::shutdown()`을 호출하여 버퍼에 남아있는 모든 로그를 강제로 Flush하고, 백트레이스 정보를 기록하도록 시도합니다.
- **백트레이스 로깅**: `backward-cpp`와 같은 라이브러리를 사용하여 크래시 발생 시점의 스택 트레이스를 로그로 남길 수 있습니다. 시그널 핸들러에서 이 정보를 포착하여 최후의 로그 메시지로 기록합니다.

**개선 코드 예시 (`src/core/common/SignalHandler.h`)**:
```cpp
#include <backward.hpp>
#include <spdlog/spdlog.h>
#include <csignal>

namespace mxrc::core::common {

inline void register_signal_handler() {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    
    sa.sa_sigaction = [](int, siginfo_t*, void*) {
        // 백트레이스 정보 캡처
        backward::StackTrace st;
        st.load_here(32);
        std::stringstream ss;
        backward::Printer p;
        p.print(st, ss);

        // 최후의 메시지 로깅 (critical 레벨로 즉시 flush 유도)
        SPDLOG_CRITICAL("FATAL SIGNAL DETECTED. SHUTTING DOWN LOGGER.\nBacktrace:\n{}", ss.str());
        
        // spdlog의 모든 리소스 정리 및 flush
        spdlog::shutdown();

        // 기본 핸들러가 코어 덤프 등을 처리하도록 다시 시그널 발생
        std::signal(SIGABRT, SIG_DFL);
        std::raise(SIGABRT);
    };

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    //... 다른 시그널들
}

} // namespace
```
**주의**: 시그널 핸들링은 매우 신중하게 구현해야 하며, 충분한 테스트가 필요합니다.

#### 4.3. 레벨 3: 로거 프로세스 분리 (장기 과제)

최고 수준의 안정성을 위해, 로깅 시스템을 메인 애플리케이션과 별개의 프로세스로 분리하는 아키텍처를 고려할 수 있습니다.

- **구조**: 메인 앱은 IPC(Inter-Process Communication), 예컨대 Unix Domain Socket이나 TCP를 통해 로그 메시지를 로거 프로세스에 전달합니다.
- **장점**: 메인 앱에 크래시가 발생해도 로거 프로세스는 전혀 영향을 받지 않으므로, 로그 유실 가능성이 원천적으로 차단됩니다.
- **단점**: 아키텍처가 복잡해지고, IPC 통신 오버헤드가 발생합니다. 이는 향후 시스템 안정성을 극한으로 높여야 할 때 고려할 수 있는 장기 과제입니다.

---

### 5. 다음 단계

- [ ] `src/core/logging/` 디렉토리에 중앙 로깅 설정 유틸리티(예: `Log.h`, `Log.cpp`) 파일 생성
- [ ] 비동기 로거를 초기화하는 `initialize_async_logger()` 함수 구현
- [ ] **(신규)** 레벨 1 안정성 강화 방안(`flush_on`, `flush_every`, `set_error_handler`)을 초기화 함수에 추가
- [ ] **(신규)** 레벨 2 안정성 강화 방안(시그널 핸들러)을 구현하고 `main.cpp`에서 등록
- [ ] `src/main.cpp`의 시작 부분에서 위 함수를 호출하도록 수정
- [ ] 프로젝트 전체에서 개별적으로 로거를 생성하는 코드를 모두 제거하고, 중앙 로거를 사용하도록 리팩토링
- [ ] 테스트 코드 역시 중앙 로거를 사용하거나, 테스트 목적의 경량 로거를 사용하되 동기 방식의 위험성을 인지하고 최소한으로 사용

### 6. 관련 이슈

- **분석 문서**: `docs/research/002-logging-datastore-analysis.md`

### 7. 참고 자료

- [spdlog Asynchronous support](https://github.com/gabime/spdlog/wiki/4.-Asynchronous-support)
- [backward-cpp (Stack-trace library)](https://github.com/bombela/backward-cpp)
