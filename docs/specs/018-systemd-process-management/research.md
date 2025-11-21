# Research: systemd 기반 프로세스 관리 고도화

**Feature**: 018-systemd-process-management
**Phase**: Phase 0 - Research
**Status**: Completed
**Last Updated**: 2025-01-21

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: systemd, watchdog, cgroups, API 등)
- 일반 설명, 결정 사항, 근거는 모두 한글로 작성합니다

---

## 개요

이 문서는 MXRC 시스템의 systemd 통합 고도화를 위한 기술 조사 결과를 정리합니다. 각 기술 결정에 대한 근거, 대안 분석, 모범 사례를 문서화하여 구현 단계에서 참조할 수 있도록 합니다.

---

## 1. systemd 서비스 Unit 파일 구성

### 결정 사항

3개의 독립적인 systemd 서비스 Unit을 생성하고, 하나의 target으로 그룹화합니다:

```ini
# mxrc-rt.service - RT 프로세스
# mxrc-nonrt.service - Non-RT 프로세스
# mxrc-monitor.service - 모니터링 프로세스
# mxrc.target - 전체 서비스 그룹
```

### 근거

**장점**:
- **독립적 재시작**: RT 프로세스 실패가 Non-RT 프로세스에 영향을 주지 않음
- **세분화된 리소스 제어**: 각 프로세스에 맞는 cgroups 설정 가능
- **명확한 의존성 관리**: `After=`, `Requires=` 디렉티브로 시작 순서 제어
- **모니터링 용이성**: 개별 서비스 상태 추적 가능

**대안 고려**:
- **단일 서비스 방식**: 하나의 프로세스에서 RT/Non-RT 스레드 모두 관리
  - **기각 사유**: RT 스레드와 Non-RT 스레드의 격리가 어려움. systemd의 서비스별 리소스 제어 활용 불가.

- **systemd instance 사용** (`mxrc@rt.service`, `mxrc@nonrt.service`)
  - **기각 사유**: 설정 파일 공유로 인해 RT/Non-RT 특성별 최적화 어려움. 명확성이 떨어짐.

### 모범 사례

**RT 프로세스 Unit 파일 구성**:
```ini
[Unit]
Description=MXRC RT Process
After=network.target time-sync.target
Requires=network.target

[Service]
Type=notify
ExecStart=/usr/local/bin/mxrc-rt
Restart=on-failure
RestartSec=5s
WatchdogSec=30s
NotifyAccess=main

# RT Scheduling
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=80
CPUAffinity=2 3

# Resource Limits
LimitRTPRIO=99
LimitMEMLOCK=infinity
MemoryMax=512M
CPUQuota=200%

# Security
PrivateTmp=yes
ProtectSystem=strict
ReadWritePaths=/var/lib/mxrc
CapabilityBoundingSet=CAP_SYS_NICE CAP_IPC_LOCK
SeccompArchitectures=native

[Install]
WantedBy=mxrc.target
```

**핵심 디렉티브**:
- `Type=notify`: sd_notify() API 사용 필수 (watchdog 통합)
- `CPUSchedulingPolicy=fifo`: 실시간 스케줄링
- `CPUAffinity=2 3`: CPU 격리 (isolcpus 커널 파라미터와 일치)
- `WatchdogSec=30s`: 30초 내 watchdog 알림 필요

---

## 2. Watchdog 통합 방식

### 결정 사항

**sd_notify() API를 사용한 watchdog 통합**을 채택합니다.

```cpp
// libsystemd API 사용
#include <systemd/sd-daemon.h>

class SdNotifyWatchdog : public IWatchdogNotifier {
public:
    void notify() override {
        sd_notify(0, "WATCHDOG=1");
    }

    void ready() override {
        sd_notify(0, "READY=1");
    }

    void stopping() override {
        sd_notify(0, "STOPPING=1");
    }
};
```

### 근거

