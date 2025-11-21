# MXRC Configuration Guide

**Version**: 1.0.0
**Feature**: 001-production-readiness
**Last Updated**: 2025-11-21

---

## Overview

This guide provides detailed documentation for all JSON configuration files used by MXRC. Each configuration file controls a specific aspect of production readiness:

1. **CPU Affinity** - RT process CPU core assignment
2. **NUMA Binding** - Memory locality optimization
3. **Failover Policy** - High availability and automatic recovery
4. **Tracing** - Distributed tracing with OpenTelemetry

---

## Configuration File Locations

| File | Location | Description |
|------|----------|-------------|
| CPU Affinity | `/home/tory/workspace/mxrc/mxrc/config/rt/cpu_affinity.json` | CPU core pinning for RT process |
| NUMA Binding | `/home/tory/workspace/mxrc/mxrc/config/rt/numa_binding.json` | NUMA node binding for memory locality |
| Failover Policy | `/home/tory/workspace/mxrc/mxrc/config/ha/failover_policy.json` | Health check and restart policy |
| Tracing Config | `/home/tory/workspace/mxrc/mxrc/config/tracing_config.json` | OpenTelemetry tracing configuration |

---

## 1. CPU Affinity Configuration

### File: `config/rt/cpu_affinity.json`

**Purpose**: Configure which CPU cores the RT process should run on and what scheduling policy to use.

### Full Configuration

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "description": "CPU Affinity Configuration for RT Process",
  "version": "1.0.0",

  "process_name": "mxrc_rt",

  "cpu_affinity": {
    "enabled": true,
    "cpu_cores": [1, 2],
    "comment": "RT process will be pinned to CPU cores 1 and 2. These cores should be isolated using isolcpus kernel parameter."
  },

  "scheduler": {
    "policy": "SCHED_FIFO",
    "priority": 99,
    "comment": "SCHED_FIFO with highest priority (99) for deterministic RT performance. Requires CAP_SYS_NICE capability."
  },

  "isolation": {
    "verify_isolcpus": true,
    "verify_cgroups": false,
    "comment": "Verify CPU cores are isolated from kernel scheduler. Requires isolcpus=1,2 kernel boot parameter."
  },

  "fallback": {
    "allow_non_isolated": false,
    "default_policy": "SCHED_OTHER",
    "default_priority": 0,
    "comment": "Fallback settings if RT scheduling fails. Set allow_non_isolated=true for testing without isolated CPUs."
  }
}
```

### Field Descriptions

#### `cpu_affinity` Section

| Field | Type | Valid Values | Description | Default |
|-------|------|--------------|-------------|---------|
| `enabled` | boolean | true, false | Enable CPU affinity | true |
| `cpu_cores` | array of int | [0-N] where N is CPU count-1 | CPU cores to pin the process to | [1, 2] |

**Example**: `"cpu_cores": [2, 3]` pins the process to cores 2 and 3.

#### `scheduler` Section

| Field | Type | Valid Values | Description | Default |
|-------|------|--------------|-------------|---------|
| `policy` | string | SCHED_FIFO, SCHED_RR, SCHED_OTHER | Scheduling policy | SCHED_FIFO |
| `priority` | integer | 1-99 (for RT policies) | Scheduling priority | 99 |

**Scheduling Policies**:
- `SCHED_FIFO`: First-in-first-out real-time scheduling (recommended for RT)
- `SCHED_RR`: Round-robin real-time scheduling
- `SCHED_OTHER`: Default Linux time-sharing scheduling

**Priority Levels**:
- `99`: Highest RT priority (recommended for critical RT tasks)
- `50`: Medium RT priority
- `1`: Lowest RT priority
- `0`: Non-RT (SCHED_OTHER only)

#### `isolation` Section

| Field | Type | Valid Values | Description | Default |
|-------|------|--------------|-------------|---------|
| `verify_isolcpus` | boolean | true, false | Verify cores are isolated with isolcpus | true |
| `verify_cgroups` | boolean | true, false | Verify cgroup CPU isolation | false |

**Note**: For `verify_isolcpus` to succeed, you must add `isolcpus=1,2` to kernel boot parameters.

#### `fallback` Section

| Field | Type | Valid Values | Description | Default |
|-------|------|--------------|-------------|---------|
| `allow_non_isolated` | boolean | true, false | Allow running on non-isolated CPUs | false |
| `default_policy` | string | SCHED_OTHER | Fallback scheduling policy | SCHED_OTHER |
| `default_priority` | integer | 0 | Fallback priority | 0 |

**When to use fallback**:
- Set `allow_non_isolated=true` for testing on systems without isolated CPUs
- Production systems should use `allow_non_isolated=false`

### Common Scenarios

#### Scenario 1: Production (4-core system)

```json
{
  "cpu_affinity": {
    "enabled": true,
    "cpu_cores": [2, 3]
  },
  "scheduler": {
    "policy": "SCHED_FIFO",
    "priority": 99
  },
  "isolation": {
    "verify_isolcpus": true
  },
  "fallback": {
    "allow_non_isolated": false
  }
}
```

Kernel boot parameter: `isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3`

#### Scenario 2: Development (no isolation)

```json
{
  "cpu_affinity": {
    "enabled": true,
    "cpu_cores": [2, 3]
  },
  "scheduler": {
    "policy": "SCHED_FIFO",
    "priority": 80
  },
  "isolation": {
    "verify_isolcpus": false
  },
  "fallback": {
    "allow_non_isolated": true
  }
}
```

No kernel boot parameters required.

---

## 2. NUMA Binding Configuration

### File: `config/rt/numa_binding.json`

**Purpose**: Configure NUMA node binding to improve memory access locality.

### Full Configuration

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "description": "NUMA Binding Configuration for RT Process",
  "version": "1.0.0",

  "process_name": "mxrc_rt",

  "numa_binding": {
    "enabled": true,
    "numa_node": 0,
    "comment": "Bind RT process memory to NUMA node 0 for memory locality."
  },

  "memory_policy": {
    "policy": "BIND",
    "strict": true,
    "comment": "BIND: Allocate memory only from specified NUMA node."
  },

  "memory_allocation": {
    "prefault_pages": true,
    "prefault_size_mb": 100,
    "comment": "Pre-fault memory pages during initialization."
  },

  "verification": {
    "verify_numa_available": true,
    "verify_node_exists": true,
    "verify_binding": true,
    "min_local_access_percent": 95.0,
    "comment": "Verify NUMA binding and target >95% local access (SC-003)."
  },

  "monitoring": {
    "enable_numa_stats": true,
    "stats_update_interval_ms": 1000,
    "comment": "Collect NUMA stats from /proc/[pid]/numa_maps."
  },

  "fallback": {
    "allow_non_numa": true,
    "default_policy": "DEFAULT",
    "comment": "Fallback for non-NUMA systems."
  }
}
```

