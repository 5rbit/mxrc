# Data Contracts: DataStore 스키마 문서

**Feature**: 022-fix-architecture-issues | **Phase**: 1 (Design) | **Date**: 2025-01-22

## 개요

본 문서는 DataStore의 5개 도메인에 대한 데이터 스키마를 정의합니다. 각 도메인은 독립적인 키 네임스페이스를 가지며, 도메인별 Accessor를 통해 접근됩니다.

**관련 문서**:
- [data-model.md](../data-model.md) - 엔티티 및 인터페이스 정의
- [plan.md](../plan.md) - Phase 0 연구 및 설계 결정

---

## DataStore 도메인 구조

```
DataStore (공유 메모리 기반 Key-Value Store)
├── sensor.*              (센서 데이터 - ISensorDataAccessor)
├── robot_state.*         (로봇 상태 - IRobotStateAccessor)
├── task_status.*         (Task 상태 - ITaskStatusAccessor)
├── system.*              (시스템 설정 - ISystemAccessor, 미래 확장)
└── config.*              (설정 값 - IConfigAccessor, 미래 확장)
```

---

## 도메인 1: sensor.* (센서 데이터)

**목적**: 하드웨어 센서에서 수집된 데이터를 저장합니다.

**Accessor**: `ISensorDataAccessor`

**접근 권한**:
- **읽기**: RT 경로 (Control Loop에서 피드백)
- **쓰기**: Non-RT 경로 (센서 드라이버)

### 스키마

| 키 이름                  | 타입       | 단위  | 범위         | 업데이트 주기 | 설명                    |
|------------------------|----------|------|-------------|-------------|------------------------|
| `sensor.temperature`   | `double` | °C   | -50 ~ 150   | 100ms       | 모터 온도                |
| `sensor.pressure`      | `double` | Pa   | 0 ~ 200000  | 100ms       | 공압 센서                |
| `sensor.humidity`      | `double` | %    | 0 ~ 100     | 1000ms      | 습도 센서                |
| `sensor.vibration`     | `double` | mm/s²| 0 ~ 1000    | 10ms        | 진동 센서                |
| `sensor.current`       | `double` | A    | 0 ~ 50      | 10ms        | 모터 전류                |

### VersionedData 래핑

모든 센서 값은 `VersionedData<double>`로 래핑됩니다:

```cpp
struct VersionedData<double> {
    double value;         // 센서 값
    uint64_t version;     // 단조 증가 시퀀스 번호
    uint64_t timestamp_ns;// 측정 시각 (나노초)
};
```

### 사용 예시

```cpp
// Non-RT 경로: 센서 값 쓰기
void SensorDriver::updateTemperature(double temp_celsius) {
    sensorAccessor->setTemperature(temp_celsius);
    // DataStore 내부:
    // 1. version++ (atomic)
    // 2. timestamp_ns = now()
    // 3. value = temp_celsius
}

// RT 경로: 센서 값 읽기
void ControlLoop::readSensorFeedback() {
    auto temp = sensorAccessor->getTemperature();

    // 버전 일관성 검증 (Non-RT에서만, RT는 최신 값 바로 사용)
    // if (!temp.isConsistentWith(previous_temp)) { retry(); }

    double temp_value = temp.value;
    // ... 제어 로직 ...
}
```

### 제약 조건

- **RT 경로 제약**: RT에서는 쓰기 금지 (읽기만 허용)
- **Non-RT 쓰기**: 센서 드라이버만 쓰기 권한 보유
- **버전 불일치 허용**: RT는 최신 값을 바로 사용 (일관성 체크 생략)
- **메모리 할당 금지**: VersionedData는 스택 할당 (POD 타입)

---

## 도메인 2: robot_state.* (로봇 상태)

**목적**: 로봇의 위치, 속도, 관절 각도 등 제어 상태를 저장합니다.

**Accessor**: `IRobotStateAccessor`

**접근 권한**:
- **읽기**: RT 경로 (Control Loop) + Non-RT 경로 (모니터링)
- **쓰기**: RT 경로 (Control Loop만)

