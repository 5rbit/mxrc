# Quick Start Guide: MXRC 아키텍처 개선

**Feature**: 019-architecture-improvements
**Version**: 1.0.0
**Date**: 2025-01-23
**소요 시간**: 5분

---

## 개요

이 가이드는 MXRC 아키텍처 개선 기능을 5분 내에 빠르게 설정하고 검증하는 방법을 제공합니다. IPC 스키마, Hot Key 최적화, EventBus 우선순위, 필드버스 추상화, Monitoring, HA 정책을 순차적으로 활성화합니다.

---

## 전제조건

### 시스템 요구사항

- **OS**: Ubuntu 24.04 LTS (PREEMPT_RT 커널 권장)
- **CPU**: x86_64, 최소 4 코어
- **메모리**: 최소 8GB RAM
- **디스크**: 최소 10GB 여유 공간
- **네트워크**: EtherCAT 전용 네트워크 인터페이스 (eth0)

### 소프트웨어 의존성

```bash
# 1. 기본 빌드 도구
sudo apt update
sudo apt install -y build-essential cmake git pkg-config

# 2. C++ 라이브러리
sudo apt install -y \
    libtbb-dev \
    libyaml-cpp-dev \
    libspdlog-dev \
    libgtest-dev \
    libfolly-dev

# 3. Python (코드 생성용)
sudo apt install -y python3 python3-pip
pip3 install pyyaml jinja2

# 4. Prometheus & Grafana (모니터링)
# Prometheus
wget https://github.com/prometheus/prometheus/releases/download/v2.48.0/prometheus-2.48.0.linux-amd64.tar.gz
tar -xvf prometheus-2.48.0.linux-amd64.tar.gz
sudo mv prometheus-2.48.0.linux-amd64 /opt/prometheus

# Grafana
sudo apt-get install -y adduser libfontconfig1
wget https://dl.grafana.com/oss/release/grafana_10.2.3_amd64.deb
sudo dpkg -i grafana_10.2.3_amd64.deb

# 5. EtherCAT (SOEM)
git clone https://github.com/OpenEtherCATsociety/SOEM.git
cd SOEM
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

### 권한 설정

```bash
# RT 프로세스 실행을 위한 권한
sudo setcap cap_sys_nice+ep /usr/local/bin/mxrc-rt

# EtherCAT 네트워크 인터페이스 접근 권한
sudo setcap cap_net_raw+ep /usr/local/bin/mxrc-rt
sudo adduser $USER realtime
```

---

## 단계별 설정 및 실행

### Step 1: 프로젝트 빌드 (1분)

```bash
# 1. 소스 코드 클론
cd /home/tory/workspace/mxrc/mxrc
git checkout 019-architecture-improvements

# 2. 빌드 디렉토리 생성
mkdir -p build
cd build

# 3. CMake 설정
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_HOT_KEY_OPTIMIZATION=ON \
    -DENABLE_PRIORITY_QUEUE=ON \
    -DENABLE_PROMETHEUS_METRICS=ON \
    -DENABLE_HA_STATE_MACHINE=ON

# 4. 빌드 (병렬 컴파일)
make -j$(nproc)

# 5. 테스트 실행 (선택사항)
ctest --output-on-failure
```

**예상 출력**:
```
[100%] Built target mxrc-rt
[100%] Built target mxrc-nonrt
Test project /home/tory/workspace/mxrc/mxrc/build
      Start  1: HotKeyCache_test
 1/10 Test  #1: HotKeyCache_test ................   Passed    0.12 sec
      Start  2: PriorityQueue_test
 2/10 Test  #2: PriorityQueue_test ...............   Passed    0.08 sec
...
100% tests passed, 0 tests failed out of 10
```

---

### Step 2: IPC 스키마 코드 생성 (30초)

```bash
# 1. 스키마 검증
cd /home/tory/workspace/mxrc/mxrc
python3 scripts/codegen/validate_schema.py \
    docs/specs/019-architecture-improvements/contracts/ipc-schema.yaml

# 2. C++ 코드 생성
python3 scripts/codegen/generate_ipc_schema.py \
    --schema docs/specs/019-architecture-improvements/contracts/ipc-schema.yaml \
    --output-dir build/generated/ipc

# 3. 생성된 파일 확인
ls -l build/generated/ipc/
```

**예상 출력**:
```
Schema validation: PASSED
  - 15 DataStore keys defined
  - 12 EventBus events defined
  - 3 custom types defined

