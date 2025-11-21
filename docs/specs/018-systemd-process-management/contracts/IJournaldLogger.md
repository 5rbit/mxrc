# Interface Contract: IJournaldLogger

**Feature**: 018-systemd-process-management
**Component**: journald Logging Integration
**Status**: Approved
**Last Updated**: 2025-01-21

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: interface, API, method 등)
- 일반 설명, 동작 설명은 모두 한글로 작성합니다

---

## 개요

`IJournaldLogger`는 systemd의 journald에 구조화된 로그를 전송하는 인터페이스입니다. sd_journal_send() API를 사용하여 W3C Trace Context 및 ECS(Elastic Common Schema) 호환 필드를 포함한 로그를 journald에 기록합니다.

---

## 인터페이스 정의

### C++ 헤더

**파일 위치**: `src/core/systemd/interfaces/IJournaldLogger.h`

```cpp
#ifndef MXRC_SYSTEMD_IJOURNALD_LOGGER_H
#define MXRC_SYSTEMD_IJOURNALD_LOGGER_H

#include "systemd/dto/JournaldEntry.h"
#include <string>
#include <map>

namespace mxrc::systemd {

/**
 * @brief journald 구조화 로깅 인터페이스
 *
 * systemd의 journald에 구조화된 로그를 전송합니다.
 * W3C Trace Context 및 ECS 필드를 지원합니다.
 *
 * @note 구현체는 스레드 안전해야 합니다.
 */
class IJournaldLogger {
public:
    virtual ~IJournaldLogger() = default;

    /**
     * @brief journald에 로그 전송
     *
     * JournaldEntry 객체를 journald에 전송합니다.
     * sd_journal_send()를 사용하여 구조화된 필드를 포함합니다.
     *
     * @param entry 로그 항목
     * @throws std::runtime_error sd_journal_send() 실패 시
     *
     * @note 성능 요구사항: 지연 < 1ms
     * @note 스레드 안전: 동시 호출 가능
     */
    virtual void log(const JournaldEntry& entry) = 0;

    /**
     * @brief 간편 로그 메서드 (메시지만)
     *
     * 메시지와 우선순위만으로 로그를 전송합니다.
     *
     * @param message 로그 메시지
     * @param priority journald 우선순위 (0-7)
     * @throws std::runtime_error sd_journal_send() 실패 시
     */
    virtual void log(const std::string& message, int priority) = 0;

    /**
     * @brief Trace Context 포함 로그
     *
     * W3C Trace Context를 포함한 로그를 전송합니다.
     *
     * @param message 로그 메시지
     * @param priority journald 우선순위 (0-7)
     * @param trace_id W3C Trace ID
     * @param span_id W3C Span ID
     * @throws std::runtime_error sd_journal_send() 실패 시
     */
    virtual void logWithTrace(
        const std::string& message,
        int priority,
        const std::string& trace_id,
        const std::string& span_id) = 0;

    /**
     * @brief 커스텀 필드 포함 로그
     *
     * 추가 필드를 포함한 로그를 전송합니다.
     *
     * @param message 로그 메시지
     * @param priority journald 우선순위 (0-7)
     * @param fields 추가 필드 (키-값 쌍)
     * @throws std::runtime_error sd_journal_send() 실패 시
     * @throws std::invalid_argument 잘못된 필드 이름 시 (대문자+밑줄만 허용)
     */
    virtual void logWithFields(
        const std::string& message,
        int priority,
        const std::map<std::string, std::string>& fields) = 0;

    /**
     * @brief 로그 플러시
     *
     * 버퍼링된 로그를 즉시 journald에 전송합니다.
     * (현재 구현은 비동기 전송이므로 플러시 불필요할 수 있음)
     */
    virtual void flush() = 0;

    /**
     * @brief journald 사용 가능 여부 확인
     *
     * journald 소켓이 존재하는지 확인합니다.
     *
     * @return true journald 사용 가능
     * @return false journald 사용 불가 (fallback to spdlog)
     */
    virtual bool isJournaldAvailable() const = 0;

    /**
     * @brief 전송 성공 카운트 조회
     *
     * @return uint64_t 누적 성공 횟수
     */
    virtual uint64_t getSuccessCount() const = 0;

    /**
     * @brief 전송 실패 카운트 조회
     *
     * @return uint64_t 누적 실패 횟수
     */
    virtual uint64_t getFailureCount() const = 0;

    /**
     * @brief 통계 초기화
     *
     * 성공/실패 카운트를 0으로 리셋합니다.
     */
    virtual void resetStats() = 0;
};

} // namespace mxrc::systemd

#endif // MXRC_SYSTEMD_IJOURNALD_LOGGER_H
```