### 스키마

| 키 이름                    | 타입                   | 단위       | 범위           | 업데이트 주기 | 설명                    |
|--------------------------|----------------------|-----------|---------------|-------------|------------------------|
| `robot_state.position`   | `Eigen::Vector3d`    | m         | -10 ~ 10      | 10μs (RT)   | 로봇 엔드이펙터 위치       |
| `robot_state.velocity`   | `Eigen::Vector3d`    | m/s       | -5 ~ 5        | 10μs (RT)   | 로봇 속도                |
| `robot_state.joint_angles`| `std::vector<double>`| rad       | -π ~ π        | 10μs (RT)   | 6축 관절 각도             |
| `robot_state.control_mode`| `ControlMode` (enum) | -         | 0 ~ 3         | 이벤트 기반   | 제어 모드 (위치/속도/힘)   |

### VersionedData 래핑

```cpp
// 벡터 타입
VersionedData<Eigen::Vector3d> {
    Eigen::Vector3d value;  // [x, y, z]
    uint64_t version;
    uint64_t timestamp_ns;
};

// 관절 각도
VersionedData<std::vector<double>> {
    std::vector<double> value;  // [θ1, θ2, θ3, θ4, θ5, θ6]
    uint64_t version;
    uint64_t timestamp_ns;
};

// Enum 타입
enum class ControlMode {
    POSITION = 0,
    VELOCITY = 1,
    FORCE = 2,
    IDLE = 3
};

VersionedData<ControlMode> {
    ControlMode value;
    uint64_t version;
    uint64_t timestamp_ns;
};
```

### 사용 예시

```cpp
// RT 경로: 로봇 상태 업데이트
void ControlLoop::updateRobotState(const Eigen::Vector3d& pos) {
    robotAccessor->setPosition(pos);
    // ✅ RT 안전: 인라인 함수, lock-free atomic increment
}

// Non-RT 경로: 로봇 상태 모니터링 (버전 일관성 체크)
void Monitor::readConsistentState() {
    auto pos1 = robotAccessor->getPosition();
    auto vel = robotAccessor->getVelocity();
    auto pos2 = robotAccessor->getPosition();

    if (!pos1.isConsistentWith(pos2)) {
        // 버전 불일치 → 재시도
        spdlog::warn("Version mismatch, retrying...");
        return readConsistentState();
    }

    // ✅ 일관된 스냅샷 획득
    processState(pos1.value, vel.value);
}
```

### 제약 조건

- **RT 쓰기 전용**: RT에서만 robot_state.* 업데이트
- **Non-RT 읽기 전용**: Non-RT는 모니터링 목적으로만 읽기
- **벡터 사전 할당**: std::vector는 6개 요소로 사전 할당 (RT 경로 메모리 할당 금지)
- **Eigen 정렬**: Eigen::Vector3d는 16-byte aligned (SIMD 최적화)

---

## 도메인 3: task_status.* (Task 상태)

**목적**: 현재 실행 중인 Task의 상태, 진행률, 에러 코드를 저장합니다.

**Accessor**: `ITaskStatusAccessor`

**접근 권한**:
- **읽기**: RT 경로 + Non-RT 경로
- **쓰기**: Non-RT 경로 (Task Manager만)

### 스키마

| 키 이름                  | 타입        | 단위 | 범위     | 업데이트 주기 | 설명                    |
|------------------------|-----------|-----|---------|-------------|------------------------|
| `task_status.state`    | `TaskState`(enum) | - | 0 ~ 4   | 이벤트 기반   | Task 상태 (IDLE/RUNNING/...) |
| `task_status.progress` | `double`  | %   | 0 ~ 100 | 100ms       | Task 진행률              |
| `task_status.error_code`| `int`    | -   | 0 ~ 999 | 오류 발생 시  | 에러 코드 (0 = 정상)      |

### VersionedData 래핑

