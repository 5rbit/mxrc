# Quickstart Guide: 아키텍처 안정성 개선

**Feature**: 022-fix-architecture-issues | **Phase**: 1 (Design) | **Date**: 2025-01-22

## 개요

본 문서는 Feature 022의 4가지 핵심 개선 사항(P1-P4)을 빠르게 적용하고 검증하는 방법을 제공합니다. 각 우선순위별로 구현 방법, 검증 절차, 문제 해결 방법을 상세히 설명합니다.

**대상 독자**:
- 시스템 아키텍트
- RT/Non-RT 프로세스 개발자
- DevOps 엔지니어 (systemd 설정)

**관련 문서**:
- [plan.md](plan.md) - Phase 0 연구 및 설계 결정
- [data-model.md](data-model.md) - 엔티티 및 인터페이스 정의
- [contracts/](contracts/) - API 계약 및 스키마

---

## 목차

1. [P1: systemd 시작 순서 경쟁 상태 해결](#p1-systemd-시작-순서-경쟁-상태-해결)
2. [P2: DataStore Accessor 패턴 사용](#p2-datastore-accessor-패턴-사용)
3. [P3: EventBus 우선순위 큐 사용](#p3-eventbus-우선순위-큐-사용)
4. [P4: Watchdog 하트비트 설정](#p4-watchdog-하트비트-설정)
5. [검증 절차](#검증-절차)
6. [문제 해결](#문제-해결)

---

## P1: systemd 시작 순서 경쟁 상태 해결

### 문제 상황

RT 프로세스와 Non-RT 프로세스가 동시에 시작되어 공유 메모리 연결 실패가 발생합니다.

```
[현재 상황]
Non-RT 시작 → 공유 메모리 연결 시도 → ❌ 실패 (RT가 아직 생성 전)
RT 시작 → 공유 메모리 생성 → ✅ 완료 (하지만 Non-RT는 이미 종료됨)
```

### 해결 방법

#### 1. systemd 서비스 파일 수정

**RT 서비스 파일 수정** (`systemd/mxrc-rt.service`):

```ini
[Unit]
Description=MXRC Real-Time Process
After=network.target
Before=mxrc-nonrt.service  # ✅ Non-RT보다 먼저 시작

[Service]
Type=notify  # ✅ sd_notify(READY=1) 신호 대기
ExecStart=/usr/local/bin/mxrc-rt
WatchdogSec=30s
Restart=always

[Install]
WantedBy=multi-user.target
```

**Non-RT 서비스 파일 수정** (`systemd/mxrc-nonrt.service`):

```ini
[Unit]
Description=MXRC Non-Real-Time Process
After=network.target mxrc-rt.service  # ✅ RT 다음에 시작

[Service]
Type=simple
ExecStart=/usr/local/bin/mxrc-nonrt
Restart=always

[Install]
WantedBy=multi-user.target
```

**변경 사항 요약**:
- RT 서비스: `Before=mxrc-nonrt.service` 추가
- RT 서비스: `Type=notify` 설정 (READY 신호 대기)
- Non-RT 서비스: `After=mxrc-rt.service` 추가

#### 2. RT 프로세스에서 READY 신호 전송

**코드 예시** (`src/core/rt/RTExecutive.cpp`):

```cpp
#include <systemd/sd-daemon.h>

void RTExecutive::initialize() {
    // 1. RT 스케줄링 설정
    setupRTScheduling();

    // 2. 공유 메모리 생성 (CRITICAL)
    createSharedMemory();
    spdlog::info("Shared memory created successfully");

    // 3. IPC 채널 초기화
    initializeIPC();

    // 4. ✅ READY 신호 전송 (Non-RT 시작 허용)
    if (sd_notify(0, "READY=1") <= 0) {
        spdlog::warn("Failed to send READY signal to systemd");
    } else {
        spdlog::info("Sent READY signal to systemd");
    }

    // 5. 나머지 초기화 (백그라운드)
    initializeMonitoring();
    initializeMetrics();
}
```

**핵심**: 공유 메모리 생성 **직후** READY 신호를 보내야 Non-RT가 안전하게 시작됩니다.

#### 3. Non-RT 프로세스에서 재시도 로직 추가

**코드 예시** (`src/core/nonrt/NonRTExecutive.cpp`):

```cpp
bool NonRTExecutive::connectSharedMemory() {
    const int MAX_RETRIES = 50;          // 5초 (100ms × 50)
    const int RETRY_INTERVAL_MS = 100;

    for (int i = 0; i < MAX_RETRIES; ++i) {
        if (tryOpenSharedMemory()) {
            spdlog::info("Connected to shared memory on attempt {}", i + 1);
            return true;
        }

        if (i < MAX_RETRIES - 1) {
            spdlog::debug("Shared memory not ready, retrying in {} ms...", RETRY_INTERVAL_MS);
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_INTERVAL_MS));
        }
    }

    spdlog::error("Failed to connect to shared memory after {} attempts", MAX_RETRIES);
    return false;
}
```

**핵심**: 최대 5초 동안 100ms 간격으로 재시도합니다.

### 검증 방법

#### 1. 서비스 파일 적용

```bash
# 서비스 파일 복사
sudo cp systemd/mxrc-rt.service /etc/systemd/system/
sudo cp systemd/mxrc-nonrt.service /etc/systemd/system/

# systemd 리로드
sudo systemctl daemon-reload

# 서비스 활성화
sudo systemctl enable mxrc-rt.service
sudo systemctl enable mxrc-nonrt.service
```

#### 2. 시작 순서 검증 (10회 재시작)

```bash
# 테스트 스크립트 실행
for i in {1..10}; do
    echo "=== Test $i/10 ==="

    # 서비스 재시작
    sudo systemctl restart mxrc-rt.service
    sudo systemctl restart mxrc-nonrt.service

    # 상태 확인
    sleep 2
    sudo systemctl status mxrc-rt.service | grep "Active:"
    sudo systemctl status mxrc-nonrt.service | grep "Active:"

    # 공유 메모리 확인
    ls -lh /dev/shm/mxrc_*

    echo ""
done
```

**기대 결과**:
- 10회 모두 성공 (Active: active (running))
- 공유 메모리 파일 존재 확인

#### 3. 로그 확인

```bash
# RT 프로세스 로그
sudo journalctl -u mxrc-rt.service -n 50 --no-pager

# Non-RT 프로세스 로그
sudo journalctl -u mxrc-nonrt.service -n 50 --no-pager

# READY 신호 확인
sudo journalctl -u mxrc-rt.service | grep "READY=1"
```

**기대 출력**:
```
Jan 22 10:15:30 mxrc-rt[12345]: Shared memory created successfully
Jan 22 10:15:30 mxrc-rt[12345]: Sent READY signal to systemd
Jan 22 10:15:31 mxrc-nonrt[12346]: Connected to shared memory on attempt 1
```

---

## P2: DataStore Accessor 패턴 사용

### 문제 상황

DataStore에 직접 접근하면 잘못된 키 사용 시 런타임 오류가 발생합니다.

```cpp
// ❌ 나쁜 예: 직접 접근
double temp = datastore->get<double>("sensor.temperature");
double pos = datastore->get<double>("robot_state.position");  // 타입 불일치 (Vector3d)
```

### 해결 방법

#### 1. Accessor 인터페이스 사용

**코드 예시** (`src/core/datastore/interfaces/ISensorDataAccessor.h`):

```cpp
class ISensorDataAccessor : public IDataAccessor {
public:
    virtual ~ISensorDataAccessor() = default;

    // 읽기 메서드 (const)
    virtual VersionedData<double> getTemperature() const = 0;
    virtual VersionedData<double> getPressure() const = 0;

    // 쓰기 메서드 (Non-RT 전용)
    virtual void setTemperature(double value) = 0;
    virtual void setPressure(double value) = 0;
};
```

#### 2. Accessor 생성 및 사용

**초기화** (`src/core/datastore/managers/DataStoreManager.cpp`):

```cpp
#include "datastore/impl/SensorDataAccessor.h"
#include "datastore/impl/RobotStateAccessor.h"

class DataStoreManager {
private:
    std::unique_ptr<IDataStore> datastore_;
    std::unique_ptr<ISensorDataAccessor> sensorAccessor_;
    std::unique_ptr<IRobotStateAccessor> robotAccessor_;

public:
    void initialize() {
        // DataStore 생성
        datastore_ = std::make_unique<DataStore>();

        // Accessor 생성 (참조로 DataStore 전달)
        sensorAccessor_ = std::make_unique<SensorDataAccessor>(*datastore_);
        robotAccessor_ = std::make_unique<RobotStateAccessor>(*datastore_);
    }

    ISensorDataAccessor& getSensorAccessor() { return *sensorAccessor_; }
    IRobotStateAccessor& getRobotAccessor() { return *robotAccessor_; }
};
```

**사용 예시 (RT 경로)** (`src/core/rt/ControlLoop.cpp`):

```cpp
void ControlLoop::execute() {
    // ✅ 좋은 예: Accessor 사용
    auto temp = sensorAccessor_->getTemperature();
    double temp_value = temp.value;  // 최신 값 바로 사용

    // 제어 로직
    if (temp_value > 80.0) {
        emergencyStop();
    }

    // 로봇 상태 업데이트 (RT 경로 쓰기)
    Eigen::Vector3d new_pos = calculateNewPosition();
    robotAccessor_->setPosition(new_pos);
}
```

**사용 예시 (Non-RT 경로, 버전 일관성 체크)** (`src/core/nonrt/Monitor.cpp`):

```cpp
void Monitor::readConsistentSnapshot() {
    const int MAX_RETRIES = 3;

    for (int i = 0; i < MAX_RETRIES; ++i) {
        // 일관된 스냅샷 읽기 시도
        auto temp1 = sensorAccessor_->getTemperature();
        auto pressure = sensorAccessor_->getPressure();
        auto temp2 = sensorAccessor_->getTemperature();  // 재확인

        if (temp1.isConsistentWith(temp2)) {
            // ✅ 일관성 확인됨
            spdlog::info("Consistent snapshot: temp={}, pressure={}",
                         temp1.value, pressure.value);
            processData(temp1.value, pressure.value);
            return;
        }

        // ⚠️ 버전 불일치 → 재시도
        spdlog::warn("Version mismatch detected (attempt {}/{})", i+1, MAX_RETRIES);
        metrics_.versionMismatchCount++;
    }

    // 최대 재시도 초과 → 최신 버전 강제 사용
    spdlog::error("Version mismatch persists, using latest version");
    auto latest = sensorAccessor_->getTemperature();
    processData(latest.value, latest.value);
}
```

### 검증 방법

#### 1. 단위 테스트

```cpp
TEST(SensorAccessorTest, TemperatureReadWrite) {
    auto datastore = std::make_unique<DataStore>();
    auto accessor = std::make_unique<SensorDataAccessor>(*datastore);

    // 쓰기
    accessor->setTemperature(25.5);

    // 읽기
    auto temp = accessor->getTemperature();

    EXPECT_DOUBLE_EQ(temp.value, 25.5);
    EXPECT_GT(temp.version, 0);
    EXPECT_GT(temp.timestamp_ns, 0);
}

TEST(SensorAccessorTest, VersionIncrement) {
    auto datastore = std::make_unique<DataStore>();
    auto accessor = std::make_unique<SensorDataAccessor>(*datastore);

    accessor->setTemperature(25.5);
    auto temp1 = accessor->getTemperature();

    accessor->setTemperature(26.0);
    auto temp2 = accessor->getTemperature();

    // 버전 증가 확인
    EXPECT_EQ(temp2.version, temp1.version + 1);
    EXPECT_TRUE(temp2.isNewerThan(temp1));
}
```

#### 2. 성능 벤치마크

```bash
# 벤치마크 실행
./build/tests/unit/datastore/accessor_benchmark

# 예상 출력:
# VersionedData read latency: 58 ns (목표: < 60ns)
# Accessor getTemperature() latency: 62 ns (목표: < 70ns)
# Version check overhead: 5 ns (목표: < 10ns)
```

---

## P3: EventBus 우선순위 큐 사용

### 문제 상황

EventBus 큐가 가득 차면 중요한 이벤트(오류, 상태 변경)도 버려집니다.

```cpp
// ❌ 현재: 우선순위 없음
eventBus->publish("sensor.fault", error_code);  // 버려질 수 있음
eventBus->publish("metrics.update", metrics);   // 동일한 우선순위
```

### 해결 방법

#### 1. 우선순위 이벤트 생성

**코드 예시** (`src/core/rt/ControlLoop.cpp`):

```cpp
#include "event/core/EventPriority.h"
#include "event/core/PrioritizedEvent.h"

void ControlLoop::handleSensorFault(int error_code) {
    // CRITICAL 이벤트 생성 (RT 경로)
    PrioritizedEvent event{
        .type = "sensor.fault",
        .priority = EventPriority::CRITICAL,
        .payload = error_code,
        .timestamp_ns = getCurrentTimeNs()
    };

    // Non-blocking push (RT 안전)
    bool pushed = eventBus_->push(std::move(event));

    if (!pushed) {
        // CRITICAL 이벤트가 버려진 경우 (매우 드묾)
        spdlog::critical("Failed to push CRITICAL event (queue full)");
    }
}
```

**코드 예시** (`src/core/nonrt/MetricsCollector.cpp`):

```cpp
void MetricsCollector::publishMetrics() {
    // DEBUG 이벤트 생성 (Non-RT 경로)
    PrioritizedEvent event{
        .type = "metrics.update",
        .priority = EventPriority::DEBUG,
        .payload = metrics_data_,
        .timestamp_ns = getCurrentTimeNs()
    };

    // 큐 80% 이상 차면 버려짐 (정상 동작)
    bool pushed = eventBus_->push(std::move(event));

    if (!pushed) {
        spdlog::debug("Metrics event dropped (queue backpressure)");
        metrics_.debugEventsDropped++;
    }
}
```

#### 2. 우선순위별 처리

**코드 예시** (`src/core/event/adapters/EventBus.cpp`):

```cpp
void EventBus::processEvents() {
    while (running_) {
        // 우선순위 순서로 pop (CRITICAL → NORMAL → DEBUG)
        auto event = priorityQueue_->pop();

        if (!event.has_value()) {
            // 큐가 비어 있음
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }

        // 이벤트 처리
        dispatchEvent(*event);

        // 지연 시간 측정
        uint64_t latency_ns = getCurrentTimeNs() - event->timestamp_ns;
        if (latency_ns > 1'000'000) {  // 1ms 이상
            spdlog::warn("Event latency: {} μs (type: {})",
                         latency_ns / 1000, event->type);
        }
    }
}
```

### 검증 방법

#### 1. 우선순위 큐 테스트

```cpp
TEST(PriorityQueueTest, CriticalEventFirst) {
    PriorityQueue queue;

    // NORMAL 이벤트 먼저 push
    queue.push(PrioritizedEvent{.priority = EventPriority::NORMAL});

    // CRITICAL 이벤트 나중에 push
    queue.push(PrioritizedEvent{.priority = EventPriority::CRITICAL});

    // CRITICAL이 먼저 pop되어야 함
    auto event1 = queue.pop();
    ASSERT_TRUE(event1.has_value());
    EXPECT_EQ(event1->priority, EventPriority::CRITICAL);

    auto event2 = queue.pop();
    ASSERT_TRUE(event2.has_value());
    EXPECT_EQ(event2->priority, EventPriority::NORMAL);
}
```

#### 2. 백프레셔 테스트

```cpp
TEST(PriorityQueueTest, DebugEventDrop) {
    PriorityQueue queue;

    // 큐를 80% 이상 채움 (3276개 이벤트)
    for (int i = 0; i < 3300; ++i) {
        queue.push(PrioritizedEvent{.priority = EventPriority::NORMAL});
    }

    // DEBUG 이벤트 push 시도
    PrioritizedEvent debug_event{.priority = EventPriority::DEBUG};
    bool pushed = queue.push(std::move(debug_event));

    EXPECT_FALSE(pushed);  // DEBUG는 버려져야 함
}

TEST(PriorityQueueTest, CriticalEventNeverDrop) {
    PriorityQueue queue;

    // 큐를 100% 채움
    fillQueueToCapacity();

    // CRITICAL 이벤트는 항상 삽입 시도
    PrioritizedEvent critical_event{.priority = EventPriority::CRITICAL};
    bool pushed = queue.push(std::move(critical_event));

    EXPECT_TRUE(pushed);  // CRITICAL은 삽입되어야 함
}
```

#### 3. 이벤트 폭주 시나리오 테스트

```bash
# 통합 테스트 실행
./build/tests/integration/event/event_storm_test

# 예상 출력:
# Pushing 100,000 events (50% CRITICAL, 30% NORMAL, 20% DEBUG)...
# CRITICAL events dropped: 0 (0%)
# NORMAL events dropped: 523 (0.52%)
# DEBUG events dropped: 1247 (1.25%)
# Total latency (avg): 245 μs
```

---

## P4: Watchdog 하트비트 설정

### 문제 상황

RT 프로세스가 멈춰도 systemd가 감지하지 못해 수동 재시작이 필요합니다.

### 해결 방법

#### 1. Watchdog 하트비트 전송

**코드 예시** (`src/core/systemd/impl/WatchdogNotifier.cpp`):

```cpp
#include <systemd/sd-daemon.h>

class WatchdogNotifier {
private:
    std::atomic<bool> running_{false};
    std::thread watchdog_thread_;
    const std::chrono::seconds WATCHDOG_INTERVAL{10};  // 30초 / 3

public:
    void start() {
        running_ = true;
        watchdog_thread_ = std::thread([this]() {
            spdlog::info("Watchdog notifier started (interval: {} s)", WATCHDOG_INTERVAL.count());

            while (running_) {
                // 하트비트 전송
                if (sd_notify(0, "WATCHDOG=1") > 0) {
                    spdlog::debug("Sent WATCHDOG=1 heartbeat");
                } else {
                    spdlog::warn("Failed to send WATCHDOG heartbeat");
                }

                std::this_thread::sleep_for(WATCHDOG_INTERVAL);
            }
        });
    }

    void stop() {
        running_ = false;
        if (watchdog_thread_.joinable()) {
            watchdog_thread_.join();
        }
        spdlog::info("Watchdog notifier stopped");
    }
};
```

#### 2. IPC 채널 장애 감지

**코드 예시** (`src/core/rt/RTExecutive.cpp`):

```cpp
void RTExecutive::monitorIPCHealth() {
    while (running_) {
        if (!ipc_channel_->isHealthy()) {
            spdlog::critical("IPC channel failure detected");

            // Watchdog 하트비트 중단 (타임아웃 유도)
            watchdog_notifier_->stop();

            // systemd가 30초 후 자동 재시작
            spdlog::info("Waiting for systemd to restart process...");
            return;
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}
```

### 검증 방법

#### 1. Watchdog 설정 확인

```bash
# systemd 서비스 파일에 WatchdogSec 설정 확인
grep "WatchdogSec" /etc/systemd/system/mxrc-rt.service

# 예상 출력:
# WatchdogSec=30s
```

#### 2. 하트비트 전송 확인

```bash
# 로그에서 WATCHDOG=1 신호 확인
sudo journalctl -u mxrc-rt.service -f | grep "WATCHDOG"

# 예상 출력 (10초마다):
# Jan 22 10:20:10 mxrc-rt[12345]: Sent WATCHDOG=1 heartbeat
# Jan 22 10:20:20 mxrc-rt[12345]: Sent WATCHDOG=1 heartbeat
# Jan 22 10:20:30 mxrc-rt[12345]: Sent WATCHDOG=1 heartbeat
```

#### 3. 타임아웃 시뮬레이션 (IPC 장애)

```bash
# RT 프로세스 PID 확인
RT_PID=$(systemctl show -p MainPID mxrc-rt.service | cut -d'=' -f2)

# SIGUSR1 신호 전송 (IPC 장애 시뮬레이션)
sudo kill -SIGUSR1 $RT_PID

# 30초 대기 후 재시작 확인
sleep 35
sudo systemctl status mxrc-rt.service

# 예상 출력:
# Active: active (running) since ... (재시작됨)
```

---

## 검증 절차

### 전체 시스템 검증

#### 1. 빌드 및 테스트

```bash
# 빌드
cd /home/tory/workspace/mxrc/mxrc
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# 단위 테스트
ctest --output-on-failure

# 통합 테스트
./tests/integration/systemd/startup_order_test
./tests/integration/event/event_storm_test
```

#### 2. 서비스 배포

```bash
# 서비스 파일 복사
sudo cp systemd/mxrc-rt.service /etc/systemd/system/
sudo cp systemd/mxrc-nonrt.service /etc/systemd/system/

# systemd 리로드
sudo systemctl daemon-reload

# 서비스 재시작
sudo systemctl restart mxrc-rt.service
sudo systemctl restart mxrc-nonrt.service

# 상태 확인
sudo systemctl status mxrc-rt.service
sudo systemctl status mxrc-nonrt.service
```

#### 3. 메트릭 확인 (Prometheus)

```bash
# Prometheus 메트릭 조회
curl http://localhost:9090/metrics | grep mxrc

# 예상 메트릭:
# mxrc_startup_failures 0
# mxrc_version_mismatch_count 5
# mxrc_events_dropped{priority="DEBUG"} 123
# mxrc_events_dropped{priority="NORMAL"} 2
# mxrc_events_dropped{priority="CRITICAL"} 0
# mxrc_watchdog_heartbeats 360
```

---

## 문제 해결

### P1: systemd 시작 순서 문제

#### 증상

```
Jan 22 10:15:30 mxrc-nonrt[12346]: Failed to connect to shared memory after 50 attempts
```

#### 원인 분석

1. RT 서비스가 READY 신호를 보내지 않음
2. RT 서비스가 Non-RT보다 늦게 시작됨

#### 해결 방법

```bash
# 1. RT 서비스 로그 확인 (READY 신호 전송 여부)
sudo journalctl -u mxrc-rt.service | grep "READY=1"

# 2. 시작 순서 확인
sudo systemctl list-dependencies mxrc-nonrt.service

# 3. Before/After 지시자 확인
systemctl show mxrc-rt.service | grep Before
systemctl show mxrc-nonrt.service | grep After
```

### P2: 버전 불일치 빈번 발생

#### 증상

```
WARN Version mismatch detected (attempt 3/3)
ERROR Version mismatch persists, using latest version
```

#### 원인 분석

RT 경로에서 빈번한 쓰기가 발생하여 Non-RT 읽기 중 버전 변경

#### 해결 방법

```cpp
// 재시도 횟수 증가
const int MAX_RETRIES = 5;  // 3 → 5

// 또는 일관성 체크 생략 (최신 값 바로 사용)
auto temp = sensorAccessor_->getTemperature();
processData(temp.value);  // 버전 체크 없이 바로 사용
```

### P3: 이벤트 손실 발생

#### 증상

```
CRITICAL events dropped: 3 (0.03%)
```

#### 원인 분석

큐 크기 부족 (4096 entries)

#### 해결 방법

```cpp
// PriorityQueue.h
const size_t QUEUE_CAPACITY = 8192;  // 4096 → 8192
```

```bash
# 재빌드
make -j$(nproc)

# 메트릭 재확인
curl http://localhost:9090/metrics | grep events_dropped
```

### P4: Watchdog 타임아웃 빈번 발생

#### 증상

```
systemd[1]: mxrc-rt.service: Watchdog timeout (after 30s)
systemd[1]: mxrc-rt.service: Killing process 12345 (SIGKILL)
```

#### 원인 분석

하트비트 전송 스레드가 차단됨 (CPU 부하 과다)

#### 해결 방법

```cpp
// 하트비트 주기 단축
const std::chrono::seconds WATCHDOG_INTERVAL{5};  // 10 → 5

// 또는 WatchdogSec 증가
// systemd/mxrc-rt.service
WatchdogSec=60s  // 30 → 60
```

---

## 성능 검증

### 지연 시간 측정

```bash
# RT 사이클 지터 측정
./build/tools/rt_latency_test

# 예상 출력:
# RT cycle jitter: 8.2 μs (목표: < 10μs)
```

### 메모리 사용량 측정

```bash
# 공유 메모리 크기 확인
ls -lh /dev/shm/mxrc_*

# 예상 출력:
# -rw-r--r-- 1 root root 16K Jan 22 10:15 /dev/shm/mxrc_datastore
```

### CPU 사용률 측정

```bash
# RT 프로세스 CPU 사용률
top -p $(pgrep mxrc-rt)

# 예상 출력:
# PID   CPU%  MEM%
# 12345 25.0  1.2
```

---

## 다음 단계

1. **P1 배포**: systemd 설정 변경 및 검증 (즉시 가능)
2. **P2 마이그레이션**: Accessor 패턴 점진적 도입 (Week 1-4)
3. **P3 통합**: EventBus 우선순위 큐 교체 (Week 5-8)
4. **P4 모니터링**: Watchdog 메트릭 수집 및 알림 설정

---

## 관련 문서

- [plan.md](plan.md) - Phase 0 연구 및 Phase 1 설계
- [data-model.md](data-model.md) - VersionedData 및 Accessor 정의
- [contracts/data-contracts.md](contracts/data-contracts.md) - DataStore 스키마
- [tasks.md](tasks.md) - Phase 2 구현 작업 목록 (/speckit.tasks 실행 필요)

---

**작성일**: 2025-01-22
**Phase**: 1 (Design)
**다음 단계**: /speckit.tasks 실행하여 tasks.md 생성