### Field Descriptions

#### `numa_binding` Section

| Field | Type | Valid Values | Description | Default |
|-------|------|--------------|-------------|---------|
| `enabled` | boolean | true, false | Enable NUMA binding | true |
| `numa_node` | integer | 0-N where N is NUMA node count-1 | NUMA node to bind to | 0 |

**Note**: CPU cores in `cpu_affinity.json` should be on the same NUMA node.

#### `memory_policy` Section

| Field | Type | Valid Values | Description | Default |
|-------|------|--------------|-------------|---------|
| `policy` | string | BIND, PREFERRED, INTERLEAVE, DEFAULT | NUMA memory policy | BIND |
| `strict` | boolean | true, false | Strict policy enforcement | true |

**Memory Policies**:
- `BIND`: Allocate memory only from specified NUMA node (recommended for RT)
- `PREFERRED`: Prefer specified node but allow fallback
- `INTERLEAVE`: Distribute memory across multiple nodes
- `DEFAULT`: Use system default policy

#### `memory_allocation` Section

| Field | Type | Valid Values | Description | Default |
|-------|------|--------------|-------------|---------|
| `prefault_pages` | boolean | true, false | Pre-fault memory pages at startup | true |
| `prefault_size_mb` | integer | 1-N | Amount of memory to pre-fault (MB) | 100 |

**Why prefault?**: Avoids page faults during RT execution, improving determinism.

#### `verification` Section

