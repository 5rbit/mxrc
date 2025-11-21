# Quickstart Guide: Production Readiness

**Feature**: [001-production-readiness](spec.md)
**Created**: 2025-11-21
**Last Updated**: 2025-11-21

---

## 개요

본 가이드는 Production Readiness 기능을 설정하고 테스트하는 방법을 단계별로 설명합니다. 다음 4가지 주요 기능을 다룹니다:

1. **P1: CPU Isolation & NUMA Optimization** - 실시간 성능 최적화
2. **P2: High Availability & Failover** - 자동 복구 및 고가용성
3. **P3: Structured Logging** - ELK 스택 통합
4. **P4: Distributed Tracing** - Jaeger 통합

각 섹션은 독립적으로 테스트 가능하며, 우선순위 순서로 진행하는 것을 권장합니다.

---

## 사전 요구사항

### 하드웨어

- **CPU**: 4코어 이상 (2코어는 RT 프로세스 전용, 2코어는 Non-RT)
- **메모리**: 4GB 이상
- **NUMA**: 2개 이상의 NUMA 노드 (선택 사항, 성능 최적화용)

### 소프트웨어

- **OS**: Ubuntu 24.04 LTS with PREEMPT_RT kernel
- **Kernel**: Linux 6.6+ with `CONFIG_PREEMPT_RT=y`
- **Dependencies**:
  - spdlog >= 1.x
  - libnuma-dev
  - systemd
  - OpenTelemetry C++ SDK >= 1.12
  - Elasticsearch 8.x (로깅용)
  - Jaeger (트레이싱용)

### 설치 확인

```bash
# PREEMPT_RT 커널 확인
uname -a
# 출력 예: Linux mxrc 6.6.87.2-rt53 #1 SMP PREEMPT_RT ...

# NUMA 노드 확인
numactl --hardware
# 출력 예: available: 2 nodes (0-1)

# libnuma 설치 확인
ldconfig -p | grep numa
# 출력 예: libnuma.so.1 (libc6,x86-64) => /lib/x86_64-linux-gnu/libnuma.so.1

# systemd 버전 확인
systemctl --version
# 출력 예: systemd 255 (255.4-1ubuntu8.4)
```

---

## P1: CPU Isolation & NUMA Optimization

### 1단계: 커널 부팅 파라미터 설정

RT 프로세스 전용 CPU 코어를 격리합니다.

```bash
# /etc/default/grub 편집
sudo nano /etc/default/grub

# GRUB_CMDLINE_LINUX에 다음 추가:
# isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3
# (코어 2, 3을 RT 프로세스 전용으로 격리)

GRUB_CMDLINE_LINUX="isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3"

# GRUB 업데이트 및 재부팅
sudo update-grub
sudo reboot
```

### 2단계: CPU Affinity 설정 파일 작성

RT 프로세스의 CPU 코어 할당을 정의합니다.

```bash
# config/cpu_affinity.json 생성
cat > config/cpu_affinity.json << 'EOF'
{
  "rt_process": {
    "cpu_cores": [2, 3],
    "isolation_mode": "HYBRID",
    "is_exclusive": true,
    "priority": 90,
    "policy": "SCHED_FIFO"
  },
  "non_rt_process": {
    "cpu_cores": [0, 1],
    "isolation_mode": "NONE",
    "is_exclusive": false,
    "priority": 0,
    "policy": "SCHED_OTHER"
  }
}
EOF
```

### 3단계: NUMA Binding 설정 파일 작성

프로세스의 NUMA 노드 바인딩을 정의합니다.

```bash
# config/numa_binding.json 생성
cat > config/numa_binding.json << 'EOF'
{
  "rt_process": {
    "numa_node": 0,
    "memory_policy": "LOCAL",
    "strict_binding": true,
    "migrate_pages": false,
    "cpu_cores_hint": [2, 3]
  },
  "non_rt_process": {
    "numa_node": 1,
    "memory_policy": "PREFERRED",
    "strict_binding": false,
    "migrate_pages": false,
    "cpu_cores_hint": [0, 1]
  }
}
EOF
```

