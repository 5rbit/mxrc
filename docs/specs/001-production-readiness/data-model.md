# Data Model: Production Readiness

**Feature**: [001-production-readiness](spec.md)
**Created**: 2025-11-21
**Last Updated**: 2025-11-21

---

## 개요

본 문서는 Production Readiness 기능에서 사용되는 핵심 데이터 모델을 정의합니다. 각 entity는 특정 기능 영역(성능 최적화, 고가용성, 로깅, 트레이싱)을 지원하며, 실시간 성능과 메모리 안전성을 보장하도록 설계되었습니다.

---

## 1. CPUAffinityConfig

### 목적
RT 프로세스의 CPU 코어 할당 정책을 정의합니다. 각 프로세스/스레드가 실행될 CPU 코어를 지정하여 예측 가능한 실시간 성능을 보장합니다.

### 필드

| 필드명 | 타입 | 설명 | 제약 조건 |
|--------|------|------|-----------|
| `process_name` | `std::string` | 프로세스 이름 (예: "rt_process", "non_rt_process") | 필수, 비어있지 않음 |
| `thread_name` | `std::string` | 스레드 이름 (예: "main", "eventbus_worker") | 선택, 비어있지 않음 |
| `cpu_cores` | `std::vector<int>` | 할당할 CPU 코어 ID 목록 | 필수, 최소 1개, 유효한 코어 범위 (0 ~ num_cpus-1) |
| `isolation_mode` | `IsolationMode` | CPU isolation 모드 | 필수 |
| `is_exclusive` | `bool` | 전용 할당 여부 (true: 다른 프로세스 차단) | 기본값: `true` |
| `priority` | `int` | 스레드 우선순위 (SCHED_FIFO/RR 사용 시) | 범위: 1-99, 기본값: 80 |
| `policy` | `SchedPolicy` | 스케줄링 정책 | 기본값: `SCHED_FIFO` |

### IsolationMode 열거형

```cpp
enum class IsolationMode {
    NONE,           // Isolation 없음
    ISOLCPUS,       // 커널 부팅 파라미터 isolcpus 사용
    CGROUPS,        // cgroups cpuset 사용
    HYBRID          // isolcpus + cgroups 조합 (권장)
};
```

### SchedPolicy 열거형

```cpp
enum class SchedPolicy {
    SCHED_OTHER,    // 일반 스케줄링
    SCHED_FIFO,     // RT FIFO (권장)
    SCHED_RR,       // RT Round-Robin
    SCHED_DEADLINE  // Deadline 스케줄링
};
```

### 검증 규칙

- `cpu_cores`의 모든 코어 ID는 시스템에 존재하는 CPU 코어 범위 내여야 함
- `isolation_mode == ISOLCPUS`인 경우, 커널 부팅 파라미터에 `isolcpus`가 설정되어 있어야 함
- `policy`가 RT 스케줄링인 경우, `priority`는 1-99 범위여야 함
- `thread_name`이 지정된 경우, 해당 스레드가 존재해야 함

### 관계

- **ProcessMonitor**에서 참조: CPU affinity 설정 검증
- **PerfMonitor**에서 참조: 실제 실행 코어 모니터링

### 상태 전이

CPUAffinityConfig는 불변(immutable) 설정이므로 상태 전이가 없습니다. 런타임 변경이 필요한 경우 새로운 설정 객체를 생성합니다.

### 예시

```json
{
  "process_name": "rt_process",
  "thread_name": "main",
  "cpu_cores": [2, 3],
  "isolation_mode": "HYBRID",
  "is_exclusive": true,
  "priority": 90,
  "policy": "SCHED_FIFO"
}
```

---

## 2. NUMABindingConfig

### 목적
프로세스/스레드의 NUMA 노드 바인딩 설정을 정의합니다. 메모리 접근 지역성을 최적화하여 지연시간을 최소화합니다.

### 필드

