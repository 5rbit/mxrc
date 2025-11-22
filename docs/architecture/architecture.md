# MXRC 시스템 아키텍처

## 1. 개요

MXRC(Mission and eXecution Robot Controller)는 C++20 기반의 고성능, 고신뢰성 로봇 제어 시스템입니다. 실시간(Real-Time) 제어의 예측성과 안정성을 보장하면서, 비실시간(Non-Real-Time) 작업의 유연성을 확보하기 위해 **듀얼 프로세스 아키텍처(Dual-Process Architecture)**를 핵심 설계로 채택했습니다.

## 2. 핵심 설계 원칙

-   **RT/Non-RT 분리**: 시간 제약이 엄격한(Time-critical) 작업과 그렇지 않은 작업을 별도의 프로세스로 분리하여 상호 간섭을 원천적으로 차단하고 시스템 전체의 안정성을 확보합니다.
-   **계층형 실행 구조**: 명확한 책임 분리를 위해 RT 프로세스 내에 Task → Sequence → Action의 3단계 계층 구조를 따릅니다.
-   **데이터 중심 통신**: 프로세스 간 통신(IPC)은 공유 메모리(`DataStore`)와 비동기 메시지 큐(`EventBus`)를 통해 이루어지며, 이는 모듈 간의 결합도를 낮춥니다.
-   **RAII 및 스마트 포인터**: 모든 리소스는 스마트 포인터(`shared_ptr`, `unique_ptr`, `weak_ptr`)로 관리하여 메모리 누수와 Dangling 포인터를 방지합니다.
-   **스레드 안전성**: `tbb::concurrent_hash_map`, `std::mutex`, `std::atomic` 등을 적극적으로 사용하여 멀티스레드 및 멀티프로세스 환경에서 데이터 무결성을 보장합니다.
-   **테스트 용이성**: 의존성 주입(DI)과 Mocking이 용이하도록 인터페이스 기반으로 설계하여 테스트 격리를 지원합니다.

## 3. 고수준 아키텍처: 듀얼 프로세스 모델

MXRC는 **RT(실시간) 프로세스**와 **Non-RT(비실시간) 프로세스** 두 개의 독립된 프로세스로 구성됩니다.

```mermaid
graph TD
    subgraph "External Systems"
        UI[User Interface / API]
        Metrics[Prometheus / Grafana]
    end

    subgraph "MXRC System"
        direction LR
        
        subgraph "Non-RT Process (mxrc-nonrt)"
            direction TB
            NonRT_Main[nonrt_main]
            Monitoring[Monitoring Module<br>Prometheus Exporter]
            Logging[Logging Module<br>File & Console Logger]
            Config[Config Module]
            HA_NonRT[HA Agent]
        end

        subgraph "RT Process (mxrc-rt)"
            direction TB
            RT_Main[rt_main]
            subgraph "Execution Layers"
                T(Task Layer)
                S(Sequence Layer)
                A(Action Layer)
            end
            HA_RT[HA Agent]
        end

        subgraph "Inter-Process Communication (IPC)"
            direction TB
            DS[DataStore<br>(Shared Memory)]
            EB[EventBus<br>(Message Queue)]
        end
    end

    UI -->|Commands/Config| NonRT_Main
    NonRT_Main <-->|State/Events| DS
    NonRT_Main <-->|State/Events| EB
    
    RT_Main <-->|State/Data| DS
    RT_Main <-->|Events| EB

    NonRT_Main -->|Manages| RT_Main
    
    T --> S --> A

    A <-->|Device I/O| Hardware

    Monitoring -->|Reads| DS
    Metrics <-->|Scrapes| Monitoring
    
    Logging -->|Reads| EB

    HA_NonRT <-->|Heartbeat| HA_RT
    
    linkStyle 2,3,4,5,6,7,12,13,14,15,16 stroke-width:1.5px
    linkStyle 8,9 stroke-width:2px,stroke-dasharray: 5 5,stroke: #ff4f4f
```

-   **RT Process (`mxrc-rt`)**: 결정성(Determinism)과 시간 정확성이 가장 중요한 모듈들이 실행되는 공간입니다.
    -   **주요 책임**: 로봇 모션 제어, 센서 데이터 처리, 안전 로직 등 밀리초(ms) 또는 마이크로초(µs) 단위의 정밀한 타이밍이 요구되는 작업을 수행합니다. 파일 I/O, 동적 메모리 할당 등 예측 불가능한 지연을 유발할 수 있는 작업은 엄격히 금지됩니다.
    -   **핵심 모듈**: `rt`, `task`, `sequence`, `action`

-   **Non-RT Process (`mxrc-nonrt`)**: 실시간성을 요구하지 않는 모든 보조 작업을 담당합니다.
    -   **주요 책임**: 외부 시스템(UI, API)과의 통신, 설정 파일 관리, 상세 로그 기록, 모니터링 데이터 제공, RT 프로세스의 상태 관리 및 복구 등 예측 불가능한 지연이 허용되는 작업을 수행합니다.
    -   **핵심 모듈**: `nonrt`, `monitoring`, `logging`, `config`, `ha`