### 4단계: 빌드 및 실행

```bash
# 빌드
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# RT 프로세스 실행 (CPU affinity 및 NUMA binding 적용)
sudo ./rt_process --config ../config/cpu_affinity.json \
                  --numa-config ../config/numa_binding.json
```

### 5단계: CPU Affinity 검증

RT 프로세스가 지정된 CPU 코어에서만 실행되는지 확인합니다.

```bash
# 프로세스 PID 확인
ps aux | grep rt_process

# CPU affinity 확인
taskset -cp <PID>
# 출력 예: pid 12345's current affinity list: 2,3

# 실시간 CPU 사용률 모니터링
htop
# CPU 2, 3에서만 RT 프로세스 실행 확인
```

### 6단계: NUMA Binding 검증

프로세스가 지정된 NUMA 노드에 바인딩되었는지 확인합니다.

```bash
# NUMA 정책 확인
numactl --show
# 출력 예: cpubind: 0
#          membind: 0

# NUMA 메모리 통계 확인
numastat -p <PID>
# 출력 예:
# Node 0 : 512 MB (95% local access)
# Node 1 : 25 MB (5% remote access)
```

### 7단계: RT 성능 측정

Deadline miss rate 및 cycle time jitter를 측정합니다.

```bash
# Prometheus 메트릭 확인
curl http://localhost:9090/metrics | grep mxrc_rt

# 출력 예:
# mxrc_rt_cycle_count_total{process="rt_process"} 10000
# mxrc_rt_deadline_miss_count_total{process="rt_process"} 5
# mxrc_rt_deadline_miss_rate{process="rt_process"} 0.0005
# mxrc_rt_jitter_us{process="rt_process"} 8.5
```

### 성공 기준 (Spec SC-001, SC-002, SC-003)

- ✅ Deadline miss rate < 0.01% (< 1 miss per 10,000 cycles)
- ✅ Cycle time jitter 감소 50% (CPU isolation 적용 전 대비)
- ✅ Local NUMA access > 95%

---

## P2: High Availability & Failover

### 1단계: Failover 정책 설정

프로세스 모니터링 및 재시작 정책을 정의합니다.

```bash
# config/failover_policy.json 생성
cat > config/failover_policy.json << 'EOF'
{
  "rt_process": {
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
}
EOF
```

### 2단계: systemd 서비스 파일 작성

systemd watchdog를 활성화합니다.

```bash
# /etc/systemd/system/mxrc-rt.service 생성
sudo tee /etc/systemd/system/mxrc-rt.service > /dev/null << 'EOF'
[Unit]
Description=MXRC RT Process
After=network.target

[Service]
Type=notify
ExecStart=/usr/local/bin/rt_process \
  --config /etc/mxrc/cpu_affinity.json \
  --numa-config /etc/mxrc/numa_binding.json \
  --failover-config /etc/mxrc/failover_policy.json
Restart=on-failure
RestartSec=1s
WatchdogSec=10s
NotifyAccess=main
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

# systemd 리로드 및 서비스 활성화
sudo systemctl daemon-reload
sudo systemctl enable mxrc-rt.service
sudo systemctl start mxrc-rt.service
```

### 3단계: Health Check API 테스트

프로세스 건강 상태를 확인합니다.

```bash
# Health check
curl http://localhost:8080/health | jq
# 출력 예:
# {
#   "status": "HEALTHY",
#   "process_name": "rt_process",
#   "pid": 12345,
#   "last_heartbeat": "2025-11-21T10:30:45.123Z",
#   "response_time_ms": 1.5,
#   "deadline_miss_count": 5,
#   "restart_count": 0
# }

# Readiness check
curl http://localhost:8080/health/ready | jq
# 출력 예:
# {
#   "ready": true,
#   "message": "All subsystems initialized"
# }

# Liveness check
curl http://localhost:8080/health/live | jq
# 출력 예:
# {
#   "alive": true,
#   "uptime_seconds": 3600
# }
```