| 필드명 | 타입 | 설명 | 제약 조건 |
|--------|------|------|-----------|
| `process_name` | `std::string` | 프로세스 이름 | 필수, 비어있지 않음 |
| `numa_node` | `int` | 바인딩할 NUMA 노드 ID | 필수, 유효한 NUMA 노드 범위 (0 ~ num_nodes-1) |
| `memory_policy` | `MemoryPolicy` | 메모리 할당 정책 | 필수 |
| `strict_binding` | `bool` | 엄격한 바인딩 여부 (실패 시 오류 반환) | 기본값: `true` |
| `migrate_pages` | `bool` | 기존 페이지를 NUMA 노드로 마이그레이션 | 기본값: `false` |
| `cpu_cores_hint` | `std::vector<int>` | 선호하는 CPU 코어 (NUMA 노드 내) | 선택 |

### MemoryPolicy 열거형

```cpp
enum class MemoryPolicy {
    LOCAL,          // NUMA_LOCAL: 현재 NUMA 노드에서만 할당 (권장)
    PREFERRED,      // NUMA_PREFERRED: 선호 노드, 실패 시 다른 노드
    INTERLEAVE,     // NUMA_INTERLEAVE: 여러 노드에 인터리빙
    BIND            // NUMA_BIND: 지정된 노드에만 바인딩
};
```

### 검증 규칙

- `numa_node`는 시스템에 존재하는 NUMA 노드 범위 내여야 함
- `cpu_cores_hint`가 지정된 경우, 모든 코어는 `numa_node`에 속해야 함
- `strict_binding == true`인 경우, 메모리 할당 실패 시 프로세스 시작 차단
- `migrate_pages == true`인 경우, 프로세스 시작 시 페이지 마이그레이션 수행

### 관계

- **CPUAffinityConfig**와 연관: 같은 NUMA 노드의 CPU 코어에 할당 권장
- **ProcessMonitor**에서 참조: NUMA 바인딩 검증
- **PerfMonitor**에서 참조: NUMA 메모리 접근 통계 수집

### 상태 전이

NUMABindingConfig는 불변(immutable) 설정이므로 상태 전이가 없습니다.

### 예시

```json
{
  "process_name": "rt_process",
  "numa_node": 0,
  "memory_policy": "LOCAL",
  "strict_binding": true,
  "migrate_pages": false,
  "cpu_cores_hint": [2, 3]
}
```

---

## 3. ProcessHealthStatus

### 목적
프로세스의 현재 건강 상태를 나타냅니다. FailoverManager가 이 정보를 기반으로 failover 결정을 수행합니다.

### 필드

| 필드명 | 타입 | 설명 | 제약 조건 |
|--------|------|------|-----------|
| `process_name` | `std::string` | 프로세스 이름 | 필수, 비어있지 않음 |
| `pid` | `pid_t` | 프로세스 ID | 필수, > 0 |
| `status` | `HealthStatus` | 건강 상태 | 필수 |
| `last_heartbeat` | `std::chrono::system_clock::time_point` | 마지막 heartbeat 시각 | 필수 |
| `response_time_ms` | `double` | Health check 응답 시간 (ms) | >= 0 |
| `cpu_usage_percent` | `double` | CPU 사용률 (%) | 0.0 ~ 100.0 |
| `memory_usage_mb` | `uint64_t` | 메모리 사용량 (MB) | >= 0 |
| `deadline_miss_count` | `uint64_t` | RT deadline miss 횟수 (누적) | >= 0 |
| `error_message` | `std::string` | 오류 메시지 (UNHEALTHY 상태 시) | 선택 |
| `restart_count` | `uint32_t` | 재시작 횟수 (누적) | >= 0 |

### HealthStatus 열거형

