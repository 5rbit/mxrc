# Data Model: 아키텍처 안정성 개선

**Feature**: 022-fix-architecture-issues | **Phase**: 1 (Design) | **Date**: 2025-01-22

## 개요

본 문서는 Feature 022의 핵심 데이터 구조와 엔티티를 정의합니다. Phase 0 연구 결과를 바탕으로 다음 4가지 우선순위(P1-P4)에 필요한 데이터 모델을 상세히 기술합니다.

**관련 문서**:
- [spec.md](spec.md) - Feature 명세
- [plan.md](plan.md) - 구현 계획 및 Phase 0 연구
- [contracts/](contracts/) - API 계약 및 인터페이스 정의

---

## 엔티티 다이어그램

```
┌─────────────────────────────────────────────────────────────────┐
│                        Data Model Overview                       │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────┐
│   VersionedData<T>  │  (P2 - 버전 관리 핵심 엔티티)
├─────────────────────┤
│ - value: T          │  값
│ - version: uint64_t │  단조 증가 시퀀스 번호 (atomic)
│ - timestamp_ns: u64 │  나노초 정밀도 타임스탬프
├─────────────────────┤
│ + isConsistentWith()│  버전 일관성 검증
│ + isNewerThan()     │  최신 여부 확인
└─────────────────────┘
          △
          │ 사용됨
          │
┌─────────────────────────────────────────────────────────────────┐
│                    IDataAccessor (Abstract)                      │
├─────────────────────────────────────────────────────────────────┤
│ + getDomain(): string                                            │
└─────────────────────────────────────────────────────────────────┘
          △
          │ 상속
          │
    ┌─────┴─────────────────────┬──────────────────────┐
    │                           │                      │
┌───────────────────┐  ┌─────────────────────┐  ┌─────────────────────┐
│ISensorDataAccessor│  │IRobotStateAccessor  │  │ITaskStatusAccessor  │
├───────────────────┤  ├─────────────────────┤  ├─────────────────────┤
│+ getTemperature() │  │+ getPosition()      │  │+ getTaskState()     │
│+ getPressure()    │  │+ getVelocity()      │  │+ getProgress()      │
│+ setTemperature() │  │+ setPosition()      │  │+ setTaskState()     │
└───────────────────┘  └─────────────────────┘  └─────────────────────┘
    │                      │                          │
    │ 접근                 │ 접근                     │ 접근
    │                      │                          │
┌───────────────────────────────────────────────────────────────────┐
│                          DataStore                                │
│  (5개 도메인: sensor, robot_state, task_status, system, config)   │
└───────────────────────────────────────────────────────────────────┘


┌─────────────────────┐
│   EventPriority     │  (P3 - EventBus 우선순위)
├─────────────────────┤
│ CRITICAL = 0        │  오류, 상태 변경
│ NORMAL = 1          │  일반 이벤트
│ DEBUG = 2           │  디버그 로그
└─────────────────────┘
          △
          │ 사용됨
          │
┌─────────────────────┐
│  PrioritizedEvent   │  (P3 - 우선순위 이벤트)
├─────────────────────┤
│ - type: string      │  이벤트 타입
│ - priority: Enum    │  우선순위
│ - payload: variant  │  페이로드
│ - timestamp_ns: u64 │  생성 시각
└─────────────────────┘
          │
          │ 저장됨
          │
┌─────────────────────┐
│   PriorityQueue     │  (P3 - 3단계 우선순위 큐)
├─────────────────────┤
│ - queues[3]         │  각 우선순위별 큐 배열
│ - total_size        │  전체 큐 크기 (atomic)
├─────────────────────┤
│ + push()            │  Non-blocking push
│ + pop()             │  CRITICAL → NORMAL → DEBUG 순서
└─────────────────────┘
```

---

## 엔티티 상세 정의

### 1. VersionedData<T> (P2)

**목적**: DataStore에 저장되는 모든 값에 버전 정보를 추가하여 데이터 일관성을 보장합니다.

