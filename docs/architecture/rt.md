# RT (Real-Time) Module 아키텍처

## 1. 개요

RT(Real-Time) 모듈은 MXRC 시스템의 듀얼-프로세스 아키텍처에서 가장 핵심적인 실시간 작업을 담당합니다. 결정성(Determinism)과 시간 정확성(Temporal Accuracy)이 최우선적으로 요구되는 로봇 모션 제어, 센서 데이터 처리, EtherCAT 통신, 안전 로직 실행 등의 작업을 수행합니다. 예측 불가능한 지연(latency)을 유발할 수 있는 파일 I/O, 동적 메모리 할당, 네트워크 통신 등은 엄격히 제한하거나 금지됩니다.

주요 역할:
-   **핵심 제어 로직**: 로봇의 동작을 정밀하게 제어하는 알고리즘을 실행합니다.
-   **센서/액추에이터 인터페이스**: EtherCAT과 같은 실시간 통신을 통해 센서 데이터를 수집하고 액추에이터에 명령을 전달합니다.
-   **안전 보장**: 시스템의 안전 관련 기능을 최우선으로 처리합니다.
-   **고정밀 타이밍**: 주기적인 제어 루프를 정확한 시간 간격으로 실행하여 결정성을 유지합니다.

## 2. 아키텍처

RT 프로세스는 `RTExecutive`를 중심으로, 정해진 제어 주기 내에서 Task, Sequence, Action Layer를 실행하고, EtherCAT 통신을 처리하며, 실시간 성능을 최적화하고 모니터링하는 다양한 컴포넌트들로 구성됩니다.

```mermaid
graph TD
    subgraph "Non-RT Process"
        NRE[NonRTExecutive]
    end

    subgraph "Inter-Process Communication (IPC)"
        DS[DataStore (Shared Memory)]
        EB[EventBus (Message Queue)]
    end

    subgraph "RT Process"
        RTEX[RTExecutive]
        RTEX -- Drives --> T[Task Layer]
        T -- Uses --> S[Sequence Layer]
        S -- Uses --> A[Action Layer]
        
        RTEX -- Manages --> RTSM[RTStateMachine]
        RTEX -- Gathers Metrics --> RTM[RTMetrics]
        RTEX -- Communicates --> EC[EtherCAT Module]
        RTEX -- Optimizes --> CPUA[CPUAffinityManager]
        RTEX -- Optimizes --> NUMA[NUMABinding]
        RTEX -- Monitors --> PERF[PerfMonitor]
        
        DS -- Accessed by --> RTDS[RTDataStore]
        EB -- Writes Log/Event --> EventBus_RT[EventBus (RT 측)]
    end

    NRE <--> DS
    NRE <-- EB

    RTEX <--> RTDS
    RTEX <--> EventBus_RT
    EC <--> RTDS
    T <--> RTDS
    S <--> RTDS
    A <--> RTDS
```

### 2.1. 핵심 구성 요소

-   **`RTExecutive`**:
    -   RT 프로세스의 메인 루프를 구현하고 전체적인 제어 주기를 조율합니다.
    -   정확한 타이밍으로 Task Layer, EtherCAT 통신, 안전 점검 등의 작업을 실행하도록 스케줄링합니다.
    -   `RTStateMachine`의 상태 전이를 관리하고, `RTMetrics`를 통해 성능 지표를 수집합니다.

-   **`RTStateMachine`**:
    -   RT 프로세스의 현재 상태(예: `INIT`, `OPERATIONAL`, `SAFE_MODE`, `ERROR`)를 관리하고 상태 전이 로직을 정의합니다.
    -   예측 불가능한 이벤트나 오류 발생 시 시스템을 안전한 상태로 전환하는 데 중요한 역할을 합니다.