```cpp
enum class HealthStatus {
    HEALTHY,        // 정상 동작 중
    DEGRADED,       // 성능 저하 (deadline miss 증가, 응답 지연)
    UNHEALTHY,      // 비정상 (응답 없음, critical error)
    STARTING,       // 시작 중
    STOPPING,       // 종료 중
    STOPPED         // 종료됨
};
```

### 검증 규칙

- `last_heartbeat`는 현재 시각보다 과거여야 함
- `status == UNHEALTHY`인 경우, `error_message`가 비어있지 않아야 함
- `response_time_ms > health_check_timeout`인 경우, `status`는 `DEGRADED` 또는 `UNHEALTHY`여야 함
- `deadline_miss_count`가 임계값 초과 시, `status`는 `DEGRADED` 이상이어야 함

### 관계

- **ProcessMonitor**에서 생성 및 업데이트
- **FailoverManager**에서 참조: Failover 결정
- **HealthCheckAPI**에서 반환: HTTP 엔드포인트 응답

### 상태 전이

```
[STOPPED] → [STARTING] → [HEALTHY]
                           ↓
[HEALTHY] ↔ [DEGRADED] → [UNHEALTHY] → [STOPPING] → [STOPPED]
```

전이 조건:
- `STARTING → HEALTHY`: Health check 성공
- `HEALTHY → DEGRADED`: deadline miss 증가 또는 응답 지연
- `DEGRADED → HEALTHY`: 성능 회복
- `DEGRADED → UNHEALTHY`: Health check 실패 또는 critical error
- `UNHEALTHY → STOPPING`: Failover 시작
- `STOPPING → STOPPED`: 프로세스 종료 완료

### 예시

```json
{
  "process_name": "rt_process",
  "pid": 12345,
  "status": "HEALTHY",
  "last_heartbeat": "2025-11-21T10:30:45.123Z",
  "response_time_ms": 1.5,
  "cpu_usage_percent": 45.2,
  "memory_usage_mb": 512,
  "deadline_miss_count": 5,
  "error_message": "",
  "restart_count": 0
}
```

---

## 4. StateCheckpoint

### 목적
프로세스의 복구 지점을 정의합니다. Failover 시 이 checkpoint에서 프로세스를 재시작하여 데이터 손실을 최소화합니다.

### 필드

| 필드명 | 타입 | 설명 | 제약 조건 |
|--------|------|------|-----------|
| `checkpoint_id` | `std::string` | Checkpoint 고유 ID (UUID) | 필수, 비어있지 않음 |
| `process_name` | `std::string` | 프로세스 이름 | 필수, 비어있지 않음 |
| `timestamp` | `std::chrono::system_clock::time_point` | Checkpoint 생성 시각 | 필수 |
| `rt_state` | `nlohmann::json` | RT 프로세스의 상태 (Task/Sequence/Action 상태) | 필수 |
| `datastore_snapshot` | `nlohmann::json` | RTDataStore 스냅샷 (선택적 키-값 데이터) | 선택 |
| `eventbus_queue_snapshot` | `std::vector<std::string>` | EventBus 큐에 남은 이벤트 ID 목록 | 선택 |
| `checkpoint_size_bytes` | `uint64_t` | Checkpoint 크기 (bytes) | >= 0 |
| `is_complete` | `bool` | Checkpoint 완료 여부 | 기본값: `false` |
| `file_path` | `std::filesystem::path` | Checkpoint 저장 경로 | 필수 |

### 검증 규칙

- `checkpoint_id`는 UUID 형식이어야 함
- `is_complete == true`인 경우, `file_path`에 파일이 존재해야 함
- `rt_state`는 유효한 JSON 객체여야 함
- `checkpoint_size_bytes`는 실제 파일 크기와 일치해야 함

### 관계

- **StateCheckpoint 클래스**에서 생성 및 저장
- **FailoverManager**에서 참조: 복구 시 로드
- **ProcessMonitor**에서 참조: Checkpoint 생성 트리거

### 상태 전이

