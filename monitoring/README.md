# MXRC 모니터링 가이드

RT/Non-RT 프로세스의 성능 메트릭을 Prometheus와 Grafana를 통해 모니터링하는 방법을 설명합니다.

## 아키텍처

```
┌──────────────┐     /metrics      ┌────────────┐
│ RT Process   │◄──────────────────┤ Prometheus │
│ :9100        │                    │            │
└──────────────┘                    │            │
                                    │            │     ┌────────────┐
┌──────────────┐     /metrics      │            │────►│  Grafana   │
│ Non-RT       │◄──────────────────┤            │     │  :3000     │
│ Process      │                    │            │     └────────────┘
│ :9101        │                    └────────────┘
└──────────────┘                           │
                                           │
                                           ▼
                                    ┌────────────┐
                                    │ Alertmanager│
                                    │  :9093     │
                                    └────────────┘
```

## 설치 및 설정

### 1. Prometheus 설치

```bash
# Ubuntu/Debian
sudo apt-get install prometheus

# 또는 Docker
docker run -d -p 9090:9090 \
  -v $(pwd)/monitoring/prometheus/prometheus.yml:/etc/prometheus/prometheus.yml \
  -v $(pwd)/monitoring/prometheus/alerts.yml:/etc/prometheus/alerts.yml \
  prom/prometheus
```

### 2. Prometheus 설정

`monitoring/prometheus/prometheus.yml` 파일을 생성:

```yaml
global:
  scrape_interval: 5s
  evaluation_interval: 5s

# 알림 규칙 로드
rule_files:
  - "alerts.yml"

# Alertmanager 설정
alerting:
  alertmanagers:
    - static_configs:
        - targets:
            - localhost:9093

scrape_configs:
  # RT 프로세스 메트릭
  - job_name: 'mxrc-rt'
    static_configs:
      - targets: ['localhost:9100']
        labels:
          process: 'rt'
          instance: 'mxrc-rt-1'

  # Non-RT 프로세스 메트릭
  - job_name: 'mxrc-nonrt'
    static_configs:
      - targets: ['localhost:9101']
        labels:
          process: 'nonrt'
          instance: 'mxrc-nonrt-1'
```

### 3. Grafana 설치

```bash
# Ubuntu/Debian
sudo apt-get install grafana

# 또는 Docker
docker run -d -p 3000:3000 grafana/grafana
```

### 4. Grafana 대시보드 임포트

1. Grafana 웹 UI 접속: http://localhost:3000 (기본 계정: admin/admin)
2. Configuration → Data Sources → Add data source
3. Prometheus 선택, URL: http://localhost:9090
4. Dashboards → Import
5. `monitoring/grafana/mxrc-dashboard.json` 파일 업로드

### 5. Alertmanager 설정 (선택적)

```bash
# Alertmanager 설치
sudo apt-get install prometheus-alertmanager

# 또는 Docker
docker run -d -p 9093:9093 \
  -v $(pwd)/monitoring/prometheus/alertmanager.yml:/etc/alertmanager/alertmanager.yml \
  prom/alertmanager
```

`monitoring/prometheus/alertmanager.yml` 예시:

```yaml
global:
  smtp_smarthost: 'smtp.gmail.com:587'
  smtp_from: 'alerts@example.com'
  smtp_auth_username: 'alerts@example.com'
  smtp_auth_password: 'your-password'

route:
  group_by: ['alertname', 'severity']
  group_wait: 10s
  group_interval: 10s
  repeat_interval: 1h
  receiver: 'email-notifications'

receivers:
  - name: 'email-notifications'
    email_configs:
      - to: 'team@example.com'
        headers:
          Subject: '[MXRC Alert] {{ .GroupLabels.alertname }}'
```

## 수집되는 메트릭

### RT 프로세스 메트릭

#### Timing Metrics
- `rt_cycle_duration_seconds{type="minor"}`: Minor cycle 실행 시간 (histogram)
- `rt_cycle_duration_seconds{type="major"}`: Major cycle 실행 시간 (histogram)
- `rt_cycle_jitter_seconds`: Cycle jitter (histogram)
- `rt_deadline_misses_total`: 데드라인 miss 횟수 (counter)