---

## 메서드 계약

### log(JournaldEntry)

**목적**: 구조화된 로그 전송

**사전 조건**:
- entry.message가 비어있지 않음
- entry.priority가 0-7 범위

**사후 조건**:
- journald에 로그 저장됨
- journalctl로 조회 가능

**필드 매핑**:
```
entry.message      → MESSAGE
entry.priority     → PRIORITY
entry.trace_id     → TRACE_ID
entry.span_id      → SPAN_ID
entry.service      → SERVICE
entry.component    → COMPONENT
entry.extra_fields → 각각 별도 필드
```

**오류 처리**:
- sd_journal_send() 실패 → `std::runtime_error`
- 실패 시 failure_count 증가

### log(message, priority)

**목적**: 간단한 로그 전송

**사용 예시**:
```cpp
logger->log("Service started", JournaldPriority::INFO);
logger->log("Watchdog timeout", JournaldPriority::ERR);
```

### logWithTrace()

**목적**: Trace Context 포함 로그

**Trace ID/Span ID 형식**:
- Trace ID: 32자 hex 문자열 (W3C Trace Context)
- Span ID: 16자 hex 문자열

**사용 예시**:
```cpp
logger->logWithTrace(
    "Task execution started",
    JournaldPriority::INFO,
    "4bf92f3577b34da6a3ce929d0e0e4736",
    "00f067aa0ba902b7"
);
```

**journald 저장 형식**:
```
MESSAGE=Task execution started
PRIORITY=6
TRACE_ID=4bf92f3577b34da6a3ce929d0e0e4736
SPAN_ID=00f067aa0ba902b7
```

### logWithFields()

**목적**: 커스텀 필드 포함 로그

**필드 이름 규칙**:
- 대문자 + 밑줄만 허용 (예: TASK_ID, ACTION_TYPE)
- 예약된 필드 이름 금지 (MESSAGE, PRIORITY 등)

**사용 예시**:
```cpp
std::map<std::string, std::string> fields = {
    {"TASK_ID", "task-001"},
    {"ACTION_ID", "move-to-pose"},
    {"EXECUTION_TIME_MS", "125"}
};

logger->logWithFields("Task completed", JournaldPriority::INFO, fields);
```

**오류 처리**:
- 잘못된 필드 이름 (소문자 포함) → `std::invalid_argument`

### isJournaldAvailable()

**목적**: journald 소켓 존재 확인

**확인 방법**:
- `/run/systemd/journal/socket` 파일 존재 확인
- 또는 `sd_journal_sendv()` 테스트 호출

**반환값**:
- `true`: journald 사용 가능
- `false`: fallback to spdlog file logging

---

## 구현 예시

### JournaldLogger (libsystemd 기반 구현)

**파일 위치**: `src/core/systemd/impl/JournaldLogger.h`