```
[CREATING] → [COMPLETE] → [LOADED]
              ↓
           [EXPIRED] (보관 기간 초과)
```

전이 조건:
- `CREATING → COMPLETE`: 모든 데이터 직렬화 완료, 파일 쓰기 성공
- `COMPLETE → LOADED`: Failover 시 checkpoint 로드
- `COMPLETE → EXPIRED`: 보관 기간(예: 24시간) 초과

### 예시

```json
{
  "checkpoint_id": "550e8400-e29b-41d4-a716-446655440000",
  "process_name": "rt_process",
  "timestamp": "2025-11-21T10:30:00.000Z",
  "rt_state": {
    "active_tasks": ["task_001", "task_002"],
    "running_sequences": [],
    "action_queue_size": 5
  },
  "datastore_snapshot": {
    "robot_position": {"x": 10.5, "y": 20.3},
    "sensor_data": {"temperature": 25.5}
  },
  "eventbus_queue_snapshot": ["event_001", "event_002"],
  "checkpoint_size_bytes": 4096,
  "is_complete": true,
  "file_path": "/var/mxrc/checkpoints/rt_process_20251121103000.json"
}
```

---

## 5. FailoverPolicy

### 목적
Failover 동작 정책을 정의합니다. ProcessMonitor와 FailoverManager가 이 정책을 기반으로 프로세스 모니터링 및 복구를 수행합니다.

### 필드

| 필드명 | 타입 | 설명 | 제약 조건 |
|--------|------|------|-----------|
| `process_name` | `std::string` | 대상 프로세스 이름 | 필수, 비어있지 않음 |
| `health_check_interval_ms` | `uint32_t` | Health check 주기 (ms) | 필수, > 0 |
| `health_check_timeout_ms` | `uint32_t` | Health check 타임아웃 (ms) | 필수, > 0 |
| `failure_threshold` | `uint32_t` | Failover 트리거 실패 횟수 | 필수, >= 1 |
| `restart_delay_ms` | `uint32_t` | 재시작 전 대기 시간 (ms) | 기본값: 100 |
| `max_restart_count` | `uint32_t` | 최대 재시작 횟수 (시간 창 내) | 기본값: 5 |
| `restart_window_sec` | `uint32_t` | 재시작 횟수 계산 시간 창 (초) | 기본값: 60 |
| `enable_state_recovery` | `bool` | Checkpoint에서 복구 여부 | 기본값: `true` |
| `checkpoint_interval_sec` | `uint32_t` | Checkpoint 생성 주기 (초) | 기본값: 60 |
| `enable_leader_election` | `bool` | 분산 환경에서 leader election 활성화 | 기본값: `false` |

### 검증 규칙

- `health_check_timeout_ms < health_check_interval_ms` (타임아웃은 주기보다 짧아야 함)
- `failure_threshold >= 1` (최소 1회 실패 후 failover)
- `max_restart_count > 0` (최소 1회 재시작 허용)
- `enable_state_recovery == true`인 경우, `checkpoint_interval_sec > 0`이어야 함

### 관계

- **ProcessMonitor**에서 참조: Health check 주기 및 타임아웃
- **FailoverManager**에서 참조: Failover 결정 및 재시작 정책
- **StateCheckpoint**와 연관: Checkpoint 생성 주기

### 상태 전이

FailoverPolicy는 불변(immutable) 설정이므로 상태 전이가 없습니다. 런타임 변경이 필요한 경우 새로운 정책 객체를 생성합니다.

### 예시

```json
{
  "process_name": "rt_process",
  "health_check_interval_ms": 1000,
  "health_check_timeout_ms": 500,
  "failure_threshold": 3,
  "restart_delay_ms": 100,
  "max_restart_count": 5,
  "restart_window_sec": 60,
  "enable_state_recovery": true,
  "checkpoint_interval_sec": 60,
  "enable_leader_election": false
}
```

---

## 6. StructuredLogEvent