#### State Machine Metrics
- `rt_state`: 현재 상태 (0=INIT, 1=READY, 2=RUNNING, 3=SAFE_MODE, 4=SHUTDOWN) (gauge)
- `rt_state_transitions_total`: 상태 전이 횟수 (counter)
- `rt_safe_mode_entries_total`: SAFE_MODE 진입 횟수 (counter)

#### Heartbeat Metrics
- `rt_nonrt_heartbeat_alive`: Non-RT heartbeat 상태 (0=lost, 1=alive) (gauge)
- `rt_nonrt_heartbeat_timeout_seconds`: Heartbeat timeout 시간 (gauge)

#### DataStore Metrics
- `rt_datastore_writes_total{key}`: 키별 쓰기 횟수 (counter)
- `rt_datastore_reads_total{key}`: 키별 읽기 횟수 (counter)
- `rt_datastore_seqlock_retries_total{key}`: Seqlock 재시도 횟수 (counter)

### Non-RT 프로세스 메트릭

#### Task Metrics
- `nonrt_tasks_total{status}`: Task 상태별 개수 (counter)
- `nonrt_task_duration_seconds{task_id}`: Task 실행 시간 (histogram)
- `nonrt_task_failures_total{task_id}`: Task 실패 횟수 (counter)

#### Action/Sequence Metrics
- `nonrt_actions_total{status}`: Action 상태별 개수 (counter)
- `nonrt_action_duration_seconds{action_type}`: Action 실행 시간 (histogram)
- `nonrt_sequences_total{status}`: Sequence 상태별 개수 (counter)
- `nonrt_sequence_duration_seconds{sequence_id}`: Sequence 실행 시간 (histogram)

#### Sync Metrics
- `nonrt_sync_operations_total`: 동기화 작업 횟수 (counter)
- `nonrt_sync_duration_seconds`: 동기화 소요 시간 (histogram)

### EventBus 메트릭

- `eventbus_published_total{process}`: 발행된 이벤트 수 (counter)
- `eventbus_processed_total{process}`: 처리된 이벤트 수 (counter)
- `eventbus_dropped_total{process}`: 드롭된 이벤트 수 (counter)
- `eventbus_queue_size{process}`: 큐 크기 (gauge)
- `eventbus_processing_duration_seconds`: 이벤트 처리 시간 (histogram)

### 시스템 메트릭

- `process_cpu_usage_percent{process}`: CPU 사용률 (gauge)
- `process_memory_rss_bytes{process}`: RSS 메모리 (gauge)
- `process_memory_vms_bytes{process}`: VMS 메모리 (gauge)
- `shm_size_bytes`: 공유 메모리 크기 (gauge)
- `shm_operations_total{operation}`: 공유 메모리 작업 횟수 (counter)

## 유용한 PromQL 쿼리

### RT 성능 분석

```promql
# Minor cycle 평균 실행 시간
rate(rt_cycle_duration_seconds_sum{type="minor"}[1m]) /
rate(rt_cycle_duration_seconds_count{type="minor"}[1m])

# Deadline miss 비율
rate(rt_deadline_misses_total[5m])

# Cycle jitter 99th percentile
histogram_quantile(0.99, rate(rt_cycle_jitter_seconds_bucket[5m]))

# SAFE_MODE 진입 빈도
increase(rt_safe_mode_entries_total[1h])
```

### Non-RT 성능 분석

```promql
# Task 성공률
rate(nonrt_tasks_total{status="completed"}[1m]) /
rate(nonrt_tasks_total[1m])

# Action 평균 실행 시간 (action_type별)
rate(nonrt_action_duration_seconds_sum[1m]) /
rate(nonrt_action_duration_seconds_count[1m])

# Sequence 실패율
rate(nonrt_sequences_total{status="failed"}[5m]) /
rate(nonrt_sequences_total[5m])
```

### EventBus 분석