### 4단계: Failover 테스트

프로세스를 강제 종료하여 자동 재시작을 검증합니다.

```bash
# 프로세스 PID 확인
systemctl status mxrc-rt.service

# 프로세스 강제 종료 (SIGKILL)
sudo kill -9 <PID>

# 5초 내 재시작 확인
sleep 5
systemctl status mxrc-rt.service
# 출력 예: Active: active (running) since ...

# 재시작 횟수 확인
curl http://localhost:8080/health | jq '.restart_count'
# 출력 예: 1
```

### 5단계: State Checkpoint 검증

프로세스가 checkpoint에서 복구되는지 확인합니다.

```bash
# Checkpoint 생성 확인
ls -lh /var/mxrc/checkpoints/
# 출력 예:
# rt_process_20251121103000.json (4 KB)

# Checkpoint 내용 확인
cat /var/mxrc/checkpoints/rt_process_20251121103000.json | jq
# 출력 예:
# {
#   "checkpoint_id": "550e8400-e29b-41d4-a716-446655440000",
#   "process_name": "rt_process",
#   "timestamp": "2025-11-21T10:30:00.000Z",
#   "rt_state": { "active_tasks": ["task_001"] },
#   "checkpoint_size_bytes": 4096,
#   "is_complete": true
# }

# 프로세스 재시작 후 로그 확인
journalctl -u mxrc-rt.service | grep "Recovered from checkpoint"
# 출력 예: Recovered from checkpoint: 550e8400-e29b-41d4-a716-446655440000
```

### 성공 기준 (Spec SC-004, SC-005)

- ✅ 프로세스 재시작 시간 < 5초
- ✅ Checkpoint에서 복구 (데이터 손실 최소화)
- ✅ Health check 응답 시간 < 500ms

---

## P3: Structured Logging Integration

### 1단계: Elasticsearch 설치 및 설정

Elasticsearch를 설치하고 로그 인덱스를 생성합니다.

```bash
# Elasticsearch 설치 (Docker)
docker run -d \
  --name elasticsearch \
  -p 9200:9200 \
  -e "discovery.type=single-node" \
  -e "xpack.security.enabled=false" \
  docker.elastic.co/elasticsearch/elasticsearch:8.11.0

# Elasticsearch 연결 확인
curl http://localhost:9200
# 출력 예:
# {
#   "name" : "elasticsearch",
#   "cluster_name" : "docker-cluster",
#   "version" : { "number" : "8.11.0" }
# }

# 로그 인덱스 생성
curl -X PUT http://localhost:9200/mxrc-logs -H 'Content-Type: application/json' -d '{
  "mappings": {
    "properties": {
      "@timestamp": { "type": "date" },
      "log.level": { "type": "keyword" },
      "process.name": { "type": "keyword" },
      "message": { "type": "text" },
      "trace.id": { "type": "keyword" },
      "mxrc.task_id": { "type": "keyword" }
    }
  }
}'
```

### 2단계: Filebeat 설치 및 설정

Filebeat를 설치하여 로그 파일을 Elasticsearch로 전송합니다.

```bash
# Filebeat 설치
wget -qO - https://artifacts.elastic.co/GPG-KEY-elasticsearch | sudo apt-key add -
sudo apt-get install apt-transport-https
echo "deb https://artifacts.elastic.co/packages/8.x/apt stable main" | \
  sudo tee /etc/apt/sources.list.d/elastic-8.x.list
sudo apt-get update && sudo apt-get install filebeat

# Filebeat 설정 파일 작성
sudo tee /etc/filebeat/filebeat.yml > /dev/null << 'EOF'
filebeat.inputs:
  - type: log
    enabled: true
    paths:
      - /var/log/mxrc/rt_process.log
      - /var/log/mxrc/non_rt_process.log
    json.keys_under_root: true
    json.add_error_key: true

output.elasticsearch:
  hosts: ["localhost:9200"]
  index: "mxrc-logs-%{+yyyy.MM.dd}"

setup.template.name: "mxrc-logs"
setup.template.pattern: "mxrc-logs-*"
EOF

# Filebeat 시작
sudo systemctl enable filebeat
sudo systemctl start filebeat
sudo systemctl status filebeat
```