**필드**:

| 필드명         | 타입        | 설명                                      | 제약 조건                     |
|---------------|-------------|------------------------------------------|------------------------------|
| `value`       | `T`         | 실제 데이터 값                             | 템플릿 타입 (double, int 등)  |
| `version`     | `uint64_t`  | 단조 증가 시퀀스 번호                       | atomic increment, 0부터 시작  |
| `timestamp_ns`| `uint64_t`  | 나노초 정밀도 타임스탬프                    | std::chrono::steady_clock    |

**메서드**:

```cpp
// 버전 일관성 검증 (두 값이 동일한 버전인지 확인)
bool isConsistentWith(const VersionedData<T>& other) const {
    return version == other.version;
}

// 최신 여부 확인 (현재 값이 다른 값보다 새로운지 확인)
bool isNewerThan(const VersionedData<T>& other) const {
    return version > other.version;
}
```

**상태 전이**:

```
[초기 상태]
version = 0
timestamp_ns = 0
value = default(T)

    │ setVersioned() 호출
    ↓

[쓰기 발생]
version = version + 1      (atomic increment)
timestamp_ns = now()
value = new_value

    │ getVersioned() 호출
    ↓

[읽기 수행]
(동일한 version 유지)
반환: VersionedData<T> 복사본
```

**불변 속성 (Invariants)**:
- `version`은 단조 증가 (절대 감소하지 않음)
- `timestamp_ns`는 쓰기 시점의 monotonic time (시스템 시간 변경 영향 없음)
- RT 경로에서는 읽기만 수행 (쓰기는 Non-RT 경로)

**사용 예시**:

```cpp
// Accessor에서 사용
VersionedData<double> temp = sensorAccessor->getTemperature();

// 버전 일관성 확인 (재읽기)
VersionedData<double> temp2 = sensorAccessor->getTemperature();
if (!temp.isConsistentWith(temp2)) {
    // 버전 불일치 감지 - 재시도 필요
    spdlog::warn("Version mismatch detected: {} vs {}", temp.version, temp2.version);
}
```

---

### 2. IDataAccessor 인터페이스 (P2)

**목적**: 모든 도메인별 Accessor의 기본 인터페이스를 정의합니다.

**메서드**:

| 메서드명       | 반환 타입      | 파라미터 | 설명                          |
|---------------|---------------|---------|------------------------------|
| `getDomain()` | `std::string` | 없음     | Accessor가 접근 가능한 도메인 이름 |

**설계 원칙**:
- 순수 가상 인터페이스 (구현 없음)
- 소멸자는 virtual (다형성 지원)
- 도메인별 Accessor가 상속하여 구현

**구현 예시**:

```cpp
class IDataAccessor {
public:
    virtual ~IDataAccessor() = default;

    // 도메인 이름 반환 (예: "sensor", "robot_state")
    virtual std::string getDomain() const = 0;
};
```

---

### 3. ISensorDataAccessor 인터페이스 (P2)

**목적**: 센서 도메인(`sensor.*` 키)에 대한 타입 안전 접근을 제공합니다.

**메서드**:

| 메서드명              | 반환 타입                 | 파라미터        | 설명                    |
|---------------------|-------------------------|----------------|------------------------|
| `getTemperature()`  | `VersionedData<double>` | 없음            | 온도 센서 값 읽기        |
| `getPressure()`     | `VersionedData<double>` | 없음            | 압력 센서 값 읽기        |
| `getHumidity()`     | `VersionedData<double>` | 없음            | 습도 센서 값 읽기        |
| `getVibration()`    | `VersionedData<double>` | 없음            | 진동 센서 값 읽기        |
| `getCurrent()`      | `VersionedData<double>` | 없음            | 전류 센서 값 읽기        |
| `setTemperature()`  | `void`                  | `double value` | 온도 센서 값 쓰기 (Non-RT) |
| `setPressure()`     | `void`                  | `double value` | 압력 센서 값 쓰기 (Non-RT) |

