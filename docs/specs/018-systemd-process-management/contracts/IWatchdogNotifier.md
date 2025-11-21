# Interface Contract: IWatchdogNotifier

**Feature**: 018-systemd-process-management
**Component**: Watchdog Integration
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

`IWatchdogNotifier`는 systemd watchdog에 프로세스 생존 알림을 전송하는 인터페이스입니다. sd_notify() API를 추상화하여 테스트 가능하고 교체 가능한 설계를 제공합니다.

---

## 인터페이스 정의

### C++ 헤더

**파일 위치**: `src/core/systemd/interfaces/IWatchdogNotifier.h`

```cpp
#ifndef MXRC_SYSTEMD_IWATCHDOG_NOTIFIER_H
#define MXRC_SYSTEMD_IWATCHDOG_NOTIFIER_H

#include <string>

namespace mxrc::systemd {

/**
 * @brief systemd watchdog 알림 인터페이스
 *
 * systemd의 watchdog 메커니즘과 통합하기 위한 추상 인터페이스입니다.
 * 프로세스가 정상 동작 중임을 systemd에 주기적으로 알립니다.
 *
 * @note 구현체는 스레드 안전해야 합니다.
 */
class IWatchdogNotifier {
public:
    virtual ~IWatchdogNotifier() = default;

    /**
     * @brief watchdog 알림 전송
     *
     * "WATCHDOG=1" 알림을 systemd에 전송하여 watchdog 타이머를 리셋합니다.
     * WatchdogSec 설정값의 절반 주기로 호출되어야 합니다.
     *
     * @throws std::runtime_error sd_notify() 호출 실패 시
     *
     * @note 성능 요구사항: 오버헤드 < 10μs
     * @note 스레드 안전: 여러 스레드에서 동시 호출 가능
     */
    virtual void notify() = 0;

    /**
     * @brief 서비스 준비 완료 알림
     *
     * "READY=1" 알림을 systemd에 전송하여 서비스가 준비되었음을 알립니다.
     * Type=notify 서비스에서 필수입니다.
     *
     * @throws std::runtime_error sd_notify() 호출 실패 시
     *
     * @note 프로세스 시작 시 한 번만 호출
     */
    virtual void ready() = 0;

    /**
     * @brief 서비스 종료 시작 알림
     *
     * "STOPPING=1" 알림을 systemd에 전송하여 서비스가 종료 중임을 알립니다.
     *
     * @throws std::runtime_error sd_notify() 호출 실패 시
     *
     * @note 프로세스 종료 시 한 번만 호출
     */
    virtual void stopping() = 0;

    /**
     * @brief 상태 메시지 전송
     *
     * "STATUS=<message>" 형태로 현재 상태를 systemd에 전송합니다.
     * systemctl status 명령으로 확인 가능합니다.
     *
     * @param message 상태 메시지 (최대 255자)
     * @throws std::runtime_error sd_notify() 호출 실패 시
     * @throws std::invalid_argument message가 255자 초과 시
     */
    virtual void status(const std::string& message) = 0;

    /**
     * @brief Watchdog 활성화 여부 확인
     *
     * systemd가 watchdog을 활성화했는지 확인합니다.
     * WATCHDOG_USEC 환경 변수를 확인하여 판단합니다.
     *
     * @return true watchdog 활성화됨
     * @return false watchdog 비활성화됨
     */
    virtual bool isWatchdogEnabled() const = 0;

    /**
     * @brief Watchdog 타임아웃 값 조회
     *
     * systemd가 설정한 watchdog 타임아웃을 마이크로초 단위로 반환합니다.
     * WATCHDOG_USEC 환경 변수 값을 반환합니다.
     *
     * @return uint64_t 타임아웃 (마이크로초), watchdog 비활성화 시 0
     */
    virtual uint64_t getWatchdogTimeoutUsec() const = 0;
};

} // namespace mxrc::systemd

#endif // MXRC_SYSTEMD_IWATCHDOG_NOTIFIER_H
```

---

## 메서드 계약

### notify()

**목적**: watchdog 타이머 리셋

**사전 조건**:
- systemd에서 WatchdogSec 설정됨
- 프로세스가 정상 동작 중

**사후 조건**:
- systemd watchdog 타이머가 리셋됨
- WatchdogSec 이내에 다시 호출하지 않으면 프로세스 재시작됨