```cpp
enum class TaskState {
    IDLE = 0,
    RUNNING = 1,
    PAUSED = 2,
    COMPLETED = 3,
    FAILED = 4
};

// Task 상태
VersionedData<TaskState> {
    TaskState value;
    uint64_t version;
    uint64_t timestamp_ns;
};

// 진행률
VersionedData<double> {
    double value;  // 0.0 ~ 100.0
    uint64_t version;
    uint64_t timestamp_ns;
};

// 에러 코드
VersionedData<int> {
    int value;  // 0 = 정상, 1-999 = 에러 코드
    uint64_t version;
    uint64_t timestamp_ns;
};
```

### 사용 예시

```cpp
// Non-RT 경로: Task 상태 업데이트
void TaskManager::startTask(const std::string& task_id) {
    taskAccessor->setTaskState(TaskState::RUNNING);
    taskAccessor->setProgress(0.0);
    taskAccessor->setErrorCode(0);
}

void TaskManager::updateProgress(double progress) {
    taskAccessor->setProgress(progress);  // 0.0 ~ 100.0
}

void TaskManager::handleError(int error_code) {
    taskAccessor->setTaskState(TaskState::FAILED);
    taskAccessor->setErrorCode(error_code);

    // CRITICAL 이벤트 발행
    PrioritizedEvent event{
        .type = "task.error",
        .priority = EventPriority::CRITICAL,
        .payload = error_code,
        .timestamp_ns = now()
    };
    eventBus->push(std::move(event));
}

// RT 경로: Task 상태 읽기 (제어 로직 분기)
void ControlLoop::checkTaskState() {
    auto state = taskAccessor->getTaskState();

    if (state.value == TaskState::PAUSED) {
        // 제어 일시 정지
        pauseControl();
    } else if (state.value == TaskState::RUNNING) {
        // 정상 제어 수행
        executeControl();
    }
}
```

### 제약 조건

- **Non-RT 쓰기 전용**: Task Manager만 상태 업데이트
- **RT 읽기 전용**: RT는 상태 확인만 (분기 로직)
- **에러 코드 범위**: 0-999 (0 = 정상, 1-99 = 경고, 100-999 = 에러)
- **진행률 범위**: 0.0-100.0 (검증 필요)

---

## 도메인 4: system.* (시스템 설정)

**목적**: 시스템 레벨 설정 및 런타임 파라미터를 저장합니다.

**Accessor**: `ISystemAccessor` (미래 확장)

**접근 권한**:
- **읽기**: RT 경로 + Non-RT 경로
- **쓰기**: Non-RT 경로 (초기화 단계만)

### 스키마 (예시, Feature 022에서 미구현)

| 키 이름                  | 타입     | 단위 | 범위     | 업데이트 주기 | 설명                    |
|------------------------|--------|-----|---------|-------------|------------------------|
| `system.rt_cycle_time` | `int`  | μs  | 1 ~ 1000| 초기화 시     | RT 사이클 시간 (10μs)    |
| `system.cpu_affinity`  | `int`  | -   | 0 ~ 3   | 초기화 시     | RT 코어 번호 (2-3)       |
| `system.log_level`     | `int`  | -   | 0 ~ 4   | 런타임 변경   | 로그 레벨 (INFO=2, DEBUG=3)|

**Note**: Feature 022에서는 구현하지 않으며, 향후 확장을 위해 네임스페이스만 예약합니다.

---

## 도메인 5: config.* (설정 값)

**목적**: 애플리케이션 레벨 설정 값을 저장합니다.

**Accessor**: `IConfigAccessor` (미래 확장)

**접근 권한**:
- **읽기**: RT 경로 + Non-RT 경로
- **쓰기**: Non-RT 경로 (설정 파일 로드 시)

### 스키마 (예시, Feature 022에서 미구현)

| 키 이름                   | 타입     | 단위 | 범위     | 업데이트 주기 | 설명                    |
|-------------------------|--------|-----|---------|-------------|------------------------|
| `config.max_velocity`   | `double`| m/s | 0 ~ 10  | 설정 파일     | 최대 속도 제한           |
| `config.safety_zone`    | `double`| m   | 0 ~ 5   | 설정 파일     | 안전 영역 반경           |
| `config.control_gain_p` | `double`| -   | 0 ~ 100 | 런타임 튜닝   | PID 제어 P 게인         |