### 목적
구조화된 로그 이벤트를 정의합니다. JSON 형식으로 Elasticsearch에 전송되어 검색 및 분석이 가능합니다.

### 필드

| 필드명 | 타입 | 설명 | 제약 조건 |
|--------|------|------|-----------|
| `@timestamp` | `std::string` (ISO 8601) | 로그 생성 시각 | 필수, ISO 8601 형식 |
| `log.level` | `std::string` | 로그 레벨 | 필수, 유효한 레벨 (trace, debug, info, warn, error, critical) |
| `log.logger` | `std::string` | Logger 이름 | 필수 |
| `process.name` | `std::string` | 프로세스 이름 | 필수 |
| `process.pid` | `int` | 프로세스 ID | 필수, > 0 |
| `process.thread.id` | `uint64_t` | 스레드 ID | 필수 |
| `process.thread.name` | `std::string` | 스레드 이름 | 선택 |
| `message` | `std::string` | 로그 메시지 | 필수, 비어있지 않음 |
| `ecs.version` | `std::string` | ECS 버전 | 기본값: "8.11" |
| `labels` | `nlohmann::json` | 커스텀 레이블 (키-값 쌍) | 선택 |
| `trace.id` | `std::string` | 분산 트레이싱 Trace ID | 선택 (트레이싱 활성화 시 필수) |
| `span.id` | `std::string` | 분산 트레이싱 Span ID | 선택 |
| `mxrc.task_id` | `std::string` | MXRC Task ID | 선택 |
| `mxrc.sequence_id` | `std::string` | MXRC Sequence ID | 선택 |
| `mxrc.action_id` | `std::string` | MXRC Action ID | 선택 |
| `mxrc.cycle_time_us` | `double` | RT cycle 소요 시간 (μs) | 선택, >= 0 |

### 검증 규칙

- `@timestamp`는 ISO 8601 형식이어야 함 (예: "2025-11-21T10:30:45.123Z")
- `log.level`은 유효한 spdlog 레벨이어야 함
- `trace.id`가 있으면 유효한 16진수 문자열이어야 함 (16자리)
- `span.id`가 있으면 유효한 16진수 문자열이어야 함 (8자리)
- `labels`는 유효한 JSON 객체여야 함

### 관계

- **StructuredLogger**에서 생성: spdlog 커스텀 formatter에서 변환
- **Filebeat**로 전송: 파일 기반 로그 수집
- **TraceContext**와 연관: trace_id, span_id 포함

### 상태 전이

StructuredLogEvent는 불변(immutable) 이벤트이므로 상태 전이가 없습니다. 생성 후 파일에 기록되고 Filebeat가 수집합니다.

### 예시

```json
{
  "@timestamp": "2025-11-21T10:30:45.123Z",
  "log.level": "info",
  "log.logger": "mxrc.rt_process",
  "process.name": "rt_process",
  "process.pid": 12345,
  "process.thread.id": 67890,
  "process.thread.name": "main",
  "message": "Task execution completed successfully",
  "ecs.version": "8.11",
  "labels": {
    "environment": "production",
    "component": "task_executor"
  },
  "trace.id": "0af7651916cd43dd",
  "span.id": "8a2c9a3b",
  "mxrc.task_id": "task_001",
  "mxrc.sequence_id": "seq_001",
  "mxrc.action_id": "action_001",
  "mxrc.cycle_time_us": 850.5
}
```

---

## 7. TraceContext

### 목적
분산 트레이싱의 컨텍스트 정보를 정의합니다. W3C Trace Context 표준을 따르며, 요청이 여러 프로세스/스레드를 거쳐도 추적이 가능합니다.

### 필드