Code generation: SUCCESS
  - Generated: DataStoreKeys.h
  - Generated: EventBusEvents.h
  - Generated: DataStoreAccessor.cpp
```

---

### Step 3: 설정 파일 준비 (1분)

```bash
# 1. 설정 파일 복사
sudo mkdir -p /etc/mxrc
sudo cp config/fieldbus.yaml /etc/mxrc/
sudo cp docs/specs/019-architecture-improvements/contracts/ha-policy.yaml /etc/mxrc/

# 2. 필드버스 설정 수정 (자신의 환경에 맞게)
sudo nano /etc/mxrc/fieldbus.yaml
# 예시:
# fieldbus:
#   type: ethercat
#   ethercat:
#     interface: eth0  # 자신의 EtherCAT 인터페이스로 변경
#     cycle_time_us: 1000

# 3. HA 정책 설정 확인
cat /etc/mxrc/ha-policy.yaml | grep -A 5 "recovery_policies"
```

---

### Step 4: Prometheus & Grafana 설정 (1분)

```bash
# 1. Prometheus 설정
cat > /opt/prometheus/prometheus.yml << EOF
global:
  scrape_interval: 10s

scrape_configs:
  - job_name: 'mxrc-rt'
    static_configs:
      - targets: ['localhost:9090']

  - job_name: 'mxrc-nonrt'
    static_configs:
      - targets: ['localhost:9091']
EOF

# 2. Prometheus 시작
cd /opt/prometheus
./prometheus --config.file=prometheus.yml &

# 3. Grafana 시작
sudo systemctl start grafana-server
sudo systemctl enable grafana-server

# 4. Grafana 대시보드 Import
# 브라우저에서 http://localhost:3000 접속 (admin/admin)
# Configuration > Data Sources > Add Prometheus (http://localhost:9090)
# Import Dashboard: config/grafana/dashboards/mxrc_overview.json
```

---

### Step 5: MXRC 프로세스 실행 (1분)

```bash
# 1. RT 프로세스 시작 (터미널 1)
cd /home/tory/workspace/mxrc/mxrc/build
sudo ./bin/mxrc-rt --config /etc/mxrc/fieldbus.yaml

# 2. Non-RT 프로세스 시작 (터미널 2)
cd /home/tory/workspace/mxrc/mxrc/build
./bin/mxrc-nonrt --config /etc/mxrc/fieldbus.yaml --ha-policy /etc/mxrc/ha-policy.yaml

# 또는 systemd 서비스로 실행
sudo systemctl start mxrc-rt
sudo systemctl start mxrc-nonrt
```

**예상 출력** (RT 프로세스):
```
[2025-01-23 10:00:00.123] [rt] [info] MXRC RT Process starting...
[2025-01-23 10:00:00.125] [rt] [info] Hot Key Cache initialized (20 keys)
[2025-01-23 10:00:00.130] [rt] [info] EtherCAT Master initialized on eth0
[2025-01-23 10:00:00.135] [rt] [info] Fieldbus state: OPERATIONAL
[2025-01-23 10:00:00.140] [rt] [info] RT Cycle started (1000us)
```

**예상 출력** (Non-RT 프로세스):
```
[2025-01-23 10:00:01.200] [nonrt] [info] MXRC Non-RT Process starting...
[2025-01-23 10:00:01.205] [nonrt] [info] EventBus initialized (Priority Queue enabled)
[2025-01-23 10:00:01.210] [nonrt] [info] HA State Machine loaded from /etc/mxrc/ha-policy.yaml
[2025-01-23 10:00:01.215] [nonrt] [info] Prometheus metrics exporter started on :9091
[2025-01-23 10:00:01.220] [nonrt] [info] Current HA State: NORMAL
```

---

## 검증 방법

### 1. Hot Key 성능 벤치마크

```bash
# Hot Key 읽기/쓰기 성능 측정
cd /home/tory/workspace/mxrc/mxrc/build
./bin/benchmark_hotkey

# 예상 결과:
# Hot Key Read:  avg=55ns, p99=95ns  ✅ (목표: <60ns)
# Hot Key Write: avg=105ns, p99=145ns ✅ (목표: <110ns)
```

### 2. EventBus 우선순위 테스트

```bash
# 우선순위 이벤트 처리 순서 확인
./bin/test_eventbus_priority