| Field | Type | Valid Values | Description | Default |
|-------|------|--------------|-------------|---------|
| `verify_numa_available` | boolean | true, false | Check if NUMA is available | true |
| `verify_node_exists` | boolean | true, false | Check if NUMA node exists | true |
| `verify_binding` | boolean | true, false | Verify binding succeeded | true |
| `min_local_access_percent` | float | 0.0-100.0 | Target for local memory access (%) | 95.0 |

**Success Criterion SC-003**: Local NUMA memory access > 95%

#### `monitoring` Section

| Field | Type | Valid Values | Description | Default |
|-------|------|--------------|-------------|---------|
| `enable_numa_stats` | boolean | true, false | Enable NUMA statistics collection | true |
| `stats_update_interval_ms` | integer | 100-10000 | Stats update interval (ms) | 1000 |

#### `fallback` Section

| Field | Type | Valid Values | Description | Default |
|-------|------|--------------|-------------|---------|
| `allow_non_numa` | boolean | true, false | Allow running on non-NUMA systems | true |
| `default_policy` | string | DEFAULT | Fallback memory policy | DEFAULT |

### Common Scenarios

#### Scenario 1: Production (2-socket NUMA system)

```json
{
  "numa_binding": {
    "enabled": true,
    "numa_node": 0
  },
  "memory_policy": {
    "policy": "BIND",
    "strict": true
  },
  "memory_allocation": {
    "prefault_pages": true,
    "prefault_size_mb": 256
  },
  "verification": {
    "min_local_access_percent": 95.0
  }
}
```

CPU cores 2, 3 should be on NUMA node 0. Check with: `numactl --hardware`

#### Scenario 2: Development (single-socket, no NUMA)

```json
{
  "numa_binding": {
    "enabled": false
  },
  "fallback": {
    "allow_non_numa": true
  }
}
```

---

## 3. Failover Policy Configuration

### File: `config/ha/failover_policy.json`

**Purpose**: Configure health check intervals, restart policy, and state recovery.

### Full Configuration

