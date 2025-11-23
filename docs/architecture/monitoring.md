# Monitoring Module 아키텍처

## 1. 개요

Monitoring 모듈은 MXRC 시스템의 운영 상태와 성능 지표를 실시간으로 수집하고, 이를 외부 모니터링 시스템(Prometheus, Grafana 등)에 노출하여 시스템의 건강 상태를 시각화하고 분석할 수 있도록 합니다. 주로 Non-RT 프로세스에서 동작하며, `DataStore`에 기록된 핵심 상태 값들을 주기적으로 읽어와 표준 Prometheus 형식의 메트릭으로 변환하여 HTTP 엔드포인트를 통해 제공합니다.

주요 특징:
-   **성능 지표 수집**: CPU 사용률, 메모리 사용량, 태스크 실행 시간, EtherCAT 통신 지연 등 다양한 시스템 메트릭을 수집합니다.
-   **Prometheus 호환**: Prometheus 표준 메트릭 형식을 준수하여 외부 수집 시스템과의 통합을 용이하게 합니다.
-   **시각화 연동**: Grafana와 같은 대시보드 도구를 통해 수집된 메트릭을 효과적으로 시각화할 수 있습니다.
-   **경고 시스템 연동**: 정의된 임계값 초과 시 Prometheus Alertmanager를 통해 경고를 발생시킬 수 있습니다.

## 2. 아키텍처

Monitoring 모듈은 메트릭 데이터를 수집, 변환, 노출하는 기능을 중심으로 구성됩니다. RT 프로세스에서도 일부 메트릭을 생성하여 `DataStore`를 통해 Non-RT 프로세스로 전달합니다.

```mermaid
graph TD
    subgraph "RT Process"
        RT_Component[RT 컴포넌트 (e.g., EtherCAT)]
        RTM[RTMetrics] -- Generates RT-Specific Metrics --> DS[DataStore]
        RT_Component -- Uses --> RTM
    end

    subgraph "Non-RT Process"
        DS -- Reads Core Data --> MC[MetricsCollector]
        
        subgraph "Metrics Server"
            MS[MetricsServer (HTTP Endpoint)]
        end
        MC -- Converts to Prometheus Format --> MS
        
        SL[StructuredLogger] -- Logs Structured Data --> LogFile[로그 파일]
    end

    subgraph "External Monitoring Systems"
        Prometheus[Prometheus Server] -- Scrapes Metrics --> MS
        Grafana[Grafana Dashboard] -- Queries --> Prometheus
        Alertmanager[Alertmanager] -- Triggers Alerts --> Prometheus
    end

    style DS fill:#ccf,stroke:#333,stroke-width:2px
    style RTM fill:#f9f,stroke:#333,stroke-width:2px
    style MC fill:#f9f,stroke:#333,stroke-width:2px
    style MS fill:#f9f,stroke:#333,stroke-width:2px
```

### 2.1. 핵심 구성 요소

-   **`MetricsCollector`**:
    -   Non-RT 프로세스의 핵심 컴포넌트로, `DataStore`에 저장된 다양한 시스템 상태 및 성능 데이터를 주기적으로 읽어옵니다.
    -   읽어온 데이터를 Prometheus 표준 메트릭(Gauge, Counter, Histogram 등) 형식으로 변환하고 집계합니다.
    -   `RTMetrics`에서 생성된 RT 프로세스 관련 메트릭도 `DataStore`를 통해 수집하여 통합합니다.

-   **`MetricsServer`**:
    -   `MetricsCollector`가 준비한 메트릭 데이터를 HTTP 엔드포인트(일반적으로 `/metrics`)를 통해 외부에 노출하는 경량 웹 서버입니다.
    -   Prometheus 서버가 이 엔드포인트에 접속하여 메트릭을 주기적으로 스크랩(scrape)해 갈 수 있도록 합니다.

-   **`StructuredLogger`**:
    -   일반적인 텍스트 로깅 외에, JSON과 같이 기계가 파싱하기 쉬운 구조화된 형식으로 로그를 기록하는 컴포넌트입니다.
    -   이를 통해 로그 데이터를 중앙 집중식 로그 분석 시스템(예: ELK Stack)으로 쉽게 전송하고, 모니터링 이벤트로 활용할 수 있습니다. `src/core/monitoring`에 위치하여 모니터링과 관련된 구조화된 로깅을 담당합니다.

-   **`RTMetrics` (RT 프로세스 내부)**:
    -   RT 프로세스 내에서 직접적으로 생성되는 고정밀, 실시간 메트릭(예: RT 주기 지터, EtherCAT 통신 지연)을 관리하고 `DataStore`에 기록합니다.
    -   `MetricsCollector`는 이 `DataStore`의 `RTMetrics` 데이터를 읽어와 통합 메트릭으로 노출합니다.

## 3. 데이터 흐름 예시: RT 프로세스 CPU 사용률 모니터링

1.  **RT 메트릭 생성**: RT 프로세스 내부의 `RTM` 컴포넌트는 주기적으로 RT 스케줄러의 CPU 사용률을 측정하고, 이 값을 `DataStore`의 특정 키(예: "rt.cpu_usage_percent")에 기록합니다. 이때 `DataStore`의 Accessor 패턴을 활용하여 타입 안전하게 기록합니다.
2.  **Non-RT 메트릭 수집**: Non-RT 프로세스의 `MetricsCollector`는 주기적으로 `DataStore`에서 "rt.cpu_usage_percent" 키의 값을 읽어옵니다.
3.  **Prometheus 형식 변환**: `MetricsCollector`는 읽어온 CPU 사용률 데이터를 Prometheus Gauge 메트릭(`mxrc_rt_cpu_usage_percent{process="rt"}`)으로 변환합니다.
4.  **메트릭 노출**: `MetricsServer`는 이 메트릭을 자신의 `/metrics` HTTP 엔드포인트를 통해 외부에 노출합니다.
5.  **Prometheus 스크랩**: 외부 Prometheus 서버는 미리 설정된 주기에 따라 `MetricsServer`의 `/metrics` 엔드포인트에 HTTP 요청을 보내 메트릭 데이터를 수집합니다.
6.  **Grafana 시각화**: Prometheus에 수집된 `mxrc_rt_cpu_usage_percent` 메트릭은 Grafana 대시보드에서 시계열 그래프 형태로 시각화되어, 운영자가 RT 프로세스의 CPU 사용률 변화를 실시간으로 모니터링할 수 있도록 합니다. 또한, CPU 사용률이 특정 임계값을 초과하면 Prometheus Alertmanager를 통해 경고 알림이 전송될 수 있습니다.