**호출 주기**:
- WatchdogSec / 2 (예: WatchdogSec=30s → 15초 주기)
- 권장: WatchdogSec / 3 (안전 마진 포함)

**오류 처리**:
- sd_notify() 실패 시 `std::runtime_error` 발생
- 실패 시 재시도 로직은 호출자 책임

### ready()

**목적**: 서비스 시작 완료 알림

**사전 조건**:
- Type=notify 서비스
- 서비스 초기화 완료

**사후 조건**:
- systemd가 서비스를 "active (running)" 상태로 전환
- 의존하는 서비스가 시작 가능해짐

**호출 시점**:
- main() 함수에서 모든 초기화 완료 후
- 첫 Task 실행 가능 상태가 된 후

**오류 처리**:
- 호출 실패 시 서비스가 "activating" 상태에 머물 수 있음
- 타임아웃 발생 시 systemd가 서비스 실패로 간주

### stopping()

**목적**: 우아한 종료(graceful shutdown) 알림

**사전 조건**:
- 종료 시그널 수신 (SIGTERM, SIGINT)

**사후 조건**:
- systemd가 서비스 종료를 인지
- systemctl status에 "stopping" 상태 표시

**호출 시점**:
- 종료 시그널 핸들러에서 호출
- 정리 작업 시작 전

**오류 처리**:
- 실패해도 종료 진행 (best effort)

### status()

**목적**: 현재 상태 정보 제공

**사전 조건**:
- message 길이 ≤ 255자

**사후 조건**:
- systemctl status 출력에 메시지 표시

**사용 예시**:
```cpp
notifier->status("Initializing Task Layer...");
notifier->status("Processing Action: move-to-pose");
notifier->status("Idle");
```

**오류 처리**:
- message > 255자 시 `std::invalid_argument`
- sd_notify() 실패 시 `std::runtime_error`

### isWatchdogEnabled()

**목적**: Watchdog 활성화 여부 확인

**반환값**:
- `true`: WATCHDOG_USEC > 0
- `false`: WATCHDOG_USEC 없음 또는 0

**사용 예시**:
```cpp
if (notifier->isWatchdogEnabled()) {
    // Watchdog 타이머 시작
    timer.start();
}
```

### getWatchdogTimeoutUsec()

**목적**: Watchdog 타임아웃 조회

**반환값**:
- `0`: watchdog 비활성화
- `> 0`: 타임아웃 (마이크로초)

**사용 예시**:
```cpp
uint64_t timeout_us = notifier->getWatchdogTimeoutUsec();
uint64_t interval_us = timeout_us / 2;  // 절반 주기로 알림
```

---

## 구현 예시

### SdNotifyWatchdog (libsystemd 기반 구현)

**파일 위치**: `src/core/systemd/impl/SdNotifyWatchdog.h`

```cpp
#ifndef MXRC_SYSTEMD_SD_NOTIFY_WATCHDOG_H
#define MXRC_SYSTEMD_SD_NOTIFY_WATCHDOG_H

#include "systemd/interfaces/IWatchdogNotifier.h"
#include <systemd/sd-daemon.h>
#include <mutex>

namespace mxrc::systemd {

/**
 * @brief libsystemd 기반 Watchdog 알림 구현
 *
 * sd_notify() API를 사용하여 실제 systemd와 통신합니다.
 */
class SdNotifyWatchdog : public IWatchdogNotifier {
public:
    SdNotifyWatchdog() = default;
    ~SdNotifyWatchdog() override = default;

    void notify() override {
        std::lock_guard<std::mutex> lock(mutex_);
        int ret = sd_notify(0, "WATCHDOG=1");
        if (ret < 0) {
            throw std::runtime_error("sd_notify(WATCHDOG=1) failed: " +
                                     std::to_string(-ret));
        }
    }

    void ready() override {
        std::lock_guard<std::mutex> lock(mutex_);
        int ret = sd_notify(0, "READY=1");
        if (ret < 0) {
            throw std::runtime_error("sd_notify(READY=1) failed: " +
                                     std::to_string(-ret));
        }
    }

    void stopping() override {
        std::lock_guard<std::mutex> lock(mutex_);
        sd_notify(0, "STOPPING=1");  // Best effort, ignore errors
    }

    void status(const std::string& message) override {
        if (message.size() > 255) {
            throw std::invalid_argument("Status message too long (max 255 chars)");
        }
        std::lock_guard<std::mutex> lock(mutex_);
        std::string msg = "STATUS=" + message;
        int ret = sd_notify(0, msg.c_str());
        if (ret < 0) {
            throw std::runtime_error("sd_notify(STATUS) failed: " +
                                     std::to_string(-ret));
        }
    }

    bool isWatchdogEnabled() const override {
        uint64_t timeout_us = 0;
        int ret = sd_watchdog_enabled(0, &timeout_us);
        return ret > 0 && timeout_us > 0;
    }

    uint64_t getWatchdogTimeoutUsec() const override {
        uint64_t timeout_us = 0;
        sd_watchdog_enabled(0, &timeout_us);
        return timeout_us;
    }

private:
    mutable std::mutex mutex_;  // 스레드 안전성 보장
};

} // namespace mxrc::systemd

#endif // MXRC_SYSTEMD_SD_NOTIFY_WATCHDOG_H
```