**접근 가능 키** (Compile-time 검증):
```cpp
static constexpr std::array<const char*, 5> ALLOWED_KEYS = {
    "sensor.temperature",
    "sensor.pressure",
    "sensor.humidity",
    "sensor.vibration",
    "sensor.current"
};
```

**제약 조건**:
- `robot_state.*`, `task_status.*` 등 다른 도메인 키 접근 시 컴파일 오류
- 읽기 메서드는 `const` (부작용 없음)
- 쓰기 메서드는 Non-RT 경로에서만 호출 가능

---

### 4. IRobotStateAccessor 인터페이스 (P2)

**목적**: 로봇 상태 도메인(`robot_state.*` 키)에 대한 타입 안전 접근을 제공합니다.

**메서드**:

| 메서드명           | 반환 타입                      | 파라미터              | 설명                    |
|------------------|-------------------------------|---------------------|------------------------|
| `getPosition()`  | `VersionedData<Eigen::Vector3d>` | 없음              | 로봇 위치 읽기           |
| `getVelocity()`  | `VersionedData<Eigen::Vector3d>` | 없음              | 로봇 속도 읽기           |
| `getJointAngles()`| `VersionedData<std::vector<double>>` | 없음        | 관절 각도 읽기           |
| `setPosition()`  | `void`                        | `Eigen::Vector3d` | 로봇 위치 쓰기 (RT)      |
| `setVelocity()`  | `void`                        | `Eigen::Vector3d` | 로봇 속도 쓰기 (RT)      |

**접근 가능 키**:
```cpp
static constexpr std::array<const char*, 4> ALLOWED_KEYS = {
    "robot_state.position",
    "robot_state.velocity",
    "robot_state.joint_angles",
    "robot_state.control_mode"
};
```

---

### 5. ITaskStatusAccessor 인터페이스 (P2)

**목적**: Task 상태 도메인(`task_status.*` 키)에 대한 타입 안전 접근을 제공합니다.

**메서드**:

| 메서드명           | 반환 타입                  | 파라미터       | 설명                    |
|------------------|--------------------------|--------------|------------------------|
| `getTaskState()` | `VersionedData<TaskState>` | 없음         | Task 상태 읽기          |
| `getProgress()`  | `VersionedData<double>`   | 없음         | Task 진행률 읽기 (0-100) |
| `getErrorCode()` | `VersionedData<int>`      | 없음         | 에러 코드 읽기           |
| `setTaskState()` | `void`                   | `TaskState`  | Task 상태 쓰기 (Non-RT)  |
| `setProgress()`  | `void`                   | `double`     | Task 진행률 쓰기         |

**접근 가능 키**:
```cpp
static constexpr std::array<const char*, 3> ALLOWED_KEYS = {
    "task_status.state",
    "task_status.progress",
    "task_status.error_code"
};
```

**TaskState enum**:
```cpp
enum class TaskState {
    IDLE = 0,
    RUNNING = 1,
    PAUSED = 2,
    COMPLETED = 3,
    FAILED = 4
};
```

---

### 6. EventPriority enum (P3)

**목적**: EventBus 이벤트의 3단계 우선순위를 정의합니다.

**값**:

| 이름       | 값 | 설명                                | 처리 순서 | Drop Policy           |
|-----------|---|------------------------------------|---------|-----------------------|
| `CRITICAL`| 0 | 오류, 상태 변경 등 즉시 처리 필요      | 1순위    | 절대 버리지 않음         |
| `NORMAL`  | 1 | 일반 이벤트 (센서 데이터, 로그 등)     | 2순위    | 큐 90% 이상 차면 버림    |
| `DEBUG`   | 2 | 디버그 로그, 메트릭 등                | 3순위    | 큐 80% 이상 차면 버림    |

**사용 예시**:

```cpp
// CRITICAL 이벤트 생성 (RT 경로)
PrioritizedEvent error_event{
    .type = "sensor.fault",
    .priority = EventPriority::CRITICAL,
    .payload = error_code,
    .timestamp_ns = now()
};
eventBus->push(std::move(error_event));  // 항상 큐에 추가 시도

// DEBUG 이벤트 생성 (Non-RT 경로)
PrioritizedEvent debug_event{
    .type = "metrics.update",
    .priority = EventPriority::DEBUG,
    .payload = metrics_data,
    .timestamp_ns = now()
};
eventBus->push(std::move(debug_event));  // 큐 80% 이상 차면 버려짐
```

---

### 7. PrioritizedEvent 엔티티 (P3)

**목적**: EventBus에서 전송되는 우선순위 이벤트를 표현합니다.

**필드**:

| 필드명         | 타입                   | 설명                          | 제약 조건                    |
|---------------|----------------------|------------------------------|------------------------------|
| `type`        | `std::string`        | 이벤트 타입 (예: "sensor.fault") | 최대 64자 (stack 할당)        |
| `priority`    | `EventPriority`      | 우선순위 (CRITICAL/NORMAL/DEBUG) | enum 값                     |
| `payload`     | `std::variant<...>`  | 이벤트 페이로드 (다양한 타입)    | variant<int, double, string> |
| `timestamp_ns`| `uint64_t`           | 이벤트 생성 시각 (나노초)        | monotonic clock             |

**상태 전이**:

```
[생성]
type = "sensor.fault"
priority = CRITICAL
payload = 42
timestamp_ns = now()

    │ push() 호출
    ↓

[큐 삽입 대기]
(백프레셔 체크)
큐 사용률 < 80% → 삽입 허용
큐 사용률 ≥ 80% → DEBUG 버림
큐 사용률 ≥ 90% → NORMAL 버림

    │ 큐 삽입 성공
    ↓

[큐 저장]
queues_[priority] 배열에 저장
total_size++ (atomic)

    │ pop() 호출
    ↓

[우선순위 순서 처리]
1. CRITICAL 큐 체크 → 있으면 즉시 반환
2. NORMAL 큐 체크
3. DEBUG 큐 체크

    │ 처리 완료
    ↓

[소멸]
total_size-- (atomic)
이벤트 객체 삭제
```

**불변 속성**:
- `timestamp_ns`는 생성 시점 이후 변경되지 않음
- `priority`는 생성 후 변경 불가 (immutable)
- RT 경로에서는 CRITICAL/NORMAL만 생성 (DEBUG는 Non-RT 전용)

---

### 8. PriorityQueue 엔티티 (P3)

**목적**: 3단계 우선순위를 지원하는 lock-free 이벤트 큐입니다.

**필드**:

| 필드명           | 타입                                      | 설명                          | 제약 조건                    |
|-----------------|------------------------------------------|------------------------------|------------------------------|
| `queues_`       | `std::array<unique_ptr<Queue>, 3>`       | 각 우선순위별 독립 큐 배열       | SPSC queue (Boost.Lockfree)  |
| `total_size_`   | `std::atomic<size_t>`                    | 전체 큐 크기 (3개 큐 합산)      | atomic load/store            |
| `metrics_`      | `QueueMetrics`                           | 메트릭 (버려진 이벤트 수 등)     | Prometheus 연동              |

**상수**:

| 상수명              | 값   | 설명                          |
|-------------------|-----|------------------------------|
| `QUEUE_CAPACITY`  | 4096 | 각 큐의 최대 크기              |
| `DROP_THRESHOLD`  | 3276 | 80% 임계값 (DEBUG 버림 시작)   |

**메서드**:

| 메서드명  | 반환 타입                      | 파라미터            | 설명                                  |
|---------|------------------------------|-------------------|--------------------------------------|
| `push()` | `bool`                       | `PrioritizedEvent&&` | Non-blocking push (RT 안전)          |
| `pop()`  | `std::optional<PrioritizedEvent>` | 없음            | 우선순위 순서로 pop (CRITICAL 우선)    |