| 필드명 | 타입 | 설명 | 제약 조건 |
|--------|------|------|-----------|
| `trace_id` | `std::string` | Trace ID (16진수 문자열, 16바이트) | 필수, 32자리 16진수 |
| `span_id` | `std::string` | 현재 Span ID (16진수 문자열, 8바이트) | 필수, 16자리 16진수 |
| `parent_span_id` | `std::string` | 부모 Span ID (루트인 경우 빈 문자열) | 선택, 16자리 16진수 또는 빈 문자열 |
| `trace_flags` | `uint8_t` | W3C Trace flags (bit 0: sampled) | 기본값: 0x01 (sampled) |
| `trace_state` | `std::string` | 벤더 특정 트레이싱 상태 (W3C tracestate) | 선택 |
| `baggage` | `std::map<std::string, std::string>` | 커스텀 컨텍스트 전파 데이터 | 선택 |
| `is_remote` | `bool` | 원격 프로세스에서 전파된 컨텍스트 여부 | 기본값: `false` |

### trace_flags 비트 정의

```cpp
constexpr uint8_t TRACE_FLAG_SAMPLED = 0x01;  // Bit 0: Sampled
```

### 검증 규칙

- `trace_id`는 32자리 16진수 문자열이어야 함 (예: "0af7651916cd43dd8448eb211c80319c")
- `span_id`는 16자리 16진수 문자열이어야 함 (예: "b7ad6b7169203331")
- `parent_span_id`가 비어있지 않으면 16자리 16진수 문자열이어야 함
- `trace_flags`는 W3C Trace Context 표준 플래그여야 함
- 루트 span인 경우, `parent_span_id`는 빈 문자열이어야 함

### 관계

- **Span**에서 참조: 각 Span은 하나의 TraceContext를 가짐
- **StructuredLogEvent**와 연관: trace_id, span_id 전파
- **EventBusTracer**에서 사용: 이벤트 전파 시 컨텍스트 주입

### 상태 전이

TraceContext는 불변(immutable)이지만, span 생성 시 새로운 컨텍스트가 파생됩니다:

```
[Root Context] → [Child Context 1] → [Child Context 2]
                 ↓
              [Child Context 3]
```

전이 조건:
- 새로운 Span 생성 시: `span_id` 갱신, `parent_span_id`에 이전 `span_id` 저장
- 프로세스 간 전파 시: `is_remote = true`로 설정

### 예시

#### 루트 컨텍스트

```json
{
  "trace_id": "0af7651916cd43dd8448eb211c80319c",
  "span_id": "b7ad6b7169203331",
  "parent_span_id": "",
  "trace_flags": 1,
  "trace_state": "",
  "baggage": {
    "user_id": "12345",
    "request_type": "task_execution"
  },
  "is_remote": false
}
```

#### 자식 컨텍스트

```json
{
  "trace_id": "0af7651916cd43dd8448eb211c80319c",
  "span_id": "8a2c9a3b12345678",
  "parent_span_id": "b7ad6b7169203331",
  "trace_flags": 1,
  "trace_state": "",
  "baggage": {
    "user_id": "12345",
    "request_type": "task_execution"
  },
  "is_remote": false
}
```

---

## 8. Span

### 목적
분산 트레이싱의 작업 단위를 정의합니다. 각 Span은 특정 작업(Task 실행, EventBus 이벤트 처리 등)의 시작/종료 시각, 상태, 메타데이터를 기록합니다.

### 필드

| 필드명 | 타입 | 설명 | 제약 조건 |
|--------|------|------|-----------|
| `span_id` | `std::string` | Span ID (16진수 문자열, 8바이트) | 필수, 16자리 16진수 |
| `trace_context` | `TraceContext` | 트레이싱 컨텍스트 | 필수 |
| `operation_name` | `std::string` | 작업 이름 (예: "task.execute", "eventbus.publish") | 필수, 비어있지 않음 |
| `start_time` | `std::chrono::system_clock::time_point` | 작업 시작 시각 | 필수 |
| `end_time` | `std::chrono::system_clock::time_point` | 작업 종료 시각 | 선택 (진행 중인 경우 null) |
| `duration_us` | `int64_t` | 작업 소요 시간 (μs) | 계산됨, >= 0 |
| `status` | `SpanStatus` | Span 상태 | 필수 |
| `status_message` | `std::string` | 상태 메시지 (오류 시 상세 정보) | 선택 |
| `attributes` | `std::map<std::string, std::string>` | Span 속성 (키-값 쌍) | 선택 |
| `events` | `std::vector<SpanEvent>` | Span 이벤트 목록 (로그) | 선택 |
| `is_recording` | `bool` | 현재 기록 중 여부 | 기본값: `true` |