-   **프로세스 간 통신 (IPC)**:
    -   **DataStore**: 공유 메모리(Shared Memory)를 기반으로 구현되어, 두 프로세스가 고성능으로 상태 데이터를 공유하는 중앙 저장소 역할을 합니다.
    -   **EventBus**: 프로세스 간 메시지 큐(Message Queue)를 사용하여, 상태 변경이나 중요 이벤트 발생을 비동기적으로 통지합니다.

## 4. 디렉토리 및 모듈 구조

`src/core` 내의 각 모듈은 아키텍처 내에서 다음과 같은 역할을 수행합니다.

```
src/core/
├── rt/              # RT 프로세스의 메인 루프 및 실시간 스레드 관리
├── nonrt/           # Non-RT 프로세스의 메인 루프 및 관리 기능
│
├── task/            # (RT) Task 계층: 임무의 실행 모드(ONCE, PERIODIC) 관리
├── sequence/        # (RT) Sequence 계층: Action의 순차/병렬/조건부 조합 실행
├── action/          # (RT) Action 계층: 개별 동작(Move, Delay)의 실제 실행
│
├── datastore/       # (IPC) 공유 메모리 기반의 중앙 데이터 저장소
├── event/           # (IPC) 프로세스 간 비동기 이벤트 버스
│
├── config/          # (Non-RT) 시스템 설정 파일 로딩 및 관리
├── logging/         # (Non-RT) spdlog를 이용한 파일/콘솔 로깅
├── monitoring/      # (Non-RT) Prometheus 메트릭 수집 및 노출
├── tracing/         # (RT/Non-RT) 시스템 성능 분석을 위한 저수준 트레이싱
└── ha/              # (RT/Non-RT) 프로세스 상태 감시 및 복구를 위한 고가용성(High Availability) 모듈
```

## 5. 주요 컴포넌트 상세

### 5.1. 실행 계층 (Execution Layers - in RT Process)
-   **Task Layer**: `TaskExecutor`가 임무의 생명주기와 실행 모드(ONCE, PERIODIC, TRIGGERED)를 관리하며, `DataStore`에 상태를 기록합니다.
-   **Sequence Layer**: `SequenceEngine`이 `Action` 조합(순차, 병렬, 조건부)을 실행하며, `DataStore`에서 조건 데이터를 읽습니다.
-   **Action Layer**: `ActionExecutor`가 `IAction` 인터페이스를 구현한 동작을 실행하고, 결과를 `DataStore`에 기록합니다.

### 5.2. 프로세스 간 통신 (IPC)
-   **DataStore**: Intel TBB의 `concurrent_hash_map`을 공유 메모리에 배치하여, 프로세스 간에 고성능으로 데이터를 동시 접근할 수 있게 설계되었습니다. `weak_ptr` 기반의 Observer 패턴을 통해 데이터 변경을 안전하게 통지합니다.
-   **EventBus**: Boost.Lockfree의 SPSC(Single-Producer, Single-Consumer) 큐를 사용하여, RT 프로세스에서 Non-RT 프로세스로의 로그/이벤트 전달과 같이 방향성이 있는 통신을 Lock-Free로 구현하여 성능을 극대화합니다.

#### 5.2.1. DataStore Accessor Pattern (Feature 022 P2)

DataStore에 직접 접근(`datastore->get<double>("sensor.temperature")`)하는 방식은 다음과 같은 문제가 있습니다:

-   **타입 안전성 부족**: 문자열 키 기반 접근으로 컴파일 타임 타입 체킹 불가
-   **키 오타 위험**: 런타임에 잘못된 키로 인한 에러 발생 가능
-   **도메인 격리 부족**: 센서/로봇/태스크 데이터가 모두 flat한 키 공간에 섞여 있음

**Accessor Pattern**은 이러한 문제를 해결하기 위해 **도메인별 타입 안전 인터페이스**를 제공합니다:

**핵심 설계 원칙**:
-   **I-prefix 인터페이스**: `IDataAccessor`, `ISensorDataAccessor`, `IRobotStateAccessor`, `ITaskStatusAccessor`
-   **도메인 격리**: 각 Accessor는 특정 도메인 (sensor.*, robot_state.*, task_status.*)만 접근
-   **타입 안전성**: 템플릿 기반 타입 체크로 컴파일 타임에 타입 검증
-   **VersionedData 지원**: 모든 읽기 연산은 `VersionedData<T>` 반환으로 torn-read 감지 가능
-   **RAII 준수**: Accessor는 DataStore의 non-owning 참조만 보유하여 생명주기 명확화
-   **RT 안전성**: 모든 메서드는 `inline` 선언으로 함수 호출 오버헤드 제거

**주요 Accessor 구현체**:

1.  **SensorDataAccessor** (`src/core/datastore/impl/SensorDataAccessor.h`)
    -   센서 데이터 접근 (temperature, pressure, humidity, vibration, current)
    -   사용 예: `sensorAccessor->getTemperature()` → `VersionedData<double>`