```json
{
  "process_name": "mxrc-rt",
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

### Field Descriptions

| Field | Type | Valid Values | Description | Default |
|-------|------|--------------|-------------|---------|
| `process_name` | string | Any valid process name | Target process to monitor | "mxrc-rt" |
| `health_check_interval_ms` | integer | 100-10000 | Health check interval (ms) | 1000 |
| `health_check_timeout_ms` | integer | 10-5000 | Health check timeout (ms) | 500 |
| `failure_threshold` | integer | 1-10 | Consecutive failures before restart | 3 |
| `restart_delay_ms` | integer | 0-5000 | Delay before restart (ms) | 100 |
| `max_restart_count` | integer | 1-100 | Max restarts in time window | 5 |
| `restart_window_sec` | integer | 10-3600 | Time window for restart count (sec) | 60 |
| `enable_state_recovery` | boolean | true, false | Enable checkpoint recovery | true |
| `checkpoint_interval_sec` | integer | 10-3600 | Checkpoint creation interval (sec) | 60 |
| `enable_leader_election` | boolean | true, false | Enable leader election (Bully) | false |

### Field Constraints

- `health_check_interval_ms` > `health_check_timeout_ms`
- `failure_threshold` >= 1
- `max_restart_count` >= 1
- If `enable_state_recovery` is true, `checkpoint_interval_sec` > 0

### Common Scenarios

#### Scenario 1: Production (aggressive failover)

```json
{
  "process_name": "mxrc-rt",
  "health_check_interval_ms": 500,
  "health_check_timeout_ms": 200,
  "failure_threshold": 2,
  "restart_delay_ms": 50,
  "max_restart_count": 10,
  "restart_window_sec": 60,
  "enable_state_recovery": true,
  "checkpoint_interval_sec": 30
}
```

**Behavior**: Fast detection (500ms interval), quick restart (2 failures), frequent checkpoints (30s).

#### Scenario 2: Development (relaxed failover)

```json
{
  "process_name": "mxrc-rt",
  "health_check_interval_ms": 2000,
  "health_check_timeout_ms": 1000,
  "failure_threshold": 5,
  "restart_delay_ms": 1000,
  "max_restart_count": 3,
  "restart_window_sec": 300,
  "enable_state_recovery": false
}
```

**Behavior**: Slower detection (2s interval), tolerant (5 failures), no checkpoint overhead.

---

## 4. Distributed Tracing Configuration

### File: `config/tracing_config.json`

**Purpose**: Configure OpenTelemetry distributed tracing, sampling, and export.

### Full Configuration

```json
{
  "tracing": {
    "enabled": true,
    "provider": "lightweight",
    "service_name": "mxrc",
    "service_version": "1.0.0",

    "sampling": {
      "strategy": "probabilistic",
      "default_rate": 0.1,
      "rt_cycle_rate": 0.05,
      "event_bus_rate": 0.1
    },

    "exporters": {
      "console": {
        "enabled": true,
        "log_level": "debug"
      },
      "file": {
        "enabled": false,
        "path": "/var/log/mxrc/traces/",
        "max_file_size_mb": 100,
        "rotation_policy": "daily"
      },
      "otlp": {
        "enabled": false,
        "endpoint": "http://localhost:4317",
        "protocol": "grpc",
        "timeout_ms": 5000,
        "compression": "gzip"
      }
    },

    "instrumentation": {
      "rt_executive": {
        "enabled": true,
        "trace_cycles": true,
        "trace_actions": true,
        "trace_timing": true,
        "sampling_rate": 0.05
      },
      "event_bus": {
        "enabled": true,
        "trace_publish": true,
        "trace_dispatch": true,
        "sampling_rate": 0.1
      },
      "datastore": {
        "enabled": false,
        "trace_reads": false,
        "trace_writes": false,
        "sampling_rate": 0.01
      }
    },

    "resource_attributes": {
      "service.name": "mxrc",
      "service.namespace": "realtime-control",
      "service.instance.id": "mxrc-rt-01",
      "deployment.environment": "production",
      "host.name": "auto",
      "process.runtime.name": "cpp",
      "process.runtime.version": "20"
    },

    "performance": {
      "max_queue_size": 2048,
      "batch_timeout_ms": 1000,
      "max_export_batch_size": 512,
      "max_export_timeout_ms": 30000,
      "schedule_delay_ms": 5000
    },

    "limits": {
      "max_attributes_per_span": 128,
      "max_events_per_span": 128,
      "max_links_per_span": 128,
      "max_attribute_value_length": 4096
    }
  }
}
```

### Field Descriptions

#### `sampling` Section

| Field | Type | Valid Values | Description | Default |
|-------|------|--------------|-------------|---------|
| `strategy` | string | probabilistic, always_on, always_off | Sampling strategy | probabilistic |
| `default_rate` | float | 0.0-1.0 | Default sampling rate | 0.1 |
| `rt_cycle_rate` | float | 0.0-1.0 | RT cycle sampling rate | 0.05 |
| `event_bus_rate` | float | 0.0-1.0 | EventBus sampling rate | 0.1 |

**Sampling Rates**:
- `1.0`: Sample all traces (high overhead)
- `0.1`: Sample 10% of traces (recommended for production)
- `0.05`: Sample 5% of traces (recommended for RT cycles)
- `0.01`: Sample 1% of traces (low overhead)

#### `exporters` Section

**Console Exporter**:
```json
{
  "enabled": true,
  "log_level": "debug"
}
```
Useful for development. Prints traces to stdout.

**File Exporter**:
```json
{
  "enabled": false,
  "path": "/var/log/mxrc/traces/",
  "max_file_size_mb": 100,
  "rotation_policy": "daily"
}
```
Write traces to local files. Not recommended for production.

**OTLP Exporter** (Recommended for production):
```json
{
  "enabled": true,
  "endpoint": "http://localhost:4317",
  "protocol": "grpc",
  "timeout_ms": 5000,
  "compression": "gzip"
}
```

**OTLP Endpoints**:
- Jaeger: `http://localhost:4317` (gRPC) or `http://localhost:4318` (HTTP)
- OpenTelemetry Collector: `http://localhost:4317`

#### `instrumentation` Section

Control which components are instrumented:

**RT Executive**:
- `trace_cycles`: Trace RT cycles (overhead: ~1-2%)
- `trace_actions`: Trace action execution
- `trace_timing`: Record timing metrics
- `sampling_rate`: Override default sampling rate

**EventBus**:
- `trace_publish`: Trace event publishing
- `trace_dispatch`: Trace event dispatching
- `sampling_rate`: Override default sampling rate