```cpp
#ifndef MXRC_SYSTEMD_JOURNALD_LOGGER_H
#define MXRC_SYSTEMD_JOURNALD_LOGGER_H

#include "systemd/interfaces/IJournaldLogger.h"
#include <systemd/sd-journal.h>
#include <mutex>
#include <atomic>

namespace mxrc::systemd {

class JournaldLogger : public IJournaldLogger {
public:
    JournaldLogger()
        : success_count_(0), failure_count_(0) {
    }

    void log(const JournaldEntry& entry) override {
        std::lock_guard<std::mutex> lock(mutex_);

        try {
            int ret = sd_journal_send(
                "MESSAGE=%s", entry.message.c_str(),
                "PRIORITY=%d", entry.priority,
                "TRACE_ID=%s", entry.trace_id.c_str(),
                "SPAN_ID=%s", entry.span_id.c_str(),
                "SERVICE=%s", entry.service.c_str(),
                "COMPONENT=%s", entry.component.c_str(),
                nullptr
            );

            // 추가 필드 전송
            for (const auto& [key, value] : entry.extra_fields) {
                sd_journal_send_with_location(
                    nullptr, nullptr, 0,
                    key.c_str(), "%s", value.c_str(),
                    nullptr
                );
            }

            if (ret < 0) {
                failure_count_++;
                throw std::runtime_error("sd_journal_send failed: " +
                                         std::to_string(-ret));
            }

            success_count_++;

        } catch (const std::exception& e) {
            failure_count_++;
            throw;
        }
    }

    void log(const std::string& message, int priority) override {
        std::lock_guard<std::mutex> lock(mutex_);

        int ret = sd_journal_send(
            "MESSAGE=%s", message.c_str(),
            "PRIORITY=%d", priority,
            nullptr
        );

        if (ret < 0) {
            failure_count_++;
            throw std::runtime_error("sd_journal_send failed");
        }

        success_count_++;
    }

    void logWithTrace(
        const std::string& message,
        int priority,
        const std::string& trace_id,
        const std::string& span_id) override {

        JournaldEntry entry;
        entry.message = message;
        entry.priority = priority;
        entry.trace_id = trace_id;
        entry.span_id = span_id;

        log(entry);
    }

    void logWithFields(
        const std::string& message,
        int priority,
        const std::map<std::string, std::string>& fields) override {

        // 필드 이름 검증
        for (const auto& [key, value] : fields) {
            if (!isValidFieldName(key)) {
                throw std::invalid_argument("Invalid field name: " + key);
            }
        }

        JournaldEntry entry;
        entry.message = message;
        entry.priority = priority;
        entry.extra_fields = fields;

        log(entry);
    }

    void flush() override {
        // sd_journal_send는 즉시 전송하므로 플러시 불필요
    }

    bool isJournaldAvailable() const override {
        return access("/run/systemd/journal/socket", F_OK) == 0;
    }

    uint64_t getSuccessCount() const override {
        return success_count_.load();
    }

    uint64_t getFailureCount() const override {
        return failure_count_.load();
    }

    void resetStats() override {
        success_count_ = 0;
        failure_count_ = 0;
    }

private:
    bool isValidFieldName(const std::string& name) const {
        // 대문자 + 밑줄만 허용
        for (char c : name) {
            if (!std::isupper(c) && c != '_') {
                return false;
            }
        }
        return !name.empty();
    }

    mutable std::mutex mutex_;
    std::atomic<uint64_t> success_count_;
    std::atomic<uint64_t> failure_count_;
};

} // namespace mxrc::systemd

#endif // MXRC_SYSTEMD_JOURNALD_LOGGER_H
```

---

## 사용 예시

### 기본 사용법

```cpp
#include "systemd/impl/JournaldLogger.h"

int main() {
    auto logger = std::make_shared<mxrc::systemd::JournaldLogger>();

    // journald 사용 가능 여부 확인
    if (!logger->isJournaldAvailable()) {
        spdlog::warn("journald not available, falling back to file logging");
        return 1;
    }

    // 간단한 로그
    logger->log("Service starting...", JournaldPriority::INFO);

    // Trace Context 포함
    logger->logWithTrace(
        "Task execution started",
        JournaldPriority::INFO,
        "4bf92f3577b34da6a3ce929d0e0e4736",
        "00f067aa0ba902b7"
    );

    // 커스텀 필드 포함
    std::map<std::string, std::string> fields = {
        {"TASK_ID", "task-001"},
        {"EXECUTION_TIME_MS", "125"}
    };
    logger->logWithFields("Task completed", JournaldPriority::INFO, fields);

    return 0;
}
```

### StructuredLogger와 통합

```cpp
class SystemdStructuredLogger : public IStructuredLogger {
private:
    std::shared_ptr<spdlog::logger> spdlog_logger_;
    std::shared_ptr<IJournaldLogger> journald_logger_;

public:
    void log(const StructuredLogEvent& event) override {
        // 1. spdlog로 파일 로깅
        spdlog_logger_->log(event.level, event.message);

        // 2. journald로 구조화 로깅 (journald 사용 가능 시)
        if (journald_logger_->isJournaldAvailable()) {
            JournaldEntry entry;
            entry.message = event.message;
            entry.priority = toJournaldPriority(event.level);
            entry.trace_id = event.trace_context.trace_id;
            entry.span_id = event.trace_context.span_id;
            entry.service = "mxrc-rt";
            entry.component = event.component;

            journald_logger_->log(entry);
        }
    }

private:
    int toJournaldPriority(spdlog::level::level_enum level) {
        switch (level) {
            case spdlog::level::critical: return JournaldPriority::CRIT;
            case spdlog::level::err:      return JournaldPriority::ERR;
            case spdlog::level::warn:     return JournaldPriority::WARNING;
            case spdlog::level::info:     return JournaldPriority::INFO;
            case spdlog::level::debug:    return JournaldPriority::DEBUG;
            case spdlog::level::trace:    return JournaldPriority::DEBUG;
            default:                      return JournaldPriority::INFO;
        }
    }
};
```