2.  **RobotStateAccessor** (`src/core/datastore/impl/RobotStateAccessor.h`)
    -   로봇 상태 접근 (position, velocity, joint angles/velocities)
    -   `Vector3d` 구조체 사용 (Eigen 대체, RT-safe POD 타입)
    -   사용 예: `robotAccessor->getPosition()` → `VersionedData<Vector3d>`

3.  **TaskStatusAccessor** (`src/core/datastore/impl/TaskStatusAccessor.h`)
    -   태스크 상태 접근 (state, progress, error_code)
    -   입력 검증 예: `setProgress(0.5)` 범위 [0.0, 1.0] 체크
    -   사용 예: `taskAccessor->getTaskState()` → `VersionedData<TaskState>`

**성능 특성** (TBB concurrent_hash_map 기반, Feature 022 P2):
-   Getter 지연: ~450ns (목표: <60ns, 향후 lock-free DataStore 최적화 필요)
-   Setter 지연: ~900ns (목표: <110ns, 향후 lock-free DataStore 최적화 필요)
-   Version 체크: ~10ns (목표: <10ns, 현재 달성)

**사용 패턴**:

```cpp
// RT Path: 최신 값 바로 사용 (버전 체크 생략)
auto temp = sensorAccessor->getTemperature();
if (temp.value > 80.0) {
    // 과열 처리
}

// Non-RT Path: 버전 일관성 체크 (torn-read 방지)
VersionedData<Vector3d> pos1, pos2;
int retries = 0;
do {
    pos1 = robotAccessor->getPosition();
    auto vel = robotAccessor->getVelocity();
    pos2 = robotAccessor->getPosition();
} while (!pos1.isConsistentWith(pos2) && ++retries < 3);
```

**마이그레이션 가이드**:
-   직접 접근: `datastore->get<double>("sensor.temperature")` → `sensorAccessor->getTemperature()`
-   직접 쓰기: `datastore->set("sensor.temperature", 25.0)` → `sensorAccessor->setTemperature(25.0)`
-   버전 체크: `datastore->getVersion("sensor.temperature")` → `sensorAccessor->getTemperature().version`

### 5.3. 비실시간 모듈 (in Non-RT Process)
-   **Monitoring**: `DataStore`의 주요 상태 값들을 주기적으로 읽어와 Prometheus 형식의 메트릭으로 변환하고, 외부에서 수집할 수 있도록 HTTP 엔드포인트를 제공합니다.
-   **Logging**: RT 프로세스로부터 `EventBus`를 통해 전달받은 고속의 로그 메시지를 파일 시스템이나 콘솔에 안전하게 기록하는 역할을 전담합니다.
-   **Config**: 시작 시 JSON/YAML 형식의 설정 파일을 읽어 `DataStore`에 초기값을 채워 넣어, 시스템의 동작 파라미터를 설정합니다.
-   **HA (High Availability)**: RT 프로세스와 Non-RT 프로세스 간의 하트비트(Heartbeat)를 교환하며, 한쪽 프로세스가 비정상 종료되었을 때 이를 감지하고 시스템을 안전한 상태로 전환하거나 재시작하는 정책을 수행합니다.

## 6. 데이터 흐름 예시: 주기적 센서 읽기 및 모니터링

1.  **설정 로드 (Non-RT)**: `Config` 모듈이 "1초마다 온도 센서 값을 읽어라"는 Task 설정을 파일에서 읽어 `DataStore`에 기록합니다.
2.  **Task 예약 (RT)**: `TaskExecutor`가 `DataStore`를 읽고 주기적인(`PERIODIC`) Task가 있음을 인지하고 스케줄링합니다.
3.  **Action 실행 (RT)**: 1초마다 `TaskExecutor` → `SequenceEngine` → `ActionExecutor`를 통해 `ReadTemperatureAction`이 실행됩니다.
4.  **데이터 저장 (RT)**: `ReadTemperatureAction`은 센서에서 읽은 값을 `DataStore`의 `sensor.temp` 키에 `25.5`와 같이 기록합니다. 이 과정은 공유 메모리에서 직접 이루어지므로 매우 빠릅니다.
5.  **메트릭 노출 (Non-RT)**: `Monitoring` 모듈은 주기적으로 `DataStore`의 `sensor.temp` 값을 읽어 자신의 내부 메트릭으로 갱신합니다.
6.  **데이터 수집 (External)**: 외부 Prometheus 서버가 Non-RT 프로세스의 HTTP 엔드포인트를 통해 `mxrc_sensor_temp 25.5`와 같은 메트릭을 수집(scrape)해 갑니다.
7.  **로그 기록 (IPC)**: `ReadTemperatureAction` 실행 중 발생한 로그 정보(예: "Read sensor value: 25.5")는 `EventBus`를 통해 Non-RT 프로세스로 비동기 전송되며, `Logging` 모듈이 이를 받아 파일에 기록합니다. RT 프로세스는 파일 I/O 대기 없이 즉시 다음 작업을 수행할 수 있습니다.
