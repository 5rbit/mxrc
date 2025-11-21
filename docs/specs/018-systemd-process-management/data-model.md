# Data Model: systemd 기반 프로세스 관리 고도화

**Feature**: 018-systemd-process-management
**Phase**: Phase 1 - Design
**Status**: Completed
**Last Updated**: 2025-01-21

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: DTO, JSON, struct 등)
- 일반 설명, 필드 설명은 모두 한글로 작성합니다

---

## 개요

이 문서는 systemd 통합 모듈에서 사용하는 데이터 구조를 정의합니다. 모든 데이터 모델은 C++ struct 또는 class로 구현되며, JSON 직렬화/역직렬화를 지원합니다.

---

## 1. systemd Unit 파일 구조

### 1.1 mxrc-rt.service (RT 프로세스)

```ini
[Unit]
Description=MXRC RT Process
Documentation=https://github.com/mxrc/mxrc
After=network.target time-sync.target
Requires=network.target
PartOf=mxrc.target

[Service]
Type=notify
NotifyAccess=main

# 실행 경로
ExecStart=/usr/local/bin/mxrc-rt --config /etc/mxrc/rt-config.json
ExecReload=/bin/kill -HUP $MAINPID

# 재시작 정책
Restart=on-failure
RestartSec=5s
StartLimitBurst=5
StartLimitIntervalSec=60s

# Watchdog
WatchdogSec=30s

# RT 스케줄링
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=80
CPUAffinity=2 3

# 리소스 제한
LimitRTPRIO=99
LimitMEMLOCK=infinity
MemoryMax=512M
CPUQuota=200%
TasksMax=100

# 보안 강화
PrivateTmp=yes
ProtectSystem=strict
ProtectHome=yes
ReadWritePaths=/var/lib/mxrc
ReadOnlyPaths=/etc/mxrc

CapabilityBoundingSet=CAP_SYS_NICE CAP_IPC_LOCK
AmbientCapabilities=CAP_SYS_NICE CAP_IPC_LOCK

SystemCallFilter=@basic-io @file-system @io-event @ipc @network-io @process @signal @timer
SystemCallFilter=~@clock @module @mount @privileged @reboot @swap
SystemCallErrorNumber=EPERM
SeccompArchitectures=native

ProtectKernelTunables=yes
ProtectKernelModules=yes
ProtectControlGroups=yes
RestrictRealtime=no
RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6
LockPersonality=yes
NoNewPrivileges=yes

# 환경 변수
Environment="SPDLOG_LEVEL=info"
Environment="MXRC_MODE=rt"

[Install]
WantedBy=mxrc.target
```

### 1.2 mxrc-nonrt.service (Non-RT 프로세스)

```ini
[Unit]
Description=MXRC Non-RT Process
Documentation=https://github.com/mxrc/mxrc
After=network.target
Requires=network.target
PartOf=mxrc.target

[Service]
Type=notify
NotifyAccess=main

ExecStart=/usr/local/bin/mxrc-nonrt --config /etc/mxrc/nonrt-config.json
ExecReload=/bin/kill -HUP $MAINPID

Restart=on-failure
RestartSec=5s
StartLimitBurst=5
StartLimitIntervalSec=60s

WatchdogSec=30s

# Non-RT 스케줄링
CPUSchedulingPolicy=other
CPUAffinity=0 1

# 리소스 제한
MemoryMax=256M
CPUQuota=150%
TasksMax=50

# 보안 강화 (RT와 동일)
PrivateTmp=yes
ProtectSystem=strict
ProtectHome=yes
ReadWritePaths=/var/lib/mxrc
ReadOnlyPaths=/etc/mxrc

CapabilityBoundingSet=
SystemCallFilter=@basic-io @file-system @io-event @ipc @network-io @process @signal @timer
SystemCallFilter=~@clock @module @mount @privileged @reboot @swap
SystemCallErrorNumber=EPERM

Environment="SPDLOG_LEVEL=info"
Environment="MXRC_MODE=nonrt"

[Install]
WantedBy=mxrc.target
```

### 1.3 mxrc-monitor.service (모니터링 프로세스)

```ini
[Unit]
Description=MXRC Monitor Process
Documentation=https://github.com/mxrc/mxrc
After=mxrc-rt.service mxrc-nonrt.service
PartOf=mxrc.target

[Service]
Type=simple

ExecStart=/usr/local/bin/mxrc-monitor --config /etc/mxrc/monitor-config.json

Restart=on-failure
RestartSec=10s

# 리소스 제한 (가벼운 프로세스)
MemoryMax=128M
CPUQuota=50%

# 보안 강화 (네트워크 필요)
PrivateTmp=yes
ProtectSystem=strict
ReadOnlyPaths=/var/lib/mxrc /etc/mxrc

Environment="SPDLOG_LEVEL=info"
Environment="PROMETHEUS_PORT=9090"

[Install]
WantedBy=mxrc.target
```