---

## journald 쿼리 예시

### 서비스별 로그 조회

```bash
# mxrc-rt 서비스 로그만 조회
journalctl SERVICE=mxrc-rt

# 최근 100줄
journalctl SERVICE=mxrc-rt -n 100

# 실시간 모니터링
journalctl SERVICE=mxrc-rt -f
```

### Trace ID로 로그 추적

```bash
# 특정 Trace ID의 모든 로그
journalctl TRACE_ID=4bf92f3577b34da6a3ce929d0e0e4736

# JSON 형식으로 출력
journalctl TRACE_ID=4bf92f3577b34da6a3ce929d0e0e4736 -o json-pretty
```

### 컴포넌트별 로그 조회

```bash
# Task Layer 로그만
journalctl COMPONENT=task

# Action Layer + Sequence Layer
journalctl COMPONENT=action -or COMPONENT=sequence
```

### 우선순위 필터

```bash
# ERROR 이상 (PRIORITY 0-3)
journalctl SERVICE=mxrc-rt PRIORITY=0..3

# WARNING 이상 (PRIORITY 0-4)
journalctl SERVICE=mxrc-rt PRIORITY=0..4
```

### 시간 범위 조회

```bash
# 오늘 로그
journalctl SERVICE=mxrc-rt --since today

# 최근 1시간
journalctl SERVICE=mxrc-rt --since "1 hour ago"

# 특정 시간 범위
journalctl SERVICE=mxrc-rt --since "2025-01-21 10:00:00" --until "2025-01-21 11:00:00"
```

---

## ECS 필드 매핑

### 중앙 로그 수집기로 전송 시

journald 필드를 ECS 필드로 매핑:

```json
{
  "@timestamp": "2025-01-21T10:30:00.000Z",
  "message": "Task execution started",
  "log.level": "info",
  "service.name": "mxrc-rt",
  "component": "task",
  "trace.id": "4bf92f3577b34da6a3ce929d0e0e4736",
  "trace.span.id": "00f067aa0ba902b7",
  "labels": {
    "task_id": "task-001",
    "execution_time_ms": "125"
  }
}
```

### Logstash 설정 예시

```ruby
input {
  journald {
    filter => {
      "SERVICE" => "mxrc-rt"
    }
  }
}

filter {
  mutate {
    rename => {
      "MESSAGE" => "message"
      "TRACE_ID" => "[trace][id]"
      "SPAN_ID" => "[trace][span][id]"
      "SERVICE" => "[service][name]"
      "COMPONENT" => "component"
      "TASK_ID" => "[labels][task_id]"
    }
  }
}

output {
  elasticsearch {
    hosts => ["localhost:9200"]
    index => "mxrc-logs-%{+YYYY.MM.dd}"
  }
}
```

---

## 성능 요구사항

- **로깅 지연**: < 1ms (평균)
- **처리량**: 초당 1000개 로그 (burst)
- **메모리 사용량**: < 5MB
- **스레드 안전성**: mutex 기반 동기화

---

## 보안 고려사항

- **권한**: sd_journal_send()는 특별한 권한 불필요
- **Seccomp**: `@basic-io` 세트에 포함
- **민감 정보**: 비밀번호, 토큰 등 로깅 금지 (필터링 필요)

---

## 의존성

- **빌드 의존성**: libsystemd-dev
- **런타임 의존성**: systemd (v255+), journald
- **헤더**: `<systemd/sd-journal.h>`

---

## 변경 이력

| 버전 | 날짜       | 변경 내용           | 작성자 |
|------|------------|---------------------|--------|
| 1.0  | 2025-01-21 | 초기 인터페이스 정의 | MXRC   |

---

**작성자**: MXRC Development Team
**검토자**: TBD
**승인 날짜**: TBD