**장점**:
- **표준 API**: systemd 공식 지원 API로 안정성 보장
- **단순성**: 단일 함수 호출로 watchdog 알림 전송
- **저지연**: 오버헤드 < 10μs (목표 충족)
- **추가 기능**: READY, STOPPING, STATUS 등 다양한 알림 지원

**대안 고려**:
- **D-Bus API 직접 사용**
  - **기각 사유**: 복잡성 증가, 오버헤드 더 큼 (수백 μs). sd_notify()가 내부적으로 D-Bus 사용하지 않고 Unix socket 사용.

- **파일 기반 heartbeat** (/run/mxrc-watchdog)
  - **기각 사유**: systemd와의 통합 불가. 별도 watchdog 데몬 필요. 표준 방식 아님.

### 모범 사례

**Watchdog 타이머 구현**:
```cpp
class WatchdogTimer {
private:
    std::atomic<bool> running_{false};
    std::thread timer_thread_;
    std::chrono::milliseconds interval_;
    std::shared_ptr<IWatchdogNotifier> notifier_;

public:
    WatchdogTimer(std::chrono::milliseconds interval,
                  std::shared_ptr<IWatchdogNotifier> notifier)
        : interval_(interval), notifier_(notifier) {}

    void start() {
        running_ = true;
        timer_thread_ = std::thread([this]() {
            while (running_) {
                notifier_->notify();
                std::this_thread::sleep_for(interval_);
            }
        });
    }

    void stop() {
        running_ = false;
        if (timer_thread_.joinable()) {
            timer_thread_.join();
        }
    }
};
```

**타이밍 가이드라인**:
- WatchdogSec=30s로 설정
- watchdog 알림 주기 = WatchdogSec / 2 = 15초
- 안전 마진 포함 시 10초 주기 권장

---

## 3. Prometheus 메트릭 수집 전략

### 결정 사항

**systemctl show 파싱 방식**을 채택하여 systemd 메트릭을 수집하고 Prometheus 형식으로 노출합니다.

```cpp
class SystemdMetricsCollector : public ISystemdMetricsCollector {
public:
    std::vector<SystemdMetric> collect(const std::string& service_name) override {
        // systemctl show mxrc-rt.service --property=RestartCount,CPUUsageNSec,MemoryCurrent
        auto output = executeCommand("systemctl show " + service_name +
                                     " --property=RestartCount,CPUUsageNSec,MemoryCurrent");
        return parseSystemctlOutput(output);
    }
};
```

### 근거

**장점**:
- **단순성**: systemctl CLI 도구 활용, 별도 라이브러리 불필요
- **안정성**: systemctl은 안정적인 인터페이스 제공
- **풍부한 메트릭**: 15개 이상의 systemd 메트릭 제공
- **구현 용이성**: 텍스트 파싱만으로 충분

**대안 고려**:
- **D-Bus API 사용** (org.freedesktop.systemd1)
  - **장점**: 직접 API 호출로 더 빠를 수 있음
  - **기각 사유**:
    - libsystemd에 D-Bus 클라이언트 코드 추가 필요 (복잡성)
    - systemctl show도 내부적으로 D-Bus 사용하므로 성능 차이 미미
    - 유지보수성 낮음 (API 변경 가능성)

- **Prometheus systemd_exporter 사용**
  - **기각 사유**: 외부 프로세스 의존성 추가. MXRC 통합 어려움. 커스텀 메트릭 추가 제한적.

### 모범 사례

**수집할 systemd 메트릭**:
```
# 서비스 안정성
- systemd_service_restart_count{service="mxrc-rt"}
- systemd_service_active_state{service="mxrc-rt"}

# 리소스 사용량
- systemd_service_cpu_usage_nsec{service="mxrc-rt"}
- systemd_service_memory_current_bytes{service="mxrc-rt"}
- systemd_service_tasks_current{service="mxrc-rt"}

# Watchdog
- systemd_service_watchdog_usec{service="mxrc-rt"}
- systemd_service_watchdog_timestamp{service="mxrc-rt"}

# cgroups
- systemd_service_cpu_quota_ratio{service="mxrc-rt"}
- systemd_service_memory_max_bytes{service="mxrc-rt"}
```

