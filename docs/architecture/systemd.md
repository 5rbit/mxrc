# systemd Module 아키텍처

## 1. 개요

systemd 모듈은 MXRC 애플리케이션과 Linux의 systemd 초기화 시스템 및 서비스 관리자 간의 통합을 담당합니다. 이를 통해 MXRC는 systemd의 강력한 프로세스 관리, 상태 모니터링, 리소스 제어, 그리고 중앙 집중식 로깅(journald) 기능을 활용하여 안정적이고 효율적인 운영 환경을 구축합니다.

주요 특징:
-   **프로세스 관리**: systemd 서비스 유닛을 통해 MXRC의 RT 및 Non-RT 프로세스를 안정적으로 시작, 중지, 재시작합니다.
-   **Watchdog 기능**: systemd의 Watchdog 타이머를 활용하여 MXRC 프로세스의 활성 상태를 모니터링하고, 응답 없는 프로세스를 자동으로 재시작하여 고가용성을 높입니다.
-   **중앙 집중식 로깅**: `systemd-journald`를 통해 MXRC의 모든 로그를 수집하고 관리하여, 로그 분석 및 문제 해결을 용이하게 합니다.
-   **리소스 제어**: systemd의 cgroup 기능을 활용하여 MXRC 프로세스의 CPU, 메모리 등의 자원 사용을 세밀하게 제어합니다.

## 2. 아키텍처

systemd 모듈은 MXRC 애플리케이션 내의 컴포넌트와 systemd 서비스 관리자 간의 상호작용을 추상화하고 제어합니다.

```mermaid
graph TD
    subgraph "systemd"
        SM[systemd Service Manager]
        J[systemd-journald]
        CG[cgroup Resource Controller]
    end

    subgraph "MXRC System (RT & Non-RT Processes)"
        MXRC_Proc[MXRC 프로세스]
        MXRC_Proc -- Uses --> SWN[SdNotifyWatchdog]
        MXRC_Proc -- Uses --> SWT[WatchdogTimer]
        MXRC_Proc -- Uses --> SU[SystemdUtil]
        MXRC_Proc -- Generates Log/Metric --> JDTO[JournaldEntry / SystemdMetric]
    end

    SM -- Manages & Monitors --> MXRC_Proc
    MXRC_Proc -- Signals ALIVE --> SM (via sd_notify)
    MXRC_Proc -- Sends Logs --> J
    SM -- Applies Resource Limits --> CG
    MXRC_Proc -- Queries --> SU
```

### 2.1. 핵심 구성 요소

-   **`IWatchdogNotifier` 인터페이스**:
    -   systemd Watchdog에 "I'm alive" 신호를 보내는 기능을 추상화한 인터페이스입니다. `SdNotifyWatchdog`가 이 인터페이스를 구현합니다.

-   **`SdNotifyWatchdog`**:
    -   `libsystemd-dev` 라이브러리의 `sd_notify()` 함수를 사용하여 systemd Watchdog에 주기적으로 활성 신호(keep-alive heartbeat)를 보냅니다.
    -   이 신호는 MXRC 프로세스가 정상적으로 작동하고 있음을 systemd에 알리며, 설정된 타임아웃 내에 신호가 수신되지 않으면 systemd는 해당 프로세스를 재시작하는 등의 조치를 취합니다.

-   **`WatchdogTimer`**:
    -   MXRC 애플리케이션 내부에서 Watchdog 타이머의 활성화 여부 및 타임아웃 값을 관리합니다.
    -   `SdNotifyWatchdog`가 `WatchdogTimer`의 설정에 따라 `sd_notify()`를 호출하도록 조율합니다.

-   **`SystemdUtil`**:
    -   systemd와 관련된 다양한 유틸리티 함수를 제공합니다. 예를 들어, 현재 프로세스가 systemd에 의해 시작되었는지 확인하거나, systemd가 제공하는 환경 변수(예: `NOTIFY_SOCKET`)를 읽는 등의 기능을 포함합니다.
    -   `SystemdException`을 통해 systemd 관련 오류를 처리합니다.

-   **데이터 전송 객체 (DTO)**:
    -   **`JournaldEntry`**: `systemd-journald`에 기록될 로그 엔트리의 구조를 정의합니다. 구조화된 로그 메시지를 `journald`에 보내는 데 사용될 수 있습니다.
    -   **`SystemdMetric`**: systemd를 통해 수집되거나 노출될 시스템 메트릭의 구조를 정의합니다.

## 3. 데이터 흐름 예시: systemd Watchdog을 통한 프로세스 모니터링

1.  **systemd 서비스 시작**: systemd는 `mxrc-rt.service`와 `mxrc-nonrt.service` 파일을 읽어 MXRC의 RT 및 Non-RT 프로세스를 시작합니다. 이때 서비스 파일에는 `WatchdogSec` 옵션을 통해 Watchdog 타이머가 설정되어 있습니다.
2.  **MXRC 프로세스 초기화**: MXRC 프로세스 내부의 `SdNotifyWatchdog`는 `SystemdUtil`을 통해 systemd 환경을 확인하고 Watchdog 타이머가 활성화되었음을 감지합니다.
3.  **주기적인 활성 신호**: MXRC 프로세스(특히 RT 프로세스)는 주기적인 제어 루프 내에서 `SdNotifyWatchdog`를 통해 `sd_notify("WATCHDOG=1")` 신호를 systemd에 보냅니다. 이는 프로세스가 "살아있음"을 알리는 Keep-alive 신호입니다.
4.  **systemd 모니터링**: systemd 서비스 매니저는 이 신호를 모니터링하며, 설정된 `WatchdogSec` 시간 동안 MXRC 프로세스로부터 신호를 받지 못하면 해당 프로세스가 응답 없다고 판단합니다.
5.  **프로세스 재시작**: systemd는 응답 없는 MXRC 프로세스를 자동으로 종료하고, 서비스 파일에 정의된 `Restart` 정책(예: `on-failure`, `always`)에 따라 프로세스를 재시작합니다.
6.  **Journald 로깅**: MXRC 프로세스에서 발생하는 모든 `spdlog` 기반의 로그 메시지들은 `systemd-journald`에 의해 자동으로 수집되어 중앙 집중식으로 관리됩니다. 이를 통해 `journalctl` 명령어를 사용하여 MXRC의 로그를 쉽게 조회하고 필터링할 수 있습니다.
이러한 systemd와의 통합을 통해 MXRC 시스템은 OS 레벨에서의 강력한 프로세스 안정성, 리소스 제어, 그리고 통합 로깅 기능을 확보하게 됩니다.