# 예상 결과:
# Event Processing Order:
#   1. AvoidReqEvent (CRITICAL, 15ms latency)
#   2. EtherCATErrorEvent (HIGH, 25ms latency)
#   3. TaskStartedEvent (NORMAL, 50ms latency)
#   4. LogEvent (LOW, 120ms latency)
# ✅ Priority order verified
# ✅ CRITICAL events 50% faster than NORMAL
```

### 3. Prometheus 메트릭 확인

```bash
# Prometheus 메트릭 엔드포인트 확인
curl http://localhost:9091/metrics | grep mxrc

# 예상 출력:
# mxrc_rt_cycle_time_microseconds_bucket{le="100"} 1234
# mxrc_rt_cycle_time_microseconds_bucket{le="500"} 5678
# mxrc_rt_deadline_miss_total 0
# mxrc_eventbus_queue_size 15
# mxrc_datastore_latency_nanoseconds_bucket{le="100"} 9876
```

### 4. Grafana 대시보드 확인

1. 브라우저에서 `http://localhost:3000` 접속
2. "MXRC Real-Time Monitoring" 대시보드 선택
3. 다음 패널 확인:
   - **RT Cycle Time**: p95, p99 그래프
   - **Deadline Miss Rate**: 0이어야 함
   - **EventBus Queue Size**: 실시간 큐 크기
   - **Hot Key Access Count**: 접근 빈도
   - **HA Current State**: NORMAL 상태 유지

### 5. HA 상태 머신 테스트

```bash
# HA 상태 전이 시뮬레이션
./bin/test_ha_state_machine

# 예상 결과:
# Test 1: RT Process Crash -> Auto Restart
#   ✅ State transition: NORMAL -> RECOVERY_IN_PROGRESS -> NORMAL
#   ✅ Recovery action: RESTART_RT_PROCESS
#   ✅ Recovery time: 8.2s (< 10s target)

# Test 2: Deadline Miss -> Safe Mode
#   ✅ State transition: NORMAL -> SAFE_MODE
#   ✅ Motors stopped
#   ✅ Fieldbus state: SAFE_OP
```

---

## 트러블슈팅

### 문제 1: Hot Key 성능이 목표치에 미달

**증상**: 벤치마크에서 읽기 >100ns, 쓰기 >200ns

**원인**:
- CPU 주파수 조절기가 절전 모드로 설정됨
- TurboBoost가 비활성화됨

**해결**:
```bash
# CPU 성능 모드로 변경
sudo cpupower frequency-set -g performance

# TurboBoost 활성화 확인
cat /sys/devices/system/cpu/intel_pstate/no_turbo
# 0이어야 함 (1이면 비활성화 상태)

# 벤치마크 재실행
./bin/benchmark_hotkey
```

---

### 문제 2: EtherCAT 통신 실패

**증상**: `EtherCAT Master initialization failed`

**원인**:
- 네트워크 인터페이스 이름이 잘못됨
- EtherCAT 슬레이브가 연결되지 않음
- 권한 부족

**해결**:
```bash
# 1. 네트워크 인터페이스 확인
ip link show
# eth0, enp0s31f6 등의 인터페이스 이름 확인

# 2. 설정 파일 수정
sudo nano /etc/mxrc/fieldbus.yaml
# interface: eth0 -> 실제 인터페이스 이름으로 변경

# 3. 권한 확인
sudo setcap cap_net_raw+ep ./bin/mxrc-rt

# 4. EtherCAT 슬레이브 확인 (SOEM 도구)
sudo ethercat slaves
```

---

### 문제 3: Prometheus 메트릭 노출 안 됨

**증상**: `curl http://localhost:9091/metrics` 응답 없음

**원인**:
- Non-RT 프로세스가 시작되지 않음
- 포트가 이미 사용 중
- 방화벽 차단

**해결**:
```bash
# 1. Non-RT 프로세스 상태 확인
ps aux | grep mxrc-nonrt
sudo systemctl status mxrc-nonrt

# 2. 포트 사용 확인
sudo netstat -tulpn | grep 9091

# 3. 방화벽 규칙 확인
sudo ufw allow 9091/tcp

# 4. 프로세스 재시작
sudo systemctl restart mxrc-nonrt

# 5. 로그 확인
sudo journalctl -u mxrc-nonrt -f
```

---

### 문제 4: HA 복구 정책이 실행되지 않음

**증상**: RT 프로세스 크래시 후 자동 재시작 안 됨

