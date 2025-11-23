# Event Module 아키텍처

## 1. 개요

Event 모듈은 MXRC 시스템 내의 다양한 컴포넌트 간 비동기적인 통신을 위한 이벤트 기반 메시징 시스템을 제공합니다. 이는 주로 실시간(RT) 프로세스에서 발생한 이벤트를 비실시간(Non-RT) 프로세스로 효율적으로 전달하는 IPC(Inter-Process Communication) 메커니즘으로 사용됩니다. `Boost.Lockfree`의 SPSC(Single-Producer, Single-Consumer) 큐를 활용하여 고성능의 Lock-Free 통신을 보장합니다.

주요 특징:
-   **비동기 통신**: 컴포넌트 간 직접적인 호출 없이 이벤트를 통해 느슨하게 결합된 통신을 가능하게 합니다.
-   **RT-Safe**: RT 프로세스에서 Lock-Free 큐를 사용하여 이벤트 발행 시 결정성을 보장하고 지터(jitter)를 최소화합니다.
-   **유연한 처리**: Non-RT 프로세스에서는 이벤트의 우선순위, 병합, 스로틀링(throttling) 등을 통해 유연하게 이벤트를 처리할 수 있습니다.

## 2. 아키텍처

Event 모듈은 이벤트를 발행하고, 전달하며, 처리하는 여러 구성 요소로 이루어져 있습니다. RT 프로세스에서는 주로 이벤트 발행에 집중하고, Non-RT 프로세스에서는 이벤트 구독 및 처리에 집중하는 구조를 가집니다.

```mermaid
graph TD
    subgraph "RT Process"
        RT_Component[RT 컴포넌트 (e.g., EtherCAT)] -- Publishes --> EventBus_RT[EventBus (RT 측)]
        EventBus_RT -- Writes to --> LFQ[LockFreeQueue / MPSCLockFreeQueue]
    end

    subgraph "Shared Memory"
        LFQ
    end

    subgraph "Non-RT Process"
        EventBus_NonRT[EventBus (Non-RT 측)] -- Reads from --> LFQ
        EventBus_NonRT -- Passes to --> PriorityQueue[PriorityQueue]
        PriorityQueue -- Dispatches to --> SubscriptionManager[SubscriptionManager]
        SubscriptionManager -- Notifies --> Observer1[로깅 옵저버]
        SubscriptionManager -- Notifies --> Observer2[메트릭 옵저버]

        subgraph "Adapters"
            CoalescingPolicy[CoalescingPolicy]
            ThrottlingPolicy[ThrottlingPolicy]
            DataStoreEventAdapter[DataStoreEventAdapter]
        end
        EventBus_NonRT -- Uses --> Adapters
    end

    EventBus_RT -- Uses --> IEventBus
    EventBus_NonRT -- Uses --> IEventBus

    style EventBus_RT fill:#f9f,stroke:#333,stroke-width:2px
    style EventBus_NonRT fill:#f9f,stroke:#333,stroke-width:2px
    style LFQ fill:#ccf,stroke:#333,stroke-width:2px
```

### 2.1. 핵심 구성 요소

-   **`IEvent` & `IEventBus` 인터페이스**:
    -   `IEvent`: 모든 이벤트가 상속받아야 하는 기본 인터페이스로, 이벤트의 타입, 타임스탬프 등을 포함합니다.
    -   `IEventBus`: 이벤트 발행(`publish`) 및 구독(`subscribe`)을 위한 추상 인터페이스를 제공하여, 실제 `EventBus` 구현체와 분리합니다.

-   **`EventBus`**:
    -   이벤트 발행자로부터 이벤트를 받아 구독자에게 전달하는 중앙 허브 역할을 합니다.
    -   RT 프로세스 측 `EventBus`는 `LockFreeQueue`에 이벤트를 기록하고, Non-RT 프로세스 측 `EventBus`는 `LockFreeQueue`에서 이벤트를 읽어와 구독자에게 디스패치합니다.

-   **`LockFreeQueue` / `MPSCLockFreeQueue`**:
    -   `Boost.Lockfree` 라이브러리를 기반으로 구현된 SPSC(Single-Producer, Single-Consumer) 또는 MPSC(Multi-Producer, Single-Consumer) 큐입니다.
    -   RT 프로세스에서 이벤트 발행 시 락킹 오버헤드 없이 고속으로 데이터를 큐에 삽입할 수 있도록 하여 실시간성을 보장합니다.

-   **`PriorityQueue` / `PrioritizedEvent`**:
    -   Non-RT 프로세스에서 이벤트를 중요도에 따라 처리하기 위한 우선순위 큐입니다.
    -   `PrioritizedEvent`는 이벤트에 우선순위 정보를 추가하여 `PriorityQueue`에서 높은 우선순위의 이벤트가 먼저 처리되도록 합니다.

