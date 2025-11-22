# Quickstart: systemd 기반 프로세스 관리 고도화

**Feature**: 018-systemd-process-management
**Phase**: Phase 11 - Documentation Polish (Final)
**Status**: Production Ready
**Last Updated**: 2025-01-22

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: systemd, API, JSON 등)
- 일반 설명, 명령어 설명은 모두 한글로 작성합니다

---

## 개요

이 가이드는 MXRC systemd 통합 기능을 설치, 설정, 실행하는 방법을 설명합니다. 5분 이내에 RT/Non-RT 프로세스를 systemd로 관리하고, Watchdog, Prometheus 메트릭, journald 로깅을 활성화할 수 있습니다.

---

## 사전 요구사항

### 시스템 요구사항

- **OS**: Ubuntu 24.04 LTS (PREEMPT_RT 커널)
- **systemd 버전**: v255 이상
- **CPU**: 최소 4코어 (RT용 2코어 격리 권장)
- **메모리**: 최소 2GB RAM

### 소프트웨어 요구사항

```bash
# systemd 버전 확인
systemd --version
# 출력: systemd 255 (255.4-1ubuntu8)

# libsystemd 설치
sudo apt-get update
sudo apt-get install -y libsystemd-dev

# 기타 의존성
sudo apt-get install -y \
    build-essential \
    cmake \
    libspdlog-dev \
    nlohmann-json3-dev \
    libgtest-dev \
    libtbb-dev
```

### PREEMPT_RT 커널 확인

```bash
# RT 커널 확인
uname -v
# 출력: #1 SMP PREEMPT_RT ...

# CPU 격리 확인 (grub 설정)
cat /proc/cmdline | grep isolcpus
# 출력: ... isolcpus=2,3 ...
```

**isolcpus 설정이 없는 경우**:
```bash
# /etc/default/grub 편집
sudo nano /etc/default/grub

# GRUB_CMDLINE_LINUX에 추가:
GRUB_CMDLINE_LINUX="... isolcpus=2,3"

# 적용
sudo update-grub
sudo reboot
```

---

## 빌드 및 설치

### 1. 프로젝트 클론 및 빌드

```bash
# 프로젝트 디렉토리로 이동
cd /path/to/mxrc

# 빌드 디렉토리 생성
mkdir -p build && cd build

# CMake 구성 (Release 모드, systemd 통합 활성화)
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_SYSTEMD_INTEGRATION=ON \
    -DENABLE_PROMETHEUS=ON \
    -DENABLE_JOURNALD=ON

# 빌드 (병렬)
make -j$(nproc)

# 테스트 실행
./run_tests

# 설치
sudo make install
```

### 2. 설치 확인

```bash
# 바이너리 확인
which mxrc-rt mxrc-nonrt mxrc-monitor
# 출력: /usr/local/bin/mxrc-rt, /usr/local/bin/mxrc-nonrt, /usr/local/bin/mxrc-monitor

# 설정 파일 확인
ls -la /etc/mxrc/
# 출력:
# rt-config.json
# nonrt-config.json
# monitor-config.json
# watchdog.json
# metrics.json
# security.json
```

---

## 설정

### 1. systemd Unit 파일 설치

```bash
# Unit 파일 복사
sudo cp systemd/*.service /etc/systemd/system/
sudo cp systemd/*.target /etc/systemd/system/

# systemd 리로드
sudo systemctl daemon-reload

# Unit 파일 확인
systemctl list-unit-files | grep mxrc
# 출력:
# mxrc-rt.service           disabled
# mxrc-nonrt.service        disabled
# mxrc-monitor.service      disabled
# mxrc.target               disabled
```

### 2. 설정 파일 편집 (선택사항)

