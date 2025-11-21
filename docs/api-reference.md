# MXRC API Reference

**Version**: 1.0.0
**Feature**: 001-production-readiness
**Last Updated**: 2025-11-21

---

## Overview

This document provides a comprehensive reference for all HTTP APIs exposed by MXRC RT and Non-RT processes. These APIs are used for:

1. **Health monitoring** - Process health status and diagnostics
2. **Metrics collection** - Prometheus-compatible performance metrics
3. **Distributed tracing** - OpenTelemetry trace context propagation

---

## Base URLs

| Service | URL | Description |
|---------|-----|-------------|
| RT Process Health | `http://localhost:8080` | Health check endpoints for RT process |
| Non-RT Process Health | `http://localhost:8081` | Health check endpoints for Non-RT process |
| RT Process Metrics | `http://localhost:9090` | Prometheus metrics for RT process |
| Non-RT Process Metrics | `http://localhost:9091` | Prometheus metrics for Non-RT process |

---

## Health Check API

### GET /health

**Description**: Returns overall health status of the process.

**Use Case**: Called by FailoverManager, systemd watchdog, or external monitoring systems to determine if the process is healthy.

**Response Codes**:
- `200 OK` - Process is HEALTHY
- `503 Service Unavailable` - Process is DEGRADED or UNHEALTHY

**Response Body**:
```json
{
  "status": "HEALTHY",
  "process_name": "rt_process",
  "pid": 12345,
  "last_heartbeat": "2025-11-21T10:30:45.123Z",
  "response_time_ms": 1.5,
  "cpu_usage_percent": 45.2,
  "memory_usage_mb": 512,
  "deadline_miss_count": 5,
  "restart_count": 0,
  "error_message": ""
}
```

**Status Values**:
- `HEALTHY` - Process operating normally
- `DEGRADED` - Performance degradation detected (high deadline miss rate, high CPU usage)
- `UNHEALTHY` - Process is not functioning properly (critical errors, no response)
- `STARTING` - Process is initializing
- `STOPPING` - Process is shutting down
- `STOPPED` - Process is stopped

**Example**:
```bash
curl http://localhost:8080/health | jq
```

---

### GET /health/ready

**Description**: Kubernetes-style readiness probe. Returns whether the process is ready to accept requests.

**Use Case**: Used by Kubernetes readinessProbe to determine if the pod should receive traffic.

**Response Codes**:
- `200 OK` - Process is ready
- `503 Service Unavailable` - Process is not ready

**Response Body**:
```json
{
  "ready": true,
  "status": "HEALTHY",
  "process_name": "rt_process"
}
```

**Example**:
```bash
curl http://localhost:8080/health/ready | jq
```

---

### GET /health/live

**Description**: Kubernetes-style liveness probe. Returns whether the process is alive and responding.

**Use Case**: Used by Kubernetes livenessProbe. If this endpoint fails, the container will be restarted.

**Response Codes**:
- `200 OK` - Process is alive
- `503 Service Unavailable` - Process is not responding

**Response Body**:
```json
{
  "alive": true,
  "status": "HEALTHY",
  "process_name": "rt_process",
  "pid": 12345
}
```

**Example**:
```bash
curl http://localhost:8080/health/live | jq
```

---

### GET /health/details

**Description**: Detailed health information including CPU affinity, NUMA binding, RT performance metrics, and failover status.

**Use Case**: Used for debugging, monitoring dashboards, and detailed diagnostics.

**Response Codes**:
- `200 OK` - Always returns 200 with detailed information

**Response Body**:
```json
{
  "process_name": "rt_process",
  "pid": 12345,
  "status": "HEALTHY",
  "is_healthy": true,
  "is_ready": true,
  "is_alive": true,
  "last_heartbeat": "2025-11-21T10:30:45.123Z",
  "performance": {
    "response_time_ms": 1.5,
    "cpu_usage_percent": 45.2,
    "memory_usage_mb": 512,
    "deadline_miss_count": 5
  },
  "restart": {
    "restart_count": 0
  },
  "assessment": {
    "level": "good",
    "message": "Process is operating normally"
  }
}
```

**Assessment Levels**:
- `good` - HEALTHY status
- `warning` - DEGRADED status
- `critical` - UNHEALTHY status
- `info` - STARTING, STOPPING, or STOPPED status

**Example**:
```bash
curl http://localhost:8080/health/details | jq
```

---

## Metrics API

### GET /metrics

**Description**: Returns Prometheus-compatible metrics in text format.

**Use Case**: Scraped by Prometheus at regular intervals (default: every 15 seconds).

**Response Code**:
- `200 OK` - Always returns 200 with metrics

**Response Format**: Prometheus text exposition format

**Metric Categories**:

#### 1. CPU Affinity & NUMA Optimization

| Metric Name | Type | Description |
|-------------|------|-------------|
| `mxrc_cpu_affinity_info` | Gauge | CPU affinity configuration (labels: process, configured_cores, isolation_mode) |
| `mxrc_cpu_current_core` | Gauge | Current CPU core the process is running on |
| `mxrc_numa_node` | Gauge | NUMA node the process is bound to |
| `mxrc_numa_local_memory_percent` | Gauge | Percentage of local NUMA memory access (target: >95%) |

#### 2. RT Performance

| Metric Name | Type | Description |
|-------------|------|-------------|
| `mxrc_rt_cycle_count_total` | Counter | Total number of RT cycles executed |
| `mxrc_rt_deadline_miss_count_total` | Counter | Total number of deadline misses |
| `mxrc_rt_deadline_miss_rate` | Gauge | Deadline miss rate (0.0 to 1.0, target: <0.0001) |
| `mxrc_rt_cycle_time_us` | Histogram | RT cycle time distribution in microseconds |
| `mxrc_rt_jitter_us` | Gauge | RT cycle time jitter in microseconds |

#### 3. High Availability & Failover

| Metric Name | Type | Description |
|-------------|------|-------------|
| `mxrc_process_health_status` | Gauge | Process health status (0=STOPPED, 1=STARTING, 2=HEALTHY, 3=DEGRADED, 4=UNHEALTHY, 5=STOPPING) |
| `mxrc_process_restart_count_total` | Counter | Total number of process restarts |
| `mxrc_failover_checkpoint_age_seconds` | Gauge | Age of the last checkpoint in seconds |
| `mxrc_failover_checkpoint_size_bytes` | Gauge | Size of the last checkpoint in bytes |

#### 4. Structured Logging

| Metric Name | Type | Description |
|-------------|------|-------------|
| `mxrc_logging_events_total` | Counter | Total number of log events by level (labels: process, level) |
| `mxrc_logging_latency_us` | Histogram | Logging latency in microseconds |

#### 5. Distributed Tracing

| Metric Name | Type | Description |
|-------------|------|-------------|
| `mxrc_tracing_spans_total` | Counter | Total number of spans created (labels: process, operation) |
| `mxrc_tracing_sampling_rate` | Gauge | Tracing sampling rate (0.0 to 1.0) |
| `mxrc_tracing_export_latency_ms` | Histogram | Tracing export latency in milliseconds |

**Example Response**:
```
# HELP mxrc_rt_cycle_count_total Total number of RT cycles executed
# TYPE mxrc_rt_cycle_count_total counter
mxrc_rt_cycle_count_total{process="rt_process"} 10000

# HELP mxrc_rt_deadline_miss_count_total Total number of deadline misses
# TYPE mxrc_rt_deadline_miss_count_total counter
mxrc_rt_deadline_miss_count_total{process="rt_process"} 5

# HELP mxrc_rt_deadline_miss_rate Deadline miss rate
# TYPE mxrc_rt_deadline_miss_rate gauge
mxrc_rt_deadline_miss_rate{process="rt_process"} 0.0005

# HELP mxrc_numa_local_memory_percent Percentage of local NUMA memory access
# TYPE mxrc_numa_local_memory_percent gauge
mxrc_numa_local_memory_percent{process="rt_process",numa_node="0"} 96.5
```

**Example**:
```bash
# Get all metrics
curl http://localhost:9090/metrics

# Filter specific metrics
curl http://localhost:9090/metrics | grep mxrc_rt

# Get deadline miss rate
curl http://localhost:9090/metrics | grep mxrc_rt_deadline_miss_rate
```

---

## Implementation Details

### Health Check Server

**Source**: `/home/tory/workspace/mxrc/mxrc/src/core/ha/HealthCheckServer.cpp`

**Key Features**:
- Single-threaded TCP server listening on localhost only (127.0.0.1)
- Lightweight HTTP/1.1 request parsing
- JSON responses with ISO 8601 timestamps
- SO_REUSEADDR for fast restart

**Security**:
- Binds to localhost only (127.0.0.1) - not accessible from external network
- No authentication required (local-only access)
- Read-only API (no state modification)

**Performance**:
- Response time: <10ms for /health endpoint
- Thread-safe using shared_ptr<IHealthCheck>
- Non-blocking server loop

### Metrics Collection

**Source**: `/home/tory/workspace/mxrc/mxrc/src/core/monitoring/MetricsCollector.cpp`

**Key Features**:
- Thread-safe metric collection using std::mutex
- Counter, Gauge, and Histogram metric types
- Prometheus text exposition format export
- Label-based metric organization

**Metric Types**:

1. **Counter**: Monotonically increasing value
   - `increment(delta)` - Add delta to counter
   - Thread-safe using atomic operations

2. **Gauge**: Value that can increase or decrease
   - `set(value)` - Set gauge to value
   - `increment(delta)` - Add delta
   - `decrement(delta)` - Subtract delta

3. **Histogram**: Distribution of values
   - `observe(value)` - Record a value
   - Automatically assigns values to buckets
   - Provides sum, count, and bucket counts

---

## Error Handling

### HTTP Error Codes

| Code | Status | When Returned |
|------|--------|---------------|
| 200 | OK | Request successful |
| 404 | Not Found | Invalid endpoint path |
| 405 | Method Not Allowed | Non-GET request |
| 503 | Service Unavailable | Process unhealthy or not ready |

### Error Response Format

```json
{
  "error": "Not Found",
  "path": "/invalid"
}
```

---

## Integration Examples

### Prometheus Configuration

Add this to `prometheus.yml`:

```yaml
scrape_configs:
  - job_name: 'mxrc-rt'
    static_configs:
      - targets: ['localhost:9090']
    scrape_interval: 15s
    scrape_timeout: 10s

  - job_name: 'mxrc-non-rt'
    static_configs:
      - targets: ['localhost:9091']
    scrape_interval: 15s
    scrape_timeout: 10s
```

### Kubernetes Probes

Add this to your Deployment:

```yaml
apiVersion: v1
kind: Pod
metadata:
  name: mxrc-rt
spec:
  containers:
  - name: rt-process
    image: mxrc:latest
    ports:
    - containerPort: 8080
      name: health
    - containerPort: 9090
      name: metrics
    livenessProbe:
      httpGet:
        path: /health/live
        port: 8080
      initialDelaySeconds: 10
      periodSeconds: 10
      timeoutSeconds: 5
      failureThreshold: 3
    readinessProbe:
      httpGet:
        path: /health/ready
        port: 8080
      initialDelaySeconds: 5
      periodSeconds: 5
      timeoutSeconds: 3
      failureThreshold: 2
```

### systemd Service

Add this to your service file:

```ini
[Unit]
Description=MXRC RT Process
After=network.target

[Service]
Type=notify
ExecStart=/usr/local/bin/rt_process
Restart=on-failure
RestartSec=1s
WatchdogSec=10s
NotifyAccess=main

[Install]
WantedBy=multi-user.target
```

---

## Monitoring and Alerting

### Recommended Prometheus Alerts

```yaml
groups:
  - name: mxrc_rt
    rules:
      # High deadline miss rate
      - alert: HighDeadlineMissRate
        expr: mxrc_rt_deadline_miss_rate{process="rt_process"} > 0.0001
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "RT process has high deadline miss rate"
          description: "Deadline miss rate is {{ $value }} (threshold: 0.0001)"

      # Low NUMA local memory access
      - alert: LowNUMALocalAccess
        expr: mxrc_numa_local_memory_percent{process="rt_process"} < 95.0
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Low NUMA local memory access"
          description: "Local memory access is {{ $value }}% (target: >95%)"

      # Process unhealthy
      - alert: ProcessUnhealthy
        expr: mxrc_process_health_status{process="rt_process"} >= 3
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Process is unhealthy"
          description: "Health status is {{ $value }} (2=HEALTHY, 3=DEGRADED, 4=UNHEALTHY)"

      # Process restarted
      - alert: ProcessRestarted
        expr: increase(mxrc_process_restart_count_total{process="rt_process"}[5m]) > 0
        labels:
          severity: warning
        annotations:
          summary: "Process was restarted"
          description: "Process restarted {{ $value }} times in the last 5 minutes"
```

---

## OpenAPI Specifications

For machine-readable API specifications, see:

- **Health Check API**: `/home/tory/workspace/mxrc/mxrc/docs/specs/001-production-readiness/contracts/health-check-api.yaml`
- **Metrics API**: `/home/tory/workspace/mxrc/mxrc/docs/specs/001-production-readiness/contracts/metrics-api.yaml`

These can be used with tools like:
- Swagger UI
- Postman
- OpenAPI Generator

---

## References

- [Prometheus Text Exposition Format](https://prometheus.io/docs/instrumenting/exposition_formats/)
- [Kubernetes Probe Configuration](https://kubernetes.io/docs/tasks/configure-pod-container/configure-liveness-readiness-startup-probes/)
- [systemd Watchdog](https://www.freedesktop.org/software/systemd/man/systemd.service.html#WatchdogSec=)
- [OpenAPI Specification 3.0](https://swagger.io/specification/)

---

**Version**: 1.0.0
**Created**: 2025-11-21
**Last Updated**: 2025-11-21