-   **`SubscriptionManager`**:
    -   어떤 구독자(Observer)가 어떤 타입의 이벤트에 관심이 있는지 관리합니다.
    -   `EventBus`로부터 전달받은 이벤트를 해당 이벤트에 등록된 모든 구독자에게 디스패치하는 역할을 합니다. `weak_ptr` 기반으로 구독자를 관리하여 Dangling 포인터 문제를 방지합니다.

-   **어댑터 (`Adapters`)**:
    -   **`CoalescingPolicy`**: 짧은 시간 내에 동일한 타입의 이벤트가 여러 번 발생했을 때 이를 하나로 병합하여 처리량 과부하를 줄입니다.
    -   **`ThrottlingPolicy`**: 특정 시간 동안 발생한 이벤트 수를 제한하여 시스템의 부하를 조절합니다.
    -   **`DataStoreEventAdapter`**: `DataStore`의 데이터 변경 이벤트를 Event 모듈의 일반 이벤트로 변환하여 발행합니다.

-   **데이터 전송 객체 (`DTOs`)**:
    -   `EventBase`: 모든 이벤트의 기본 클래스.
    -   `EventType`: 이벤트의 종류를 식별하는 열거형.
    -   `ActionEvents`, `DataStoreEvents`, `RTEvents`, `SequenceEvents`, `TaskEvents`: 각 도메인에서 발생하는 특정 이벤트들을 정의합니다.

## 3. 데이터 흐름 예시: EtherCAT 오류 이벤트 처리

1.  **RT 프로세스 (이벤트 발생)**: 로봇의 RT 제어 주기 중 EtherCAT 통신에 문제가 발생하면, `EtherCATMaster` 또는 `RTEtherCATCycle`은 `EtherCATErrorEvent` 객체를 생성하고 RT 프로세스 측 `EventBus`에 `publish`합니다.
2.  **Lock-Free 큐 삽입**: RT 프로세스 측 `EventBus`는 `EtherCATErrorEvent`를 공유 메모리 영역에 위치한 `LockFreeQueue`에 Lock-Free 방식으로 빠르게 삽입합니다. 이 과정은 RT 프로세스의 결정성에 영향을 주지 않습니다.
3.  **Non-RT 프로세스 (이벤트 읽기)**: Non-RT 프로세스 내의 `EventBus` 리스너 스레드는 주기적으로 `LockFreeQueue`에서 이벤트를 가져옵니다.
4.  **우선순위 및 어댑터 처리**: 가져온 `EtherCATErrorEvent`는 `PriorityQueue`에 삽입되어 중요도에 따라 처리 대기합니다. 필요에 따라 `CoalescingPolicy`나 `ThrottlingPolicy`와 같은 어댑터를 거쳐 이벤트 처리량을 조절할 수 있습니다.
5.  **구독자 알림**: `SubscriptionManager`는 `PriorityQueue`에서 이벤트를 꺼내 `EtherCATErrorEvent`에 구독 등록된 모든 옵저버(예: `LogManager`의 로깅 옵저버, `MetricsCollector`의 메트릭 옵저버, UI 업데이트 모듈)에 이벤트를 디스패치합니다.
6.  **이벤트 처리**: `LogManager`는 이 이벤트를 받아 EtherCAT 오류 메시지를 로그 파일에 기록하고, `MetricsCollector`는 EtherCAT 오류 카운트를 증가시킵니다.
이 흐름을 통해 RT 프로세스는 빠르게 다음 제어 주기로 넘어갈 수 있고, Non-RT 프로세스는 시스템 안정성에 영향을 주지 않으면서 발생한 이벤트를 안정적으로 처리할 수 있습니다.

## 4. 개선 제안 반영: IPC 계약의 공식화

`docs/research/007-architecture-analysis-and-improvement-proposal.md`에서 제안된 바와 같이, Event 모듈의 IPC 계약을 공식화하는 것이 중요합니다.
-   **이벤트 스키마 정의**: 모든 이벤트 타입(`ActionEvents`, `DataStoreEvents` 등)과 그 페이로드(payload)를 YAML, JSON Schema 또는 Protobuf와 같은 IDL(Interface Definition Language)을 사용하여 명시적으로 정의합니다.
-   **코드 자동 생성**: 정의된 스키마로부터 이벤트 클래스 및 시리얼라이저/디시리얼라이저 코드를 자동으로 생성하여, 이벤트 정의와 사용 간의 불일치를 컴파일 시점에 감지하고 런타임 오류를 방지합니다.
-   **문서화 및 버전 관리**: 공식 스키마는 어떤 이벤트가 시스템 내에 존재하고 어떻게 사용되는지에 대한 명확한 문서 역할을 하며, 향후 이벤트 구조 변경 시 버전 관리를 용이하게 합니다.