### 3단계: Structured Logging 활성화

RT 프로세스가 JSON 형식으로 로그를 출력하도록 설정합니다.

```bash
# RT 프로세스 재시작 (structured logging 활성화)
sudo systemctl restart mxrc-rt.service

# 로그 파일 확인 (JSON 형식)
tail -f /var/log/mxrc/rt_process.log
# 출력 예:
# {"@timestamp":"2025-11-21T10:30:45.123Z","log.level":"info","process.name":"rt_process","message":"Task execution completed"}
```

### 4단계: Elasticsearch 로그 조회

Elasticsearch에서 로그를 검색합니다.

```bash
# 최근 10개 로그 조회
curl -X GET "http://localhost:9200/mxrc-logs-*/_search?size=10&pretty" -H 'Content-Type: application/json' -d '{
  "sort": [{"@timestamp": {"order": "desc"}}],
  "query": {"match_all": {}}
}'

# 특정 프로세스 로그 조회
curl -X GET "http://localhost:9200/mxrc-logs-*/_search?pretty" -H 'Content-Type: application/json' -d '{
  "query": {
    "term": {"process.name": "rt_process"}
  }
}'

# 오류 로그만 조회
curl -X GET "http://localhost:9200/mxrc-logs-*/_search?pretty" -H 'Content-Type: application/json' -d '{
  "query": {
    "term": {"log.level": "error"}
  }
}'
```

### 5단계: Kibana 대시보드 설정 (선택 사항)

Kibana를 설치하여 로그를 시각화합니다.

```bash
# Kibana 설치 (Docker)
docker run -d \
  --name kibana \
  --link elasticsearch:elasticsearch \
  -p 5601:5601 \
  docker.elastic.co/kibana/kibana:8.11.0

# Kibana 접속
# 브라우저에서 http://localhost:5601 열기

# Index Pattern 생성:
# Management > Index Patterns > Create Index Pattern
# Index pattern: mxrc-logs-*
# Time field: @timestamp
```

### 성공 기준 (Spec SC-008, SC-009)

- ✅ 로그 전송 지연 < 1초 (Filebeat → Elasticsearch)
- ✅ 로그 형식: JSON (ECS 스키마)
- ✅ 로그 검색: 프로세스, 레벨, Task ID로 필터링 가능

---

## P4: Distributed Tracing Integration

### 1단계: Jaeger 설치 및 설정

Jaeger를 설치하여 분산 트레이싱을 수집합니다.

```bash
# Jaeger All-in-One 설치 (Docker)
docker run -d \
  --name jaeger \
  -p 6831:6831/udp \
  -p 6832:6832/udp \
  -p 16686:16686 \
  -p 14268:14268 \
  jaegertracing/all-in-one:1.52

# Jaeger UI 접속
# 브라우저에서 http://localhost:16686 열기
```

### 2단계: Tracing 설정 파일 작성

OpenTelemetry 설정을 정의합니다.

```bash
# config/tracing_config.json 생성
cat > config/tracing_config.json << 'EOF'
{
  "service_name": "mxrc-rt",
  "sampling_rate": 0.1,
  "exporter": {
    "type": "OTLP",
    "endpoint": "http://localhost:4318",
    "protocol": "http"
  },
  "propagator": "W3C",
  "instrumentation": {
    "eventbus": true,
    "task_executor": true,
    "rt_cycle": true
  }
}
EOF
```