**백프레셔 로직**:

```cpp
bool push(PrioritizedEvent&& event) {
    size_t current_size = total_size_.load(std::memory_order_relaxed);

    // 80% 이상: DEBUG 버림
    if (current_size > DROP_THRESHOLD) {
        if (event.priority == EventPriority::DEBUG) {
            metrics_.debugEventsDropped++;
            return false;
        }
    }

    // 90% 이상: NORMAL도 버림
    if (current_size > DROP_THRESHOLD * 1.125) {  // 90%
        if (event.priority == EventPriority::NORMAL) {
            metrics_.normalEventsDropped++;
            return false;
        }
    }

    // CRITICAL은 항상 삽입 시도
    int idx = static_cast<int>(event.priority);
    if (queues_[idx]->push(std::move(event))) {
        total_size_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    return false;  // 큐 가득 찬 경우
}
```

---

## 엔티티 관계

### 1. DataStore ↔ Accessor 관계 (P2)

```
┌─────────────┐       1:N       ┌──────────────────┐
│  DataStore  │◄───────────────►│ IDataAccessor    │
│             │                  │ (Abstract)       │
│  5개 도메인  │                  └──────────────────┘
│  - sensor   │                           △
│  - robot    │                           │ 상속
│  - task     │                  ┌────────┴─────────┐
│  - system   │                  │                  │
│  - config   │         ┌────────────────┐  ┌──────────────┐
└─────────────┘         │SensorAccessor  │  │RobotAccessor │
                        │(sensor.* 전용) │  │(robot.* 전용)│
                        └────────────────┘  └──────────────┘
```

**관계 설명**:
- DataStore는 5개 도메인의 모든 키를 관리 (God Object)
- 각 Accessor는 특정 도메인 키에만 접근 가능 (Facade 패턴)
- Accessor는 DataStore를 참조로 보유 (소유권 없음, RAII 준수)

### 2. EventBus ↔ PriorityQueue 관계 (P3)

```
┌──────────────┐       1:1       ┌──────────────────┐
│   EventBus   │◄───────────────►│  PriorityQueue   │
│              │                  │                  │
│  publish()   │                  │  queues_[3]:     │
│  subscribe() │                  │  - CRITICAL      │
└──────────────┘                  │  - NORMAL        │
                                  │  - DEBUG         │
                                  └──────────────────┘
                                           │ 저장
                                           ↓
                                  ┌──────────────────┐
                                  │ PrioritizedEvent │
                                  │ - type           │
                                  │ - priority       │
                                  │ - payload        │
                                  └──────────────────┘
```

**관계 설명**:
- EventBus는 PriorityQueue를 unique_ptr로 소유
- PriorityQueue는 3개의 독립 SPSC 큐를 배열로 관리
- 각 큐는 동일한 우선순위의 PrioritizedEvent만 저장

### 3. RT ↔ Non-RT 데이터 흐름 (전체)

```
┌─────────────────────────────────────────────────────────────┐
│                      RT Process                              │
│                                                              │
│  [Control Loop]                                              │
│       │ 쓰기 (10μs 주기)                                      │
│       ↓                                                      │
│  RobotStateAccessor ──► DataStore ──► 공유 메모리             │
│       │                                                      │
│       │ 읽기 (센서 피드백)                                    │
│       ↓                                                      │
│  SensorDataAccessor ──► VersionedData<double>                │
│                                                              │
│  [Event Push]                                                │
│  EventBus.push(CRITICAL) ──► Non-blocking                    │
└──────────────────────────────────────────────────────────────┘
                              │
                              │ IPC (공유 메모리)
                              ↓
┌──────────────────────────────────────────────────────────────┐
│                    Non-RT Process                            │
│                                                              │
│  [Event Loop]                                                │
│       │ pop()                                                │
│       ↓                                                      │
│  PriorityQueue ──► CRITICAL 우선 처리                         │
│                                                              │
│  [Data Read]                                                 │
│  TaskStatusAccessor ──► 버전 일관성 체크                      │
│       │                                                      │
│       ↓                                                      │
│  if (version_mismatch) ──► 재시도 (최대 3회)                 │
└──────────────────────────────────────────────────────────────┘
```