### 1.4 mxrc.target (서비스 그룹)

```ini
[Unit]
Description=MXRC Service Group
Documentation=https://github.com/mxrc/mxrc
Wants=mxrc-rt.service mxrc-nonrt.service mxrc-monitor.service

[Install]
WantedBy=multi-user.target
```

---

## 2. C++ 데이터 구조

### 2.1 SystemdMetric (메트릭 DTO)

```cpp
namespace mxrc::systemd {

/**
 * @brief systemd 메트릭을 나타내는 DTO
 *
 * systemctl show 출력을 파싱한 결과를 저장합니다.
 */
struct SystemdMetric {
    std::string service_name;        // 서비스 이름 (예: "mxrc-rt")
    std::string property_name;       // 속성 이름 (예: "RestartCount")
    std::string property_value;      // 속성 값 (예: "0")
    uint64_t timestamp_ms;           // 수집 시각 (Unix timestamp, ms)

    // Prometheus 메트릭으로 변환
    std::string toPrometheusMetric() const;

    // JSON 직렬화
    nlohmann::json toJson() const;
    static SystemdMetric fromJson(const nlohmann::json& j);
};

} // namespace mxrc::systemd
```

**사용 예시**:
```cpp
SystemdMetric metric;
metric.service_name = "mxrc-rt";
metric.property_name = "RestartCount";
metric.property_value = "0";
metric.timestamp_ms = 1705862400000;

// Prometheus 형식: systemd_service_restart_count{service="mxrc-rt"} 0 1705862400000
std::string prom_metric = metric.toPrometheusMetric();
```

### 2.2 JournaldEntry (로그 항목 DTO)

```cpp
namespace mxrc::systemd {

/**
 * @brief journald 로그 항목을 나타내는 DTO
 *
 * sd_journal_send()로 전송할 필드를 정의합니다.
 */
struct JournaldEntry {
    std::string message;             // 로그 메시지
    int priority;                    // syslog 우선순위 (0-7)
    std::string trace_id;            // W3C Trace ID
    std::string span_id;             // W3C Span ID
    std::string service;             // 서비스 이름
    std::string component;           // 컴포넌트 이름 (예: "action", "sequence")
    std::map<std::string, std::string> extra_fields;  // 추가 필드

    // spdlog level을 journald priority로 변환
    static int spdlogLevelToJournaldPriority(spdlog::level::level_enum level);

    // ECS 필드 매핑
    std::map<std::string, std::string> toEcsFields() const;

    // JSON 직렬화
    nlohmann::json toJson() const;
    static JournaldEntry fromJson(const nlohmann::json& j);
};

} // namespace mxrc::systemd
```

**journald Priority 값**:
```cpp
enum JournaldPriority {
    EMERG   = 0,  // 시스템 사용 불가
    ALERT   = 1,  // 즉시 조치 필요
    CRIT    = 2,  // 치명적 상태
    ERR     = 3,  // 오류
    WARNING = 4,  // 경고
    NOTICE  = 5,  // 정상이지만 중요
    INFO    = 6,  // 정보
    DEBUG   = 7   // 디버그
};
```

**사용 예시**:
```cpp
JournaldEntry entry;
entry.message = "Task execution started";
entry.priority = JournaldPriority::INFO;
entry.trace_id = "4bf92f3577b34da6a3ce929d0e0e4736";
entry.span_id = "00f067aa0ba902b7";
entry.service = "mxrc-rt";
entry.component = "task";
entry.extra_fields["task_id"] = "task-001";

// journald로 전송
journald_logger->log(entry);
```

### 2.3 WatchdogConfig (Watchdog 설정)

```cpp
namespace mxrc::systemd {

/**
 * @brief Watchdog 설정을 나타내는 DTO
 *
 * watchdog.json 파일의 구조를 정의합니다.
 */
struct WatchdogConfig {
    bool enabled;                    // Watchdog 활성화 여부
    uint32_t interval_ms;            // Watchdog 알림 주기 (ms)
    uint32_t timeout_sec;            // Watchdog 타임아웃 (초, WatchdogSec 값)
    bool notify_on_start;            // 시작 시 READY=1 알림 여부
    bool notify_on_stop;             // 종료 시 STOPPING=1 알림 여부

    // JSON 직렬화
    nlohmann::json toJson() const;
    static WatchdogConfig fromJson(const nlohmann::json& j);

    // 파일에서 로드
    static WatchdogConfig loadFromFile(const std::string& path);
};

} // namespace mxrc::systemd
```