**Note**: Feature 022에서는 구현하지 않으며, 향후 확장을 위해 네임스페이스만 예약합니다.

---

## 도메인별 접근 매트릭스

| 도메인          | RT 읽기 | RT 쓰기 | Non-RT 읽기 | Non-RT 쓰기 | Accessor 인터페이스        |
|---------------|--------|--------|------------|------------|--------------------------|
| `sensor.*`    | ✅      | ❌      | ✅          | ✅          | `ISensorDataAccessor`    |
| `robot_state.*`| ✅     | ✅      | ✅          | ❌          | `IRobotStateAccessor`    |
| `task_status.*`| ✅     | ❌      | ✅          | ✅          | `ITaskStatusAccessor`    |
| `system.*`    | ✅      | ❌      | ✅          | ✅ (초기화)  | `ISystemAccessor` (미래)  |
| `config.*`    | ✅      | ❌      | ✅          | ✅ (설정)    | `IConfigAccessor` (미래)  |

**범례**:
- ✅ 허용
- ❌ 금지
- ✅ (조건) 특정 조건에서만 허용

---

## 버전 관리 정책

### 1. 버전 증가 규칙

**언제 버전이 증가하는가?**
- `setVersioned(key, value)` 호출 시마다 **무조건** 증가
- 동일한 값으로 쓰기를 해도 버전은 증가 (타임스탬프 갱신)

**예시**:
```cpp
// 초기 상태: version=0, value=0.0
accessor->setTemperature(25.5);  // version=1
accessor->setTemperature(25.5);  // version=2 (동일 값이지만 증가)
accessor->setTemperature(26.0);  // version=3
```

**이유**: 쓰기 발생 여부를 추적하기 위해 (디버깅 및 모니터링)

### 2. 타임스탬프 정책

**시간 소스**: `std::chrono::steady_clock` (monotonic)
- **장점**: 시스템 시간 변경(NTP 동기화 등)의 영향 없음
- **정밀도**: 나노초 (실제 정밀도는 플랫폼 의존)
- **용도**: 데이터 신선도(freshness) 확인

**예시**:
```cpp
auto temp = accessor->getTemperature();
uint64_t age_ns = now() - temp.timestamp_ns;

if (age_ns > 1'000'000'000) {  // 1초 이상 오래된 데이터
    spdlog::warn("Stale data detected: {} ns old", age_ns);
}
```

### 3. 버전 불일치 처리

**Non-RT 경로 (일관성 보장 필요)**:
```cpp
const int MAX_RETRIES = 3;
for (int i = 0; i < MAX_RETRIES; ++i) {
    auto temp1 = accessor->getTemperature();
    auto pressure = accessor->getPressure();
    auto temp2 = accessor->getTemperature();

    if (temp1.isConsistentWith(temp2)) {
        // ✅ 일관성 확인됨
        return processData(temp1.value, pressure.value);
    }

    // ⚠️ 버전 불일치 → 재시도
    metrics_.versionMismatchCount++;
}

// 최대 재시도 초과 → 최신 버전 강제 사용
spdlog::error("Version mismatch persists after {} retries", MAX_RETRIES);
auto latest = accessor->getTemperature();
return processData(latest.value, latest.value);
```

**RT 경로 (일관성 체크 생략)**:
```cpp
// RT는 최신 값을 바로 사용 (버전 체크 없음)
auto temp = accessor->getTemperature();
double temp_value = temp.value;  // 바로 사용
// ... 제어 로직 ...
```

**이유**: RT 경로는 deterministic latency가 중요하므로 재시도 불가

---

## 메모리 레이아웃 및 정렬

### 공유 메모리 구조