**Prometheus 노출 방법**:
- 기존 Prometheus HTTP 서버 활용 (Phase 1에서 구축됨)
- `/metrics` 엔드포인트에 systemd 메트릭 추가
- 수집 주기: 1초 (spec 요구사항)

---

## 4. journald 구조화 로깅 통합

### 결정 사항

**sd_journal_send() API를 사용한 직접 통합**을 채택하되, 기존 StructuredLogger와 연동합니다.

```cpp
class JournaldLogger : public IJournaldLogger {
public:
    void log(const JournaldEntry& entry) override {
        sd_journal_send(
            "MESSAGE=%s", entry.message.c_str(),
            "PRIORITY=%d", entry.priority,
            "TRACE_ID=%s", entry.trace_id.c_str(),
            "SPAN_ID=%s", entry.span_id.c_str(),
            "SERVICE=%s", entry.service.c_str(),
            nullptr
        );
    }
};
```

### 근거

**장점**:
- **표준 API**: systemd 공식 journald API
- **구조화 데이터**: 키-값 쌍으로 메타데이터 전송
- **저지연**: < 1ms (목표 충족)
- **W3C Trace Context 통합**: trace_id, span_id 필드 추가 가능
- **ECS 호환성**: 필드 이름 매핑으로 ECS 스키마 준수

**대안 고려**:
- **spdlog journald sink 사용**
  - **장점**: 기존 spdlog 인프라 활용
  - **기각 사유**:
    - spdlog journald sink가 구조화 필드 전송을 제한적으로 지원
    - W3C Trace Context 필드 추가 어려움
    - 커스텀 sink 구현 필요 시 sd_journal_send() 직접 사용이 더 단순

- **syslog 사용**
  - **기각 사유**: journald보다 기능 제한적. 구조화 로깅 미지원. systemd 통합 부족.

### 모범 사례

**기존 StructuredLogger와 통합**:
```cpp
class SystemdStructuredLogger : public IStructuredLogger {
private:
    std::shared_ptr<spdlog::logger> spdlog_logger_;
    std::shared_ptr<IJournaldLogger> journald_logger_;

public:
    void log(const StructuredLogEvent& event) override {
        // 1. spdlog로 파일 로깅
        spdlog_logger_->log(event.level, event.message);

        // 2. journald로 구조화 로깅
        JournaldEntry entry;
        entry.message = event.message;
        entry.priority = toJournaldPriority(event.level);
        entry.trace_id = event.trace_context.trace_id;
        entry.span_id = event.trace_context.span_id;
        entry.service = "mxrc-rt";

        journald_logger_->log(entry);
    }
};
```

**journald 필드 매핑 (ECS 호환)**:
```
MXRC Field          → journald Field    → ECS Field
--------------------|-------------------|------------------
trace_id            → TRACE_ID          → trace.id
span_id             → SPAN_ID           → trace.span.id
service             → SERVICE           → service.name
level               → PRIORITY          → log.level
message             → MESSAGE           → message
timestamp           → (자동)            → @timestamp
```

---

## 5. Seccomp 필터 구성

### 결정 사항

**Whitelist 기반 Seccomp 필터**를 사용하여 필수 syscall만 허용합니다.

```ini
[Service]
SystemCallFilter=@basic-io @file-system @io-event @ipc @network-io @process @signal @timer
SystemCallFilter=~@clock @cpu-emulation @debug @module @mount @obsolete @privileged @raw-io @reboot @resources @swap
SystemCallErrorNumber=EPERM
```

### 근거