-   **`RTDataStore` & `RTDataStoreShared`**:
    -   `RTDataStore`: `DataStore`의 RT-safe 인터페이스를 제공하여, RT 프로세스 내에서 공유 메모리 데이터를 안전하고 효율적으로 접근할 수 있도록 합니다. 락킹을 최소화하거나 Lock-Free 방식을 통해 결정성을 유지합니다.
    -   `RTDataStoreShared`: 공유 메모리 자체의 관리 및 접근을 위한 저수준 구현체를 담당합니다.

-   **`RTMetrics`**:
    -   RT 프로세스 내에서 발생하는 고정밀 성능 지표(예: 제어 주기 지터, CPU 부하, EtherCAT 통신 지연)를 수집합니다.
    -   수집된 메트릭은 `DataStore`에 기록되어 Non-RT 프로세스의 Monitoring 모듈로 전달됩니다.

-   **성능 최적화 및 모니터링 (`perf` 서브모듈)**:
    -   **`CPUAffinityManager`**: RT 스레드를 특정 CPU 코어에 고정(pinning)하여 캐시 미스(cache miss)를 줄이고 컨텍스트 스위칭 오버헤드를 최소화합니다.
    -   **`NUMABinding`**: NUMA(Non-Uniform Memory Access) 아키텍처에서 RT 프로세스가 접근하는 메모리를 물리적으로 가까운 노드에 할당하여 메모리 접근 지연 시간을 줄입니다.
    -   **`PerfMonitor`**: RT 프로세스의 실제 실행 시간, 지터 등을 측정하고 기록하여 성능 분석을 돕습니다.

-   **IPC (`ipc` 서브모듈)**:
    -   **`SharedMemory`**: Non-RT 프로세스와의 `DataStore` 기반 IPC를 위한 공유 메모리 할당 및 관리 기능을 제공합니다.

## 3. 데이터 흐름 예시: 주기적 로봇 제어

1.  **RT 프로세스 시작**: Non-RT 프로세스에 의해 RT 프로세스(`mxrc-rt`)가 시작되면, `RTExecutive`가 초기화됩니다.
2.  **시스템 초기화**: `RTExecutive`는 `RTStateMachine`을 `INIT` 상태로 설정하고, `CPUAffinityManager`, `NUMABinding` 등을 사용하여 시스템 자원을 최적화합니다. `EtherCAT` 모듈을 초기화하고 통신을 시작합니다.
3.  **제어 주기 시작**: `RTExecutive`는 정해진 주기(예: 1ms)마다 제어 루프를 시작합니다.
4.  **센서 데이터 수집**: `EtherCAT` 모듈은 센서로부터 최신 데이터를 읽어 `RTDataStore`에 기록합니다.
5.  **Task/Sequence/Action 실행**: `RTExecutive`는 `RTDataStore`에서 Task 명령을 읽어 `Task Layer`에 전달하고, `Task Layer`는 `Sequence Layer`를 통해 정의된 `Action`들을 실행합니다. 이 과정에서 `RTDataStore`의 센서 데이터를 활용하여 로봇의 움직임을 계산하고 모터 명령을 생성합니다.
6.  **모터 명령 전송**: 생성된 모터 명령은 `RTDataStore`에 기록되고, `EtherCAT` 모듈은 이 명령을 읽어 액추에이터로 전송합니다.
7.  **상태 및 메트릭 업데이트**: `RTExecutive`는 제어 주기 완료 후 `RTStateMachine`의 상태를 `OPERATIONAL`로 유지하고, `RTMetrics`를 통해 현재 주기의 실행 시간, 지터 등의 성능 지표를 수집하여 `RTDataStore`에 업데이트합니다.
8.  **이벤트 발생**: 만약 제어 주기 중 심각한 오류가 발생하면, `RTExecutive`는 `RTStateMachine`을 `SAFE_MODE` 또는 `ERROR` 상태로 전환하고, `EventBus`의 RT 측을 통해 오류 이벤트를 Non-RT 프로세스로 전달합니다.
이 과정을 통해 RT 모듈은 로봇의 핵심 기능을 결정성 있게 수행하며, 시스템의 안정성과 성능을 극대화합니다.