### SpanStatus 열거형

```cpp
enum class SpanStatus {
    UNSET,          // 상태 미설정 (기본값)
    OK,             // 성공
    ERROR           // 오류
};
```

### SpanEvent 구조체

```cpp
struct SpanEvent {
    std::chrono::system_clock::time_point timestamp;
    std::string name;
    std::map<std::string, std::string> attributes;
};
```

### 검증 규칙

- `span_id`는 `trace_context.span_id`와 일치해야 함
- `end_time`이 설정된 경우, `end_time >= start_time`여야 함
- `duration_us`는 `(end_time - start_time)`로 계산됨
- `status == ERROR`인 경우, `status_message`가 비어있지 않아야 함
- `is_recording == false`인 경우, 속성 및 이벤트 추가 불가

### 관계

- **TraceContext**와 연관: 각 Span은 하나의 TraceContext를 가짐
- **RTCycleTracer**에서 생성: RT cycle 트레이싱
- **EventBusTracer**에서 생성: EventBus 이벤트 트레이싱
- **OTLP Exporter**로 전송: Jaeger로 전송

### 상태 전이

```
[RECORDING] → [ENDED (OK/ERROR)] → [EXPORTED]
```

전이 조건:
- `RECORDING → ENDED`: Span.End() 호출 시
- `ENDED → EXPORTED`: OTLP Exporter가 Jaeger로 전송

### 예시

#### Task 실행 Span

```json
{
  "span_id": "b7ad6b7169203331",
  "trace_context": {
    "trace_id": "0af7651916cd43dd8448eb211c80319c",
    "span_id": "b7ad6b7169203331",
    "parent_span_id": "",
    "trace_flags": 1
  },
  "operation_name": "task.execute",
  "start_time": "2025-11-21T10:30:45.000Z",
  "end_time": "2025-11-21T10:30:45.850Z",
  "duration_us": 850,
  "status": "OK",
  "status_message": "",
  "attributes": {
    "task.id": "task_001",
    "task.type": "periodic",
    "task.priority": "high"
  },
  "events": [
    {
      "timestamp": "2025-11-21T10:30:45.500Z",
      "name": "sequence.started",
      "attributes": {
        "sequence.id": "seq_001"
      }
    }
  ],
  "is_recording": false
}
```

#### EventBus 이벤트 Span

```json
{
  "span_id": "8a2c9a3b12345678",
  "trace_context": {
    "trace_id": "0af7651916cd43dd8448eb211c80319c",
    "span_id": "8a2c9a3b12345678",
    "parent_span_id": "b7ad6b7169203331",
    "trace_flags": 1
  },
  "operation_name": "eventbus.publish",
  "start_time": "2025-11-21T10:30:45.100Z",
  "end_time": "2025-11-21T10:30:45.150Z",
  "duration_us": 50,
  "status": "OK",
  "status_message": "",
  "attributes": {
    "event.type": "sensor_data",
    "event.topic": "robot/sensors"
  },
  "events": [],
  "is_recording": false
}
```

---

## Entity 관계 다이어그램