**watchdog.json 예시**:
```json
{
  "enabled": true,
  "interval_ms": 10000,
  "timeout_sec": 30,
  "notify_on_start": true,
  "notify_on_stop": true
}
```

### 2.4 MetricsCollectorConfig (메트릭 수집 설정)

```cpp
namespace mxrc::systemd {

/**
 * @brief 메트릭 수집 설정을 나타내는 DTO
 *
 * metrics.json 파일의 구조를 정의합니다.
 */
struct MetricsCollectorConfig {
    bool enabled;                    // 메트릭 수집 활성화 여부
    uint32_t interval_ms;            // 수집 주기 (ms)
    std::vector<std::string> services;  // 모니터링할 서비스 목록
    std::vector<std::string> properties;  // 수집할 속성 목록

    // JSON 직렬화
    nlohmann::json toJson() const;
    static MetricsCollectorConfig fromJson(const nlohmann::json& j);

    // 파일에서 로드
    static MetricsCollectorConfig loadFromFile(const std::string& path);
};

} // namespace mxrc::systemd
```

**metrics.json 예시**:
```json
{
  "enabled": true,
  "interval_ms": 1000,
  "services": [
    "mxrc-rt",
    "mxrc-nonrt",
    "mxrc-monitor"
  ],
  "properties": [
    "ActiveState",
    "SubState",
    "RestartCount",
    "CPUUsageNSec",
    "MemoryCurrent",
    "TasksCurrent",
    "WatchdogTimestampMonotonic",
    "WatchdogUSec",
    "CPUQuotaPerSecUSec",
    "MemoryMax",
    "IOReadBytes",
    "IOWriteBytes",
    "ExecMainStartTimestamp",
    "ExecMainExitTimestamp",
    "InvocationID"
  ]
}
```

### 2.5 SecurityConfig (보안 설정)

```cpp
namespace mxrc::systemd {

/**
 * @brief 보안 설정을 나타내는 DTO
 *
 * security.json 파일의 구조를 정의합니다.
 */
struct SecurityConfig {
    double target_score;             // 목표 보안 점수 (0.0 - 10.0)
    bool enforce_seccomp;            // Seccomp 필터 강제 적용 여부
    bool enforce_capabilities;       // Capabilities 제한 강제 적용 여부
    std::vector<std::string> allowed_syscalls;  // 허용할 syscall 세트
    std::vector<std::string> allowed_capabilities;  // 허용할 capabilities

    // JSON 직렬화
    nlohmann::json toJson() const;
    static SecurityConfig fromJson(const nlohmann::json& j);

    // 파일에서 로드
    static SecurityConfig loadFromFile(const std::string& path);
};

} // namespace mxrc::systemd
```

**security.json 예시**:
```json
{
  "target_score": 8.0,
  "enforce_seccomp": true,
  "enforce_capabilities": true,
  "allowed_syscalls": [
    "@basic-io",
    "@file-system",
    "@io-event",
    "@ipc",
    "@network-io",
    "@process",
    "@signal",
    "@timer"
  ],
  "allowed_capabilities": [
    "CAP_SYS_NICE",
    "CAP_IPC_LOCK"
  ]
}
```

---

## 3. Prometheus 메트릭 스키마

### 3.1 수집할 systemd 메트릭

```
# 서비스 상태
systemd_service_active_state{service="mxrc-rt"} 1  # 1=active, 0=inactive
systemd_service_sub_state{service="mxrc-rt",substate="running"} 1

# 재시작 카운트
systemd_service_restart_count{service="mxrc-rt"} 0

# CPU 사용량
systemd_service_cpu_usage_nsec{service="mxrc-rt"} 125000000  # nanoseconds

# 메모리 사용량
systemd_service_memory_current_bytes{service="mxrc-rt"} 134217728  # 128MB

# Task 개수
systemd_service_tasks_current{service="mxrc-rt"} 12

# Watchdog
systemd_service_watchdog_usec{service="mxrc-rt"} 30000000  # 30 seconds
systemd_service_watchdog_timestamp_monotonic{service="mxrc-rt"} 1234567890

# cgroups 리소스 제한
systemd_service_cpu_quota_ratio{service="mxrc-rt"} 2.0  # 200%
systemd_service_memory_max_bytes{service="mxrc-rt"} 536870912  # 512MB

# I/O
systemd_service_io_read_bytes{service="mxrc-rt"} 1048576  # 1MB
systemd_service_io_write_bytes{service="mxrc-rt"} 2097152  # 2MB

# 타이밍
systemd_service_start_timestamp_seconds{service="mxrc-rt"} 1705862400
systemd_service_exit_timestamp_seconds{service="mxrc-rt"} 0  # 실행 중이면 0

# Invocation ID (서비스 재시작마다 변경)
systemd_service_invocation_id{service="mxrc-rt",id="4bf92f3577b34da6"} 1
```

