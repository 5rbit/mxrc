# MXRC 통합 시스템 아키텍처

## 1. 개요

본 문서는 MXRC(Mission and eXecution Robot Controller) 시스템을 구성하는 모든 모듈의 통합적인 아키텍처와 상호작용을 설명합니다. MXRC는 고성능, 고신뢰성 로봇 제어를 위해 **듀얼-프로세스 아키텍처(Dual-Process Architecture)**를 핵심 설계로 채택하여, 실시간(Real-Time) 제어의 예측성과 안정성을 보장하면서 비실시간(Non-Real-Time) 작업의 유연성을 확보합니다.

## 2. 핵심 설계 원칙

-   **RT/Non-RT 분리**: 시간 제약이 엄격한(Time-critical) 작업과 그렇지 않은 작업을 별도의 프로세스(`mxrc-rt`, `mxrc-nonrt`)로 분리하여 상호 간섭을 원천적으로 차단하고 시스템 전체의 안정성을 확보합니다.
-   **계층형 실행 구조**: RT 프로세스 내에 Task → Sequence → Action의 3단계 계층 구조를 적용하여, 명확한 책임 분리와 코드의 모듈성 및 재사용성을 극대화합니다.
-   **데이터 중심 통신 (IPC)**: 두 프로세스 간 통신은 공유 메모리 기반의 `DataStore`와 비동기 메시지 큐인 `EventBus`를 통해 이루어지며, 이는 모듈 간의 결합도를 낮추고 성능을 최적화합니다.
-   **RAII 및 스마트 포인터**: 모든 리소스는 스마트 포인터(`shared_ptr`, `unique_ptr`, `weak_ptr`)로 관리하여 메모리 누수와 Dangling 포인터를 방지합니다.
-   **스레드 안전성 및 동시성**: `tbb::concurrent_hash_map`, `Boost.Lockfree`, `std::atomic` 등을 적극적으로 사용하여 멀티스레드 및 멀티프로세스 환경에서 데이터 무결성과 고성능을 보장합니다.
-   **테스트 용이성**: 모든 모듈은 의존성 주입(DI)과 Mocking이 용이하도록 인터페이스 기반으로 설계하여, 단위 및 통합 테스트의 격리와 신뢰성을 높입니다.

## 3. 통합 아키텍처 다이어그램

```mermaid
graph TD
    subgraph "External Systems"
        UI[User Interface / API]
        Prometheus[Prometheus / Grafana]
        Journal[systemd-journald]
        Systemd[systemd Service Manager]
    end

    subgraph "MXRC System"
        direction LR
        subgraph "Non-RT Process (mxrc-nonrt)"
            NRE[NonRTExecutive]
            NRE -- Manages --> Config[Config Module]
            NRE -- Manages --> Logging[Logging Module]
            NRE -- Manages --> Monitoring[Monitoring Module]
            NRE -- Manages --> NHA[HA Agent (Non-RT)]
            NRE -- Handles --> API_Handler[API Handler]
            
            Logging -- Writes to --> LogFile[File/Console Log]
            Logging -- Also sends to --> Journal
            Monitoring -- Exposes Metrics --> Prometheus
            API_Handler -- Receives Commands from --> UI
        end

        subgraph "Inter-Process Communication (IPC)"
            DS[DataStore (Shared Memory)]
            EB[EventBus (Lock-Free Queue)]
        end

        subgraph "RT Process (mxrc-rt)"
            RTE[RTExecutive]
            RTE -- Manages --> RTSM[RTStateMachine]
            RTE -- Drives --> Task[Task Layer]
            Task -- Uses --> Sequence[Sequence Layer]
            Sequence -- Uses --> Action[Action Layer]
            Action -- Controls --> EC[EtherCAT Module]
            RTE -- Manages --> RHA[HA Agent (RT)]
            RTE -- Gathers --> RTM[RTMetrics]
        end
    end
    
    %% System Management
    Systemd -- Manages/Monitors --> NRE
    Systemd -- Manages/Monitors --> RTE
    NRE -- Notifies Alive --> Systemd
    RTE -- Notifies Alive --> Systemd

    %% Data Flow
    API_Handler -- Writes Task --> DS
    Task -- Reads Task --> DS
    
    RTM -- Writes Metrics --> DS
    EC -- Writes Sensor Data --> DS
    Action -- Reads Commands from --> DS
    Monitoring -- Reads Metrics/State --> DS
    
    RTE -- Publishes RT Events --> EB
    NHA -- Publishes HA Events --> EB
    Logging -- Subscribes to --> EB

    %% Cross-Process Communication
    NRE <--> DS
    RTE <--> DS
    RTE --> EB
    NRE <-- EB
    
    NHA <--> RHA

    style DS fill:#ccf,stroke:#333,stroke-width:2px
    style EB fill:#f9f,stroke:#333,stroke-width:2px
```

## 4. 주요 컴포넌트 및 상호작용

-   **RT 프로세스 (`mxrc-rt`)**:
    -   `RTExecutive`를 중심으로 정해진 주기(예: 1ms)마다 핵심 제어 루프를 실행합니다.
    -   `Task` → `Sequence` → `Action` 계층을 통해 로봇의 동작을 수행하며, `EtherCAT` 모듈을 통해 하드웨어와 실시간으로 통신합니다.
    -   `RTMetrics`를 통해 제어 주기 지터, CPU 부하 등 실시간 성능 지표를 수집하여 `DataStore`에 기록합니다.
    -   `HA Agent`를 통해 Non-RT 프로세스의 상태를 감시하고, 자신의 활성 상태를 알립니다.
    -   `EventBus`를 통해 로그, 오류 등 비동기 이벤트를 Non-RT 프로세스로 전달합니다.