---

## 상태 전이 다이어그램

### VersionedData 상태 전이

```
     ┌──────────┐
     │  초기화   │ version=0, timestamp_ns=0
     └────┬─────┘
          │
          │ setVersioned(key, value)
          ↓
     ┌──────────┐
     │ 쓰기 대기 │ DataStore lock 획득
     └────┬─────┘
          │
          │ atomic increment
          ↓
     ┌──────────┐
     │ 버전 증가 │ version++
     └────┬─────┘
          │
          │ timestamp 갱신
          ↓
     ┌──────────┐
     │ 완료      │ 공유 메모리 업데이트
     └────┬─────┘
          │
          │ getVersioned(key)
          ↓
     ┌──────────┐
     │ 읽기 수행 │ VersionedData 복사본 반환
     └──────────┘
          │
          │ (다음 쓰기까지 version 유지)
          │
          └────► 쓰기 대기로 복귀
```

### EventBus 이벤트 상태 전이

```
     ┌──────────┐
     │ 이벤트 생성│ PrioritizedEvent 객체
     └────┬─────┘
          │
          │ push()
          ↓
     ┌──────────────────┐
     │ 백프레셔 체크      │ total_size vs DROP_THRESHOLD
     └────┬────┬────┬────┘
          │    │    │
   CRITICAL NORMAL DEBUG
          │    │    │
          │    │    └──► 큐 80% 이상? ──Yes──► DROP
          │    └───────► 큐 90% 이상? ──Yes──► DROP
          │
          │ 큐 삽입
          ↓
     ┌──────────┐
     │ 큐 저장   │ queues_[priority]
     └────┬─────┘
          │
          │ pop() (Non-RT)
          ↓
     ┌──────────────────┐
     │ 우선순위 순서 체크 │ CRITICAL → NORMAL → DEBUG
     └────┬─────────────┘
          │
          │ 큐에서 제거
          ↓
     ┌──────────┐
     │ 이벤트 처리│ 핸들러 실행
     └────┬─────┘
          │
          │ 처리 완료
          ↓
     ┌──────────┐
     │ 소멸      │ total_size--
     └──────────┘
```

---

## 데이터 무결성 보장

### 1. VersionedData 일관성 보장 (P2)

**문제**: RT에서 쓰는 중간에 Non-RT가 읽으면 부분적으로 업데이트된 데이터를 볼 수 있음

**해결책**: Seqlock 패턴
```cpp
// Non-RT 읽기 경로
VersionedData<double> temp1 = accessor->getTemperature();
// ... 다른 값 읽기 ...
VersionedData<double> temp2 = accessor->getTemperature();

if (!temp1.isConsistentWith(temp2)) {
    // 버전 불일치 → 재시도
    retry();
}
```

**보장**:
- 최대 3회 재시도
- RT 경로는 영향 받지 않음 (Non-RT만 재시도)
- 버전 불일치 발생 시 메트릭 수집

### 2. EventBus 메시지 손실 방지 (P3)

**문제**: 큐 가득 차면 CRITICAL 이벤트도 손실될 수 있음

**해결책**: 우선순위별 Drop Policy
```cpp
// CRITICAL은 절대 버리지 않음 (큐가 가득 차도 삽입 시도)
if (event.priority == EventPriority::CRITICAL) {
    while (!queues_[0]->push(event)) {
        // CRITICAL은 busy-wait (매우 드문 경우)
        std::this_thread::yield();
    }
}
```

**보장**:
- CRITICAL 이벤트 손실 = 0
- NORMAL 손실률 < 1% (90% 임계값)
- DEBUG 손실률 < 10% (80% 임계값)