### 3.2 메트릭 타입

```cpp
enum class PrometheusMetricType {
    Gauge,      // 증감 가능한 값 (메모리, CPU)
    Counter,    // 증가만 가능한 값 (재시작 카운트, I/O 바이트)
    Histogram,  // 분포 (현재 미사용)
    Summary     // 요약 (현재 미사용)
};
```

---

## 4. journald 필드 매핑 (ECS 호환)

### 4.1 필드 매핑 테이블

| MXRC Field       | journald Field | ECS Field        | 설명                          |
|------------------|----------------|------------------|------------------------------|
| message          | MESSAGE        | message          | 로그 메시지                   |
| level            | PRIORITY       | log.level        | 로그 레벨 (0-7)               |
| timestamp        | (자동)         | @timestamp       | 타임스탬프 (자동 생성)         |
| trace_id         | TRACE_ID       | trace.id         | W3C Trace ID                  |
| span_id          | SPAN_ID        | trace.span.id    | W3C Span ID                   |
| service          | SERVICE        | service.name     | 서비스 이름                   |
| component        | COMPONENT      | component        | 컴포넌트 (action/sequence/task)|
| task_id          | TASK_ID        | labels.task_id   | Task ID                       |
| action_id        | ACTION_ID      | labels.action_id | Action ID                     |
| sequence_id      | SEQUENCE_ID    | labels.sequence_id | Sequence ID                 |

### 4.2 journald 쿼리 예시

```bash
# 특정 서비스 로그 조회
journalctl SERVICE=mxrc-rt

# Trace ID로 로그 추적
journalctl TRACE_ID=4bf92f3577b34da6a3ce929d0e0e4736

# 컴포넌트별 로그 조회
journalctl COMPONENT=task

# 우선순위 필터 (ERROR 이상)
journalctl SERVICE=mxrc-rt PRIORITY=0..3

# JSON 출력
journalctl -o json SERVICE=mxrc-rt | jq '.'
```

---

## 5. 설정 파일 스키마

### 5.1 /etc/mxrc/rt-config.json (RT 프로세스 설정)

```json
{
  "process": {
    "name": "mxrc-rt",
    "mode": "rt",
    "log_level": "info"
  },
  "realtime": {
    "scheduling_policy": "fifo",
    "priority": 80,
    "cpu_affinity": [2, 3],
    "numa_node": 0,
    "lock_memory": true
  },
  "watchdog": {
    "enabled": true,
    "interval_ms": 10000,
    "timeout_sec": 30
  },
  "metrics": {
    "enabled": true,
    "interval_ms": 1000
  },
  "logging": {
    "journald_enabled": true,
    "file_enabled": true,
    "file_path": "/var/log/mxrc/mxrc-rt.log"
  }
}
```

### 5.2 /etc/mxrc/nonrt-config.json (Non-RT 프로세스 설정)

```json
{
  "process": {
    "name": "mxrc-nonrt",
    "mode": "nonrt",
    "log_level": "info"
  },
  "scheduling": {
    "policy": "other",
    "cpu_affinity": [0, 1]
  },
  "watchdog": {
    "enabled": true,
    "interval_ms": 10000,
    "timeout_sec": 30
  },
  "metrics": {
    "enabled": true,
    "interval_ms": 1000
  },
  "logging": {
    "journald_enabled": true,
    "file_enabled": true,
    "file_path": "/var/log/mxrc/mxrc-nonrt.log"
  }
}
```

### 5.3 /etc/mxrc/monitor-config.json (모니터링 프로세스 설정)

```json
{
  "process": {
    "name": "mxrc-monitor",
    "mode": "monitor",
    "log_level": "info"
  },
  "prometheus": {
    "enabled": true,
    "port": 9090,
    "path": "/metrics"
  },
  "metrics_collection": {
    "enabled": true,
    "interval_ms": 1000,
    "services": ["mxrc-rt", "mxrc-nonrt"]
  },
  "logging": {
    "journald_enabled": true,
    "file_enabled": true,
    "file_path": "/var/log/mxrc/mxrc-monitor.log"
  }
}
```