**장점**:
- **보안 강화**: 불필요한 syscall 차단으로 공격 표면 축소
- **명확성**: systemd의 syscall set (@basic-io 등) 활용으로 가독성 향상
- **유지보수성**: systemd 버전 업데이트 시 syscall set 자동 갱신

**대안 고려**:
- **Blacklist 방식** (위험한 syscall만 차단)
  - **기각 사유**: 새로운 위험 syscall 추가 시 취약점 발생 가능. Whitelist가 더 안전.

- **커스텀 Seccomp BPF 프로그램**
  - **기각 사유**: 구현 복잡도 높음. systemd의 SystemCallFilter가 충분히 강력함.

### 모범 사례

**RT 프로세스용 필수 syscall 세트**:
```
@basic-io      - read, write, open, close 등 기본 I/O
@file-system   - stat, access, mkdir 등 파일 시스템
@io-event      - epoll, poll, select 등 이벤트
@ipc           - shmget, msgget 등 IPC
@network-io    - socket, bind, connect 등 네트워크
@process       - fork, exec, wait 등 프로세스 관리
@signal        - signal, sigaction 등 시그널
@timer         - nanosleep, timer_create 등 타이머
```

**차단할 위험 syscall 세트**:
```
@clock         - settimeofday, adjtimex (시간 변경)
@module        - init_module, delete_module (커널 모듈)
@mount         - mount, umount (마운트)
@privileged    - setuid, setgid (권한 변경)
@reboot        - reboot, kexec_load (재부팅)
@swap          - swapon, swapoff (스왑)
```

**테스트 전략**:
1. Seccomp 필터 없이 정상 동작 확인
2. Seccomp 필터 적용 후 기능 테스트
3. journald에서 차단된 syscall 확인 (`journalctl -xe | grep SECCOMP`)
4. 필요 시 whitelist에 syscall 추가

---

## 6. 성능 최적화 전략

### 결정 사항

다음 최적화 기법을 조합하여 성능 목표를 달성합니다:

1. **CPU 격리 (isolcpus)**: RT 프로세스 전용 CPU 코어 할당
2. **NUMA 인식 메모리 할당**: numactl로 로컬 메모리 접근
3. **비동기 로깅**: spdlog async logger로 I/O 지연 최소화
4. **Lock-free 자료구조**: TBB concurrent_hash_map 활용
5. **메모리 사전 할당**: mlockall()로 페이지 폴트 방지

### 근거

**RT Jitter < 50μs 달성 전략**:
- isolcpus=2,3으로 CPU 2-3번 격리 → 스케줄러 경합 제거
- FIFO 스케줄링 우선순위 80 → 높은 우선순위 보장
- mlockall(MCL_CURRENT | MCL_FUTURE) → 페이지 폴트 방지
- 비동기 로깅 → 로그 I/O가 RT 경로에 영향 없음

**Watchdog 오버헤드 < 10μs 달성 전략**:
- sd_notify()는 Unix socket write만 수행 → 매우 빠름
- 별도 Non-RT 스레드에서 watchdog 타이머 실행 → RT 경로 간섭 없음

**journald 지연 < 1ms 달성 전략**:
- sd_journal_send()는 비동기 전송
- 필드 개수 최소화 (< 10개)
- 메시지 크기 제한 (< 1KB)

### 모범 사례

**NUMA 최적화**:
```ini
[Service]
# CPU 2-3이 NUMA node 0에 속한다고 가정
ExecStart=/usr/bin/numactl --cpunodebind=0 --membind=0 /usr/local/bin/mxrc-rt
```

**메모리 잠금**:
```cpp
// RT 프로세스 시작 시
if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
    logger->error("mlockall failed: {}", strerror(errno));
}
```

**성능 측정**:
```cpp
// RT jitter 측정 (cyclictest 활용)
cyclictest -p 80 -m -n -i 1000 -l 100000

// watchdog 오버헤드 측정
auto start = std::chrono::high_resolution_clock::now();
sd_notify(0, "WATCHDOG=1");
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
```