```
/dev/shm/mxrc_datastore (공유 메모리 세그먼트)

Offset   | Size  | Domain         | Keys
---------|-------|----------------|---------------------------
0x0000   | 2KB   | sensor.*       | 5개 키 × 24 bytes/key = 120 bytes
0x0800   | 4KB   | robot_state.*  | 4개 키 × ~100 bytes/key = 400 bytes
0x1800   | 1KB   | task_status.*  | 3개 키 × 24 bytes/key = 72 bytes
0x1C00   | 1KB   | system.*       | (예약, 미사용)
0x2000   | 1KB   | config.*       | (예약, 미사용)
---------|-------|----------------|---------------------------
Total: 9KB (실제 할당: 16KB, 2의 거듭제곱)
```

### 캐시 라인 정렬

**VersionedData<double> 정렬**:
```cpp
struct alignas(64) VersionedData<double> {
    double value;         // Offset 0x00 (8 bytes)
    uint64_t version;     // Offset 0x08 (8 bytes)
    uint64_t timestamp_ns;// Offset 0x10 (8 bytes)
    // Padding: 40 bytes (총 64 bytes, 단일 캐시 라인)
};
```

**이유**: False sharing 방지 (각 도메인의 데이터가 독립 캐시 라인 점유)

---

## DataStore API 계약

### 기존 IDataStore 인터페이스 (유지)

```cpp
class IDataStore {
public:
    virtual ~IDataStore() = default;

    // 기존 메서드 (하위 호환성 유지)
    template <typename T>
    T get(const std::string& key) const;

    template <typename T>
    void set(const std::string& key, const T& value);

    // ✅ 새로운 메서드 (P2 - VersionedData 지원)
    template <typename T>
    VersionedData<T> getVersioned(const std::string& key) const;

    template <typename T>
    void setVersioned(const std::string& key, const T& value);
};
```

### Accessor를 통한 간접 접근 (P2)

```cpp
class SensorDataAccessor : public ISensorDataAccessor {
private:
    IDataStore& datastore_;  // 참조로 보유 (소유권 없음)

public:
    VersionedData<double> getTemperature() const override {
        return datastore_.getVersioned<double>("sensor.temperature");
    }

    void setTemperature(double value) override {
        datastore_.setVersioned("sensor.temperature", value);
    }
};
```

---

## 성능 벤치마크 목표

### 1. 읽기 성능

| 작업                         | 목표 지연 시간 | 측정 방법                  |
|----------------------------|-------------|--------------------------|
| `getVersioned<double>()`   | < 50ns      | rdtsc() CPU cycle counter |
| Accessor `getTemperature()`| < 60ns      | 인라인 함수 + cache hit     |
| 버전 일관성 체크 (2회 읽기)    | < 120ns     | isConsistentWith() 호출   |

### 2. 쓰기 성능

| 작업                         | 목표 지연 시간 | 측정 방법                  |
|----------------------------|-------------|--------------------------|
| `setVersioned<double>()`   | < 100ns     | atomic increment + lock   |
| Accessor `setTemperature()`| < 110ns     | 인라인 함수               |

### 3. 메모리 오버헤드

| 항목                    | 기존 크기 | 새로운 크기 | 오버헤드 |
|-----------------------|---------|-----------|---------|
| `double` 값            | 8 bytes | 24 bytes  | +200%   |
| DataStore 총 크기       | 3KB     | 9KB       | +200%   |

**허용 가능 근거**: 공유 메모리는 충분히 큼 (현재 16KB 할당, 9KB 사용 = 56%)

---

## 테스트 시나리오

### 1. 도메인 격리 테스트

```cpp
TEST(DataContractsTest, SensorAccessorDomainIsolation) {
    auto sensorAccessor = std::make_unique<SensorDataAccessor>(datastore);

    // ✅ sensor.* 키 접근 가능
    EXPECT_NO_THROW(sensorAccessor->getTemperature());

    // ❌ 컴파일 오류 (robot_state.* 키 접근 불가)
    // sensorAccessor->getPosition();  // 컴파일 오류 발생해야 함
}
```

### 2. 버전 일관성 테스트

