# Non-RT Module 아키텍처

## 1. 개요

Non-RT(Non-Real-Time) 모듈은 MXRC 시스템의 듀얼-프로세스 아키텍처에서 비실시간 작업을 담당하는 부분입니다. 실시간성(Determinism)이 중요하지 않은 모든 보조 작업을 처리하며, RT 프로세스의 안정성과 예측 가능성에 영향을 주지 않도록 완전히 격리되어 운영됩니다.

주요 역할:
-   **외부 시스템 통신**: 사용자 인터페이스(UI), 외부 API, 진단 도구 등과의 통신을 담당합니다.
-   **시스템 관리**: 설정 파일 로드 및 관리, 상세 로그 기록, 시스템 메트릭 모니터링 및 노출 기능을 제공합니다.
-   **RT 프로세스 관리**: RT 프로세스의 상태를 감시하고, 필요시 복구 절차를 시작하는 등 RT 프로세스의 고가용성(HA)을 지원합니다.
-   **예측 불가능한 작업 처리**: 파일 I/O, 네트워크 통신, 동적 메모리 할당 등 예측 불가능한 지연을 유발할 수 있는 모든 작업을 처리합니다.

## 2. 아키텍처

Non-RT 프로세스는 `NonRTExecutive`를 중심으로 Config, Logging, Monitoring, HA 모듈 등 다양한 비실시간 관련 모듈들이 유기적으로 결합되어 동작합니다. RT 프로세스와는 `DataStore`와 `EventBus`를 통해 안전하게 통신합니다.

```mermaid
graph TD
    subgraph "RT Process"
        RTA[RT Executive]
    end

    subgraph "Inter-Process Communication (IPC)"
        DS[DataStore (Shared Memory)]
        EB[EventBus (Message Queue)]
    end

    subgraph "Non-RT Process"
        NRE[NonRTExecutive]
        NRE -- Manages Lifecycle --> C[Config Module]
        NRE -- Manages Lifecycle --> L[Logging Module]
        NRE -- Manages Lifecycle --> M[Monitoring Module]
        NRE -- Manages Lifecycle --> H[HA Module]
        NRE -- Manages Lifecycle --> UI[User Interface / API]
    end

    RTA <--> DS
    RTA --> EB

    NRE <--> DS
    NRE <-- EB

    UI -- Requests/Commands --> NRE
```

### 2.1. 핵심 구성 요소

-   **`NonRTExecutive`**:
    -   Non-RT 프로세스의 메인 진입점(entry point)이자 총괄 관리자 역할을 합니다.
    -   Non-RT 프로세스 내에서 동작하는 모든 주요 모듈(Config, Logging, Monitoring, HA 등)의 생명주기를 관리하고, 이들 간의 초기화 및 종속성 관계를 조율합니다.
    -   `DataStore` 및 `EventBus`와의 통신을 설정하고 관리합니다.

-   **연결된 주요 모듈**:
    -   **Config 모듈**: 시스템 시작 시 설정 파일을 로드하여 `DataStore`를 초기화합니다.
    -   **Logging 모듈**: `EventBus`를 통해 RT 프로세스로부터 전달받은 로그 메시지를 포함하여, Non-RT 프로세스에서 발생하는 모든 로그를 파일 시스템이나 콘솔에 기록합니다.
    -   **Monitoring 모듈**: `DataStore`에서 시스템 메트릭을 수집하여 외부 Prometheus 서버에 노출하고, Grafana 대시보드와 연동을 지원합니다.
    -   **HA 모듈**: RT 프로세스와 하트비트 통신을 통해 RT 프로세스의 상태를 감시하고, 장애 발생 시 복구 절차를 조정합니다.

### 2.2. IPC 메커니즘

-   **`DataStore`**: 공유 메모리 기반의 `DataStore`를 통해 RT 프로세스와 시스템 상태 및 설정 데이터를 공유합니다. `NonRTExecutive` 및 연관 모듈들은 `DataStore`를 읽어 시스템의 현재 상태를 파악하거나, 설정 데이터를 활용합니다.
-   **`EventBus`**: `EventBus`의 Non-RT 측은 RT 프로세스로부터 발생한 비동기 이벤트를 수신합니다. 주로 RT 프로세스의 로그 메시지, 오류 이벤트 등을 전달받아 Logging 모듈로 전달하거나, 다른 모듈의 동작을 트리거합니다.

## 3. 데이터 흐름 예시: 시스템 시작 및 RT 프로세스 관리

1.  **Non-RT 프로세스 시작**: `nonrt_main.cpp`를 통해 `NonRTExecutive`가 초기화되고 시작됩니다.
2.  **설정 로드**: `NonRTExecutive`는 Config 모듈을 초기화하고, `config/` 디렉토리의 설정 파일들을 로드하여 `DataStore`에 초기값을 기록합니다.
3.  **HA 모듈 시작**: HA 모듈을 시작하여 `ProcessMonitor`가 RT 프로세스로 하트비트 신호를 보내고, RT 프로세스의 `ProcessMonitor`로부터 하트비트를 받기 시작합니다.
4.  **Logging 및 Monitoring 초기화**: Logging 및 Monitoring 모듈이 초기화되고, `EventBus`를 통해 RT 프로세스로부터 로그 이벤트를 수신할 준비를 마칩니다. `MetricsServer`는 외부 Prometheus의 스크랩 요청에 응답할 준비를 합니다.
5.  **RT 프로세스 시작 및 관리**: `NonRTExecutive`는 `systemd` 또는 다른 프로세스 관리 도구를 통해 RT 프로세스(`mxrc-rt`)를 시작하도록 명령합니다. 이후 HA 모듈을 통해 RT 프로세스의 상태를 지속적으로 모니터링합니다.
6.  **사용자 상호작용**: 외부 UI 또는 API로부터 명령(예: "로봇 동작 시작")이 들어오면 `NonRTExecutive`는 이를 해석하여 `DataStore`의 적절한 키에 Task 시작 명령을 기록하고, RT 프로세스의 `TaskExecutor`가 이를 감지하여 동작을 시작하도록 합니다.
이와 같이 Non-RT 모듈은 시스템의 관리, 모니터링, 외부와의 상호작용 등 모든 비실시간 작업을 통합적으로 처리하며 RT 프로세스의 안정적인 운영을 지원합니다.