```promql
# 이벤트 처리 지연
rate(eventbus_published_total[1m]) -
rate(eventbus_processed_total[1m])

# 이벤트 드롭율
rate(eventbus_dropped_total[1m]) /
rate(eventbus_published_total[1m])

# 큐 사용률
eventbus_queue_size / 10000 * 100
```

## 알림 규칙

`monitoring/prometheus/alerts.yml` 파일에 정의된 알림:

### Critical Alerts
- **RTProcessDown**: RT 프로세스 다운 (30초)
- **NonRTProcessDown**: Non-RT 프로세스 다운 (30초)
- **NonRTHeartbeatLost**: Non-RT heartbeat 끊김 (1분)
- **EventBusEventsDropped**: EventBus 이벤트 드롭 발생

### Warning Alerts
- **RTSafeModeEntered**: RT SAFE_MODE 진입 (1분)
- **RTHighDeadlineMissRate**: 높은 deadline miss 비율 (2분)
- **RTCycleDurationHigh**: Cycle 실행 시간 초과 (5분)
- **NonRTHighTaskFailureRate**: 높은 Task 실패율 (5분)
- **EventBusQueueNearFull**: EventBus 큐 가득 참 (1분)
- **DataStoreHighSeqlockRetries**: 높은 Seqlock 재시도율 (5분)

## 대시보드 사용법

### RT Process Panel
1. **RT Cycle Duration**: Minor/Major cycle 실행 시간 추이
2. **RT State**: 현재 RT 상태 표시
3. **Non-RT Heartbeat**: Non-RT 연결 상태
4. **Deadline Miss Rate**: 데드라인 위반 비율
5. **DataStore Operations**: 키별 읽기/쓰기 작업량

### Non-RT Process Panel
1. **Task Status**: Task 상태별 개수
2. **Task Duration**: Task 실행 시간 추이
3. **Action/Sequence Metrics**: Action과 Sequence 실행 통계

### EventBus Panel
1. **EventBus Operations**: 발행/처리/드롭 이벤트 추이
2. **EventBus Queue Size**: 큐 사용량

## 문제 해결

### 메트릭이 수집되지 않음

1. RT/Non-RT 프로세스가 실행 중인지 확인:
   ```bash
   ps aux | grep mxrc
   ```

2. 메트릭 엔드포인트 접근 가능 확인:
   ```bash
   curl http://localhost:9100/metrics  # RT
   curl http://localhost:9101/metrics  # Non-RT
   ```

3. Prometheus targets 상태 확인:
   - http://localhost:9090/targets

### 알림이 작동하지 않음

1. Prometheus 알림 규칙 확인:
   - http://localhost:9090/alerts

2. Alertmanager 상태 확인:
   - http://localhost:9093

3. 알림 규칙 문법 검증:
   ```bash
   promtool check rules monitoring/prometheus/alerts.yml
   ```

### Grafana 대시보드가 데이터를 표시하지 않음

1. Data Source 연결 확인:
   - Configuration → Data Sources → Prometheus → Test

2. 시간 범위 확인:
   - 대시보드 우측 상단 시간 범위 확인

3. 메트릭 이름 확인:
   - Explore 탭에서 메트릭 검색

## 성능 최적화

### 메트릭 수집 오버헤드 최소화

1. **scrape_interval 조정**: 기본 5초, 필요시 10초로 증가
2. **불필요한 메트릭 비활성화**: 코드에서 메트릭 수집 주석 처리
3. **라벨 개수 제한**: 고 카디널리티 라벨 최소화

### 저장 공간 최적화

```yaml
# prometheus.yml에 추가
storage:
  tsdb:
    retention.time: 7d  # 7일간 보관
    retention.size: 10GB  # 최대 10GB
```

## 보안

- 메트릭 엔드포인트는 localhost에만 바인딩됨
- 프로덕션 환경에서는 reverse proxy (nginx) 사용 권장
- Basic authentication 또는 TLS 추가 가능

## 참고 자료

- [Prometheus 공식 문서](https://prometheus.io/docs/)
- [Grafana 공식 문서](https://grafana.com/docs/)
- [PromQL 가이드](https://prometheus.io/docs/prometheus/latest/querying/basics/)