**Datastore**:
- `trace_reads`: Trace read operations (high overhead)
- `trace_writes`: Trace write operations (high overhead)
- `sampling_rate`: Override default sampling rate (use low rate)

#### `performance` Section

| Field | Type | Description | Default |
|-------|------|-------------|---------|
| `max_queue_size` | integer | Max spans in export queue | 2048 |
| `batch_timeout_ms` | integer | Batch export timeout (ms) | 1000 |
| `max_export_batch_size` | integer | Max spans per export batch | 512 |
| `max_export_timeout_ms` | integer | Max time for export (ms) | 30000 |
| `schedule_delay_ms` | integer | Delay between export attempts (ms) | 5000 |

### Common Scenarios

#### Scenario 1: Production (Jaeger)

```json
{
  "tracing": {
    "enabled": true,
    "sampling": {
      "default_rate": 0.1,
      "rt_cycle_rate": 0.05
    },
    "exporters": {
      "console": {"enabled": false},
      "otlp": {
        "enabled": true,
        "endpoint": "http://localhost:4317",
        "protocol": "grpc"
      }
    },
    "instrumentation": {
      "rt_executive": {
        "enabled": true,
        "sampling_rate": 0.05
      },
      "event_bus": {
        "enabled": true,
        "sampling_rate": 0.1
      },
      "datastore": {
        "enabled": false
      }
    }
  }
}
```

#### Scenario 2: Development (Console)

```json
{
  "tracing": {
    "enabled": true,
    "sampling": {
      "default_rate": 1.0,
      "rt_cycle_rate": 1.0
    },
    "exporters": {
      "console": {
        "enabled": true,
        "log_level": "debug"
      }
    },
    "instrumentation": {
      "rt_executive": {"enabled": true},
      "event_bus": {"enabled": true},
      "datastore": {"enabled": true}
    }
  }
}
```

---

## Validation and Testing

### Validate JSON Syntax

```bash
# Using jq
jq empty config/rt/cpu_affinity.json
jq empty config/rt/numa_binding.json
jq empty config/ha/failover_policy.json
jq empty config/tracing_config.json
```

### Test Configuration Loading

```bash
# Test with RT process
./rt_process \
  --config config/rt/cpu_affinity.json \
  --numa-config config/rt/numa_binding.json \
  --failover-config config/ha/failover_policy.json \
  --tracing-config config/tracing_config.json \
  --dry-run

# Check for errors in logs
journalctl -u mxrc-rt.service -n 50 | grep -i error
```

---

## Troubleshooting

### CPU Affinity Not Applied

**Check**:
```bash
# Verify isolcpus kernel parameter
cat /proc/cmdline | grep isolcpus

# Check CPU affinity
taskset -cp <PID>
```

**Fix**: Add `isolcpus=1,2` to `/etc/default/grub` and reboot.

### NUMA Binding Failed

**Check**:
```bash
# Verify NUMA is available
numactl --hardware

# Check NUMA policy
numactl --show
```

**Fix**: Set `allow_non_numa=true` for non-NUMA systems.

### Health Check Timeout

**Check**:
```bash
# Test health endpoint
curl -w "@-" http://localhost:8080/health << 'EOF'
    time_total: %{time_total}s
EOF
```

**Fix**: Increase `health_check_timeout_ms` in failover_policy.json.

### Traces Not Appearing in Jaeger

**Check**:
```bash
# Verify Jaeger is running
curl http://localhost:16686/api/services

# Check OTLP endpoint
telnet localhost 4317
```

**Fix**: Ensure `exporters.otlp.enabled=true` and Jaeger is running.

---

## References

- [CPU Affinity Documentation](/home/tory/workspace/mxrc/mxrc/config/rt/cpu_affinity.json)
- [NUMA Binding Documentation](/home/tory/workspace/mxrc/mxrc/config/rt/numa_binding.json)
- [Failover Policy Documentation](/home/tory/workspace/mxrc/mxrc/config/ha/failover_policy.json)
- [Tracing Configuration Documentation](/home/tory/workspace/mxrc/mxrc/config/tracing_config.json)
- [Quickstart Guide](docs/specs/001-production-readiness/quickstart.md)
- [API Reference](docs/api-reference.md)

---

**Version**: 1.0.0
**Created**: 2025-11-21
**Last Updated**: 2025-11-21