### 3. systemd 시작 순서 보장 (P1)

**문제**: RT와 Non-RT 시작 순서가 보장되지 않음

**해결책**: Before/After 지시자 + sd_notify
```ini
# RT 서비스
Before=mxrc-nonrt.service
Type=notify

# Non-RT 서비스
After=mxrc-rt.service
```

**보장**:
- RT가 READY 신호를 보내기 전까지 Non-RT는 시작되지 않음
- 공유 메모리 생성 완료 후 READY 신호 전송
- Non-RT는 재시도 로직으로 안전성 추가 보장

---

## 성능 요구사항

### 1. VersionedData 읽기 오버헤드

**목표**: < 10ns (기존 DataStore 대비)

**측정 방법**:
```cpp
auto start = std::chrono::high_resolution_clock::now();
for (int i = 0; i < 1000000; ++i) {
    auto data = accessor->getTemperature();  // 인라인 함수
}
auto end = std::chrono::high_resolution_clock::now();
auto avg_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1000000;
```

**최적화 전략**:
- `getTemperature()` 등 모든 Accessor 메서드를 `inline` 선언
- VersionedData는 POD 타입 (Plain Old Data)
- 컴파일러 최적화 활성화 (`-O3 -march=native`)

### 2. EventBus push() 지연 시간

**목표**: < 1μs (Non-blocking)

**측정 방법**:
```cpp
auto start = rdtsc();  // CPU cycle counter
eventBus->push(std::move(event));
auto end = rdtsc();
auto cycles = end - start;  // < 3000 cycles @ 3GHz
```

**최적화 전략**:
- SPSC 큐 사용 (lock-free)
- 백프레셔 체크는 atomic load만 사용 (CAS 없음)
- 이벤트 객체는 move semantics (복사 없음)

### 3. DataStore 버전 증가 오버헤드

**목표**: < 50ns

**측정 방법**:
```cpp
auto start = std::chrono::high_resolution_clock::now();
datastore->setVersioned("sensor.temperature", 25.5);
auto end = std::chrono::high_resolution_clock::now();
auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
```

**최적화 전략**:
- `version`은 `std::atomic<uint64_t>` (fetch_add 사용)
- `timestamp_ns`는 steady_clock (시스템 콜 없음)
- Lock 범위 최소화 (버전 증가만)

---

## 메모리 레이아웃

### VersionedData<double> 메모리 레이아웃

```
Offset  | Size | Field        | Alignment
--------|------|--------------|----------
0x00    | 8    | value        | 8-byte aligned
0x08    | 8    | version      | 8-byte aligned
0x10    | 8    | timestamp_ns | 8-byte aligned
--------|------|--------------|----------
Total: 24 bytes (cache-line friendly)
```

### PrioritizedEvent 메모리 레이아웃

```
Offset  | Size | Field        | Alignment
--------|------|--------------|----------
0x00    | 64   | type (string)| 8-byte aligned
0x40    | 4    | priority     | 4-byte aligned
0x44    | 4    | (padding)    | -
0x48    | 32   | payload      | 8-byte aligned
0x68    | 8    | timestamp_ns | 8-byte aligned
--------|------|--------------|----------
Total: 112 bytes (< 128 bytes, 2 cache lines)
```

**최적화 고려사항**:
- VersionedData는 단일 캐시 라인에 적재 (64 bytes 미만)
- PrioritizedEvent는 2개 캐시 라인 (false sharing 최소화)
- Accessor는 vtable 포인터만 보유 (8 bytes)

---

## 테스트 전략

### 1. VersionedData 단위 테스트

