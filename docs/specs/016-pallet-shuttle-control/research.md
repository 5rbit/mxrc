# Research Report: Behavior Arbitration 알고리즘 설계

**Feature**: 016-pallet-shuttle-control
**Research Phase**: Phase 0
**Date**: 2025-11-25
**Status**: Completed

---

## 조사 개요

본 연구는 팔렛 셔틀 로봇의 우선순위 기반 행동 선택(Behavior Arbitration)을 위한 구체적인 알고리즘 설계를 목표로 합니다. 특히 다음 항목들을 중점적으로 조사했습니다:

1. Priority Queue 구현 방식 비교 (TBB vs Custom)
2. 동일 우선순위 행동의 부가 규칙 처리
3. 제어 모드 전환 상태 머신 설계
4. 행동 전환 시 현재 작업 처리 전략
5. BehaviorArbiter 알고리즘 최종 설계

---

## 1. Priority Queue 구현 방식 비교

### 1.1 TBB concurrent_priority_queue 분석

**장점**:
- 표준화된 API와 검증된 구현
- Multi-producer, Multi-consumer (MPMC) 지원
- 메모리 안전성 보장

**단점 및 제약사항**:
- **실시간 제약 부적합**: TBB scheduler는 hard real-time 시스템에 적합하지 않음 ([Using Task Priorities](https://link.springer.com/chapter/10.1007/978-1-4842-4398-5_14))
- **낮은 동시성**: mutual exclusion과 blocking 사용으로 실제로는 "not all that concurrent" ([1024cores - Priority Queues](https://www.1024cores.net/home/lock-free-algorithms/queues/priority-queues))
- **스케일링 한계**: 스레드 수 증가 시 처리량 감소, heap 구조의 aggregator가 모든 연산을 직렬화 ([Priority Queues Are Not Good Concurrent Priority Schedulers](https://link.springer.com/chapter/10.1007/978-3-662-48096-0_17))
- **성능 제한**: 24 코어 환경에서 약 3배 속도 향상에 그침, custom scheduler(OBIM)는 거의 완벽한 속도 향상 달성

### 1.2 현재 MXRC 코드베이스 분석

**기존 PriorityQueue 구현** (`/home/tory/workspace/mxrc/mxrc/src/core/event/core/PriorityQueue.h`):

```cpp
// Feature 022 P3: 4-level priority queue for EventBus
class PriorityQueue {
    // CRITICAL > HIGH > NORMAL > LOW
    // Backpressure: Drop lower-priority events when full
    // Thread-safe: std::mutex (non-RT path)
    // Bounded capacity: 4096 events (configurable)
private:
    std::priority_queue<PrioritizedEvent> queue_;
    mutable std::mutex mutex_;
    std::atomic<size_t> size_{0};
};
```

**특징**:
- Lock-based 설계 (std::mutex)
- Non-RT 경로 전용
- Backpressure policy (우선순위별 Drop 정책)
- MPSC (Multiple Producer, Single Consumer) 패턴

### 1.3 BehaviorArbiter를 위한 권장 구현

**선택**: **Custom Priority Queue with Quantized Priorities**

**근거**:
1. **제한된 우선순위 레벨**: 5단계 우선순위(비상 정지 > 안전 이슈 > 긴급 작업 > 일반 작업 > 정기 점검)로 충분
2. **실시간 성능 요구**: Critical 행동(비상 정지)은 100ms 이내 처리 필요
3. **단순성 및 예측 가능성**: Quantized priority별로 독립적인 큐 사용 시 lock contention 최소화
4. **기존 MXRC 패턴 활용**: EventBus PriorityQueue 구조를 참조하되, Behavior 전용으로 최적화
5. **배터리 충전 제외**: 상위 시스템이 충전을 Task로 할당하므로 별도 우선순위 불필요

**권장 설계**:
```cpp
// 5개의 독립적인 concurrent_queue (TBB 또는 std::queue + mutex)
struct BehaviorPriorityQueue {
    tbb::concurrent_queue<BehaviorRequest> emergency_stop_;     // Priority 0
    tbb::concurrent_queue<BehaviorRequest> safety_issue_;       // Priority 1
    tbb::concurrent_queue<BehaviorRequest> urgent_task_;        // Priority 2 (충전 작업 포함)
    tbb::concurrent_queue<BehaviorRequest> normal_task_;        // Priority 3
    tbb::concurrent_queue<BehaviorRequest> maintenance_;        // Priority 4
};
```

**장점**:
- O(1) enqueue/dequeue (우선순위 레벨당)
- Lock-free 가능 (TBB concurrent_queue 사용 시)
- 예측 가능한 실시간 성능
- 간단한 구현 및 유지보수

---

## 2. 우선순위 규칙 및 처리 방법

### 2.1 5단계 우선순위 구조

**설계 원칙**: BehaviorArbiter는 **우선순위 기반 선행 처리**만 담당합니다. 어떤 Task가 긴급한지는 상위 시스템(Fleet Manager/Mission Planner)이 결정하여 우선순위를 할당합니다.

| 우선순위 | 레벨 | 행동 유형 | 처리 전략 | 예시 |
|---------|------|----------|-----------|------|
| 0 | EMERGENCY_STOP | 비상 정지 | 즉시 중단 | 비상 정지 버튼, 안전 펜스 트리거 |
| 1 | SAFETY_ISSUE | 안전 이슈 | 즉시 중단 | 장애물 감지, 센서 고장 |
| 2 | URGENT_TASK | 긴급 작업 | 일시 중단 후 선행 처리 | 긴급 팔렛 운반, 충전, 긴급 회수 등 (상위 결정) |
| 3 | NORMAL_TASK | 일반 작업 | 순차 처리 | 일반 팔렛 운반 작업 |
| 4 | MAINTENANCE | 정기 점검 | 선점 가능 (Preemptable) | 주기적 상태 점검 |

**우선순위 처리 원칙**:
- URGENT_TASK가 들어오면 **현재 NORMAL_TASK를 일시 중단(Suspend)**하고 선행 처리
- 긴급 작업 완료 후 중단된 작업을 **재개(Resume)**
- 어떤 Task가 긴급한지는 **상위 시스템 판단** (배터리, 고객 요청, 시간 제약 등)
- BehaviorArbiter는 우선순위만 보고 처리 (Task 내용 불문)

### 2.2 우선순위 처리 알고리즘

```cpp
BehaviorRequest* BehaviorArbiter::selectNextBehavior() {
    // 우선순위 순서대로 큐 검색 (높은 순 -> 낮은 순)
    if (auto req = tryPop(emergency_stop_)) return req;
    if (auto req = tryPop(safety_issue_)) return req;
    if (auto req = tryPop(urgent_task_)) return req;
    if (auto req = tryPop(normal_task_)) return req;
    if (auto req = tryPop(maintenance_)) return req;

    return nullptr;  // 대기 중인 행동 없음
}
```

**특징**:
- Strict priority ordering: 높은 우선순위가 항상 먼저 처리
- Starvation 방지: 낮은 우선순위 작업은 높은 우선순위 작업 완료 후 처리
- Preemption 지원: Priority 0-2는 현재 작업 중단 가능, Priority 3-5는 완료 후 전환

---

## 3. 동일 우선순위 처리 (부가 규칙)

### 3.1 부가 규칙 적용 방법

동일한 우선순위 레벨 내에서 여러 행동 요청이 경쟁할 때 다음 부가 규칙을 적용합니다:

**Rule 1: FIFO (First-In, First-Out)** - 기본 규칙
- 먼저 요청된 작업을 우선 처리
- `concurrent_queue`의 기본 동작이 FIFO 보장
- 구현 간단, 공정성 보장

**Rule 2: Distance-based (선택적 적용)**
- 로봇과 목표 위치의 거리 기반 정렬
- 가까운 작업을 우선 처리하여 효율성 향상
- Priority 3-4 (긴급/일반 작업)에만 적용

**Rule 3: Resource Availability (선택적 적용)**
- 현재 리소스 가용성 기반 (배터리, 센서 상태)
- 실행 가능한 작업을 우선 처리
- Priority 5 (정기 점검)에 적용

### 3.2 권장 구현 전략

**Phase 1 (초기 구현)**: FIFO만 사용
- 간단하고 예측 가능
- `concurrent_queue`의 기본 동작 활용
- 복잡도 최소화

**Phase 2 (향후 개선)**: Distance-based 추가
- 작업 큐에 삽입 시 거리 계산
- Priority 3-4 큐를 `priority_queue`로 변경 (거리 기반 정렬)
- 설정 파일로 활성화/비활성화 가능

```cpp
struct BehaviorRequest {
    std::string id;
    BehaviorType type;
    uint64_t timestamp_ns;  // FIFO 정렬용
    float distance_m;       // Distance-based 정렬용 (선택적)

    // FIFO 비교 연산자
    bool operator<(const BehaviorRequest& other) const {
        return timestamp_ns > other.timestamp_ns;  // 먼저 요청된 것이 높은 우선순위
    }
};
```

---

## 4. 제어 모드 상태 머신 설계

### 4.1 제어 모드 정의

기존 MXRC 코드베이스의 상태 머신 구현을 참조하여 설계합니다:

**참조 1**: HA State Machine (`/home/tory/workspace/mxrc/mxrc/src/core/ha/HAStateMachine.h`)
```cpp
enum class HAState {
    NORMAL, DEGRADED, SAFE_MODE,
    RECOVERY_IN_PROGRESS, MANUAL_INTERVENTION, SHUTDOWN
};
```

**참조 2**: RT State Machine (`/home/tory/workspace/mxrc/mxrc/src/core/rt/RTStateMachine.h`)
```cpp
enum class RTState : uint8_t {
    INIT, READY, RUNNING, PAUSED,
    SAFE_MODE, ERROR, SHUTDOWN
};
```

**BehaviorArbiter 제어 모드** (9가지):
```cpp
enum class ControlMode : uint8_t {
    BOOT,         // 부팅 모드 (시스템 초기화 중, 하드웨어 체크)
    INIT,         // 초기화 모드 (드라이버 로딩, DataStore 연결)
    STANDBY,      // 대기 모드 (초기화 완료, 작업 대기 중)
    MANUAL,       // 수동 모드 (운영자 직접 제어)
    READY,        // 준비 모드 (자동 모드 전환 가능, 안전 점검 완료)
    AUTO,         // 자동 모드 (일반/긴급 작업 자동 수행)
    FAULT,        // 고장 모드 (Critical Alarm, 비상 정지)
    MAINT,        // 정비 모드 (주기적 점검, 예방 정비)
    CHARGING      // 충전 모드 (충전소 이동 또는 충전 중)
};
```

### 4.2 상태 전환 다이어그램

```
     ┌──────┐
     │ BOOT │ ◄─── 시스템 시작
     └──────┘
         │ HW Check OK
         ▼
     ┌──────┐
     │ INIT │ (드라이버 로딩)
     └──────┘
         │ Init Complete
         ▼
   ┌─────────┐
   │STANDBY  │ ◄──────────────┐
   └─────────┘                │
    │    │                    │
    │    └─► Ready Check      │
    │            │            │
    │            ▼            │
    │      ┌────────┐         │
    │      │ READY  │         │
    │      └────────┘         │
    │         │   │           │
Manual  Auto Mode │           │
Request      │   │           │
    │         │   └─► Task    │
    │         ▼       Assign  │
    │     ┌──────┐      │     │
    │     │ AUTO │◄─────┘     │
    │     └──────┘            │
    │      │  │  │            │
    │  Task  Battery  Critical│
    │  Done  Low     Alarm    │
    │      │  │      │        │
    ▼      ▼  │      │        │
┌────────┐   │      │        │
│ MANUAL │   │      │        │
└────────┘   │      │        │
    │        │      │        │
    │        ▼      ▼        │
    │   ┌──────┐┌───────┐   │
    │   │CHARGE││ FAULT │   │
    │   └──────┘└───────┘   │
    │        │      │        │
    │   Charged  Resolved    │
    │        │      │        │
    │        └──────┴────────┘
    │               │
    │ Maint         │
    │ Schedule      │
    ▼               ▼
┌───────┐      ┌─────────┐
│ MAINT │◄─────│STANDBY  │
└───────┘      └─────────┘
    │
Completed
    │
    └──────────► STANDBY
```

### 4.3 상태 전환 규칙

| From | To | Trigger | Condition | Action |
|------|----|---------|-----------| -------|
| BOOT | INIT | HW Check 완료 | 하드웨어 정상 | 드라이버 초기화 시작 |
| INIT | STANDBY | 초기화 완료 | 모든 드라이버 로드 완료 | 작업 대기 상태 진입 |
| STANDBY | READY | Ready Check | 안전 센서 정상 & 로봇 상태 정상 | 자동 모드 전환 가능 |
| STANDBY | MANUAL | 수동 모드 요청 | 운영자 명령 | 수동 제어 모드 진입 |
| STANDBY | MAINT | 정기 점검 주기 | 스케줄 도래 | 점검 Sequence 실행 |
| READY | AUTO | 작업 할당 | 작업 큐에 Task 존재 | 자동 작업 수행 시작 |
| READY | STANDBY | Ready 해제 | 센서 이상 또는 대기 타임아웃 | 대기 모드로 복귀 |
| AUTO | STANDBY | 작업 완료 | 작업 큐 비어있음 | 다음 작업 대기 |
| AUTO | FAULT | Critical Alarm | 비상 정지, 안전 센서 트리거 | 즉시 모든 동작 중단 |
| MANUAL | STANDBY | 수동 모드 종료 | 운영자 명령 | 자동 모드 전환 가능 대기 |

**Note**: CHARGING 모드 전환은 상위 시스템이 `ChargingTask`를 URGENT_TASK 우선순위로 할당하여 처리합니다. AUTO 상태에서 충전 Task를 받으면 일반 작업 흐름으로 실행됩니다.
| FAULT | STANDBY | 고장 해제 | 운영자 확인 & 센서 정상 | 대기 상태 복귀 |
| CHARGING | STANDBY | ChargingTask 완료 | Task 완료 이벤트 수신 | 작업 큐 재개 가능 |
| MAINT | STANDBY | 점검 완료 | Sequence 성공 | 작업 큐 재개 가능 |

### 4.4 모드별 허용 행동

| Mode | 허용 행동 | 금지 행동 | 특이사항 |
|------|----------|-----------|---------|
| BOOT | 하드웨어 자가 진단, 펌웨어 체크 | 모든 작업 수행 | 시스템 시작 시 자동 진입 |
| INIT | 드라이버 로딩, DataStore 연결 | 작업 수행, 수동 제어 | BOOT 성공 후 자동 진입 |
| STANDBY | 상태 모니터링, 작업 대기 | 작업 수행 | 모든 정상 작업의 시작/복귀점 |
| MANUAL | 운영자 직접 제어, 조그 이동 | 자동 작업 수행 | 디버깅 및 수동 작업용 |
| READY | 안전 점검, 작업 할당 대기 | 작업 수행 | AUTO 전환 전 안전 확인 단계 |
| AUTO | 일반/긴급 작업 자동 수행 | 수동 제어 | 정상 자동 운영 모드 |
| FAULT | 정지 상태 유지, Alarm 발생 | 모든 모터 동작 | Critical Alarm 발생 시 |
| MAINT | 상태 점검 Sequence, 예방 정비 | 팔렛 운반 작업 | 주기적 점검용 |
| CHARGING | 충전소 이동, 충전 수행 | 팔렛 운반 작업 | ChargingTask 실행 중 |

### 4.5 구현 패턴 (기존 HAStateMachine 참조)

```cpp
class BehaviorArbiter {
public:
    using ModeTransitionCallback =
        std::function<void(ControlMode from, ControlMode to, BehaviorRequest* trigger)>;

    bool transitionTo(ControlMode newMode, BehaviorRequest* trigger);
    ControlMode getCurrentMode() const;

    // 상태 전환 검증 (HAStateMachine::isValidTransition 패턴)
    static bool isValidTransition(ControlMode from, ControlMode to);

private:
    std::atomic<ControlMode> currentMode_{ControlMode::BOOT};  // 시작 시 BOOT
    std::atomic<ControlMode> previousMode_{ControlMode::BOOT};
    ModeTransitionCallback modeTransitionCallback_;
};
```

### 4.6 모드별 우선순위 및 전환 정책

| Mode | 우선순위 | 선점 가능 여부 | 선점 가능한 모드 | 특징 |
|------|---------|-------------|---------------|------|
| BOOT | 최고 | 불가 | 없음 | 시스템 시작 시 필수 단계 |
| INIT | 최고 | 불가 | 없음 | 초기화 완료 필수 |
| FAULT | 최고 | 모든 모드 선점 | 모든 모드 | Critical Alarm 발생 시 즉시 전환 |
| STANDBY | 중립 | - | - | 모든 모드의 기본 복귀점 |
| MANUAL | 높음 | AUTO, READY 선점 가능 | AUTO, READY, STANDBY | 운영자 우선 |
| READY | 중간 | STANDBY만 선점 가능 | STANDBY | 안전 점검 단계 |
| AUTO | 중간 | FAULT만 선점 가능 | - | 정상 자동 운영 |
| CHARGING | 낮음 | FAULT만 선점 가능 | - | ChargingTask 실행 중 |
| MAINT | 최저 | 모든 모드에 선점됨 | - | 대기 시간에만 수행 |

---

## 5. 작업 처리 전략 (우선순위별)

### 5.1 작업 중단/완료 전략

기존 TaskExecutor의 pause/resume 기능 활용 (`/home/tory/workspace/mxrc/mxrc/src/core/task/interfaces/ITaskExecutor.h`):

```cpp
class ITaskExecutor {
    virtual void pause(const std::string& taskId) = 0;
    virtual void resume(const std::string& taskId) = 0;
    virtual void cancel(const std::string& taskId) = 0;
};
```

**현재 작업 처리 전략**:

| 우선순위 | 새로운 행동 | 현재 작업 처리 | 메서드 | 재개 여부 |
|---------|------------|-------------|--------|----------|
| 0 (Emergency) | 비상 정지 | **즉시 중단** | `cancel()` | 재개 불가 (작업 실패) |
| 1 (Safety) | 안전 이슈 | **즉시 중단** | `cancel()` | 재개 불가 |
| 2 (Urgent) | 긴급 작업 (충전 포함) | **일시 중단** | `pause()` | 재개 가능 (`resume()`) |
| 3 (Normal) | 일반 작업 | **일시 중단** | `pause()` | 재개 가능 (`resume()`) |
| 4 (Maintenance) | 정기 점검 | **선점 불가** | - | 현재 작업 완료 후 실행 |

**Note**: 충전 작업(ChargingTask)은 상위 시스템이 URGENT_TASK 우선순위로 할당합니다. BehaviorArbiter는 배터리 모니터링을 하지 않습니다.

### 5.2 상세 처리 흐름

**Case 1: FAULT 모드 (Priority 0 - Emergency) - 즉시 중단**
```cpp
void BehaviorArbiter::handleEmergency(BehaviorRequest* req) {
    // 1. 현재 실행 중인 작업 취소
    if (currentTaskId_) {
        taskExecutor_->cancel(currentTaskId_);
        LOG_CRITICAL("Task {} cancelled due to emergency: {}",
                     currentTaskId_, req->id);
    }

    // 2. 모드 전환 (ANY → FAULT)
    transitionTo(ControlMode::FAULT, req);

    // 3. 비상 정지 Action 실행
    executeEmergencyStop();

    // 4. Alarm 발생 (DataStore 기록)
    alarmManager_->raise(AlarmType::EMERGENCY_STOP, req->reason);
}
```

**Case 2: Urgent/Normal Task (Priority 2-3) - 일시 중단 및 재개**
```cpp
void BehaviorArbiter::handleTaskPreemption(BehaviorRequest* urgentReq) {
    // 1. 현재 NORMAL_TASK 일시 중단
    if (currentTaskId_ && currentPriority_ == Priority::NORMAL_TASK) {
        taskExecutor_->pause(currentTaskId_);
        suspendedTasks_.push(currentTaskId_);  // 재개 큐에 추가
        LOG_INFO("Task {} suspended for urgent task {}",
                 currentTaskId_, urgentReq->id);
    }

    // 2. 긴급 작업 실행
    currentTaskId_ = urgentReq->id;
    currentPriority_ = Priority::URGENT_TASK;
    taskExecutor_->execute(urgentReq->taskDef, context_);

    // 3. 긴급 작업 완료 후 중단된 작업 재개
    // (TaskCompletedEvent 핸들러에서 처리)
}

void BehaviorArbiter::onTaskCompleted(const TaskCompletedEvent& event) {
    // 중단된 작업이 있으면 재개
    if (!suspendedTasks_.empty()) {
        auto taskId = suspendedTasks_.pop();
        taskExecutor_->resume(taskId);
        LOG_INFO("Resuming suspended task {}", taskId);
    }
}
```

### 5.3 작업 상태 관리

**TaskState Tracking** (기존 TaskExecutor 패턴 활용):
```cpp
struct BehaviorState {
    std::string currentTaskId;
    Priority currentPriority;
    ControlMode currentMode;

    // 중단된 작업 관리
    std::queue<std::string> suspendedTasks;  // 재개 대기 큐

    // 작업 실행 시간 추적
    std::chrono::steady_clock::time_point taskStartTime;

    // 참고: 배터리 수준, 고객 우선순위 등은 상위 시스템이 관리
    // BehaviorArbiter는 상위에서 할당된 Priority만 사용
};
```

---

## 6. BehaviorArbiter 알고리즘 최종 설계

### 6.1 핵심 알고리즘

```cpp
class BehaviorArbiter : public IBehaviorArbiter {
public:
    // 주기적으로 호출 (예: 100ms 주기)
    void tick() {
        // 1. 최고 우선순위 행동 선택
        auto* nextBehavior = selectNextBehavior();
        if (!nextBehavior) {
            return;  // 대기 중인 행동 없음
        }

        // 2. 우선순위 비교 및 선점 여부 결정
        if (shouldPreemptCurrentTask(nextBehavior)) {
            preemptCurrentTask(nextBehavior);
        } else {
            enqueueBehavior(nextBehavior);  // 현재 작업 완료 후 처리
        }
    }

private:
    bool shouldPreemptCurrentTask(BehaviorRequest* newBehavior) {
        // 현재 작업이 없으면 즉시 실행
        if (state_.currentTaskId.empty()) {
            return true;
        }

        // Priority 0-2: 즉시 선점
        if (newBehavior->priority <= Priority::URGENT_TASK) {
            return true;
        }

        // Priority 3-4: 낮은 우선순위만 선점
        if (newBehavior->priority < state_.currentPriority) {
            return true;
        }

        // 같거나 낮은 우선순위면 선점 불가
        return false;
    }

    void preemptCurrentTask(BehaviorRequest* newBehavior) {
        // 우선순위별 처리 전략 적용
        if (newBehavior->priority <= Priority::SAFETY_ISSUE) {
            handleEmergency(newBehavior);  // 즉시 중단 (재개 불가)
        } else if (newBehavior->priority == Priority::URGENT_TASK) {
            handleUrgentTask(newBehavior);  // 일시 중단 후 재개
        } else {
            handleTaskPreemption(newBehavior);  // 우선순위 기반 선점
        }
    }

    // 참고: 배터리 모니터링, 스케줄 관리 등은 상위 시스템 책임
    // BehaviorArbiter는 상위에서 할당된 Task와 Priority만 처리
};
```

### 6.2 인터페이스 설계

```cpp
// /home/tory/workspace/mxrc/mxrc/src/core/task/interfaces/IBehaviorArbiter.h
class IBehaviorArbiter {
public:
    virtual ~IBehaviorArbiter() = default;

    // 행동 요청 추가
    virtual bool requestBehavior(
        const std::string& behaviorId,
        Priority priority,
        const TaskDefinition& taskDef) = 0;

    // 주기적 tick (100ms 주기 권장)
    virtual void tick() = 0;

    // 상태 조회
    virtual ControlMode getCurrentMode() const = 0;
    virtual std::string getCurrentTaskId() const = 0;
    virtual Priority getCurrentPriority() const = 0;

    // 모드 전환 콜백 등록
    virtual void registerModeTransitionCallback(
        ModeTransitionCallback callback) = 0;
};
```

### 6.3 DataStore 통합

**상태 저장** (기존 HAStateMachine 패턴 참조):
```cpp
void BehaviorArbiter::transitionTo(ControlMode newMode, BehaviorRequest* trigger) {
    auto oldMode = state_.currentMode;

    // 1. 상태 전환 검증
    if (!isValidTransition(oldMode, newMode)) {
        LOG_ERROR("Invalid mode transition: {} -> {}",
                  toString(oldMode), toString(newMode));
        return;
    }

    // 2. DataStore 업데이트 (RT-safe)
    dataStore_->set("behavior.mode", static_cast<int>(newMode));
    dataStore_->set("behavior.mode_timestamp", getCurrentTimeNs());

    // 3. 상태 변경
    state_.previousMode = oldMode;
    state_.currentMode = newMode;

    // 4. 콜백 호출
    if (modeTransitionCallback_) {
        modeTransitionCallback_(oldMode, newMode, trigger);
    }

    // 5. EventBus 이벤트 발행 (Non-RT)
    publishModeChangedEvent(oldMode, newMode);

    LOG_INFO("Mode transition: {} -> {} (trigger: {})",
             toString(oldMode), toString(newMode), trigger->id);
}
```

### 6.4 성능 고려사항

**RT 안전성**:
- `tick()` 실행 시간 < 1ms 목표
- Priority 0-1 처리 시간 < 100ms (비상 정지)
- Lock-free 자료구조 사용 (가능한 경우)
- Memory allocation 최소화 (사전 할당된 버퍼 사용)

**메모리 사용**:
- BehaviorRequest 구조체: ~128 bytes
- 각 우선순위 큐: 최대 100개 요청 (12.8 KB)
- 총 메모리: ~80 KB (6개 큐 + overhead)

---

## 7. 기존 Task Layer 호환성 분석

### 7.1 현재 ITaskExecutor 인터페이스 평가

**지원 기능**:
- ✅ `execute()`: Task 실행
- ✅ `pause()`: 일시 중단
- ✅ `resume()`: 재개
- ✅ `cancel()`: 취소
- ✅ `getStatus()`: 상태 조회
- ✅ `getProgress()`: 진행률 조회

**결론**: 기존 ITaskExecutor는 BehaviorArbiter 요구사항을 **완전히 지원**합니다.

### 7.2 필요한 확장 사항

**1. Task 우선순위 지원 추가** (선택적):
```cpp
struct TaskDefinition {
    std::string id;
    TaskType type;
    // 새로 추가
    Priority priority{Priority::NORMAL_TASK};
};
```

**2. Task 일시 중단 시 콜백 지원** (선택적):
```cpp
class ITaskExecutor {
    // 새로 추가
    virtual void registerTaskPausedCallback(
        std::function<void(const std::string& taskId)> callback) = 0;
};
```

**3. EventBus 통합 강화**:
- TaskCompletedEvent, TaskPausedEvent, TaskResumedEvent 발행
- BehaviorArbiter가 이벤트 구독하여 작업 완료 감지

---

## 8. 설정 파일 구조 (참조: ha-policy.yaml)

### 8.1 behavior-arbitration.yaml

```yaml
# Behavior Arbitration Configuration
# Feature: 016-pallet-shuttle-control
# Version: 1.0.0
# Control Modes: Boot, Init, Standby, Manual, Ready, Auto, Fault, Maint, Charging

---
behavior_arbitration_version: "1.0.0"

# =============================================================================
# 우선순위 정의
# =============================================================================
priorities:
  EMERGENCY_STOP:
    level: 0
    description: "비상 정지 (즉시 중단)"
    preemption: IMMEDIATE_CANCEL
    examples:
      - emergency_button_pressed
      - safety_fence_triggered

  SAFETY_ISSUE:
    level: 1
    description: "안전 이슈 (즉시 중단)"
    preemption: IMMEDIATE_CANCEL
    examples:
      - obstacle_detected
      - sensor_failure

  URGENT_TASK:
    level: 2
    description: "긴급 작업 (일시 중단 후 선행 처리)"
    preemption: SUSPEND_RESUME
    examples:
      - urgent_customer_order      # 고객 긴급 주문
      - time_critical_transport    # 시간 제약 운송
      - charging_task              # 배터리 부족 시 충전
      - emergency_retrieval        # 긴급 회수
    note: "상위 시스템(Fleet Manager/Mission Planner)이 업무 우선순위에 따라 할당"

  NORMAL_TASK:
    level: 3
    description: "일반 작업 (일시 중단)"
    preemption: SUSPEND_RESUME
    examples:
      - normal_pallet_transport

  MAINTENANCE:
    level: 4
    description: "정기 점검 (선점 불가)"
    preemption: NON_PREEMPTIVE
    examples:
      - periodic_health_check

# =============================================================================
# 제어 모드 정의 (9가지)
# =============================================================================
control_modes:
  BOOT:
    description: "부팅 모드 (시스템 초기화)"
    allowed_priorities: []
    next_mode: INIT

  INIT:
    description: "초기화 모드 (드라이버 로딩)"
    allowed_priorities: []
    next_mode: STANDBY

  STANDBY:
    description: "대기 모드 (작업 대기)"
    allowed_priorities: []
    allowed_transitions: [READY, MANUAL, MAINT, FAULT]

  MANUAL:
    description: "수동 모드 (운영자 직접 제어)"
    allowed_priorities: []
    allowed_transitions: [STANDBY, FAULT]

  READY:
    description: "준비 모드 (자동 모드 전환 가능)"
    allowed_priorities: []
    allowed_transitions: [AUTO, STANDBY, FAULT]

  AUTO:
    description: "자동 모드 (작업 자동 수행)"
    allowed_priorities: [2, 3, 4]  # URGENT_TASK, NORMAL_TASK, MAINTENANCE
    allowed_transitions: [STANDBY, FAULT]

  FAULT:
    description: "고장 모드 (Critical Alarm)"
    allowed_priorities: [0]  # EMERGENCY_STOP only
    allowed_transitions: [STANDBY]

  MAINT:
    description: "정비 모드 (주기적 점검)"
    allowed_priorities: [4]  # MAINTENANCE only
    allowed_transitions: [STANDBY, FAULT]

  CHARGING:
    description: "충전 모드 (배터리 충전 중)"
    allowed_priorities: [2]  # URGENT_TASK
    allowed_transitions: [STANDBY, FAULT]
    note: "상위 시스템이 ChargingTask를 URGENT_TASK로 할당"

# =============================================================================
# 상태 전환 규칙
# =============================================================================
mode_transitions:
  - from: BOOT
    to: INIT
    trigger: hardware_check_ok
    action: load_drivers

  - from: INIT
    to: STANDBY
    trigger: init_complete
    action: enter_standby

  - from: STANDBY
    to: READY
    trigger: ready_check
    condition: sensors_normal_and_robot_health_ok

  - from: STANDBY
    to: MANUAL
    trigger: operator_manual_request
    action: enter_manual_mode

  - from: STANDBY
    to: MAINT
    trigger: maintenance_schedule
    action: start_maintenance_sequence

  - from: READY
    to: AUTO
    trigger: task_assigned
    action: start_auto_mode

  - from: READY
    to: STANDBY
    trigger: ready_timeout
    action: return_to_standby

  - from: AUTO
    to: STANDBY
    trigger: task_complete
    condition: task_queue_empty

  - from: AUTO
    to: FAULT
    trigger: [EMERGENCY_STOP, SAFETY_ISSUE]
    action: stop_all_motors

  # 참고: CHARGING 모드는 ChargingTask를 URGENT_TASK로 받으면 자동 진입
  # BehaviorArbiter는 ControlMode를 CHARGING으로 전환, ChargingTask 실행

  - from: MANUAL
    to: STANDBY
    trigger: operator_exit_manual
    action: return_to_standby

  - from: FAULT
    to: STANDBY
    trigger: operator_reset
    condition: all_sensors_normal

  - from: CHARGING
    to: STANDBY
    trigger: charging_task_completed

  - from: MAINT
    to: STANDBY
    trigger: maintenance_completed

# =============================================================================
# 부가 규칙 (동일 우선순위)
# =============================================================================
tie_breaking_rules:
  URGENT_TASK:
    rule: FIFO  # 먼저 요청된 작업

  NORMAL_TASK:
    rule: DISTANCE_BASED  # 거리 기반 (Phase 2)
    fallback: FIFO

  MAINTENANCE:
    rule: RESOURCE_AVAILABILITY  # 리소스 가용성 (Phase 3)
    fallback: FIFO

# =============================================================================
# 성능 설정
# =============================================================================
performance:
  tick_interval_ms: 100  # Arbiter tick 주기
  emergency_response_time_ms: 100  # 비상 정지 응답 시간
  task_preemption_timeout_ms: 5000  # 작업 선점 타임아웃

# =============================================================================
# 모니터링 및 로깅
# =============================================================================
monitoring:
  log_mode_transitions: true
  log_behavior_selection: true
  metrics_export: true
  datastore_update: true
```

---

## 9. 최종 권장 사항

### 9.1 구현 우선순위

**Phase 1 (MVP - 필수 기능)**:
1. ✅ Custom Priority Queue (5 quantized levels)
2. ✅ FIFO 부가 규칙만 지원
3. ✅ 9가지 제어 모드 상태 머신 (Boot, Init, Standby, Manual, Ready, Auto, Fault, Maint, Charging)
4. ✅ Priority 0-1 즉시 중단, Priority 2-3 일시 중단/재개
5. ✅ DataStore 통합 (모드, 상태 저장)
6. ✅ EventBus 통합 (이벤트 발행/구독)
7. ✅ 충전 관리는 상위 시스템 책임 (ChargingTask 할당)

**Phase 2 (개선)**:
1. Distance-based 부가 규칙 (Priority 3-4)
2. Task Suspend/Resume 고도화
3. 성능 최적화 (Lock-free 자료구조)

**Phase 3 (고급 기능)**:
1. Resource Availability 부가 규칙
2. Machine Learning 기반 우선순위 동적 조정
3. Multi-robot Coordination (범위 외)

### 9.2 기술 스택

- **Priority Queue**: Custom implementation (5 독립 큐)
  - Emergency/Safety: `std::queue` + `std::mutex` (단순성)
  - Task queues: `tbb::concurrent_queue` (성능)
- **우선순위 할당**: 상위 시스템(Fleet Manager/Mission Planner) 책임
  - BehaviorArbiter는 리소스 모니터링 제외 (배터리, 고객 우선순위 등)
  - 상위에서 Task + Priority를 함께 할당
  - BehaviorArbiter는 Priority만 보고 선점/실행
- **State Machine**: HAStateMachine 패턴 활용
- **Task 관리**: 기존 ITaskExecutor (pause/resume/cancel)
- **설정 파일**: YAML (ha-policy.yaml 패턴 참조)
- **모니터링**: DataStore + EventBus

### 9.3 성공 기준

- ✅ 비상 정지 응답 시간 < 100ms
- ✅ Arbiter tick 오버헤드 < 1ms
- ✅ 작업 선점 시 메모리 누수 없음 (AddressSanitizer 검증)
- ✅ 100개 이상 행동 요청 동시 처리 가능
- ✅ URGENT_TASK가 NORMAL_TASK를 성공적으로 선점 (Suspend/Resume)

---

## 10. 참고 자료

### 10.1 학술 자료

1. [Priority-based state machine synthesis for multi-arm manipulators](https://www.tandfonline.com/doi/full/10.1080/01691864.2023.2177122) - 로봇 제어를 위한 우선순위 기반 상태 머신 설계
2. [Hierarchical Behavior-Based Arbitration for Automated Vehicles](https://www.arxiv-vanity.com/papers/2003.01149/) - 계층적 행동 중재 아키텍처
3. [Priority Queues Are Not Good Concurrent Priority Schedulers](https://link.springer.com/chapter/10.1007/978-3-662-48096-0_17) - Concurrent Priority Queue의 성능 한계
4. [Using Task Priorities (TBB)](https://link.springer.com/chapter/10.1007/978-1-4842-4398-5_14) - TBB 우선순위 시스템 활용
5. [1024cores - Priority Queues](https://www.1024cores.net/home/lock-free-algorithms/queues/priority-queues) - Lock-free Priority Queue 설계

### 10.2 MXRC 코드베이스

- `/home/tory/workspace/mxrc/mxrc/src/core/event/core/PriorityQueue.h` - EventBus Priority Queue 구현
- `/home/tory/workspace/mxrc/mxrc/src/core/ha/HAStateMachine.h` - HA 상태 머신 참조
- `/home/tory/workspace/mxrc/mxrc/src/core/rt/RTStateMachine.h` - RT 상태 머신 참조
- `/home/tory/workspace/mxrc/mxrc/src/core/task/interfaces/ITaskExecutor.h` - Task 실행 인터페이스
- `/home/tory/workspace/mxrc/mxrc/docs/specs/019-architecture-improvements/contracts/ha-policy.yaml` - 설정 파일 패턴

---

## 부록 A: BehaviorRequest 데이터 구조

```cpp
// /home/tory/workspace/mxrc/mxrc/src/core/task/dto/BehaviorRequest.h
#pragma once

#include <string>
#include <chrono>
#include "core/task/dto/TaskDefinition.h"

namespace mxrc::core::task {

enum class Priority : uint8_t {
    EMERGENCY_STOP = 0,    // 비상 정지
    SAFETY_ISSUE = 1,      // 안전 이슈
    URGENT_TASK = 2,       // 긴급 작업 (충전 작업 포함 - 상위 할당)
    NORMAL_TASK = 3,       // 일반 작업
    MAINTENANCE = 4        // 정기 점검
};

struct BehaviorRequest {
    std::string id;                    // 행동 요청 ID
    Priority priority;                 // 우선순위
    TaskDefinition taskDef;            // 실행할 Task 정의
    uint64_t timestamp_ns;             // 요청 시각 (FIFO 정렬)
    float distance_m{0.0f};            // 거리 (Distance-based 정렬, 선택적)
    std::string reason;                // 요청 사유 (로깅용)

    // FIFO 비교 연산자
    bool operator<(const BehaviorRequest& other) const {
        return timestamp_ns > other.timestamp_ns;
    }
};

inline const char* toString(Priority priority) {
    switch (priority) {
        case Priority::EMERGENCY_STOP: return "EMERGENCY_STOP";
        case Priority::SAFETY_ISSUE: return "SAFETY_ISSUE";
        case Priority::URGENT_TASK: return "URGENT_TASK";  // 충전 작업도 포함
        case Priority::NORMAL_TASK: return "NORMAL_TASK";
        case Priority::MAINTENANCE: return "MAINTENANCE";
        default: return "UNKNOWN";
    }
}

} // namespace mxrc::core::task
```

---

## 부록 B: 상태 전환 검증 함수

```cpp
// BehaviorArbiter.cpp
bool BehaviorArbiter::isValidTransition(ControlMode from, ControlMode to) {
    // HAStateMachine 패턴 참조 - 9개 제어 모드 지원
    static const std::map<std::pair<ControlMode, ControlMode>, bool> validTransitions = {
        // BOOT에서 가능한 전환
        {{ControlMode::BOOT, ControlMode::INIT}, true},
        {{ControlMode::BOOT, ControlMode::FAULT}, true},  // 부팅 실패

        // INIT에서 가능한 전환
        {{ControlMode::INIT, ControlMode::STANDBY}, true},
        {{ControlMode::INIT, ControlMode::FAULT}, true},  // 초기화 실패

        // STANDBY에서 가능한 전환
        {{ControlMode::STANDBY, ControlMode::READY}, true},
        {{ControlMode::STANDBY, ControlMode::MANUAL}, true},
        {{ControlMode::STANDBY, ControlMode::MAINT}, true},
        {{ControlMode::STANDBY, ControlMode::FAULT}, true},

        // MANUAL에서 가능한 전환
        {{ControlMode::MANUAL, ControlMode::STANDBY}, true},
        {{ControlMode::MANUAL, ControlMode::FAULT}, true},

        // READY에서 가능한 전환
        {{ControlMode::READY, ControlMode::AUTO}, true},
        {{ControlMode::READY, ControlMode::STANDBY}, true},
        {{ControlMode::READY, ControlMode::FAULT}, true},

        // AUTO에서 가능한 전환
        {{ControlMode::AUTO, ControlMode::STANDBY}, true},  // 작업 완료
        {{ControlMode::AUTO, ControlMode::FAULT}, true},    // Critical Alarm
        // Note: CHARGING 전환은 ChargingTask 실행으로 처리 (모드 전환 없음)

        // FAULT에서 가능한 전환
        {{ControlMode::FAULT, ControlMode::STANDBY}, true}, // 고장 해제

        // MAINT에서 가능한 전환
        {{ControlMode::MAINT, ControlMode::STANDBY}, true}, // 점검 완료
        {{ControlMode::MAINT, ControlMode::FAULT}, true},   // 점검 중 고장 발견

        // CHARGING에서 가능한 전환
        {{ControlMode::CHARGING, ControlMode::STANDBY}, true}, // 충전 완료
        {{ControlMode::CHARGING, ControlMode::FAULT}, true},   // 충전 중 고장
    };

    auto it = validTransitions.find({from, to});
    return it != validTransitions.end() && it->second;
}
```

---

**End of Research Report**
