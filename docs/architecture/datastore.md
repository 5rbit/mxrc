# DataStore 아키텍처

## 1. 개요

DataStore 모듈은 MXRC 아키텍처의 중추적인 역할을 합니다. 고성능, 스레드 안전성, 프로세스 안전성을 보장하는 데이터 저장소로서, 실시간(RT) 프로세스와 비실시간(Non-RT) 프로세스 간의 원활한 통신과 상태 공유를 가능하게 합니다.

주요 특징:
-   **프로세스 간 통신 (IPC)**: 공유 메모리를 기반으로 구축되어, `mxrc-rt` 및 `mxrc-nonrt` 프로세스 간에 네트워크 오버헤드 없이 매우 빠른 데이터 교환을 지원합니다.
-   **동시 접근**: 내부적으로 Intel의 TBB(Threading Building Blocks) `tbb::concurrent_hash_map`을 저장 엔진으로 활용합니다. 이는 세분화된 락킹(fine-grained locking)을 제공하며, 여러 스레드 및 두 프로세스에서 고성능의 스레드 안전한 접근을 가능하게 합니다.
-   **도메인-특화 접근**: 원시적인 키-값 접근 대신, 타입 안전하고 도메인이 분리된 "Accessor 패턴"을 사용하도록 권장합니다.
-   **변경 알림**: `weak_ptr` 기반의 옵저버 패턴(`MapNotifier`)을 통해 데이터 변경 사항에 관심 있는 구성 요소에 안전하게 알립니다.

## 2. 아키텍처

DataStore의 아키텍처는 견고하고 안전한 데이터 공유 메커니즘을 제공하기 위해 여러 핵심 구성 요소로 이루어져 있습니다.

```mermaid
graph TD
    subgraph "RT Process"
        RT_Thread1[RT 스레드 1]
        RT_Thread2[RT 스레드 2]
    end
    
    subgraph "Non-RT Process"
        NonRT_Thread1[Non-RT 스레드 1]
        subgraph "Observers"
            Observer1[로깅 옵저버]
            Observer2[메트릭 옵저버]
        end
    end

    subgraph "Shared Memory"
        subgraph "DataStore 모듈"
            DS[DataStore<br>(tbb::concurrent_hash_map)]
            MN[MapNotifier<br>(weak_ptr 옵저버)]
            
            subgraph "Accessor 패턴"
                direction LR
                SA[SensorDataAccessor]
                RA[RobotStateAccessor]
                TA[TaskStatusAccessor]
            end

            subgraph "관리"
                direction LR
                ACM[AccessControlManager]
                EM[ExpirationManager]
                LM[LogManager]
            end
        end
    end

    RT_Thread1 -- 읽기/쓰기 via --> SA
    RT_Thread2 -- 읽기/쓰기 via --> RA
    NonRT_Thread1 -- 읽기/쓰기 via --> TA
    
    SA -- 접근 --> DS
    RA -- 접근 --> DS
    TA -- 접근 --> DS

    DS -- 업데이트 트리거 --> MN
    MN -- 알림 --> Observer1
    MN -- 알림 --> Observer2

    ACM -- 관리 --> DS
    EM -- 관리 --> DS
    LM -- 관리 --> DS
```

### 2.1. 핵심 구성 요소

-   **`DataStore` 클래스**: `tbb::concurrent_hash_map`을 캡슐화하는 중앙 클래스입니다. 공유 메모리의 생명 주기를 담당하며 저수준 `get`, `set`, `notify` 기본 함수를 제공합니다.

-   **Accessor 패턴**: DataStore와 상호 작용하는 **권장 방식**입니다. 개발자는 원시 문자열 키 대신 도메인 특화 Accessor 객체를 사용합니다.
    -   **해결하는 문제**: 잘못된 키 입력으로 인한 컴파일 타임 오류를 방지하고, 타입 안전성(예: `Vector3d`를 예상하는 키에 `double`을 쓸 수 없음)을 보장하며, 모듈을 기본 데이터 레이아웃으로부터 분리합니다.
    -   **주요 인터페이스**: `ISensorDataAccessor`, `IRobotStateAccessor`, `ITaskStatusAccessor`.
    -   **예시**:
        ```cpp
        // Bad: 직접 접근 (오류 발생 가능성 높음)
        datastore->set<double>("sensor.temp.motor_1", 35.5);
        
        // Good: Accessor 패턴 (타입 안전, 명확함)
        sensorAccessor->setTemperature("motor_1", 35.5);
        ```