```cpp
TEST(DataContractsTest, VersionConsistency) {
    auto accessor = std::make_unique<SensorDataAccessor>(datastore);

    accessor->setTemperature(25.5);
    auto temp1 = accessor->getTemperature();
    auto temp2 = accessor->getTemperature();

    // 동일한 버전 (쓰기 없었으므로)
    EXPECT_TRUE(temp1.isConsistentWith(temp2));
    EXPECT_EQ(temp1.version, temp2.version);
}

TEST(DataContractsTest, VersionIncrement) {
    auto accessor = std::make_unique<SensorDataAccessor>(datastore);

    accessor->setTemperature(25.5);
    auto temp1 = accessor->getTemperature();

    accessor->setTemperature(26.0);  // 쓰기 발생
    auto temp2 = accessor->getTemperature();

    // 버전 증가
    EXPECT_TRUE(temp2.isNewerThan(temp1));
    EXPECT_EQ(temp2.version, temp1.version + 1);
}
```

### 3. 타임스탬프 신선도 테스트

```cpp
TEST(DataContractsTest, TimestampFreshness) {
    auto accessor = std::make_unique<SensorDataAccessor>(datastore);

    accessor->setTemperature(25.5);
    auto temp = accessor->getTemperature();

    uint64_t age_ns = getCurrentTimeNs() - temp.timestamp_ns;

    // 1ms 이내 (신선한 데이터)
    EXPECT_LT(age_ns, 1'000'000);
}
```

### 4. RT/Non-RT 동시 접근 테스트

```cpp
TEST(DataContractsTest, ConcurrentRTNonRTAccess) {
    auto robotAccessor = std::make_unique<RobotStateAccessor>(datastore);

    std::atomic<bool> running{true};

    // RT 스레드: 10μs 주기로 쓰기
    std::thread rt_thread([&]() {
        while (running) {
            Eigen::Vector3d pos(1.0, 2.0, 3.0);
            robotAccessor->setPosition(pos);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });

    // Non-RT 스레드: 100ms 주기로 읽기 (버전 일관성 체크)
    std::thread nonrt_thread([&]() {
        int mismatch_count = 0;
        for (int i = 0; i < 100; ++i) {
            auto pos1 = robotAccessor->getPosition();
            auto pos2 = robotAccessor->getPosition();

            if (!pos1.isConsistentWith(pos2)) {
                mismatch_count++;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 버전 불일치는 드물게 발생 가능 (< 5%)
        EXPECT_LT(mismatch_count, 5);
    });

    std::this_thread::sleep_for(std::chrono::seconds(10));
    running = false;

    rt_thread.join();
    nonrt_thread.join();
}
```

---

## 마이그레이션 체크리스트

### Phase 1: Accessor 도입 (P2)

- [ ] VersionedData 구조체 구현 및 단위 테스트
- [ ] IDataStore에 getVersioned/setVersioned 메서드 추가
- [ ] ISensorDataAccessor 인터페이스 정의
- [ ] SensorDataAccessor 구현 및 테스트
- [ ] IRobotStateAccessor 인터페이스 정의
- [ ] RobotStateAccessor 구현 및 테스트
- [ ] ITaskStatusAccessor 인터페이스 정의
- [ ] TaskStatusAccessor 구현 및 테스트
- [ ] 기존 코드를 Accessor 사용으로 마이그레이션 (점진적)
- [ ] 통합 테스트 및 성능 벤치마크

### Phase 2: 하위 호환성 유지

- [ ] 기존 `get<T>()`/`set<T>()` 메서드 유지 (deprecated 표시)
- [ ] 레거시 코드 마이그레이션 가이드 작성
- [ ] Accessor 사용 예시 추가 (quickstart.md)

---

## 관련 문서

- [data-model.md](../data-model.md) - VersionedData 및 Accessor 인터페이스 정의
- [data-accessor-interface.h](data-accessor-interface.h) - IDataAccessor 헤더 스니펫
- [sensor-data-accessor.h](sensor-data-accessor.h) - ISensorDataAccessor 헤더 스니펫
- [quickstart.md](../quickstart.md) - Accessor 사용 예시

---

**작성일**: 2025-01-22
**Phase**: 1 (Design)
**다음 단계**: contracts/ 헤더 스니펫 파일 생성