---

## 6. 데이터 흐름

### 6.1 Watchdog 데이터 흐름

```
RT Process (mxrc-rt)
    ↓
WatchdogTimer (10초 주기)
    ↓
SdNotifyWatchdog::notify()
    ↓
sd_notify(0, "WATCHDOG=1")
    ↓
systemd (Unix socket /run/systemd/notify)
    ↓
WatchdogSec=30s 타이머 리셋
```

### 6.2 Prometheus 메트릭 데이터 흐름

```
Monitor Process (mxrc-monitor)
    ↓
SystemdMetricsCollector::collect() (1초 주기)
    ↓
systemctl show mxrc-rt --property=...
    ↓
파싱 → SystemdMetric DTO
    ↓
PrometheusExporter::expose()
    ↓
HTTP /metrics 엔드포인트 (port 9090)
    ↓
Prometheus Server (scrape)
    ↓
Grafana (시각화)
```

### 6.3 journald 로깅 데이터 흐름

```
RT Process (mxrc-rt)
    ↓
SystemdStructuredLogger::log()
    ↓
JournaldLogger::log(entry)
    ↓
sd_journal_send("MESSAGE=...", "TRACE_ID=...", ...)
    ↓
journald (Unix socket /run/systemd/journal/socket)
    ↓
/var/log/journal/ (영구 저장)
    ↓
journalctl 쿼리 또는 중앙 로그 수집기
```

---

## 7. 데이터 검증 및 제약 조건

### 7.1 WatchdogConfig 검증

```cpp
bool WatchdogConfig::validate() const {
    if (interval_ms == 0 || interval_ms > timeout_sec * 1000) {
        return false;  // interval은 timeout보다 작아야 함
    }
    if (timeout_sec < 10 || timeout_sec > 300) {
        return false;  // timeout은 10초~5분 범위
    }
    return true;
}
```

### 7.2 MetricsCollectorConfig 검증

```cpp
bool MetricsCollectorConfig::validate() const {
    if (interval_ms < 100 || interval_ms > 60000) {
        return false;  // 수집 주기는 100ms~60초 범위
    }
    if (services.empty() || properties.empty()) {
        return false;  // 서비스와 속성은 필수
    }
    return true;
}
```

### 7.3 SecurityConfig 검증

```cpp
bool SecurityConfig::validate() const {
    if (target_score < 0.0 || target_score > 10.0) {
        return false;  // 보안 점수는 0.0~10.0 범위
    }
    if (allowed_capabilities.empty() && enforce_capabilities) {
        return false;  // Capabilities 강제 시 최소 1개 필요
    }
    return true;
}
```

---

## 8. 성능 고려사항

### 8.1 메모리 사용량 추정

- **SystemdMetric**: 약 128 bytes/항목
- **JournaldEntry**: 약 256 bytes/항목
- **수집 주기**: 1초
- **서비스 개수**: 3개
- **속성 개수**: 15개

**총 메모리**: 3 서비스 × 15 속성 × 128 bytes = **5.625 KB/초**
**1시간 데이터**: 5.625 KB × 3600 = **20.25 MB** (인메모리 버퍼 미사용 시)

### 8.2 디스크 I/O 추정

- **journald 로그**: 평균 512 bytes/항목
- **로그 빈도**: 초당 10개 (RT 프로세스)
- **압축률**: 약 50%

**일일 로그 크기**: 10 × 512 bytes × 86400 × 0.5 = **221 MB/일**
**월간 로그 크기**: 221 MB × 30 = **6.6 GB/월**

---

## 결론

이 데이터 모델은 다음을 정의합니다:

1. **systemd Unit 파일**: RT/Non-RT/Monitor 서비스 및 target 구성
2. **C++ DTO**: SystemdMetric, JournaldEntry, 설정 DTO
3. **Prometheus 메트릭**: 15개 systemd 메트릭 스키마
4. **journald 필드**: ECS 호환 필드 매핑
5. **설정 파일**: JSON 스키마 정의
6. **데이터 흐름**: Watchdog, 메트릭, 로깅 흐름도
7. **검증 규칙**: 설정 유효성 검사 로직
8. **성능 추정**: 메모리 및 디스크 사용량

모든 데이터 구조는 JSON 직렬화를 지원하며, RAII 원칙을 따릅니다. 다음 단계는 인터페이스 정의 (contracts/) 작성입니다.

---

**작성자**: MXRC Development Team
**검토자**: TBD
**승인 날짜**: TBD