-   **`MapNotifier`**: 안전한 옵저버 패턴을 구현합니다. DataStore의 데이터가 수정될 때, 노티파이어는 구독하는 구성 요소에 알릴 수 있습니다. 옵저버에 대한 참조를 `std::weak_ptr`로 유지하여 Dangling 포인터 문제를 효과적으로 해결합니다. 옵저버가 소멸되면 `weak_ptr`는 단순히 만료되고, 노티파이어는 이를 자동으로 정리하여 세그멘테이션 폴트를 방지합니다.

-   **매니저 클래스**: "God Object" 리팩토링의 일환으로, 여러 책임이 전용 매니저 클래스에 위임되었습니다:
    -   `AccessControlManager`: 데이터 접근에 대한 권한 및 유효성 검사를 처리합니다.
    -   `ExpirationManager`: 임시 데이터의 TTL(Time-To-Live)을 관리합니다.
    -   `LogManager`: 데이터 변경 사항이 로깅되는 방식을 조정합니다.
    -   `MetricsCollector`: DataStore 사용량에 대한 통계를 수집합니다.

### 2.2. 일관성을 위한 `VersionedData`

torn read(다른 스레드/프로세스에 의해 값이 부분적으로 업데이트되는 동안 값을 읽는 것)를 방지하기 위해, DataStore는 모든 읽기 작업을 `VersionedData<T>` 구조체로 래핑합니다.

```cpp
template <typename T>
struct VersionedData {
    T value;
    uint64_t version;

    bool isConsistentWith(const VersionedData<T>& other) const;
};
```

-   **작동 방식**: 맵의 모든 데이터는 쓰기 작업마다 증가하는 버전 번호를 가집니다. 읽는 스레드는 일련의 읽기 작업 전후에 버전을 확인하여 작업 중간에 데이터가 변경되지 않았는지 확인할 수 있습니다.
-   **사용**: 이는 특히 Non-RT 프로세스에서 여러 관련 값(예: 로봇 위치 및 속도)을 읽고, 이들이 동일하게 일관된 스냅샷에서 가져왔음을 확인해야 하는 경우에 중요합니다.

## 3. 데이터 흐름 예시: 로봇 위치 읽기

1.  **RT 프로세스 (쓰기)**: 실시간 제어 루프가 모션 계산을 완료합니다. `RobotStateAccessor`를 사용하여 로봇의 새로운 위치를 업데이트합니다.
    ```cpp
    // RT 스레드 내
    robotAccessor->setPosition(new_position); // 이 작업은 데이터의 버전 번호를 증가시킵니다.
    ```
2.  **알림**: Accessor에 의해 호출된 `DataStore::set` 메서드는 `MapNotifier`를 호출합니다.
3.  **Non-RT 프로세스 (읽기)**: Non-RT 프로세스의 모니터링 스레드가 UI에 표시할 위치와 속도를 읽으려고 합니다.
    ```cpp
    // Non-RT 스레드 내
    VersionedData<Vector3d> pos1, pos2;
    VersionedData<Vector3d> vel;
    
    // 일관성 보장을 위한 루프
    do {
        pos1 = robotAccessor->getPosition();
        vel = robotAccessor->getVelocity(); // 여기서 다른 값이 업데이트될 수 있습니다.
        pos2 = robotAccessor->getPosition();
    } while (!pos1.isConsistentWith(pos2)); // 읽기 중에 위치가 변경된 경우 재시도
    
    // 이제 pos1.value와 vel.value는 일관된 스냅샷에서 가져온 값입니다.
    update_ui(pos1.value, vel.value);
    ```
이 흐름은 RT 프로세스에서 고성능(쓰고 잊기)을 보장하면서, Non-RT 프로세스의 소비자에게 데이터 일관성을 보장합니다.