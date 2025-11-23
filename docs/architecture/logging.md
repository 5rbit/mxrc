# Logging Module 아키텍처

## 1. 개요

Logging 모듈은 MXRC 시스템에서 발생하는 다양한 이벤트, 상태 변화, 오류 등을 효율적이고 신뢰성 있게 기록하는 역할을 담당합니다. `spdlog` 라이브러리를 기반으로 고성능의 비동기 로깅을 지원하며, 특히 실시간(RT) 프로세스로부터 `EventBus`를 통해 전달받은 로그 메시지를 비실시간(Non-RT) 프로세스에서 파일 시스템이나 콘솔에 안전하게 기록합니다. 또한, 시스템의 주요 데이터를 기록하고 재생할 수 있는 "Bag Logging" 기능을 제공하여 디버깅 및 분석에 활용됩니다.

주요 특징:
-   **고성능 비동기 로깅**: RT 프로세스의 성능에 영향을 주지 않으면서 로그를 기록합니다.
-   **Bag Logging**: 로봇 시스템의 중요 데이터를 시간에 따라 기록하고, 필요할 때 재생하여 시스템의 과거 상태를 분석할 수 있도록 합니다.
-   **견고성**: 시스템 크래시와 같은 비정상적인 상황에서도 로그를 최대한 보존합니다.

## 2. 아키텍처

Logging 모듈은 크게 실시간 로그 메시지 처리와 Bag Logging 기능으로 나눌 수 있습니다.

```mermaid
graph TD
    subgraph "RT Process"
        RT_Component[RT 컴포넌트] -- Log Message --> EventBus_RT[EventBus (RT 측)]
        RT_Component -- Data Change --> DataStore[DataStore]
    end

    subgraph "Shared Memory"
        EventBus_RT -- Transmits Log Event --> LockFreeQueue[LockFreeQueue]
    end

    subgraph "Non-RT Process"
        EventBus_NonRT[EventBus (Non-RT 측)] -- Receives Log Event --> AsyncWriter[AsyncWriter]
        AsyncWriter -- Writes --> Spdlog[spdlog]
        Spdlog -- Writes --> LogFile[로그 파일]
        
        DataStore -- Triggers --> DSBL[DataStoreBagLogger]
        DSBL -- Writes --> BagWriter[IBagWriter / SimpleBagWriter]
        BagWriter -- Writes --> BagFile[Bag 파일]

        subgraph "Bag Analysis Tools"
            BR[BagReader] -- Reads --> BagFile
            BReplayer[BagReplayer] -- Replays --> BagFile
            Util[FileUtils, Indexer, Serializer, RetentionManager]
        end
    end

    SignalHandler[SignalHandler] -- Catches --> Crash[시스템 크래시]
    Crash -- Logs --> Spdlog

    style EventBus_RT fill:#f9f,stroke:#333,stroke-width:2px
    style EventBus_NonRT fill:#f9f,stroke:#333,stroke-width:2px
    style LockFreeQueue fill:#ccf,stroke:#333,stroke-width:2px
```

### 2.1. 핵심 구성 요소

-   **`Log.h`**: 애플리케이션 전반에서 사용할 로깅 매크로 또는 인터페이스를 정의합니다. `spdlog`의 기능을 추상화하여 사용합니다.
-   **`AsyncWriter`**: Non-RT 프로세스에서 `EventBus`를 통해 전달받은 로그 메시지를 비동기적으로 처리하는 역할을 합니다. 별도의 스레드에서 `spdlog`로 로그를 기록함으로써, `EventBus` 메시지 처리 스레드의 블로킹을 방지하고 Non-RT 프로세스의 반응성을 유지합니다.
-   **`spdlog`**: 실제 로그 메시지를 포맷하고, 콘솔, 파일 등 다양한 싱크(sink)로 출력하는 고성능 로깅 라이브러리입니다.
-   **`SignalHandler`**: 시스템 크래시(`SIGSEGV`, `SIGABRT` 등)와 같은 치명적인 신호를 포착하여, 프로그램이 종료되기 전에 중요한 로그 정보를 기록하거나 스택 트레이스를 출력하는 등 안정적인 종료 절차를 수행합니다.
-   **`IBagWriter` & `SimpleBagWriter`**:
    -   `IBagWriter`: Bag 파일에 데이터를 기록하기 위한 추상 인터페이스입니다.
    -   `SimpleBagWriter`: `IBagWriter`의 기본 구현체로, 지정된 형식에 따라 Bag 파일에 데이터를 효율적으로 저장합니다.