-   **Non-RT 프로세스 (`mxrc-nonrt`)**:
    -   `NonRTExecutive`가 전체 생명주기를 관리하며, 외부와의 통신 및 시스템 관리 작업을 총괄합니다.
    -   `Config` 모듈이 시스템 시작 시 모든 설정을 `DataStore`에 로드합니다.
    -   `API Handler`가 외부 UI나 API로부터 명령을 수신하고, 이를 해석하여 `DataStore`에 Task 명령을 기록합니다.
    -   `Logging` 모듈이 `EventBus`를 통해 전달받은 모든 이벤트를 포함하여 시스템 로그를 파일 또는 `journald`에 기록합니다.
    -   `Monitoring` 모듈이 `DataStore`의 상태 정보를 주기적으로 읽어와 Prometheus에 메트릭으로 노출합니다.
    -   `HA Agent`가 RT 프로세스의 상태를 감시하고, 장애 발생 시 복구 절차를 시작합니다.

-   **프로세스 간 통신 (IPC)**:
    -   **`DataStore`**: RT 프로세스와 Non-RT 프로세스가 시스템의 현재 상태, 설정, Task 명령, 메트릭 등을 공유하는 핵심적인 공유 메모리 저장소입니다.
    -   **`EventBus`**: `Boost.Lockfree` 큐를 사용하여 RT 프로세스에서 Non-RT 프로세스로 이벤트(로그, 오류 등)를 비동기적으로, Lock-Free 방식으로 전달하여 RT 프로세스의 결정성을 보장합니다.

-   **시스템 관리 및 모니터링**:
    -   **`systemd`**: `mxrc-rt`와 `mxrc-nonrt` 프로세스를 서비스로 등록하여 생명주기를 관리하고, Watchdog 기능을 통해 프로세스의 응답 없음을 감지하여 자동으로 재시작합니다.
    -   **`Prometheus` & `Grafana`**: `Monitoring` 모듈이 노출하는 메트릭을 수집하고 시각화하여, 운영자가 시스템의 상태를 실시간으로 파악할 수 있도록 합니다.

## 5. OSI 데이터 흐름 예시: 외부 API를 통한 '정밀 용접' Task 실행

1.  **명령 수신 (Non-RT)**: 외부 API 서버가 '정밀 용접' Task 실행 요청을 `NonRTExecutive`의 `API Handler`로 전송합니다.
2.  **Task 생성 (Non-RT)**: `API Handler`는 요청을 파싱하여 '정밀 용접' `TaskDefinition`에 따라 `DataStore`에 새로운 Task 실행 명령을 기록합니다.
3.  **Task 감지 및 시작 (RT)**: RT 프로세스의 `TaskExecutor`는 주기적으로 `DataStore`를 확인하여 새로운 Task 명령을 감지하고, `RTStateMachine`을 `OPERATIONAL` 상태로 유지하며 Task 실행을 시작합니다.
4.  **시퀀스 및 Action 실행 (RT)**: `TaskExecutor`는 '정밀 용접'에 해당하는 `Sequence`를 `SequenceEngine`에 제출합니다. `SequenceEngine`은 "용접 지점으로 이동" → "용접 시작" → "경로 따라 용접" → "용접 종료" 등의 `Action`들을 순차적으로 실행합니다.
5.  **하드웨어 제어 (RT)**: "경로 따라 용접" `Action`은 `DataStore`에서 실시간 목표 경로를 읽어와, `EtherCAT` 모듈을 통해 로봇 팔의 모터에 정밀한 위치/속도 명령을 전달합니다. 동시에 `EtherCAT` 모듈은 모터의 현재 상태(엔코더 값, 토크 등)를 읽어 `DataStore`에 업데이트합니다.
6.  **성능 추적 (RT/Tracing)**: `RTCycleTracer`는 용접이 진행되는 동안 각 제어 주기의 실행 시간과 주요 함수 호출 지연을 스팬(Span)으로 기록하여, 추후 성능 분석이 가능하도록 합니다.
7.  **메트릭 기록 (RT/Monitoring)**: `RTMetrics`는 용접 중인 모터의 토크, 온도 등의 값을 `DataStore`에 주기적으로 기록합니다.
8.  **이벤트 발생 및 전파 (IPC)**: '정밀 용접' 시퀀스가 성공적으로 완료되면, `TaskExecutor`는 `TaskCompleted` 이벤트를 `EventBus`를 통해 Non-RT 프로세스로 발행합니다.
9.  **로그 기록 (Non-RT/Logging)**: `Logging` 모듈은 `EventBus`로부터 `TaskCompleted` 이벤트를 수신하고, "Task '정밀 용접' completed successfully"와 같은 메시지를 로그 파일에 기록합니다.
10. **메트릭 노출 (Non-RT/Monitoring)**: `Monitoring` 모듈은 `DataStore`에서 모터의 토크, 온도 등의 메트릭을 읽어와 Prometheus에 노출합니다. Grafana 대시보드에서는 용접 과정 동안의 토크 및 온도 변화를 실시간 그래프로 확인할 수 있습니다.
이처럼 MXRC 시스템은 각 모듈이 명확한 책임을 가지고 유기적으로 상호작용하여, 복잡한 로봇 임무를 안정적이고 효율적으로 수행합니다.