**원인**:
- systemd 서비스 파일이 설정되지 않음
- HA 정책 파일 경로가 잘못됨

**해결**:
```bash
# 1. systemd 서비스 파일 생성
sudo nano /etc/systemd/system/mxrc-rt.service
# [Unit]
# Description=MXRC RT Process
# After=network.target
#
# [Service]
# Type=simple
# ExecStart=/usr/local/bin/mxrc-rt --config /etc/mxrc/fieldbus.yaml
# Restart=on-failure
# RestartSec=5
#
# [Install]
# WantedBy=multi-user.target

# 2. systemd 리로드
sudo systemctl daemon-reload

# 3. 서비스 활성화
sudo systemctl enable mxrc-rt
sudo systemctl enable mxrc-nonrt

# 4. HA 정책 파일 경로 확인
./bin/mxrc-nonrt --ha-policy /etc/mxrc/ha-policy.yaml --verbose
```

---

### 문제 5: Grafana 대시보드에 데이터가 표시되지 않음

**증상**: Grafana 패널에 "No data" 표시

**원인**:
- Prometheus Data Source가 올바르게 설정되지 않음
- 메트릭 쿼리 문법 오류
- 시간 범위가 잘못 설정됨

**해결**:
```bash
# 1. Prometheus 타겟 확인
# 브라우저에서 http://localhost:9090/targets 접속
# mxrc-rt, mxrc-nonrt 타겟이 "UP" 상태여야 함

# 2. Prometheus에서 쿼리 테스트
# http://localhost:9090/graph 접속
# 쿼리: mxrc_rt_cycle_time_microseconds_bucket
# 데이터가 표시되는지 확인

# 3. Grafana Data Source 재설정
# Configuration > Data Sources > Prometheus
# URL: http://localhost:9090
# "Save & Test" 클릭 -> "Data source is working" 확인

# 4. 시간 범위 조정
# 대시보드 우측 상단에서 "Last 5 minutes"로 변경
```

---

## 다음 단계

### 성능 튜닝

```bash
# RT 프로세스 CPU 친화도 설정
sudo taskset -c 0,1 ./bin/mxrc-rt

# NUMA 최적화
sudo numactl --cpunodebind=0 --membind=0 ./bin/mxrc-rt

# Huge Pages 활성화 (메모리 성능 향상)
sudo sysctl -w vm.nr_hugepages=512
```

### 추가 기능 활성화

```bash
# 1. Coalescing 정책 적용
# config/eventbus.yaml에서 coalescing_enabled: true로 설정

# 2. Backpressure 정책 변경
# EventBus 초기화 시 DROP_OLDEST -> BLOCK으로 변경

# 3. Hot Key 목록 확장
# docs/specs/019-architecture-improvements/contracts/ipc-schema.yaml 편집
# 새 Hot Key 추가 후 코드 재생성
```

### 로그 분석

```bash
# RT 프로세스 로그
sudo journalctl -u mxrc-rt -n 100

# Non-RT 프로세스 로그
sudo journalctl -u mxrc-nonrt -n 100

# HA 상태 전이 로그
cat /var/log/mxrc/ha_state_transitions.log

# 성능 메트릭 로그
cat /var/log/mxrc/performance.log
```

---

## 참고 자료

- **Feature Spec**: `docs/specs/019-architecture-improvements/spec.md`
- **Implementation Plan**: `docs/specs/019-architecture-improvements/plan.md`
- **Research**: `docs/specs/019-architecture-improvements/research.md`
- **Data Model**: `docs/specs/019-architecture-improvements/data-model.md`
- **IPC Schema**: `docs/specs/019-architecture-improvements/contracts/ipc-schema.yaml`
- **HA Policy**: `docs/specs/019-architecture-improvements/contracts/ha-policy.yaml`

---

**축하합니다!** MXRC 아키텍처 개선 기능이 성공적으로 설정되었습니다.

**확인사항 체크리스트**:
- ✅ Hot Key 성능 목표 달성 (<60ns 읽기, <110ns 쓰기)
- ✅ EventBus 우선순위 이벤트 처리 확인
- ✅ Prometheus 메트릭 20개 이상 노출
- ✅ Grafana 대시보드에서 실시간 모니터링 가능
- ✅ HA 상태 머신 NORMAL 상태 유지
- ✅ 모든 테스트 통과

문제가 발생하면 위의 **트러블슈팅** 섹션을 참조하거나, GitHub Issue를 생성하세요.
