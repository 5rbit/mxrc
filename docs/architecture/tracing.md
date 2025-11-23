# Tracing Module 아키텍처

## 1. 개요

Tracing 모듈은 MXRC 시스템의 런타임 동작을 심층적으로 분석하기 위한 저수준 진단 도구입니다. 시스템의 성능 병목 현상, 지연 시간 분석, 이벤트 전달 경로 추적 등을 통해 시스템 최적화 및 디버깅을 지원합니다. 이 모듈은 Real-Time(RT) 및 Non-Real-Time(Non-RT) 프로세스 모두에서 작동하여 시스템 전반에 걸친 통합적인 가시성을 제공합니다.

주요 특징:
-   **성능 분석**: 제어 주기 지연, 함수 호출 시간 등을 정밀하게 측정하여 성능 병목 지점을 식별합니다.
-   **이벤트 추적**: 시스템 내에서 발생하는 이벤트의 흐름과 전파 경로를 추적하여 복잡한 상호작용을 이해하는 데 도움을 줍니다.
-   **분산 트레이싱 지원**: OpenTelemetry와 같은 표준을 사용하여 분산 시스템 환경에서의 트레이싱을 지원할 수 있는 기반을 제공합니다.
-   **디버깅 효율성 증대**: 오류 발생 시 관련 이벤트 및 실행 흐름을 재구성하여 근본 원인을 빠르게 파악할 수 있도록 합니다.

## 2. 아키텍처

Tracing 모듈은 트레이스(Trace)를 생성, 전파, 기록하는 핵심 컴포넌트들로 구성됩니다. RT 프로세스에서는 최소한의 오버헤드로 데이터를 수집하고, Non-RT 프로세스에서는 이를 집계하고 외부에 노출하는 방식으로 동작합니다.

```mermaid
graph TD
    subgraph "RT Process"
        RT_COMP[RT 컴포넌트]
        RTC[RTCycleTracer] -- Instruments --> RT_COMP
        RTC -- Emits Spans --> TP[TracerProvider]
    end

    subgraph "Non-RT Process"
        NONRT_COMP[Non-RT 컴포넌트]
        EBT[EventBusTracer] -- Instruments --> NONRT_COMP
        EBT -- Emits Spans --> TP
    end

    subgraph "Tracing Module"
        TP[TracerProvider] -- Manages --> SC[SpanContext]
        TP -- Exports --> Exporter[Tracing Exporter (e.g., Jaeger)]
    end

    subgraph "External Tracing System"
        JAEGER[Jaeger UI] -- Visualizes --> Exporter
    end

    RT_COMP -- Sends/Receives Events --> EventBus[EventBus]
    NONRT_COMP -- Sends/Receives Events --> EventBus
    EventBus -- Traced by --> EBT
```

### 2.1. 핵심 구성 요소

-   **`TracerProvider`**:
    -   트레이싱 시스템의 중앙 허브 역할을 합니다. `Tracer` 인스턴스를 생성하고 관리하며, 생성된 스팬(span) 데이터를 외부 Exporter로 전송하는 역할을 담당합니다.
    -   일반적으로 Singleton 패턴으로 구현되어 애플리케이션 전체에서 단일 인스턴스로 접근됩니다.

-   **`SpanContext`**:
    -   분산 트레이싱의 핵심 개념으로, 단일 트레이스(trace) 내에서 스팬(span)들 간의 관계를 정의합니다.
    -   `Trace ID`와 `Span ID`를 포함하며, 이 정보는 프로세스 및 스레드 경계를 넘어 전달되어 분산된 작업의 흐름을 연결합니다.

-   **`EventBusTracer`**:
    -   `EventBus`를 통해 이벤트가 발행되고 구독자에게 전달되는 과정을 추적합니다.
    -   이벤트가 발행될 때 `SpanContext`를 이벤트 페이로드에 주입하고, 이벤트가 수신될 때 이 컨텍스트를 추출하여 트레이스 연결성을 유지합니다. 이를 통해 이벤트 기반 통신의 전체 지연 시간과 경로를 분석할 수 있습니다.

-   **`RTCycleTracer`**:
    -   RT 프로세스의 주기적인 제어 루프 내부의 핵심 작업(예: EtherCAT 통신 시작/종료, 센서 데이터 처리, 제어 알고리즘 실행)을 추적합니다.
    -   각 단계의 시작 및 종료 시간을 기록하여 정밀한 지연 시간 분석을 가능하게 합니다. RT 프로세스의 결정성에 영향을 주지 않도록 최소한의 오버헤드로 설계됩니다.

## 3. 데이터 흐름 예시: 로봇 동작 명령 트레이싱

1.  **명령 시작 (Non-RT)**: 외부 UI에서 로봇에게 "특정 경로로 이동" 명령이 전달되면, Non-RT 프로세스의 해당 서비스는 새로운 트레이스(Trace)를 시작하고 루트 스팬(Root Span)을 생성합니다. 이 스팬은 `SpanContext`에 저장됩니다.
2.  **Task Layer 명령 전달**: Non-RT 프로세스는 `DataStore`를 통해 Task Layer에 로봇 이동 Task를 생성하도록 명령합니다. 이때 `SpanContext` 정보도 `DataStore`에 함께 기록됩니다 (또는 `EventBus`를 통해 전달될 수 있음).
3.  **RT 프로세스 Task 시작**: RT 프로세스의 `TaskExecutor`는 `DataStore`에서 로봇 이동 Task 명령을 읽고, `SpanContext`를 추출하여 현재 RT 제어 주기의 트레이스 컨텍스트로 설정합니다.
4.  **RT 제어 주기 트레이싱**: `RTCycleTracer`는 로봇 이동 Task가 실행되는 RT 제어 주기의 시작과 끝, 그리고 내부의 주요 단계(예: EtherCAT 통신, 모터 제어 Action 실행)를 스팬으로 기록합니다. 이때 모든 스팬은 `SpanContext`를 통해 루트 스팬과 연결됩니다.
5.  **이벤트 전달 트레이싱**: 만약 로봇 이동 중 RT 프로세스에서 오류 이벤트가 발생하여 `EventBus`를 통해 Non-RT 프로세스로 전달된다면, `EventBusTracer`는 해당 이벤트에 `SpanContext`를 삽입하여 전달하고, Non-RT 프로세스에서 이 이벤트를 처리하는 스팬을 기존 트레이스와 연결합니다.
6.  **스팬 데이터 전송**: `TracerProvider`는 RT/Non-RT 프로세스에서 생성된 모든 스팬 데이터를 수집하여 외부 트레이싱 시스템(예: Jaeger)으로 전송합니다.
7.  **시각화 및 분석**: Jaeger UI에서 "로봇 이동" 트레이스를 조회하면, 명령 발생부터 RT 제어 주기, 이벤트 전파, 오류 처리까지의 전체 흐름을 시간 순서에 따라 시각적으로 분석할 수 있습니다. 각 스팬의 지연 시간을 확인하여 어떤 단계에서 병목 현상이 발생하는지, 이벤트가 얼마나 빠르게 전달되는지 등을 파악할 수 있습니다.
이러한 Tracing 모듈은 MXRC 시스템의 복잡한 런타임 동작을 투명하게 분석하고 이해하는 데 강력한 도구를 제공하여, 시스템의 성능 및 안정성 최적화에 기여합니다.
