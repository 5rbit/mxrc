# Feature 007: 모니터링 및 관측성

## 개요

RT/Non-RT 프로세스의 성능 메트릭을 수집하고 시각화하기 위한 모니터링 시스템을 구현합니다.

## 목표

1. **실시간 메트릭 수집**: RT/Non-RT 프로세스의 주요 메트릭 수집
2. **Prometheus 통합**: 표준 메트릭 포맷으로 노출
3. **대시보드**: Grafana를 통한 시각화
4. **알림**: 임계값 기반 알림 시스템

## 주요 기능

### 1. RT 프로세스 메트릭

#### 타이밍 메트릭
- **Cycle Time**: Minor/Major cycle 실행 시간
  - `rt_cycle_duration_seconds{type="minor"}`: Minor cycle 실행 시간
  - `rt_cycle_duration_seconds{type="major"}`: Major cycle 실행 시간
  - `rt_cycle_jitter_seconds`: Cycle 지터 (표준편차)

- **Deadline Miss**: 데드라인 위반 횟수
  - `rt_deadline_misses_total`: 총 데드라인 miss 횟수
  - `rt_deadline_miss_rate`: 데드라인 miss 비율

#### 상태 메트릭
- **State Machine**: RTStateMachine 상태
  - `rt_state`: 현재 상태 (INIT=0, READY=1, RUNNING=2, SAFE_MODE=3, SHUTDOWN=4)
  - `rt_state_transitions_total`: 상태 전이 횟수
  - `rt_safe_mode_entries_total`: SAFE_MODE 진입 횟수

- **Heartbeat**: Non-RT 연결 상태
  - `rt_nonrt_heartbeat_alive`: Non-RT heartbeat 상태 (0=lost, 1=alive)
  - `rt_nonrt_heartbeat_timeout_seconds`: Heartbeat timeout 시간

#### 데이터 메트릭
- **RTDataStore**:
  - `rt_datastore_writes_total{key}`: 키별 쓰기 횟수
  - `rt_datastore_reads_total{key}`: 키별 읽기 횟수
  - `rt_datastore_seqlock_retries_total{key}`: Seqlock 재시도 횟수

### 2. Non-RT 프로세스 메트릭

#### Task 실행 메트릭
- **Task Executor**:
  - `nonrt_tasks_total{status}`: Task 상태별 개수
  - `nonrt_task_duration_seconds{task_id}`: Task 실행 시간
  - `nonrt_task_failures_total{task_id}`: Task 실패 횟수

#### Action/Sequence 메트릭
- **Action Executor**:
  - `nonrt_actions_total{status}`: Action 상태별 개수
  - `nonrt_action_duration_seconds{action_type}`: Action 실행 시간

- **Sequence Engine**:
  - `nonrt_sequences_total{status}`: Sequence 상태별 개수
  - `nonrt_sequence_duration_seconds{sequence_id}`: Sequence 실행 시간

#### 동기화 메트릭
- **Sync Thread**:
  - `nonrt_sync_operations_total`: 동기화 작업 횟수
  - `nonrt_sync_duration_seconds`: 동기화 소요 시간

### 3. EventBus 메트릭

- `eventbus_published_total{process}`: 발행된 이벤트 수
- `eventbus_processed_total{process}`: 처리된 이벤트 수
- `eventbus_dropped_total{process}`: 드롭된 이벤트 수
- `eventbus_queue_size{process}`: 큐 크기
- `eventbus_processing_duration_seconds`: 이벤트 처리 시간

### 4. 시스템 메트릭

- **CPU/메모리**:
  - `process_cpu_usage_percent{process}`: CPU 사용률
  - `process_memory_rss_bytes{process}`: RSS 메모리
  - `process_memory_vms_bytes{process}`: VMS 메모리

- **공유 메모리**:
  - `shm_size_bytes`: 공유 메모리 크기
  - `shm_operations_total{operation}`: 공유 메모리 작업 횟수

## 아키텍처

### 메트릭 수집기 (Metrics Collector)

```cpp
class MetricsCollector {
public:
    // 카운터 증가
    void incrementCounter(const std::string& name,
                         const std::map<std::string, std::string>& labels = {});

    // 게이지 설정
    void setGauge(const std::string& name, double value,
                  const std::map<std::string, std::string>& labels = {});

    // 히스토그램 관찰
    void observeHistogram(const std::string& name, double value,
                         const std::map<std::string, std::string>& labels = {});

    // Prometheus 포맷으로 내보내기
    std::string exportPrometheus() const;
};
```

### HTTP 서버 (Metrics Endpoint)

RT 프로세스와 Non-RT 프로세스 각각:
- RT: `http://localhost:9100/metrics`
- Non-RT: `http://localhost:9101/metrics`

간단한 HTTP 서버로 `/metrics` 엔드포인트 제공

## 구현 계획

### Phase 1: 기본 메트릭 인프라
1. MetricsCollector 클래스 구현
2. HTTP 서버 구현 (간단한 TCP 소켓)
3. Prometheus 포맷 exporter

### Phase 2: RT 메트릭 통합
1. RTExecutive에 메트릭 수집 추가
2. RTStateMachine 상태 메트릭
3. RTDataStore 메트릭

### Phase 3: Non-RT 메트릭 통합
1. TaskExecutor 메트릭
2. ActionExecutor/SequenceEngine 메트릭
3. 동기화 메트릭

### Phase 4: 대시보드 및 알림
1. Grafana 대시보드 JSON 제공
2. Prometheus 알림 규칙
3. 문서화

## 사용 예시

### Prometheus 설정

```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'mxrc-rt'
    static_configs:
      - targets: ['localhost:9100']
        labels:
          process: 'rt'

  - job_name: 'mxrc-nonrt'
    static_configs:
      - targets: ['localhost:9101']
        labels:
          process: 'nonrt'
```

### Grafana 쿼리 예시

```promql
# RT Cycle Time
rate(rt_cycle_duration_seconds_sum[1m]) / rate(rt_cycle_duration_seconds_count[1m])

# Deadline Miss Rate
rate(rt_deadline_misses_total[5m])

# EventBus Queue Size
eventbus_queue_size{process="rt"}

# Task Success Rate
rate(nonrt_tasks_total{status="completed"}[1m]) / rate(nonrt_tasks_total[1m])
```

## 성능 영향

- **RT 프로세스**: 메트릭 수집은 비동기로 처리하여 RT 성능에 영향 최소화
- **메모리 오버헤드**: 약 1-2MB 추가 메모리
- **CPU 오버헤드**: < 1% (메트릭 HTTP 서버는 별도 스레드)

## 보안

- 메트릭 엔드포인트는 localhost에만 바인딩
- 필요시 basic auth 추가 가능
- 민감한 데이터는 메트릭에 포함하지 않음

## 테스트

1. **단위 테스트**: MetricsCollector 동작 검증
2. **통합 테스트**: 실제 메트릭 수집 검증
3. **부하 테스트**: 메트릭 수집의 성능 영향 측정