```cpp
TEST(VersionedDataTest, VersionIncrement) {
    VersionedData<int> data{42, 0, 0};
    data.version++;
    EXPECT_EQ(data.version, 1);
}

TEST(VersionedDataTest, ConsistencyCheck) {
    VersionedData<int> data1{42, 1, 100};
    VersionedData<int> data2{42, 1, 100};
    EXPECT_TRUE(data1.isConsistentWith(data2));
}

TEST(VersionedDataTest, NewerThan) {
    VersionedData<int> old_data{42, 1, 100};
    VersionedData<int> new_data{43, 2, 200};
    EXPECT_TRUE(new_data.isNewerThan(old_data));
}
```

### 2. Accessor 단위 테스트

```cpp
TEST(SensorAccessorTest, TemperatureReadWrite) {
    auto accessor = std::make_unique<SensorDataAccessor>(datastore);

    accessor->setTemperature(25.5);
    auto temp = accessor->getTemperature();

    EXPECT_DOUBLE_EQ(temp.value, 25.5);
    EXPECT_GT(temp.version, 0);
    EXPECT_GT(temp.timestamp_ns, 0);
}

TEST(SensorAccessorTest, DomainIsolation) {
    auto accessor = std::make_unique<SensorDataAccessor>(datastore);

    // 컴파일 오류 발생해야 함 (다른 도메인 접근)
    // accessor->getPosition();  // ❌ IRobotStateAccessor 메서드
}
```

### 3. PriorityQueue 통합 테스트

```cpp
TEST(PriorityQueueTest, CriticalEventFirst) {
    PriorityQueue queue;

    queue.push(PrioritizedEvent{.priority = EventPriority::NORMAL});
    queue.push(PrioritizedEvent{.priority = EventPriority::CRITICAL});

    auto event = queue.pop();
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->priority, EventPriority::CRITICAL);
}

TEST(PriorityQueueTest, DebugEventDrop) {
    PriorityQueue queue;

    // 큐를 80% 이상 채움
    fillQueueTo80Percent();

    PrioritizedEvent debug_event{.priority = EventPriority::DEBUG};
    bool pushed = queue.push(std::move(debug_event));

    EXPECT_FALSE(pushed);  // DEBUG는 버려져야 함
}
```

---

## 마이그레이션 계획

### Phase 1: Accessor 도입 (P2)

1. **Week 1**: VersionedData 구조체 구현 및 테스트
2. **Week 2**: IDataAccessor 인터페이스 정의 및 SensorDataAccessor 구현
3. **Week 3**: 기존 코드를 Accessor 사용으로 점진적 마이그레이션
4. **Week 4**: 통합 테스트 및 성능 검증

**마이그레이션 예시**:

```cpp
// Before (직접 접근)
double temp = datastore->get<double>("sensor.temperature");

// After (Accessor 사용)
auto temp = sensorAccessor->getTemperature();  // VersionedData<double>
double temp_value = temp.value;
```

### Phase 2: EventBus 우선순위 (P3)

1. **Week 5**: EventPriority enum 및 PrioritizedEvent 구조체 구현
2. **Week 6**: PriorityQueue 구현 및 백프레셔 테스트
3. **Week 7**: 기존 EventBus를 PriorityQueue로 교체
4. **Week 8**: 이벤트 폭주 시나리오 테스트

**마이그레이션 예시**:

```cpp
// Before (우선순위 없음)
eventBus->publish("sensor.fault", error_code);

// After (우선순위 지정)
PrioritizedEvent event{
    .type = "sensor.fault",
    .priority = EventPriority::CRITICAL,
    .payload = error_code,
    .timestamp_ns = now()
};
eventBus->push(std::move(event));
```

---

## 관련 문서

- [plan.md](plan.md) - Phase 0 연구 및 의사 결정 로그
- [contracts/data-contracts.md](contracts/data-contracts.md) - DataStore 스키마 문서
- [contracts/data-accessor-interface.h](contracts/data-accessor-interface.h) - IDataAccessor 인터페이스 정의
- [quickstart.md](quickstart.md) - 빠른 시작 가이드

---

**작성일**: 2025-01-22
**Phase**: 1 (Design)
**다음 단계**: contracts/ 디렉토리 파일 생성