```
┌─────────────────────┐
│  CPUAffinityConfig  │
│  NUMABindingConfig  │
└──────────┬──────────┘
           │
           │ references
           ↓
┌─────────────────────┐      monitors      ┌─────────────────────┐
│  ProcessMonitor     │◄──────────────────│  FailoverPolicy     │
└──────────┬──────────┘                    └─────────────────────┘
           │
           │ generates
           ↓
┌─────────────────────┐      triggers      ┌─────────────────────┐
│ ProcessHealthStatus │───────────────────►│  FailoverManager    │
└─────────────────────┘                    └──────────┬──────────┘
                                                      │
                                                      │ loads
                                                      ↓
                                           ┌─────────────────────┐
                                           │  StateCheckpoint    │
                                           └─────────────────────┘

┌─────────────────────┐      includes      ┌─────────────────────┐
│ StructuredLogEvent  │◄──────────────────│   TraceContext      │
└─────────────────────┘                    └──────────┬──────────┘
                                                      │
                                                      │ contains
                                                      ↓
                                           ┌─────────────────────┐
                                           │       Span          │
                                           └─────────────────────┘
```

---

## 메모리 관리 및 RAII 원칙

모든 entity는 MXRC Constitution의 RAII 원칙을 따릅니다:

1. **CPUAffinityConfig, NUMABindingConfig, FailoverPolicy**:
   - 불변(immutable) 설정 객체
   - `std::shared_ptr`로 관리
   - JSON 파일에서 로드, 런타임 변경 시 새 객체 생성

2. **ProcessHealthStatus**:
   - ProcessMonitor가 주기적으로 업데이트
   - `std::shared_ptr`로 관리
   - Thread-safe access (mutex 또는 atomic)

3. **StateCheckpoint**:
   - 파일 I/O를 포함하므로 RAII 패턴 필수
   - 소멸자에서 파일 핸들 자동 닫기
   - `std::unique_ptr`로 관리

4. **StructuredLogEvent**:
   - spdlog formatter에서 생성 후 즉시 직렬화
   - 스택 객체로 관리 (복사 최소화)

5. **TraceContext**:
   - Thread-local storage에 저장
   - 스택 객체 또는 `std::shared_ptr`로 관리
   - 프로세스 간 전파 시 직렬화 (W3C Trace Context 헤더)

6. **Span**:
   - OpenTelemetry SDK가 메모리 관리
   - RAII Span Guard 패턴 사용 (소멸자에서 자동 종료)
   - `opentelemetry::nostd::shared_ptr`로 관리

---

## 성능 고려사항

### RT Path에서의 사용

1. **허용됨** (RT path에서 사용 가능):
   - CPUAffinityConfig, NUMABindingConfig 읽기 (초기화 시)
   - ProcessHealthStatus 읽기 (atomic 또는 seqlock)
   - TraceContext 읽기/쓰기 (thread-local, 락 없음)
   - Span 생성/종료 (lockless queue, <5% 오버헤드)

2. **금지됨** (RT path에서 사용 불가):
   - StateCheckpoint 생성 (파일 I/O, 별도 스레드에서 수행)
   - StructuredLogEvent 생성 (비동기 로깅 사용)
   - FailoverPolicy 변경 (설정 불변)

### 메모리 할당

- 모든 entity는 초기화 시 메모리 할당 (RT path 진입 전)
- RT path에서는 기존 객체 재사용 또는 lockless 자료구조 사용
- TraceContext: thread-local storage (malloc 없음)
- Span: OpenTelemetry의 lockless queue에 추가

---

## 버전 관리

- **Version**: 1.0.0
- **Created**: 2025-11-21
- **Last Updated**: 2025-11-21
- **Compatibility**: MXRC v1.0+, ECS v8.11, OpenTelemetry v1.12+

---

## 참조

- **Specification**: [spec.md](spec.md)
- **Implementation Plan**: [plan.md](plan.md)
- **Research**: [research.md](research.md)
- **Architecture**: `docs/architecture/ARCH-001-task-layer.md`
- **Constitution**: `.specify/memory/constitution.md`
