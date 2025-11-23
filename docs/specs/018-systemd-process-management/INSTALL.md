# MXRC systemd 통합 - 설치 가이드

**Feature**: 018-systemd-process-management
**문서 버전**: 1.0
**최종 수정**: 2025-01-22

---

## 목차

1. [시스템 요구사항](#시스템-요구사항)
2. [사전 준비](#사전-준비)
3. [빌드 및 설치](#빌드-및-설치)
4. [서비스 설정](#서비스-설정)
5. [사용자 및 그룹 설정](#사용자-및-그룹-설정)
6. [디렉토리 구조](#디렉토리-구조)
7. [설치 검증](#설치-검증)
8. [문제 해결](#문제-해결)

---

## 시스템 요구사항

### 하드웨어

- **CPU**: 최소 4코어 (RT 격리를 위해 6코어 이상 권장)
- **메모리**: 최소 4GB RAM (8GB 이상 권장)
- **디스크**: 최소 10GB 여유 공간

### 운영 체제

- **OS**: Ubuntu 24.04 LTS
- **커널**: PREEMPT_RT 패치 적용된 실시간 커널
  - 권장 버전: 6.6.87.2-rt 이상
- **systemd**: v255 이상

### 소프트웨어 의존성

```bash
# 필수 패키지
- build-essential (gcc/g++ 11 이상)
- cmake (3.20 이상)
- libsystemd-dev (255 이상)
- libspdlog-dev (1.10 이상)
- nlohmann-json3-dev (3.10 이상)
- libgtest-dev (1.12 이상)
- libtbb-dev (2021.5 이상)

# 선택적 패키지
- python3 (3.10 이상) - Prometheus exporter용
- rt-tests - RT 성능 측정용
- perf - 성능 프로파일링용
```

---

## 사전 준비

### 1. PREEMPT_RT 커널 설치

Ubuntu 24.04에서 PREEMPT_RT 커널 설치:

```bash
# RT 커널 패키지 확인
apt-cache search linux-image-rt

# RT 커널 설치
sudo apt-get update
sudo apt-get install -y \
    linux-image-rt-amd64 \
    linux-headers-rt-amd64

# 재부팅
sudo reboot

# 재부팅 후 커널 확인
uname -v
# 출력 예시: #1 SMP PREEMPT_RT Tue Jan 21 10:00:00 UTC 2025
```

### 2. CPU 격리 설정

GRUB 설정으로 RT 전용 CPU 격리:

```bash
# GRUB 설정 편집
sudo nano /etc/default/grub

# GRUB_CMDLINE_LINUX에 다음 추가:
# (CPU 2-3을 RT 전용으로 격리)
GRUB_CMDLINE_LINUX="isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3"

# GRUB 업데이트
sudo update-grub

# 재부팅
sudo reboot

# 재부팅 후 확인
cat /proc/cmdline | grep isolcpus
# 출력: ... isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3
```

**CPU 격리 파라미터 설명**:
- `isolcpus=2,3`: CPU 2-3을 일반 스케줄러에서 제외
- `nohz_full=2,3`: CPU 2-3에서 타이머 틱 최소화
- `rcu_nocbs=2,3`: CPU 2-3에서 RCU 콜백 오프로드

### 3. 의존성 설치

```bash
# 시스템 업데이트
sudo apt-get update
sudo apt-get upgrade -y

# 빌드 도구 설치
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config

# 라이브러리 설치
sudo apt-get install -y \
    libsystemd-dev \
    libspdlog-dev \
    nlohmann-json3-dev \
    libgtest-dev \
    libtbb-dev

# Python 및 도구 (선택적)
sudo apt-get install -y \
    python3 \
    python3-pip \
    rt-tests \
    linux-tools-common \
    linux-tools-generic

# systemd 버전 확인
systemd --version
# 출력: systemd 255 (255.4-1ubuntu8)

# libsystemd 확인
pkg-config --modversion libsystemd
# 출력: 255
```

---

## 빌드 및 설치

### 1. 소스 코드 준비

```bash
# 프로젝트 클론
git clone https://github.com/your-org/mxrc.git
cd mxrc

# 브랜치 확인 (production-ready)
git checkout 001-production-readiness
```

### 2. 빌드 구성

```bash
# 빌드 디렉토리 생성
mkdir -p build
cd build

# CMake 구성 (Release 모드, systemd 통합 활성화)
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DENABLE_SYSTEMD_INTEGRATION=ON \
    -DENABLE_PROMETHEUS=ON \
    -DENABLE_JOURNALD=ON \
    -DENABLE_SECURITY_HARDENING=ON

# CMake 출력 확인
# -- Found systemd: /usr/lib/x86_64-linux-gnu/libsystemd.so
# -- systemd integration: ENABLED
# -- Prometheus metrics: ENABLED
# -- journald logging: ENABLED
# -- Security hardening: ENABLED
```

**CMake 옵션 설명**:
- `CMAKE_BUILD_TYPE=Release`: 최적화 빌드 (프로덕션)
- `CMAKE_INSTALL_PREFIX=/usr/local`: 설치 경로
- `ENABLE_SYSTEMD_INTEGRATION`: systemd 통합 기능 활성화
- `ENABLE_PROMETHEUS`: Prometheus 메트릭 수집 활성화
- `ENABLE_JOURNALD`: journald 구조화 로깅 활성화
- `ENABLE_SECURITY_HARDENING`: 보안 강화 설정 활성화

### 3. 빌드 및 테스트

```bash
# 병렬 빌드 (모든 코어 사용)
make -j$(nproc)

# 빌드 결과 확인
ls -lh
# 출력:
# -rwxr-xr-x 1 user user 2.3M Jan 22 10:00 mxrc-rt
# -rwxr-xr-x 1 user user 1.8M Jan 22 10:00 mxrc-nonrt
# -rwxr-xr-x 1 user user 512K Jan 22 10:00 mxrc-monitor

# 통합 테스트 실행
./run_tests

# 테스트 결과 예시:
# [==========] Running 122 tests from 35 test suites.
# [----------] Global test environment tear-down
# [==========] 122 tests from 35 test suites ran. (1234 ms total)
# [  PASSED  ] 117 tests.
# [  SKIPPED ] 5 tests.
```

### 4. 설치

```bash
# 시스템 설치 (root 권한 필요)
sudo make install

# 설치 확인
which mxrc-rt mxrc-nonrt mxrc-monitor
# 출력:
# /usr/local/bin/mxrc-rt
# /usr/local/bin/mxrc-nonrt
# /usr/local/bin/mxrc-monitor

# 바이너리 검증
mxrc-rt --version
# 출력: MXRC RT Executive v1.0.0 (systemd integration)

mxrc-nonrt --version
# 출력: MXRC Non-RT Executive v1.0.0
```

---

## 서비스 설정

### 1. systemd Unit 파일 설치

```bash
# systemd 디렉토리로 이동
cd /path/to/mxrc

# Unit 파일 복사
sudo cp systemd/mxrc-rt.service /etc/systemd/system/
sudo cp systemd/mxrc-nonrt.service /etc/systemd/system/
sudo cp systemd/mxrc-monitor.service /etc/systemd/system/
sudo cp systemd/mxrc.target /etc/systemd/system/
sudo cp systemd/mxrc-metrics.service /etc/systemd/system/

# 권한 설정 (root 읽기 전용)
sudo chmod 644 /etc/systemd/system/mxrc*.service
sudo chmod 644 /etc/systemd/system/mxrc.target

# systemd 리로드
sudo systemctl daemon-reload

# Unit 파일 확인
systemctl list-unit-files | grep mxrc
# 출력:
# mxrc-rt.service           disabled
# mxrc-nonrt.service        disabled
# mxrc-monitor.service      disabled
# mxrc-metrics.service      disabled
# mxrc.target               disabled

# Unit 파일 검증
systemd-analyze verify mxrc-rt.service
systemd-analyze verify mxrc-nonrt.service
# 오류 없이 완료되어야 함
```

### 2. 설정 파일 설치

```bash
# 설정 디렉토리 생성
sudo mkdir -p /etc/mxrc

# 기본 설정 파일 복사
sudo cp config/rt-config.json /etc/mxrc/
sudo cp config/nonrt-config.json /etc/mxrc/
sudo cp config/monitor-config.json /etc/mxrc/
sudo cp config/watchdog.json /etc/mxrc/
sudo cp config/metrics.json /etc/mxrc/
sudo cp config/security.json /etc/mxrc/
sudo cp config/rt/cpu_affinity.json /etc/mxrc/

# 권한 설정 (root 읽기 전용)
sudo chmod 644 /etc/mxrc/*.json

# 설정 파일 검증 (JSON 유효성)
for f in /etc/mxrc/*.json; do
    echo "Validating $f..."
    python3 -m json.tool "$f" > /dev/null && echo "  ✓ Valid" || echo "  ✗ Invalid"
done
```

### 3. Prometheus Exporter 설치

```bash
# Python 스크립트 복사
sudo cp scripts/prometheus-exporter.py /usr/local/bin/
sudo chmod 755 /usr/local/bin/prometheus-exporter.py

# Python 의존성 설치
pip3 install --user prometheus-client

# 실행 테스트
python3 /usr/local/bin/prometheus-exporter.py &
sleep 2
curl http://localhost:9100/metrics
# 출력: Prometheus metrics...
kill %1
```

---

## 사용자 및 그룹 설정

### 1. mxrc 사용자/그룹 생성

```bash
# 시스템 그룹 생성 (--system: 시스템 그룹)
sudo groupadd --system mxrc

# 시스템 사용자 생성
sudo useradd --system \
    --gid mxrc \
    --home-dir /var/lib/mxrc \
    --shell /usr/sbin/nologin \
    --comment "MXRC Service User" \
    mxrc

# 확인
id mxrc
# 출력: uid=999(mxrc) gid=999(mxrc) groups=999(mxrc)

getent passwd mxrc
# 출력: mxrc:x:999:999:MXRC Service User:/var/lib/mxrc:/usr/sbin/nologin
```

### 2. 추가 권한 설정 (RT 스케줄링)

RT 프로세스는 `CAP_SYS_NICE`, `CAP_IPC_LOCK` 권한이 필요하지만, systemd Unit 파일에서 `AmbientCapabilities`로 설정되어 있어 별도 설정 불필요.

**확인**:
```bash
# mxrc-rt.service의 Capabilities 확인
grep -E "AmbientCapabilities|CapabilityBoundingSet" /etc/systemd/system/mxrc-rt.service
# 출력:
# AmbientCapabilities=CAP_SYS_NICE CAP_IPC_LOCK
# CapabilityBoundingSet=CAP_SYS_NICE CAP_IPC_LOCK
```

---

## 디렉토리 구조

### 1. 런타임 디렉토리 생성

```bash
# 데이터 디렉토리
sudo mkdir -p /var/lib/mxrc
sudo chown mxrc:mxrc /var/lib/mxrc
sudo chmod 750 /var/lib/mxrc

# 로그 디렉토리
sudo mkdir -p /var/log/mxrc
sudo chown mxrc:mxrc /var/log/mxrc
sudo chmod 755 /var/log/mxrc

# 임시 디렉토리
sudo mkdir -p /tmp/mxrc
sudo chown mxrc:mxrc /tmp/mxrc
sudo chmod 750 /tmp/mxrc

# PID 파일 디렉토리 (systemd가 자동 생성하지만 명시적 생성)
sudo mkdir -p /run/mxrc
sudo chown mxrc:mxrc /run/mxrc
sudo chmod 755 /run/mxrc
```

### 2. 디렉토리 구조 확인

```bash
tree -L 2 -d /var/lib/mxrc /var/log/mxrc /etc/mxrc
# 출력:
# /var/lib/mxrc
# /var/log/mxrc
# /etc/mxrc
# ├── rt-config.json
# ├── nonrt-config.json
# ├── monitor-config.json
# ├── watchdog.json
# ├── metrics.json
# └── security.json
```

### 3. SELinux / AppArmor 설정 (선택적)

**Ubuntu 24.04는 기본적으로 AppArmor 사용**:

```bash
# AppArmor 프로파일 생성 (선택적)
sudo aa-genprof /usr/local/bin/mxrc-rt

# 또는 complain 모드로 실행
sudo aa-complain /usr/local/bin/mxrc-rt
```

---

## 설치 검증

### 1. 서비스 시작 테스트

```bash
# RT 서비스 시작
sudo systemctl start mxrc-rt.service

# 상태 확인
sudo systemctl status mxrc-rt.service
# 출력:
# ● mxrc-rt.service - MXRC Real-Time Process
#      Loaded: loaded (/etc/systemd/system/mxrc-rt.service; disabled; preset: enabled)
#      Active: active (running) since Wed 2025-01-22 10:00:00 KST; 5s ago
#    Main PID: 12345 (mxrc-rt)
#      Status: "Service ready"
#       Tasks: 12 (limit: 100)
#      Memory: 128.0M (max: 2.0G)
#         CPU: 125ms

# 서비스 중지
sudo systemctl stop mxrc-rt.service
```

### 2. 리소스 제한 검증

```bash
# cgroups 설정 확인 (서비스 실행 중)
sudo systemctl start mxrc-rt.service

# CPU quota 확인
cat /sys/fs/cgroup/system.slice/mxrc-rt.service/cpu.max
# 출력: 200000 100000

# 메모리 제한 확인
cat /sys/fs/cgroup/system.slice/mxrc-rt.service/memory.max
# 출력: 2147483648

# I/O 가중치 확인
cat /sys/fs/cgroup/system.slice/mxrc-rt.service/io.weight
# 출력: default 500

sudo systemctl stop mxrc-rt.service
```

### 3. Watchdog 검증

```bash
# Watchdog 설정 확인
systemctl show mxrc-rt.service --property=WatchdogUSec
# 출력: WatchdogUSec=30s

# 서비스 시작 후 Watchdog 확인
sudo systemctl start mxrc-rt.service
sleep 5

# Watchdog timestamp 확인 (값이 업데이트되어야 함)
systemctl show mxrc-rt.service --property=WatchdogTimestampMonotonic
# 출력: WatchdogTimestampMonotonic=1234567890

sudo systemctl stop mxrc-rt.service
```

### 4. Prometheus 메트릭 검증

```bash
# 메트릭 서비스 시작
sudo systemctl start mxrc-metrics.service

# 메트릭 엔드포인트 확인
curl http://localhost:9100/metrics | grep mxrc_service_state
# 출력: mxrc_service_state{service="mxrc-rt"} 0

# 서비스 시작 후 메트릭 확인
sudo systemctl start mxrc-rt.service
sleep 2

curl http://localhost:9100/metrics | grep mxrc_service_state
# 출력: mxrc_service_state{service="mxrc-rt"} 1

# 정리
sudo systemctl stop mxrc-rt.service
sudo systemctl stop mxrc-metrics.service
```

### 5. journald 로깅 검증

```bash
# 서비스 시작
sudo systemctl start mxrc-rt.service

# journald 로그 확인
journalctl -u mxrc-rt.service -n 20
# 출력: 최근 20줄의 로그

# JSON 형식 로그 확인
journalctl -u mxrc-rt.service -n 1 -o json-pretty

# 컴포넌트별 로그 확인 (COMPONENT 필드)
journalctl COMPONENT=rt

# 정리
sudo systemctl stop mxrc-rt.service
```

### 6. 보안 설정 검증

```bash
# 보안 점수 확인 (목표: ≥ 8.0/10.0)
systemd-analyze security mxrc-rt.service
# 출력:
# NAME                  DESCRIPTION                  EXPOSURE
# ...
# → Overall exposure level for mxrc-rt.service: 8.2 (OK) 🙂

# NoNewPrivileges 확인
systemctl show mxrc-rt.service --property=NoNewPrivileges
# 출력: NoNewPrivileges=yes

# ProtectSystem 확인
systemctl show mxrc-rt.service --property=ProtectSystem
# 출력: ProtectSystem=strict

# Capabilities 확인
systemctl show mxrc-rt.service --property=AmbientCapabilities
# 출력: AmbientCapabilities=CAP_SYS_NICE CAP_IPC_LOCK
```

### 7. 부팅 시 자동 시작 검증

```bash
# mxrc.target 활성화
sudo systemctl enable mxrc.target

# 확인
systemctl is-enabled mxrc.target
# 출력: enabled

# 의존 서비스 확인
systemctl list-dependencies mxrc.target
# 출력:
# mxrc.target
# ● ├─mxrc-monitor.service
# ● ├─mxrc-nonrt.service
# ● └─mxrc-rt.service

# 재부팅 테스트 (선택적)
sudo reboot

# 재부팅 후 확인
systemctl status mxrc.target
# 출력: Active: active
```

---

## 문제 해결

### 1. 서비스 시작 실패

**증상**:
```bash
sudo systemctl start mxrc-rt.service
# Job for mxrc-rt.service failed because the control process exited with error code.
```

**확인**:
```bash
# 상세 로그
sudo systemctl status mxrc-rt.service -l
sudo journalctl -xe -u mxrc-rt.service

# 일반적인 원인:
# 1. 바이너리 권한: /usr/local/bin/mxrc-rt 실행 권한 확인
# 2. 설정 파일 오류: /etc/mxrc/*.json 유효성 검증
# 3. mxrc 사용자 부재: id mxrc 확인
# 4. 디렉토리 권한: /var/lib/mxrc, /var/log/mxrc 권한 확인
```

### 2. RT 스케줄링 실패

**증상**:
```bash
journalctl -u mxrc-rt.service | grep "sched_setscheduler failed"
# mxrc-rt: sched_setscheduler failed: Operation not permitted
```

**해결**:
```bash
# Capabilities 확인
systemctl show mxrc-rt.service --property=AmbientCapabilities
# 출력: AmbientCapabilities=CAP_SYS_NICE CAP_IPC_LOCK

# 없다면 Unit 파일 수정
sudo nano /etc/systemd/system/mxrc-rt.service
# [Service]
# AmbientCapabilities=CAP_SYS_NICE CAP_IPC_LOCK
# CapabilityBoundingSet=CAP_SYS_NICE CAP_IPC_LOCK

sudo systemctl daemon-reload
sudo systemctl restart mxrc-rt.service
```

### 3. CPU 격리 미적용

**증상**:
```bash
cat /proc/cmdline | grep isolcpus
# (출력 없음)
```

**해결**:
```bash
# GRUB 설정 확인
sudo nano /etc/default/grub
# GRUB_CMDLINE_LINUX="... isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3"

sudo update-grub
sudo reboot
```

### 4. libsystemd 버전 불일치

**증상**:
```bash
cmake ..
# CMake Error: Could NOT find systemd (missing: SYSTEMD_LIBRARIES)
```

**해결**:
```bash
# libsystemd 재설치
sudo apt-get install --reinstall libsystemd-dev

# pkg-config 확인
pkg-config --libs libsystemd
# 출력: -lsystemd

# 버전 확인
pkg-config --modversion libsystemd
# 출력: 255
```

---

## 다음 단계

설치가 완료되었습니다! 이제 다음 문서를 참조하세요:

1. **[Quickstart Guide](quickstart.md)**: 서비스 시작 및 기본 사용법
2. **[Architecture](../../architecture/architecture.md)**: 시스템 아키텍처 이해
3. **[Spec](spec.md)**: 상세 기능 명세
4. **[Plan](plan.md)**: 구현 계획 및 진행 상황

---

## 참고 자료

- [systemd.service 매뉴얼](https://www.freedesktop.org/software/systemd/man/systemd.service.html)
- [systemd.exec 매뉴얼](https://www.freedesktop.org/software/systemd/man/systemd.exec.html)
- [PREEMPT_RT 패치 문서](https://wiki.linuxfoundation.org/realtime/start)
- [CPU Isolation 가이드](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux_for_real_time/)

---

**작성자**: MXRC Development Team
**버전**: 1.0
**최종 수정**: 2025-01-22