---

## 7. 보안 강화 계층

### 결정 사항

**Defense in Depth** 접근 방식으로 다층 보안을 구현합니다:

1. **Filesystem 격리**: PrivateTmp, ProtectSystem, ReadWritePaths
2. **Capabilities 제한**: CAP_SYS_NICE, CAP_IPC_LOCK만 허용
3. **Seccomp 필터**: Whitelist 기반 syscall 제한
4. **네임스페이스 격리**: PrivateNetwork (monitor 제외)
5. **리소스 제한**: cgroups v2로 CPU/메모리/I/O 제한

### 근거

**PrivateTmp=yes**:
- /tmp, /var/tmp를 서비스 전용 네임스페이스로 격리
- Symlink 공격, 공유 /tmp 취약점 방지

**ProtectSystem=strict**:
- /, /boot, /usr, /etc를 읽기 전용으로 마운트
- ReadWritePaths=/var/lib/mxrc만 쓰기 허용
- 시스템 파일 변조 방지

**Capabilities 최소화**:
- CAP_SYS_NICE: FIFO 스케줄링에 필요
- CAP_IPC_LOCK: mlockall()에 필요
- 그 외 모든 capabilities 제거 (root 권한 불필요)

### 모범 사례

**보안 강화 설정 전체**:
```ini
[Service]
# Filesystem 격리
PrivateTmp=yes
ProtectSystem=strict
ProtectHome=yes
ReadWritePaths=/var/lib/mxrc
ReadOnlyPaths=/etc/mxrc

# Capabilities 제한
CapabilityBoundingSet=CAP_SYS_NICE CAP_IPC_LOCK
AmbientCapabilities=CAP_SYS_NICE CAP_IPC_LOCK

# Seccomp
SystemCallFilter=@basic-io @file-system @io-event @ipc @network-io @process @signal @timer
SystemCallFilter=~@clock @module @mount @privileged @reboot @swap
SystemCallErrorNumber=EPERM
SeccompArchitectures=native

# 추가 격리
ProtectKernelTunables=yes
ProtectKernelModules=yes
ProtectControlGroups=yes
RestrictRealtime=no  # RT 스케줄링 필요
RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6
LockPersonality=yes
NoNewPrivileges=yes
```

**보안 검증**:
```bash
# systemd-analyze security로 보안 점수 확인
systemd-analyze security mxrc-rt.service

# 목표: 8.0/10.0 이상 (spec 요구사항)
```

---

## 8. 부팅 시간 최적화

### 결정 사항

**병렬 시작 + 의존성 최소화**를 통해 부팅 시간을 단축합니다.

```ini
[Unit]
# 필수 의존성만 선언
After=network.target
Requires=network.target

# DefaultDependencies=no로 불필요한 의존성 제거
DefaultDependencies=no
```

### 근거

**30% 부팅 시간 단축 전략**:
1. **DefaultDependencies=no**: systemd의 기본 의존성 (basic.target, sysinit.target 등) 제거
2. **After/Requires 최소화**: 꼭 필요한 서비스만 의존
3. **병렬 시작**: RT/Non-RT/Monitor 서비스를 동시에 시작
4. **Type=notify**: READY=1 알림으로 실제 준비 완료 시점 정확히 전달

### 모범 사례

**의존성 그래프**:
```
network.target
    ↓
mxrc-rt.service (병렬) ← mxrc.target
mxrc-nonrt.service (병렬) ←┘
mxrc-monitor.service (병렬, After=mxrc-rt.service)
```

**부팅 시간 측정**:
```bash
# 전체 부팅 시간
systemd-analyze

# 서비스별 시작 시간
systemd-analyze blame | grep mxrc

# Critical chain 분석
systemd-analyze critical-chain mxrc.target
```