-   **`DataStoreBagLogger`**: `DataStore`의 특정 키 값 변경 이력을 모니터링하고, 변경이 발생할 때마다 해당 데이터를 Bag 파일에 기록하는 역할을 합니다. 이를 통해 `DataStore`의 시간에 따른 변화를 정밀하게 분석할 수 있습니다.
-   **`BagReader` & `BagReplayer`**:
    -   `BagReader`: 기록된 Bag 파일을 읽고, 저장된 데이터를 추출하는 기능을 제공합니다.
    -   `BagReplayer`: `BagReader`를 활용하여 Bag 파일에 기록된 데이터를 시간 순서에 맞춰 재생하며, 시스템이 과거의 특정 시점으로 돌아간 것처럼 동작을 시뮬레이션할 수 있도록 합니다.
-   **유틸리티 (`FileUtils`, `Indexer`, `RetentionManager`, `Serializer`)**:
    -   `FileUtils`: 파일 시스템 관련 작업을 처리합니다 (파일 생성, 삭제, 이동 등).
    -   `Indexer`: Bag 파일 내 데이터의 효율적인 검색을 위한 인덱스를 생성하고 관리합니다.
    -   `RetentionManager`: 오래된 Bag 파일을 자동으로 삭제하거나 보관 정책에 따라 관리합니다.
    -   `Serializer`: 다양한 데이터 타입을 Bag 파일에 기록하기 위한 직렬화/역직렬화 기능을 제공합니다.

## 3. 데이터 흐름 예시: RT 프로세스 로그 및 DataStore 변경 기록

1.  **RT 프로세스 로그 발생**: RT 제어 주기 중 중요 이벤트(예: 모터 오류)가 발생하면 `RT_Component`는 `Log.h`를 통해 로그 메시지를 발행합니다.
2.  **EventBus 통한 전달**: 발행된 로그 메시지는 `EventBus`의 RT 측을 통해 공유 메모리의 `LockFreeQueue`에 Lock-Free 방식으로 기록됩니다. 이 과정은 RT 프로세스에 지연을 유발하지 않습니다.
3.  **Non-RT 프로세스 로그 처리**: Non-RT 프로세스의 `EventBus` 리스너 스레드는 `LockFreeQueue`에서 로그 메시지를 읽어 `AsyncWriter`에게 전달합니다. `AsyncWriter`는 이 메시지를 별도의 스레드에서 `spdlog`를 통해 로그 파일에 기록합니다.
4.  **DataStore 변경 기록**: RT 프로세스에서 `DataStore`의 로봇 위치 값이 변경되면, `DataStoreBagLogger`는 이 변경을 감지하고 `IBagWriter`를 통해 해당 변경 데이터를 Bag 파일에 기록합니다. 이 데이터는 나중에 `BagReplayer`를 통해 정확한 시간 순서대로 재생될 수 있습니다.
5.  **크래시 시 로그 보존**: 만약 시스템이 크래시되면 `SignalHandler`가 이를 감지하고, `spdlog`를 사용하여 크래시 시점의 중요한 진단 정보(예: 스택 트레이스)를 최대한 로그 파일에 기록하여 사후 분석을 돕습니다.

## 4. 개선 제안 반영: Bag 데이터 분석 도구 개발

`docs/research/007-architecture-analysis-and-improvement-proposal.md`에서 제안된 바와 같이, Logging 모듈의 핵심 기능 중 하나인 Bag 데이터의 활용성을 극대화하기 위해 전용 분석 도구 개발을 권장합니다.
-   **Bag 파일 시각화 도구**: 기록된 Bag 파일의 데이터를 그래프, 테이블 등으로 시각화하여 사용자가 시스템의 동작을 직관적으로 이해할 수 있도록 돕습니다.
-   **데이터 필터링 및 검색**: 특정 시간 범위, 특정 데이터 타입 또는 특정 값 조건을 만족하는 데이터를 Bag 파일에서 효율적으로 필터링하고 검색하는 기능을 제공합니다.
-   **회귀 테스트 케이스 생성**: 과거에 기록된 Bag 데이터를 기반으로 시스템의 동작을 재현하고, 이를 자동으로 실행 가능한 회귀 테스트 케이스로 변환하는 유틸리티를 개발합니다. 이는 버그 재현 및 수정 후 검증 과정의 효율을 크게 높일 것입니다.
이러한 도구들은 Logging 모듈의 가치를 증대시키고, MXRC 시스템의 개발, 테스트 및 유지보수 프로세스를 한층 더 강화할 것입니다.