### MockWatchdogNotifier (테스트용 구현)

```cpp
class MockWatchdogNotifier : public IWatchdogNotifier {
public:
    MOCK_METHOD(void, notify, (), (override));
    MOCK_METHOD(void, ready, (), (override));
    MOCK_METHOD(void, stopping, (), (override));
    MOCK_METHOD(void, status, (const std::string&), (override));
    MOCK_METHOD(bool, isWatchdogEnabled, (), (const, override));
    MOCK_METHOD(uint64_t, getWatchdogTimeoutUsec, (), (const, override));
};
```

---

## 사용 예시

### 기본 사용법

```cpp
#include "systemd/impl/SdNotifyWatchdog.h"
#include "systemd/util/WatchdogTimer.h"

int main() {
    auto notifier = std::make_shared<mxrc::systemd::SdNotifyWatchdog>();

    // Watchdog 활성화 여부 확인
    if (!notifier->isWatchdogEnabled()) {
        spdlog::warn("systemd watchdog is not enabled");
        return 1;
    }

    // Watchdog 타이머 시작
    uint64_t timeout_us = notifier->getWatchdogTimeoutUsec();
    auto interval = std::chrono::microseconds(timeout_us / 2);

    mxrc::systemd::WatchdogTimer timer(interval, notifier);
    timer.start();

    // 서비스 초기화
    initializeTaskLayer();
    initializeSequenceLayer();
    initializeActionLayer();

    // 준비 완료 알림
    notifier->ready();
    notifier->status("Service ready");

    // 메인 루프
    while (running) {
        processRequests();
    }

    // 종료 알림
    notifier->stopping();
    timer.stop();

    return 0;
}
```

### 테스트 예시

```cpp
TEST(WatchdogNotifierTest, NotifyCallsSDNotify) {
    auto mock_notifier = std::make_shared<MockWatchdogNotifier>();

    EXPECT_CALL(*mock_notifier, notify())
        .Times(1);

    mock_notifier->notify();
}

TEST(WatchdogNotifierTest, ReadyCallsSDNotify) {
    auto mock_notifier = std::make_shared<MockWatchdogNotifier>();

    EXPECT_CALL(*mock_notifier, ready())
        .Times(1);

    mock_notifier->ready();
}
```

---

## 성능 요구사항

- **notify() 오버헤드**: < 10μs (평균)
- **스레드 안전성**: 동시 호출 지원 (mutex 사용)
- **메모리 사용량**: < 1KB (구현체 포함)

---

## 보안 고려사항

- **권한**: sd_notify()는 특별한 권한 불필요 (Unix socket 사용)
- **Seccomp**: `@basic-io` 세트에 포함된 syscall만 사용
- **격리**: PrivateTmp=yes 환경에서도 동작

---

## 의존성

- **빌드 의존성**: libsystemd-dev
- **런타임 의존성**: systemd (v255+)
- **헤더**: `<systemd/sd-daemon.h>`

---

## 변경 이력

| 버전 | 날짜       | 변경 내용           | 작성자 |
|------|------------|---------------------|--------|
| 1.0  | 2025-01-21 | 초기 인터페이스 정의 | MXRC   |

---

**작성자**: MXRC Development Team
**검토자**: TBD
**승인 날짜**: TBD