#### RT 프로세스 설정 (`/etc/mxrc/rt-config.json`)

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
  }
}
```

#### Watchdog 설정 (`/etc/mxrc/watchdog.json`)

```json
{
  "enabled": true,
  "interval_ms": 10000,
  "timeout_sec": 30,
  "notify_on_start": true,
  "notify_on_stop": true
}
```

#### 메트릭 수집 설정 (`/etc/mxrc/metrics.json`)

```json
{
  "enabled": true,
  "interval_ms": 1000,
  "services": ["mxrc-rt", "mxrc-nonrt"],
  "properties": [
    "ActiveState",
    "RestartCount",
    "CPUUsageNSec",
    "MemoryCurrent",
    "TasksCurrent"
  ]
}
```

### 3. 권한 설정

```bash
# 설정 파일 권한 (root 읽기 전용)
sudo chmod 644 /etc/mxrc/*.json

# 데이터 디렉토리 생성
sudo mkdir -p /var/lib/mxrc
sudo chown mxrc:mxrc /var/lib/mxrc
sudo chmod 750 /var/lib/mxrc
```

---

## 서비스 시작

### 1. 개별 서비스 시작

```bash
# RT 프로세스 시작
sudo systemctl start mxrc-rt.service

# 상태 확인
sudo systemctl status mxrc-rt.service
# 출력:
# ● mxrc-rt.service - MXRC RT Process
#      Loaded: loaded (/etc/systemd/system/mxrc-rt.service; disabled; preset: enabled)
#      Active: active (running) since Tue 2025-01-21 10:00:00 KST; 5s ago
#    Main PID: 12345 (mxrc-rt)
#      Status: "Service ready"
#       Tasks: 12 (limit: 100)
#      Memory: 128.0M (max: 512.0M)
#         CPU: 125ms

# Non-RT 프로세스 시작
sudo systemctl start mxrc-nonrt.service

# 모니터링 프로세스 시작
sudo systemctl start mxrc-monitor.service
```

### 2. 전체 서비스 그룹 시작

```bash
# mxrc.target 시작 (모든 서비스 동시 시작)
sudo systemctl start mxrc.target

# 상태 확인
sudo systemctl status mxrc.target
# 출력:
# ● mxrc.target - MXRC Service Group
#      Loaded: loaded (/etc/systemd/system/mxrc.target; disabled; preset: enabled)
#      Active: active since Tue 2025-01-21 10:00:00 KST; 10s ago

# 의존 서비스 확인
systemctl list-dependencies mxrc.target
# 출력:
# mxrc.target
# ● ├─mxrc-monitor.service
# ● ├─mxrc-nonrt.service
# ● └─mxrc-rt.service
```

### 3. 부팅 시 자동 시작

```bash
# mxrc.target 활성화 (부팅 시 자동 시작)
sudo systemctl enable mxrc.target

# 확인
systemctl is-enabled mxrc.target
# 출력: enabled
```

---

## 리소스 제어 (User Story 3)

### 1. cgroups 리소스 제한

mxrc-rt.service와 mxrc-nonrt.service는 cgroups를 통해 CPU, 메모리, I/O 리소스를 제어합니다:

```bash
# RT 프로세스 리소스 제한
# - CPUQuota=200% (2 코어 최대)
# - MemoryMax=2G
# - IOWeight=500 (높은 I/O 우선순위)

# Non-RT 프로세스 리소스 제한
# - CPUQuota=100% (1 코어 최대)
# - MemoryMax=1G
# - IOWeight=100 (기본 I/O 우선순위)
```

### 2. 리소스 사용량 모니터링

```bash
# 실시간 리소스 모니터링 (systemd-cgtop)
./scripts/monitor-cgroups.sh

# cgroups 설정 검증
./scripts/verify-cgroups.sh mxrc-rt
./scripts/verify-cgroups.sh mxrc-nonrt

# 수동 확인 (cgroup v2)
cat /sys/fs/cgroup/system.slice/mxrc-rt.service/cpu.max
# 출력: 200000 100000 (200% quota)

cat /sys/fs/cgroup/system.slice/mxrc-rt.service/memory.max
# 출력: 2147483648 (2GB)

cat /sys/fs/cgroup/system.slice/mxrc-rt.service/io.weight
# 출력: default 500
```

### 3. 리소스 제한 조정

```bash
# 런타임 CPU quota 변경 (재시작 불필요)
sudo systemctl set-property mxrc-rt.service CPUQuota=300%

# 메모리 제한 변경
sudo systemctl set-property mxrc-rt.service MemoryMax=3G

# I/O 가중치 변경
sudo systemctl set-property mxrc-rt.service IOWeight=700

# 변경사항 영구 저장
sudo systemctl daemon-reload
```

---

## 모니터링

### 1. 서비스 상태 확인

```bash
# 실시간 로그 모니터링
journalctl -u mxrc-rt.service -f

# 최근 100줄 로그
journalctl -u mxrc-rt.service -n 100

# 오류 로그만 (PRIORITY 0-3)
journalctl -u mxrc-rt.service PRIORITY=0..3
```

### 2. Watchdog 확인

```bash
# Watchdog 타임아웃 값 확인
systemctl show mxrc-rt.service --property=WatchdogUSec
# 출력: WatchdogUSec=30s

# 마지막 Watchdog 알림 시각
systemctl show mxrc-rt.service --property=WatchdogTimestampMonotonic
# 출력: WatchdogTimestampMonotonic=1234567890
```

### 3. Prometheus 메트릭 확인

```bash
# Prometheus 메트릭 서비스 시작
sudo systemctl start mxrc-metrics.service

# Prometheus 엔드포인트 접속 (포트 9100)
curl http://localhost:9100/metrics

# 출력 예시:
# HELP mxrc_service_state Service state (1=active, 0=inactive)
# TYPE mxrc_service_state gauge
# mxrc_service_state{service="mxrc-rt"} 1
# mxrc_service_state{service="mxrc-nonrt"} 1
#
# HELP mxrc_cpu_usage_seconds_total Total CPU time in seconds
# TYPE mxrc_cpu_usage_seconds_total counter
# mxrc_cpu_usage_seconds_total{service="mxrc-rt"} 123.456789
# mxrc_cpu_usage_seconds_total{service="mxrc-nonrt"} 45.678901
#
# HELP mxrc_memory_bytes Memory usage in bytes
# TYPE mxrc_memory_bytes gauge
# mxrc_memory_bytes{service="mxrc-rt"} 2147483648
# mxrc_memory_bytes{service="mxrc-nonrt"} 1073741824
#
# HELP mxrc_restart_count_total Number of service restarts
# TYPE mxrc_restart_count_total counter
# mxrc_restart_count_total{service="mxrc-rt"} 0
# mxrc_restart_count_total{service="mxrc-nonrt"} 0

# Health check 엔드포인트
curl http://localhost:9100/health
# 출력: OK

# 수동으로 메트릭 수집 스크립트 실행
python3 scripts/prometheus-exporter.py
```

### 4. journald 구조화 로깅 확인

```bash
# Trace ID로 로그 추적
journalctl TRACE_ID=4bf92f3577b34da6a3ce929d0e0e4736

# 컴포넌트별 로그 조회
journalctl COMPONENT=task

# JSON 출력
journalctl SERVICE=mxrc-rt -o json-pretty
```

---

## 부팅 최적화 (User Story 8)

MXRC 서비스는 빠른 부팅 시간을 위해 최적화되어 있습니다.

### 1. Type=notify로 빠른 시작

RT 서비스는 `Type=notify`로 설정되어 있어, sd_notify()로 준비 완료 신호를 보내면 즉시 다른 서비스들이 시작됩니다:

```bash
# RT 서비스 Type 확인
systemctl show mxrc-rt.service --property=Type
# 출력: Type=notify

# Non-RT 서비스는 Type=simple (더 빠른 시작)
systemctl show mxrc-nonrt.service --property=Type
# 출력: Type=simple
```

### 2. 최소한의 의존성

불필요한 부팅 지연을 방지하기 위해 필수 의존성만 설정되어 있습니다:

```bash
# RT 서비스 의존성 확인
systemctl show mxrc-rt.service --property=After
# 출력: After=network.target mxrc-nonrt.service

# Non-RT 서비스 의존성 확인
systemctl show mxrc-nonrt.service --property=After
# 출력: After=network.target
```

**주의**: `multi-user.target`, `graphical.target` 등 불필요한 의존성은 제거되어 있습니다.

### 3. 타임아웃 설정

모든 서비스는 적절한 타임아웃이 설정되어 있어 부팅 블로킹을 방지합니다:

```bash
# 시작 타임아웃 확인 (30초 이하)
systemctl show mxrc-rt.service --property=TimeoutStartUSec
# 출력: TimeoutStartUSec=30s

systemctl show mxrc-nonrt.service --property=TimeoutStartUSec
# 출력: TimeoutStartUSec=30s
```

### 4. 부팅 시간 측정

```bash
# systemd 부팅 분석
systemd-analyze blame | grep mxrc
# 출력 예시:
#   1.234s mxrc-rt.service
#   0.567s mxrc-nonrt.service
#   0.123s mxrc-monitor.service

# 전체 부팅 시간 확인
systemd-analyze
# 출력:
# Startup finished in 2.345s (kernel) + 3.456s (userspace) = 5.801s

# Critical chain 분석 (부팅 경로)
systemd-analyze critical-chain mxrc-rt.service
# 출력:
# mxrc-rt.service +1.234s
# └─mxrc-nonrt.service @0.567s +0.567s
#   └─network.target @0.123s
#     └─NetworkManager.service @0.050s +0.073s
```

### 5. 부팅 최적화 팁

**sd_notify() 호출 타이밍**:
```cpp
// 초기화 완료 후 즉시 sd_notify 호출
void RTExecutive::initialize() {
    // 필수 초기화만 수행
    initializeRT();
    initializeIPC();

    // 준비 완료 신호 (부팅 지연 최소화)
    sd_notify(0, "READY=1");

    // 나머지 초기화는 백그라운드에서
    initializeMonitoring();
    initializeMetrics();
}
```

**부팅 병렬화**:
```bash
# mxrc-nonrt.service와 mxrc-rt.service는 순차 실행
# (Before/After 의존성)

# mxrc-monitor.service는 병렬 실행 가능
# (의존성 없음, 독립적으로 시작)
```

---

## 성능 검증

### 1. RT Jitter 측정

```bash
# cyclictest 설치
sudo apt-get install rt-tests

# RT jitter 측정 (목표: < 50μs)
sudo cyclictest -p 80 -m -n -i 1000 -l 100000
# 출력:
# T: 0 ( 1234) P:80 I:1000 C: 100000 Min:      3 Act:    8 Avg:    7 Max:      42
```

### 2. Watchdog 오버헤드 측정

```bash
# perf로 sd_notify() 호출 시간 측정
sudo perf stat -e cycles,instructions ./mxrc-rt --benchmark-watchdog

# 목표: < 10μs
```

### 3. journald 지연 측정

```bash
# strace로 sd_journal_send() 호출 시간 측정
sudo strace -T -e trace=sendmsg ./mxrc-rt

# 목표: < 1ms
```

---

## 보안 강화 (User Story 7)

MXRC 서비스는 다층 보안(Defense in Depth) 원칙을 따릅니다.

### 1. 최소 권한 원칙 (Principle of Least Privilege)

**Capabilities 제한**:
```bash
# RT 프로세스: 필요한 capability만 허용
systemctl show mxrc-rt.service --property=AmbientCapabilities
# 출력: AmbientCapabilities=CAP_SYS_NICE CAP_IPC_LOCK

systemctl show mxrc-rt.service --property=CapabilityBoundingSet
# 출력: CapabilityBoundingSet=CAP_SYS_NICE CAP_IPC_LOCK

# Non-RT 프로세스: capability 없음
systemctl show mxrc-nonrt.service --property=AmbientCapabilities
# 출력: AmbientCapabilities= (비어있음)
```

**User/Group 격리**:
```bash
# 전용 시스템 사용자로 실행 (root 아님)
systemctl show mxrc-rt.service --property=User
# 출력: User=mxrc

systemctl show mxrc-rt.service --property=Group
# 출력: Group=mxrc

# 사용자 확인
id mxrc
# 출력: uid=999(mxrc) gid=999(mxrc) groups=999(mxrc)
```

### 2. 파일시스템 격리

**ProtectSystem=strict**:
```bash
# /usr, /boot, /efi를 읽기 전용으로 마운트
systemctl show mxrc-rt.service --property=ProtectSystem
# 출력: ProtectSystem=strict

# /var, /etc도 읽기 전용 (ReadWritePaths 예외)
```

**ReadWritePaths 최소화**:
```bash
# 필요한 경로만 쓰기 허용
systemctl show mxrc-rt.service --property=ReadWritePaths
# 출력: ReadWritePaths=/var/lib/mxrc /var/log/mxrc /tmp/mxrc

# 다른 경로는 모두 읽기 전용
```

**ProtectHome**:
```bash
# 홈 디렉토리 접근 차단
systemctl show mxrc-rt.service --property=ProtectHome
# 출력: ProtectHome=yes
```

**PrivateTmp**:
```bash
# 격리된 /tmp 사용 (다른 프로세스와 분리)
systemctl show mxrc-rt.service --property=PrivateTmp
# 출력: PrivateTmp=yes
```

### 3. 권한 상승 방지

**NoNewPrivileges**:
```bash
# setuid/setgid 실행 방지
systemctl show mxrc-rt.service --property=NoNewPrivileges
# 출력: NoNewPrivileges=yes

# 자식 프로세스가 더 높은 권한을 가질 수 없음
```

### 4. 보안 검증

**systemd-analyze security**:
```bash
# 보안 점수 확인 (목표: ≥ 8.0/10.0)
systemd-analyze security mxrc-rt.service

# 출력 예시:
# NAME                  DESCRIPTION                  EXPOSURE
# ✓ PrivateDevices=     Service has no access to hardware devices
# ✓ ProtectClock=       Service cannot write to the system clock
# ✓ ProtectKernelLogs=  Service cannot read from or write to the kernel log ring buffer
# ✓ ProtectKernelModules= Service cannot load or read kernel modules
# ✓ ProtectKernelTunables= Service cannot alter kernel tunables
# ✓ ProtectControlGroups= Service cannot modify the control group file system
# ✓ ProtectHome=        Service has no access to home directories
# ✓ ProtectSystem=      Service has strict read-only access to the OS file hierarchy
# ✓ NoNewPrivileges=    Service processes cannot acquire new privileges
# ✓ PrivateTmp=         Service has a private /tmp/ and /var/tmp/ directories
# ...
# → Overall exposure level for mxrc-rt.service: 8.2 (OK) 🙂
```

### 5. 보안 모니터링

**실패한 권한 요청 추적**:
```bash
# 권한 관련 오류 모니터링
journalctl -u mxrc-rt.service | grep -i "permission denied"
journalctl -u mxrc-rt.service | grep -i "operation not permitted"

# Seccomp 위반 (syscall 차단)
journalctl -xe | grep SECCOMP

# Capability 부족 오류
journalctl -u mxrc-rt.service | grep -i "capability"
```

**침입 탐지 (Audit)**:
```bash
# auditd 활성화 (선택적)
sudo apt-get install -y auditd

# mxrc 프로세스 감사 규칙 추가
sudo auditctl -w /usr/local/bin/mxrc-rt -p x -k mxrc_exec
sudo auditctl -w /etc/mxrc/ -p wa -k mxrc_config

# 감사 로그 확인
sudo ausearch -k mxrc_exec
sudo ausearch -k mxrc_config
```

---

## 보안 검증

### 1. systemd-analyze security

```bash
# 보안 점수 확인 (목표: ≥ 8.0/10.0)
systemd-analyze security mxrc-rt.service

# 출력 예시:
# NAME                  DESCRIPTION                  EXPOSURE
# ...
# → Overall exposure level for mxrc-rt.service: 8.2 (OK) 🙂
```

### 2. Seccomp 필터 확인

```bash
# Seccomp 상태 확인
systemctl show mxrc-rt.service --property=SystemCallFilter

# 출력:
# SystemCallFilter=@basic-io @file-system @io-event @ipc @network-io @process @signal @timer

# 차단된 syscall 시도 로그
journalctl -xe | grep SECCOMP
```

### 3. Capabilities 확인

```bash
# Capabilities 확인
systemctl show mxrc-rt.service --property=CapabilityBoundingSet

# 출력:
# CapabilityBoundingSet=CAP_SYS_NICE CAP_IPC_LOCK

# 프로세스 Capabilities 확인
sudo cat /proc/$(pidof mxrc-rt)/status | grep Cap
```

---

## 트러블슈팅

### 1. 서비스 시작 실패

**증상**: `systemctl start mxrc-rt.service` 실패

**원인 확인**:
```bash
# 상세 로그 확인
sudo systemctl status mxrc-rt.service -l
sudo journalctl -xe -u mxrc-rt.service
```

**일반적인 원인**:
- 설정 파일 오류 → `/etc/mxrc/*.json` 검증
- 권한 문제 → `/var/lib/mxrc` 권한 확인
- CPU 격리 미설정 → `isolcpus` 커널 파라미터 확인

### 2. Watchdog 타임아웃

**증상**: 서비스가 자동으로 재시작됨

**원인 확인**:
```bash
# Watchdog 관련 로그
journalctl -u mxrc-rt.service | grep -i watchdog

# 출력 예시:
# Jan 21 10:05:00 systemd[1]: mxrc-rt.service: Watchdog timeout (no keep-alive ping within 30s)
```

**해결 방법**:
- WatchdogSec 값 증가 (30s → 60s)
- watchdog.json의 interval_ms 감소 (15000 → 10000)
- 프로세스 블로킹 원인 조사 (데드락, 무한 루프 등)

### 3. Prometheus 메트릭 수집 실패

**증상**: `/metrics` 엔드포인트에 systemd 메트릭 없음

**원인 확인**:
```bash
# mxrc-monitor 서비스 상태 확인
sudo systemctl status mxrc-monitor.service

# 메트릭 수집 로그
journalctl -u mxrc-monitor.service | grep -i metric
```

**해결 방법**:
- systemctl show 명령 권한 확인
- metrics.json 설정 검증
- mxrc-monitor 서비스 재시작

### 4. journald 로그 저장 안 됨

**증상**: `journalctl SERVICE=mxrc-rt` 출력 없음

**원인 확인**:
```bash
# journald 소켓 확인
ls -la /run/systemd/journal/socket
# 출력: srw-rw-rw- 1 root systemd-journal 0 Jan 21 10:00 socket

# sd_journal_send 호출 확인
sudo strace -e trace=sendmsg -p $(pidof mxrc-rt)
```

**해결 방법**:
- journald 서비스 재시작: `sudo systemctl restart systemd-journald`
- /var/log/journal/ 권한 확인
- journald 영구 저장 활성화: `/etc/systemd/journald.conf`에 `Storage=persistent`

### 5. RT Jitter 목표 초과

**증상**: cyclictest 결과 Max > 50μs

**원인 확인**:
```bash
# CPU 격리 확인
cat /proc/cmdline | grep isolcpus

# CPU affinity 확인
taskset -pc $(pidof mxrc-rt)

# 실행 중인 프로세스 확인 (RT 코어에서)
ps -eLo pid,tid,class,rtprio,psr,comm | grep -E '(PID|^.*[23].*FIFO)'
```

**해결 방법**:
- isolcpus=2,3 커널 파라미터 추가 및 재부팅
- RT 코어에서 불필요한 프로세스 종료
- IRQ affinity 조정 (RT 코어에서 IRQ 제외)

---

## 고급 설정

### 1. CPU Affinity 동적 변경

```bash
# RT 프로세스를 CPU 4-5로 이동
sudo systemctl set-property mxrc-rt.service CPUAffinity="4 5"

# 적용 (재시작 필요)
sudo systemctl restart mxrc-rt.service
```

### 2. 메모리 제한 조정

```bash
# 메모리 제한을 1GB로 증가
sudo systemctl set-property mxrc-rt.service MemoryMax=1G

# 현재 메모리 사용량 확인
systemctl show mxrc-rt.service --property=MemoryCurrent
```

### 3. CPU Quota 조정

```bash
# CPU quota를 300%로 증가 (3개 코어 상당)
sudo systemctl set-property mxrc-rt.service CPUQuota=300%

# 확인
systemctl show mxrc-rt.service --property=CPUQuotaPerSecUSec
```

### 4. Watchdog 비활성화 (개발 환경)

```bash
# Unit 파일 편집
sudo systemctl edit mxrc-rt.service

# 추가:
[Service]
WatchdogSec=0

# 적용
sudo systemctl daemon-reload
sudo systemctl restart mxrc-rt.service
```

---

## 제거

### 1. 서비스 중지 및 비활성화

```bash
# 서비스 중지
sudo systemctl stop mxrc.target

# 부팅 시 자동 시작 비활성화
sudo systemctl disable mxrc.target
```

### 2. Unit 파일 제거

```bash
# Unit 파일 삭제
sudo rm /etc/systemd/system/mxrc*.service
sudo rm /etc/systemd/system/mxrc.target

# systemd 리로드
sudo systemctl daemon-reload
```

### 3. 바이너리 및 설정 제거

```bash
# 바이너리 제거
sudo rm /usr/local/bin/mxrc-rt
sudo rm /usr/local/bin/mxrc-nonrt
sudo rm /usr/local/bin/mxrc-monitor

# 설정 파일 제거
sudo rm -rf /etc/mxrc/

# 데이터 디렉토리 제거
sudo rm -rf /var/lib/mxrc/
```

---

## 다음 단계

1. **Grafana 대시보드 설정**: Prometheus 메트릭 시각화
2. **중앙 로그 수집기 연동**: journald → Logstash → Elasticsearch
3. **알림 설정**: Alertmanager로 Watchdog 타임아웃 알림
4. **성능 튜닝**: RT jitter 추가 최적화 (IRQ affinity, CPU governor 등)
5. **보안 강화**: SELinux/AppArmor 프로파일 추가

---

## 참고 자료

### 공식 문서
- [systemd.service](https://www.freedesktop.org/software/systemd/man/systemd.service.html)
- [systemd.exec](https://www.freedesktop.org/software/systemd/man/systemd.exec.html)
- [sd_notify](https://www.freedesktop.org/software/systemd/man/sd_notify.html)
- [sd_journal_send](https://www.freedesktop.org/software/systemd/man/sd_journal_send.html)

### MXRC 문서
- [Architecture](../../architecture/architecture.md)
- [Spec](spec.md)
- [Plan](plan.md)
- [Research](research.md)
- [Data Model](data-model.md)
- [Interface Contracts](contracts/)

---

**작성자**: MXRC Development Team
**검토자**: TBD
**승인 날짜**: TBD