### 3단계: Distributed Tracing 활성화

RT 프로세스에서 트레이싱을 활성화합니다.

```bash
# RT 프로세스 재시작 (tracing 활성화)
sudo ./rt_process \
  --config config/cpu_affinity.json \
  --numa-config config/numa_binding.json \
  --tracing-config config/tracing_config.json
```

### 4단계: Trace 생성 및 확인

Task를 실행하여 trace를 생성합니다.

```bash
# Task 실행 (예: JSON-RPC API 호출)
curl -X POST http://localhost:8080/api/v1/task/start -H 'Content-Type: application/json' -d '{
  "task_id": "task_001",
  "mode": "once"
}'

# Jaeger UI에서 trace 확인
# 1. http://localhost:16686 접속
# 2. Service 선택: mxrc-rt
# 3. Operation 선택: task.execute
# 4. Find Traces 클릭
```

### 5단계: Trace Context 전파 검증

EventBus 이벤트 전파 시 trace context가 유지되는지 확인합니다.

```bash
# Jaeger UI에서 trace 상세 보기
# 1. Trace ID 클릭
# 2. Span 목록 확인:
#    - task.execute (root span)
#      - sequence.run (child span)
#        - action.execute (child span)
#      - eventbus.publish (child span)
#
# 3. Tags 확인:
#    - trace.id: 0af7651916cd43dd8448eb211c80319c
#    - span.id: b7ad6b7169203331
#    - task.id: task_001
```

### 6단계: Tracing 오버헤드 측정

트레이싱이 RT 성능에 미치는 영향을 측정합니다.

```bash
# Tracing 비활성화 시 cycle time 측정
# (tracing_config.json에서 instrumentation을 false로 설정)
curl http://localhost:9090/metrics | grep mxrc_rt_cycle_time_us_sum
# 출력 예: mxrc_rt_cycle_time_us_sum{process="rt_process"} 8000000 (avg: 800 μs)

# Tracing 활성화 시 cycle time 측정
# (tracing_config.json에서 instrumentation을 true로 설정)
curl http://localhost:9090/metrics | grep mxrc_rt_cycle_time_us_sum
# 출력 예: mxrc_rt_cycle_time_us_sum{process="rt_process"} 8400000 (avg: 840 μs)

# 오버헤드 계산: (840 - 800) / 800 = 5%
```

### 성공 기준 (Spec SC-011, SC-012, SC-013)

- ✅ Trace context 전파: Task → Sequence → Action → EventBus
- ✅ Tracing 오버헤드 < 5% (RT cycle time 기준)
- ✅ Trace 검색: 10초 이내 원인 식별 가능

---

## 통합 테스트

모든 기능을 함께 실행하여 통합 테스트를 수행합니다.

### 1단계: 모든 서비스 시작

```bash
# Elasticsearch 시작
docker start elasticsearch

# Jaeger 시작
docker start jaeger

# Filebeat 시작
sudo systemctl start filebeat

# RT 프로세스 시작 (모든 기능 활성화)
sudo systemctl start mxrc-rt.service
```

### 2단계: 부하 테스트

RT 프로세스에 부하를 가하여 성능을 검증합니다.

```bash
# 1,000개 Task 실행
for i in {1..1000}; do
  curl -X POST http://localhost:8080/api/v1/task/start -H 'Content-Type: application/json' -d "{
    \"task_id\": \"task_$i\",
    \"mode\": \"once\"
  }" &
done
wait

# Prometheus 메트릭 확인
curl http://localhost:9090/metrics | grep -E "mxrc_rt_deadline_miss_rate|mxrc_rt_jitter_us|mxrc_process_health_status"
# 출력 예:
# mxrc_rt_deadline_miss_rate{process="rt_process"} 0.0008
# mxrc_rt_jitter_us{process="rt_process"} 9.2
# mxrc_process_health_status{process="rt_process"} 2  (HEALTHY)
```

