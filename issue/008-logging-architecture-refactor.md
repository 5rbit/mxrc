# TROUBLESHOOTING - MXRC 문제 해결 가이드

이 문서는 MXRC 프로젝트에서 발생한 주요 문제와 해결 방법을 기록합니다.

---

## 🟡 이슈 #008: 로깅 아키텍처 평가 및 리팩터링 제안

**날짜**: 2025-11-19
**심각도**: High
**브랜치**: N/A
**상태**: 🔍 조사 중 (Under Investigation)

### 문제 증상

현재 로깅 아키텍처는 `spdlog` 기반으로 비동기 로깅을 구현했으나, 여러 구조적 문제점으로 인해 안정성과 확장성이 저하되고 있습니다.

- **테스트 격리 실패**: 다수의 테스트 코드(`SequenceTaskManagerIntegration_test.cpp` 등)가 자체 로거를 등록(`spdlog::register_logger`)하여 전역 상태를 오염시키고, 애플리케이션의 실제 로깅 경로(비동기)를 테스트하지 못합니다. 이는 프로젝트의 개발 가이드라인(`GEMINI.md`)에서 금지하는 싱글턴 의존성의 전형적인 부작용입니다.
- **일관성 없는 로깅 패턴**: 코어 로직(Action, Sequence, Task)에서는 `Logger::get()` 래퍼(`src/core/action/util/Logger.h`)를 통해 로거를 복제하여 사용하는 반면, 다른 곳에서는 `spdlog::info()`를 직접 호출하여 일관성이 깨져있습니다.
- **비효율적인 구현**: `Log.h`의 `initialize_async_logger` 함수 내에 수동으로 생성된 `g_flush_thread`는 `spdlog`가 제공하는 `spdlog::flush_every()` 기능과 중복되어 불필요한 복잡성을 야기합니다.
- **경직된 설정**: 로그 레벨, 포맷, 파일 경로, 큐 사이즈 등 모든 설정이 하드코딩되어 있어 환경별로 설정을 변경하기 어렵습니다.
- **구조화된 로깅 부재**: 현재 로그는 단순 문자열 포맷팅에 의존하여, 로그 데이터의 정규화 및 분석이 어렵습니다.

### 근본 원인 분석

`issue/006-spdlog-async-refactor.md`에서 제안된 비동기 로깅 아키텍처가 도입되었으나, 구현 과정에서 여러 편의주의적 해결책과 표준화되지 않은 패턴이 추가되었습니다. 특히 테스트 코드에서의 부주의한 로거 사용과 불필요한 래퍼 클래스 도입이 문제의 핵심 원인입니다.

### 해결 방법 (리팩터링 제안)

#### 1. 테스트 환경 로깅 격리

- **문제**: 테스트가 전역 로거 상태에 의존하며 서로에게 영향을 줍니다.
- **해결**: 테스트용 로깅 헬퍼 함수 `mxrc::testing::setup_test_logging()`를 `tests/TestHelper.h` 같은 공용 헤더에 만듭니다.
    - 이 함수는 테스트 환경에 맞는 로거(예: `null_sink` 또는 동기식 콘솔 로거)를 설정합니다.
    - 모든 테스트 파일의 `SetUp()` 메소드에서 이 함수를 호출하도록 하고, 각 테스트에 산재된 `spdlog::register_logger` 호출을 전부 제거합니다.

#### 2. 로깅 패턴 단일화

- **문제**: `Logger::get()`과 `spdlog::info()` 호출이 혼재합니다.
- **해결**:
    - `src/core/action/util/Logger.h` 파일을 **삭제**합니다.
    - `Logger::get()->info(...)` 형태의 모든 호출을 표준 `spdlog` 호출(예: `spdlog::info(...)`)로 변경합니다.

#### 3. `Log.h` 구현 개선

- **문제**: 불필요한 수동 flush 스레드가 존재합니다.
- **해결**:
    - `g_flush_thread` 및 관련 `while` 루프를 제거합니다.
    - `initialize_async_logger` 함수 내에서 `spdlog::flush_every(std::chrono::seconds(3));`를 호출하여 `spdlog`의 내장 기능을 활용합니다.

#### 4. 설정 외부화

- **문제**: 로깅 설정이 하드코딩되어 있습니다.
- **해결**: `main.cpp`에서 시작 시 설정 파일(예: `config.json`)이나 환경 변수를 읽어 로그 레벨, 파일 경로 등을 동적으로 설정하도록 변경합니다.

#### 5. 구조화된 로깅 도입

- **문제**: 로그가 단순 문자열이라 분석이 어렵습니다.
- **해결**: `spdlog`의 `fmt` 라이브러리를 활용하여 구조화된 로깅을 생활화합니다.
    - **예시**: `spdlog::info("event={}, status={}, duration_ms={}", "user_login", "success", 123);`
    - 필요시, 이를 강제하는 간단한 래퍼나 가이드라인을 추가합니다.

### 권장 진행 순서

1.  **[High]** 테스트 격리 문제 해결: `setup_test_logging()` 헬퍼를 구현하고 모든 테스트 코드를 리팩터링합니다. 이는 코드베이스의 안정성을 위해 가장 시급합니다.
2.  **[Medium]** 로깅 패턴 단일화: `Logger.h`를 제거하고 모든 호출을 표준화합니다.
3.  **[Medium]** `Log.h` 리팩터링: 중복 flush 스레드를 제거합니다.
4.  **[Low]** 설정 외부화 및 구조화된 로깅 도입: 유연성 및 관찰 가능성을 개선합니다.

### 관련 파일

- **주요 개선 대상**: `src/core/logging/Log.h`
- **삭제 대상**: `src/core/action/util/Logger.h`
- **리팩터링 대상**: `tests/**/*.cpp` 전체, `src/main.cpp`
- **참고 이슈**: `issue/006-spdlog-async-refactor.md`