**최적화 목표**:
- mxrc-rt.service 시작 시간 < 500ms
- mxrc.target 전체 시작 시간 < 1.5s
- 기존 대비 30% 단축 (예: 2s → 1.4s)

---

## 9. 테스트 전략

### 단위 테스트

**GoogleTest 기반 테스트**:
- `WatchdogNotifierTest`: sd_notify() 호출 검증 (mock)
- `MetricsCollectorTest`: systemctl show 파싱 검증
- `JournaldLoggerTest`: sd_journal_send() 호출 검증 (mock)

### 통합 테스트

**실제 systemd 환경 테스트**:
- `SystemdIntegrationTest`: 실제 systemd에서 서비스 시작/중지/재시작
- `WatchdogIntegrationTest`: watchdog 타임아웃 시 재시작 검증
- `FailoverIntegrationTest`: 프로세스 크래시 시 자동 복구 검증

### 성능 테스트

**실시간 성능 검증**:
- cyclictest로 RT jitter 측정 (목표: < 50μs)
- perf로 watchdog 오버헤드 측정 (목표: < 10μs)
- strace로 journald 지연 측정 (목표: < 1ms)

### 보안 테스트

**보안 강화 검증**:
- systemd-analyze security로 보안 점수 확인 (목표: ≥ 8.0/10.0)
- Seccomp 필터 테스트: 차단된 syscall 호출 시 EPERM 확인
- Capabilities 테스트: 불필요한 권한 획득 시도 차단 확인

---

## 10. 마이그레이션 계획

### 기존 시스템에서 전환

**현재 상태**:
- mxrc-rt.service, mxrc-nonrt.service 존재 (기본 설정)
- Watchdog 없음
- Prometheus 메트릭 없음
- journald 통합 없음

**마이그레이션 단계**:

1. **Phase 1: 코드 구현**
   - src/core/systemd/ 모듈 구현
   - 단위 테스트 작성 및 통과

2. **Phase 2: systemd Unit 파일 업데이트**
   - WatchdogSec, NotifyAccess 추가
   - 보안 설정 추가 (Seccomp, Capabilities 등)
   - 리소스 제한 추가 (cgroups)

3. **Phase 3: 통합 테스트**
   - 개발 환경에서 새 Unit 파일 테스트
   - watchdog 동작 검증
   - 성능 검증

4. **Phase 4: 프로덕션 배포**
   - systemctl daemon-reload
   - systemctl restart mxrc.target
   - 모니터링 및 롤백 준비

### 롤백 계획

**롤백 조건**:
- RT jitter > 50μs
- Watchdog false positive (정상 동작 중 재시작)
- 보안 설정으로 인한 기능 장애

**롤백 절차**:
1. 기존 Unit 파일 복원
2. systemctl daemon-reload
3. systemctl restart mxrc.target
4. 문제 분석 및 수정 후 재배포

---

## 결론

이 연구를 통해 다음 기술 결정을 확정했습니다:

1. **systemd Unit 구성**: 3개 독립 서비스 + 1개 target
2. **Watchdog**: sd_notify() API 사용
3. **Prometheus 메트릭**: systemctl show 파싱
4. **journald 로깅**: sd_journal_send() 직접 통합
5. **Seccomp**: Whitelist 기반 필터
6. **성능 최적화**: CPU 격리 + NUMA + 비동기 로깅
7. **보안 강화**: Defense in Depth (다층 보안)
8. **부팅 최적화**: 병렬 시작 + 의존성 최소화

모든 결정은 MXRC Constitution의 7가지 원칙을 준수하며, 성능 목표(RT jitter < 50μs, watchdog < 10μs, journald < 1ms)와 보안 목표(≥ 8.0/10.0)를 달성할 수 있도록 설계되었습니다.

다음 단계는 Phase 1 (Design)로, data-model.md, contracts/, quickstart.md를 작성합니다.

---

**작성자**: MXRC Development Team
**검토자**: TBD
**승인 날짜**: TBD