### 3단계: Failover 테스트 (부하 중)

부하 중 프로세스를 강제 종료하여 복구를 검증합니다.

```bash
# 부하 생성 (백그라운드)
for i in {1..10000}; do
  curl -X POST http://localhost:8080/api/v1/task/start -H 'Content-Type: application/json' -d "{
    \"task_id\": \"task_$i\",
    \"mode\": \"once\"
  }" > /dev/null 2>&1 &
  sleep 0.1
done

# 5초 후 프로세스 강제 종료
sleep 5
sudo kill -9 $(systemctl show -p MainPID --value mxrc-rt.service)

# 재시작 대기
sleep 5

# Health 확인
curl http://localhost:8080/health | jq
# 출력 예: {"status": "HEALTHY", "restart_count": 1}
```

### 4단계: 로그 및 Trace 상관 관계 확인

오류 로그와 해당 trace를 연결하여 문제를 진단합니다.

```bash
# Elasticsearch에서 오류 로그 조회
curl -X GET "http://localhost:9200/mxrc-logs-*/_search?pretty" -H 'Content-Type: application/json' -d '{
  "query": {
    "term": {"log.level": "error"}
  }
}'
# 출력 예:
# {
#   "@timestamp": "2025-11-21T10:35:20.500Z",
#   "log.level": "error",
#   "process.name": "rt_process",
#   "message": "Action timeout",
#   "trace.id": "0af7651916cd43dd8448eb211c80319c",
#   "mxrc.task_id": "task_500"
# }

# Jaeger에서 해당 trace 조회
# 1. http://localhost:16686 접속
# 2. Trace ID로 검색: 0af7651916cd43dd8448eb211c80319c
# 3. Span 목록에서 timeout 발생 지점 확인
```

### 성공 기준 (통합)

- ✅ CPU Isolation: Deadline miss rate < 0.01%
- ✅ NUMA Optimization: Local access > 95%
- ✅ Failover: 재시작 시간 < 5초, checkpoint 복구 성공
- ✅ Logging: 오류 로그가 Elasticsearch에 1초 이내 전송
- ✅ Tracing: 오류 trace를 10초 이내 식별
- ✅ 통합: 모든 기능이 동시에 정상 동작

---

## 벤치마크 및 성능 목표

### Performance Optimization (P1)

| 메트릭 | 목표 | 측정 방법 |
|--------|------|-----------|
| Deadline miss rate | < 0.01% | `mxrc_rt_deadline_miss_rate` |
| Cycle time jitter | 감소 50% | `mxrc_rt_jitter_us` (before/after) |
| Local NUMA access | > 95% | `mxrc_numa_local_memory_percent` |

### High Availability (P2)

| 메트릭 | 목표 | 측정 방법 |
|--------|------|-----------|
| 재시작 시간 | < 5초 | `systemctl status` 타임스탬프 |
| Health check 응답 | < 500ms | `/health` API 응답 시간 |
| Checkpoint 생성 | 60초 주기 | `/var/mxrc/checkpoints/` 파일 시간 |

### Structured Logging (P3)

| 메트릭 | 목표 | 측정 방법 |
|--------|------|-----------|
| 로그 전송 지연 | < 1초 | Elasticsearch 타임스탬프 - 로그 생성 시각 |
| 로깅 오버헤드 | < 1% | RT cycle time 증가율 |
| 로그 처리량 | > 5,000 msg/sec | `mxrc_logging_events_total` |

### Distributed Tracing (P4)

| 메트릭 | 목표 | 측정 방법 |
|--------|------|-----------|
| Tracing 오버헤드 | < 5% | RT cycle time 증가율 |
| Trace 검색 시간 | < 10초 | Jaeger UI 응답 시간 |
| Sampling rate | 10% | `mxrc_tracing_sampling_rate` |

---

## 문제 해결 (Troubleshooting)

### CPU Isolation이 동작하지 않음

**증상**: RT 프로세스가 다른 코어에서도 실행됨

**해결**:
```bash
# 1. 커널 부팅 파라미터 확인
cat /proc/cmdline | grep isolcpus
# 출력: ... isolcpus=2,3 ...

# 2. CPU affinity 재설정
sudo taskset -cp 2,3 <PID>

# 3. cgroups 확인
cat /sys/fs/cgroup/cpuset/rt_process/cpuset.cpus
# 출력: 2,3
```

### NUMA 로컬 메모리 접근율이 낮음

**증상**: `mxrc_numa_local_memory_percent` < 95%

**해결**:
```bash
# 1. NUMA 정책 확인
numactl --show

# 2. 프로세스 메모리 마이그레이션
sudo numactl --membind=0 --cpunodebind=0 ./rt_process

# 3. 메모리 할당 확인
cat /proc/<PID>/numa_maps | grep N0
# 출력: N0=<대부분의 메모리>
```

### Failover가 동작하지 않음

**증상**: 프로세스 종료 후 재시작되지 않음

**해결**:
```bash
# 1. systemd 서비스 상태 확인
sudo systemctl status mxrc-rt.service
# 출력: Active: failed (Result: exit-code)

# 2. systemd 로그 확인
sudo journalctl -u mxrc-rt.service -n 50

# 3. Watchdog 설정 확인
systemctl show mxrc-rt.service | grep Watchdog
# 출력: WatchdogUSec=10s

# 4. Health check 응답 확인
curl http://localhost:8080/health
```

### 로그가 Elasticsearch에 전송되지 않음

**증상**: Kibana에서 로그가 보이지 않음

**해결**:
```bash
# 1. Filebeat 상태 확인
sudo systemctl status filebeat

# 2. Filebeat 로그 확인
sudo journalctl -u filebeat -n 50

# 3. Elasticsearch 연결 확인
curl http://localhost:9200/_cluster/health

# 4. 로그 파일 권한 확인
ls -l /var/log/mxrc/
# filebeat가 읽을 수 있어야 함
```

### Trace가 Jaeger에 표시되지 않음

**증상**: Jaeger UI에서 trace가 보이지 않음

**해결**:
```bash
# 1. Jaeger 연결 확인
curl http://localhost:16686/api/services
# 출력: {"data":["mxrc-rt"]}

# 2. OpenTelemetry exporter 로그 확인
journalctl -u mxrc-rt.service | grep "OTLP export"

# 3. Sampling rate 확인
curl http://localhost:9090/metrics | grep mxrc_tracing_sampling_rate
# 10% 이상이어야 함

# 4. Trace 생성 확인
curl http://localhost:9090/metrics | grep mxrc_tracing_spans_total
# counter가 증가해야 함
```

---

## 다음 단계

1. **Phase 2: Task Breakdown**
   - `/speckit.tasks` 명령 실행
   - 구현 작업 목록 생성
   - 우선순위별 작업 순서 결정

2. **Implementation**
   - P1 → P2 → P3 → P4 순서로 구현
   - 각 단계마다 테스트 실행
   - Prometheus/Grafana 대시보드 구성

3. **Production Deployment**
   - 프로덕션 환경 설정 검토
   - 모니터링 알림 설정 (Alertmanager)
   - 백업 및 복구 절차 수립

---

## 참조

- **Specification**: [spec.md](spec.md)
- **Implementation Plan**: [plan.md](plan.md)
- **Research**: [research.md](research.md)
- **Data Model**: [data-model.md](data-model.md)
- **API Contracts**:
  - [health-check-api.yaml](contracts/health-check-api.yaml)
  - [metrics-api.yaml](contracts/metrics-api.yaml)
- **Constitution**: `.specify/memory/constitution.md`

---

**Version**: 1.0.0
**Created**: 2025-11-21
**Last Updated**: 2025-11-21
