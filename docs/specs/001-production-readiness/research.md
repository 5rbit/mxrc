# Research: Production Readiness

**Feature**: Production Readiness for MXRC Real-Time Robotics Control System
**Date**: 2025-11-21
**Status**: Research Complete
**Branch**: `001-production-readiness`

---

## 목차

1. [CPU Isolation & NUMA Optimization](#1-cpu-isolation--numa-optimization)
2. [High Availability & Failover](#2-high-availability--failover)
3. [Structured Logging Integration](#3-structured-logging-integration)
4. [Distributed Tracing](#4-distributed-tracing)

---

## 1. CPU Isolation & NUMA Optimization

### Decision 1.1: CPU Affinity 설정 방법

**Decision**: `pthread_setaffinity_np` 사용

**Rationale**:
- **Thread-level control**: MXRC는 RT/Non-RT 프로세스 내에서 여러 스레드를 사용하므로, 스레드별 affinity 설정이 필요
- **기존 코드 호환성**: 현재 `RTExecutive.cpp`에서 `util::pinToCPU(1)` 사용 중이며, 이는 `sched_setaffinity`를 사용하는 것으로 추정됨. 그러나 pthread 기반으로 전환하는 것이 더 명확한 POSIX API 사용
- **이식성**: POSIX 표준 API로 Linux 외 POSIX 호환 시스템에서도 동작
- **유연성**: Thread ID를 직접 지정하여 특정 스레드의 affinity만 변경 가능

**구현 예시**:
```cpp
#include <pthread.h>
#include <sched.h>

int setThreadAffinity(pthread_t thread, int cpu_core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_core, &cpuset);
    return pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
}
```

**Alternatives Considered**:
- `sched_setaffinity(pid_t pid, ...)`: 프로세스 전체 또는 TID 기반 설정
  - **단점**: TID는 Linux-specific이며, 프로세스 전체 설정 시 유연성 부족
  - **장점**: 시스템 콜이므로 약간 더 빠를 수 있음 (차이는 미미함)
- `taskset` 명령어: 외부 프로세스 실행 필요, 프로그래밍 제어 불가
  - **단점**: 런타임 동적 변경 불가, 설정 파일 의존성

**Performance Impact**: Negligible (설정은 초기화 시 1회만 수행)

---

### Decision 1.2: CPU Isolation 메커니즘

**Decision**: `isolcpus` 커널 부트 파라미터 + `cgroups` 조합

**Rationale**:
- **RT 보장**: `isolcpus`는 커널 스케줄러가 해당 CPU를 일반 프로세스 스케줄링에서 제외하여, RT 프로세스 전용으로 사용 가능
- **Hard isolation**: 다른 프로세스가 해당 CPU를 사용할 수 없어 jitter 최소화
- **PREEMPT_RT 호환성**: PREEMPT_RT 커널과 함께 사용 시 최적의 RT 성능 제공
- **cgroups 보완**: CPU isolation 이후 cgroups로 Non-RT 프로세스의 CPU 사용량 제한 (RT 간섭 방지 2차 방어선)

**설정 예시**:
```bash
# /etc/default/grub 수정
GRUB_CMDLINE_LINUX_DEFAULT="isolcpus=1,2,3 nohz_full=1,2,3 rcu_nocbs=1,2,3"

# RT 프로세스를 isolated CPU에 할당
chrt -f 90 taskset -c 1 ./rt_executive

# Non-RT 프로세스는 CPU 0, 4-7 사용
taskset -c 0,4-7 ./nonrt_executive
```

**Cgroups 추가 설정**:
```bash
# Non-RT 프로세스 CPU 제한
cgcreate -g cpu:/nonrt
cgset -r cpu.cfs_quota_us=300000 /nonrt  # 3 cores equivalent
cgset -r cpu.cfs_period_us=100000 /nonrt
cgexec -g cpu:/nonrt ./nonrt_executive
```

**Alternatives Considered**:
- `taskset` 단독 사용: Affinity 설정만으로는 다른 프로세스가 해당 CPU를 사용할 수 있음
  - **단점**: Hard isolation 불가, RT 보장 약함
- `cgroups` 단독 사용: CPU 사용량 제한이지 배타적 할당이 아님
  - **단점**: RT 성능 보장 부족
- `cpuset` cgroup controller: CPU 할당은 가능하나 커널 스케줄러 제외는 불가
  - **단점**: 커널 작업(IRQ, 타이머 등)이 여전히 해당 CPU 사용 가능

**Performance Impact**:
- **Jitter 감소**: 50-80% 예상 (기존 10μs → 2-5μs)
- **Deadline miss 감소**: < 0.01% 목표 달성 가능

---

### Decision 1.3: NUMA 노드 바인딩 API

**Decision**: `libnuma` C API 사용

**Rationale**:
- **프로그래밍 제어**: 런타임에 NUMA 정책을 동적으로 변경 가능
- **세밀한 제어**: 메모리 영역별로 다른 NUMA 정책 적용 가능
- **성능**: numactl 명령어는 프로세스 시작 시에만 적용되나, libnuma는 런타임에도 제어 가능
- **정보 수집**: `numa_available()`, `numa_max_node()` 등으로 시스템 NUMA 토폴로지 확인 가능

**구현 예시**:
```cpp
#include <numa.h>
#include <numaif.h>

class NUMABinding {
public:
    static bool initialize() {
        if (numa_available() < 0) {
            spdlog::warn("NUMA not available on this system");
            return false;
        }
        return true;
    }

    static bool bindToNode(int node) {
        if (node > numa_max_node()) {
            return false;
        }

        // Thread를 특정 NUMA 노드의 CPU에 바인딩
        struct bitmask* mask = numa_allocate_cpumask();
        numa_node_to_cpus(node, mask);
        numa_sched_setaffinity(0, mask);  // 0 = current thread
        numa_free_cpumask(mask);

        // 메모리 할당 정책을 해당 노드로 설정
        numa_set_preferred(node);

        return true;
    }

    static bool setLocalAllocation() {
        // 현재 실행 중인 CPU의 NUMA 노드에서만 메모리 할당
        numa_set_localalloc();
        return true;
    }
};
```

**CMakeLists.txt 변경**:
```cmake
find_package(NUMA REQUIRED)
target_link_libraries(rt PRIVATE numa)
```

**Alternatives Considered**:
- `numactl` 명령어: 외부 프로세스 래퍼
  - **단점**: 프로세스 시작 시에만 적용, 런타임 변경 불가
  - **장점**: 설정 간단 (예: `numactl --cpunodebind=0 --membind=0 ./rt`)
- `set_mempolicy()` 시스템 콜 직접 사용: Low-level API
  - **단점**: 복잡도 증가, NUMA 토폴로지 수동 관리 필요
  - **장점**: 외부 의존성 없음

**Performance Impact**:
- **NUMA local access rate**: > 95% 목표 달성
- **Memory latency 감소**: 30-50% (remote → local access)

---

### Decision 1.4: NUMA 메모리 정책

**Decision**: `NUMA_LOCAL` (local allocation) + `NUMA_PREFERRED` 조합

**Rationale**:
- **RT 성능**: `NUMA_LOCAL`은 현재 실행 중인 CPU의 NUMA 노드에서만 메모리 할당 → 최저 레이턴시
- **Fallback 허용**: `NUMA_PREFERRED`는 지정 노드를 선호하지만, 메모리 부족 시 다른 노드 사용 → OOM 방지
- **RT/Non-RT 분리**:
  - **RT 프로세스**: NUMA node 0에 바인딩, `NUMA_LOCAL` 정책 (엄격한 local 할당)
  - **Non-RT 프로세스**: NUMA node 1에 바인딩, `NUMA_PREFERRED` 정책 (유연한 할당)

**구현 전략**:
```cpp
// RT 프로세스 초기화
numa_set_localalloc();  // 엄격한 local 할당
bindToNode(0);

// Non-RT 프로세스 초기화
numa_set_preferred(1);  // 선호하지만 강제는 아님
bindToNode(1);
```

**Shared Memory 고려사항**:
- RT와 Non-RT 간 shared memory는 **RT 프로세스의 NUMA 노드(node 0)**에 할당
- 이유: RT 프로세스의 메모리 접근 레이턴시가 더 중요함

**Alternatives Considered**:
- `NUMA_INTERLEAVE`: 메모리를 모든 NUMA 노드에 분산 배치
  - **단점**: 평균 레이턴시는 개선되나 최악의 경우 레이턴시 증가 (RT 부적합)
  - **적용 사례**: 메모리 대역폭이 중요한 경우 (MXRC는 레이턴시가 더 중요)
- `NUMA_BIND`: 지정된 노드에서만 할당, 실패 시 OOM
  - **단점**: 메모리 부족 시 프로세스 종료 위험
  - **장점**: 가장 엄격한 NUMA 보장
- `NUMA_PREFERRED` 단독: 유연하지만 RT 보장 부족
  - **단점**: 메모리 압박 시 remote access 발생 가능

**Performance Impact**:
- **Local access rate**: 95-98% 예상
- **P99 latency 감소**: 40-60% (remote access 최소화)

---

### Decision 1.5: CPU Hotplug 시 Affinity 유지

**Decision**: Affinity 재설정 + CPU hotplug 비활성화 (production 환경)

**Rationale**:
- **Production 환경**: CPU hotplug 자체를 비활성화하는 것이 가장 안전
  - PREEMPT_RT 환경에서 CPU hotplug는 예상치 못한 스케줄링 변경 유발 가능
- **Development 환경**: CPU hotplug 감지 및 affinity 재설정 로직 구현
  - `/sys/devices/system/cpu/cpu*/online` 파일 모니터링

**Production 설정**:
```bash
# CPU hotplug 비활성화 (커널 부트 파라미터)
GRUB_CMDLINE_LINUX_DEFAULT="... maxcpus=8"

# 또는 런타임에 비활성화
echo 0 > /sys/devices/system/cpu/cpu1/online  # 비활성화
echo 1 > /sys/devices/system/cpu/cpu1/online  # 활성화 (필요시)
```

**Development/Fallback 로직**:
```cpp
class CPUAffinityManager {
private:
    std::thread monitor_thread_;
    std::atomic<bool> running_{true};

    void monitorCPUHotplug() {
        while (running_) {
            // inotify로 /sys/devices/system/cpu/cpu*/online 감시
            // CPU offline 감지 시 affinity 재설정
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

public:
    void startMonitoring() {
        monitor_thread_ = std::thread(&CPUAffinityManager::monitorCPUHotplug, this);
    }

    void reapplyAffinity() {
        // 설정된 affinity를 다시 적용
        setThreadAffinity(pthread_self(), target_cpu_);
    }
};
```

**Alternatives Considered**:
- **자동 재설정 없음**: CPU offline 시 프로세스 종료
  - **단점**: 가용성 저하
- **Fallback CPU 자동 선택**: Offline 시 다른 CPU로 자동 이동
  - **단점**: RT 성능 보장 어려움 (다른 CPU가 isolated가 아닐 수 있음)

**Decision**: Production에서는 CPU hotplug 비활성화가 최선

---

### Decision 1.6: NUMA 노드 간 마이그레이션 감지

**Decision**: `/proc/[pid]/numa_maps` 주기적 파싱 + Prometheus 메트릭

**Rationale**:
- **실시간 모니터링**: NUMA 정책 위반을 감지하여 성능 문제 조기 발견
- **메트릭 기반 알림**: Remote access 비율이 5% 초과 시 알람 발생
- **디버깅 정보**: NUMA 정책이 제대로 작동하는지 검증

**구현 예시**:
```cpp
class NUMAMonitor {
public:
    struct NUMAStats {
        uint64_t local_pages;
        uint64_t remote_pages;
        int current_node;
        double remote_access_ratio;
    };

    static NUMAStats getStats(pid_t pid = 0) {
        if (pid == 0) pid = getpid();

        std::ifstream file("/proc/" + std::to_string(pid) + "/numa_maps");
        NUMAStats stats{};

        std::string line;
        while (std::getline(file, line)) {
            // Parse "N0=1234 N1=567" format
            if (line.find("N0=") != std::string::npos) {
                // Extract page counts per node
            }
        }

        stats.remote_access_ratio =
            static_cast<double>(stats.remote_pages) /
            (stats.local_pages + stats.remote_pages);

        return stats;
    }

    static void publishMetrics(MetricsCollector& collector) {
        auto stats = getStats();

        collector.setGauge("numa_local_pages", stats.local_pages);
        collector.setGauge("numa_remote_pages", stats.remote_pages);
        collector.setGauge("numa_remote_access_ratio", stats.remote_access_ratio);
        collector.setGauge("numa_current_node", stats.current_node);
    }
};
```

**Prometheus 알림 규칙**:
```yaml
groups:
  - name: numa_alerts
    rules:
      - alert: HighNUMARemoteAccess
        expr: numa_remote_access_ratio > 0.05
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High NUMA remote access ratio detected"
          description: "{{ $labels.process }} has {{ $value | humanizePercentage }} remote access"
```

**Alternatives Considered**:
- `perf` 기반 모니터링: 더 정확하지만 성능 오버헤드 큼
  - **단점**: RT 성능 영향 (perf는 PMU 이벤트 사용)
- `/sys/devices/system/node/node*/numastat`: 시스템 전체 통계만 제공
  - **단점**: 프로세스별 분리 불가
- `numastat` 명령어: 외부 프로세스 실행 필요
  - **단점**: 프로그래밍 제어 어려움

**Performance Impact**: 매우 낮음 (1초마다 /proc 파일 읽기, Non-RT 스레드에서 수행)

---

## 2. High Availability & Failover

### Decision 2.1: Process Monitoring

**Decision**: `systemd` watchdog + custom health check 조합

**Rationale**:
- **systemd watchdog**: 프로세스가 완전히 멈춘 경우 자동 재시작
  - Linux 표준 도구, 별도 설치 불필요
  - 설정 간단 (`WatchdogSec=30s`)
- **Custom health check**: 프로세스가 실행 중이지만 기능적으로 문제가 있는 경우 감지
  - RT deadline miss rate > 0.1% → UNHEALTHY
  - EventBus queue full → UNHEALTHY
  - Shared memory corruption → UNHEALTHY

**systemd 서비스 설정**:
```ini
# /etc/systemd/system/mxrc-rt.service
[Unit]
Description=MXRC Real-Time Executive
After=network.target

[Service]
Type=notify
ExecStart=/usr/local/bin/mxrc-rt
Restart=always
RestartSec=5
WatchdogSec=30
TimeoutStartSec=60

# RT priority (CAP_SYS_NICE 필요)
LimitRTPRIO=95

# Memory lock
LimitMEMLOCK=infinity

# CPU affinity (systemd에서도 설정 가능)
CPUAffinity=1

[Install]
WantedBy=multi-user.target
```

**Custom Health Check 구현**:
```cpp
class HealthCheck : public IHealthCheck {
public:
    enum class HealthStatus {
        HEALTHY,
        DEGRADED,
        UNHEALTHY
    };

    HealthStatus check() override {
        // 1. Deadline miss rate 확인
        double miss_rate = metrics_collector_->getDeadlineMissRate();
        if (miss_rate > 0.1) {
            return HealthStatus::UNHEALTHY;
        }

        // 2. EventBus 상태 확인
        if (event_bus_->getQueueSize() > 9000) {  // 90% full
            return HealthStatus::DEGRADED;
        }

        // 3. Shared memory 무결성 확인
        if (!shared_memory_->isValid()) {
            return HealthStatus::UNHEALTHY;
        }

        return HealthStatus::HEALTHY;
    }

    void notifySystemd() {
        // systemd watchdog에 주기적으로 신호 전송
        sd_notify(0, "WATCHDOG=1");
    }
};
```

**Alternatives Considered**:
- **monit**: 외부 프로세스 모니터링 도구
  - **단점**: 추가 의존성, systemd와 중복 기능
- **supervisord**: Python 기반 프로세스 관리
  - **단점**: systemd보다 무겁고 Linux 표준 아님
- **Custom daemon**: 자체 모니터링 데몬 구현
  - **단점**: 개발 비용 높음, systemd가 이미 제공하는 기능

**Performance Impact**: Negligible (systemd watchdog는 외부 프로세스, health check는 1초마다 실행)

---

### Decision 2.2: State Checkpoint 저장 방식

**Decision**: Selective serialization (JSON 기반)

**Rationale**:
- **선택적 저장**: RT state machine state, critical DataStore keys만 저장
  - 전체 메모리 덤프는 크기가 크고 복원 시간 오래 걸림
- **사람이 읽을 수 있음**: 디버깅 용이, 수동 편집 가능
- **기존 인프라 활용**: 이미 nlohmann_json 사용 중
- **Fast 복원**: JSON 파싱은 충분히 빠름 (< 100ms for ~1MB)

**구현 예시**:
```cpp
class StateCheckpoint {
public:
    struct Checkpoint {
        uint64_t timestamp_ns;
        std::string rt_state;
        uint32_t current_slot;
        uint64_t cycle_count;
        std::map<std::string, std::string> critical_keys;  // DataStore snapshot
    };

    static void save(const Checkpoint& cp, const std::string& path) {
        nlohmann::json j;
        j["timestamp_ns"] = cp.timestamp_ns;
        j["rt_state"] = cp.rt_state;
        j["current_slot"] = cp.current_slot;
        j["cycle_count"] = cp.cycle_count;
        j["datastore"] = cp.critical_keys;

        std::ofstream file(path);
        file << j.dump(2);  // Pretty print
        fsync(fileno(file));  // Ensure written to disk
    }

    static Checkpoint load(const std::string& path) {
        std::ifstream file(path);
        nlohmann::json j;
        file >> j;

        Checkpoint cp;
        cp.timestamp_ns = j["timestamp_ns"];
        cp.rt_state = j["rt_state"];
        cp.current_slot = j["current_slot"];
        cp.cycle_count = j["cycle_count"];
        cp.critical_keys = j["datastore"].get<std::map<std::string, std::string>>();

        return cp;
    }
};
```

**Checkpoint 주기**:
- **Periodic**: 1분마다 자동 저장
- **Event-driven**: State 변경 시 저장 (INIT → READY, RUNNING → SAFE_MODE)

**Alternatives Considered**:
- **Memory dump (core dump)**: 전체 프로세스 메모리 덤프
  - **단점**: 파일 크기 큼 (수백 MB), 복원 복잡, 보안 문제 (메모리에 민감 정보)
  - **장점**: 완전한 상태 복원 가능
- **Binary serialization**: Protocol Buffers, FlatBuffers 등
  - **단점**: 추가 의존성, 디버깅 어려움
  - **장점**: 더 빠르고 작은 파일
- **Database (SQLite)**: 트랜잭션 기반 저장
  - **단점**: 오버헤드 큼, RT 성능 영향 가능
  - **장점**: ACID 보장

**Decision Reason**: JSON이 MXRC 규모(작은 state)에 충분하며, 디버깅 편의성이 중요

---

### Decision 2.3: Leader Election 알고리즘

**Decision**: Simplified Bully Algorithm (작은 규모) + etcd (대규모 확장 시)

**Rationale**:
- **현재 요구사항**: 3-5 노드 소규모 클러스터
  - Bully algorithm이 충분히 간단하고 효과적
- **Bully 장점**:
  - 구현 간단 (< 200 lines)
  - 외부 의존성 없음 (embedded system 적합)
  - 빠른 선출 (< 1초)
- **etcd 준비**: 향후 10+ 노드로 확장 시 etcd로 전환
  - Raft 기반으로 검증된 분산 합의

**Bully Algorithm 구현**:
```cpp
class LeaderElection {
private:
    int node_id_;
    int current_leader_ = -1;
    std::vector<std::string> peer_addresses_;

    enum class MessageType {
        ELECTION,
        OK,
        COORDINATOR
    };

public:
    void startElection() {
        spdlog::info("Node {} starting election", node_id_);

        // 자신보다 높은 ID의 노드에게 ELECTION 메시지 전송
        bool received_ok = false;
        for (int i = node_id_ + 1; i < peer_addresses_.size(); ++i) {
            if (sendMessage(peer_addresses_[i], MessageType::ELECTION)) {
                received_ok = true;
            }
        }

        if (!received_ok) {
            // 자신이 리더
            becomeLeader();
        }
        // else: 더 높은 ID 노드가 응답했으므로 대기
    }

    void becomeLeader() {
        current_leader_ = node_id_;
        spdlog::info("Node {} became leader", node_id_);

        // 모든 피어에게 COORDINATOR 메시지 전송
        for (const auto& addr : peer_addresses_) {
            sendMessage(addr, MessageType::COORDINATOR);
        }
    }

    void onPeerDown(int peer_id) {
        if (peer_id == current_leader_) {
            // 리더가 다운되면 선거 시작
            startElection();
        }
    }
};
```

**etcd 전환 시나리오** (10+ 노드):
```cpp
// etcd-cpp-apiv3 사용
#include <etcd/Client.hpp>

class EtcdLeaderElection {
    etcd::Client client_;
    std::string key_ = "/mxrc/leader";

public:
    void campaign() {
        // Lease 생성 (TTL 30초)
        auto lease = client_.leasegrant(30).get();

        // Key에 자신의 ID 쓰기 (CAS 사용)
        client_.put(key_, std::to_string(node_id_), lease.value().ID());

        // Lease 갱신 (heartbeat)
        client_.leasekeepalive(lease.value().ID());
    }
};
```

**Alternatives Considered**:
- **Raft** (자체 구현): 가장 강력하지만 복잡
  - **단점**: 구현 비용 높음 (1000+ lines), 테스트 어려움
  - **장점**: 완전한 분산 합의, 검증된 알고리즘
- **Paxos**: Raft보다 복잡, 구현 어려움
  - **단점**: 이해하기 어렵고 구현 복잡
- **etcd 바로 사용**: 외부 의존성 증가
  - **단점**: Embedded system에 부적합, etcd 서버 별도 실행 필요
  - **장점**: Production-proven, 관리 기능 풍부

**Decision Reason**: 소규모 클러스터에는 Bully가 충분하며, 확장 시 etcd로 전환 가능

---

### Decision 2.4: Split-Brain 방지

**Decision**: Quorum-based decision + Fencing (shared memory lock)

**Rationale**:
- **Quorum (과반수)**: N=3인 경우 최소 2개 노드 동의 필요
  - Network partition 발생 시 과반수 그룹만 작동
  - Split-brain 원천 차단
- **Fencing**: Shared memory에 leader lock 설정
  - RT 프로세스가 shared memory 쓰기 전 lock 확인
  - 이전 리더는 lock 획득 실패 시 즉시 중단

**Quorum 구현**:
```cpp
class QuorumManager {
private:
    int num_nodes_;
    int quorum_size_;  // (num_nodes_ / 2) + 1

public:
    QuorumManager(int num_nodes)
        : num_nodes_(num_nodes)
        , quorum_size_((num_nodes / 2) + 1) {}

    bool hasQuorum(const std::vector<bool>& peer_alive) const {
        int alive_count = 1;  // 자신 포함
        for (bool alive : peer_alive) {
            if (alive) alive_count++;
        }
        return alive_count >= quorum_size_;
    }

    bool canOperate() const {
        if (!hasQuorum(getPeerStatus())) {
            spdlog::error("Lost quorum, entering safe mode");
            return false;
        }
        return true;
    }
};
```

**Fencing 구현** (Shared Memory):
```cpp
struct SharedMemoryHeader {
    std::atomic<uint64_t> leader_lease_expiry_ns;
    std::atomic<int> leader_node_id;
};

class SharedMemoryFencing {
public:
    bool acquireLease(int node_id, uint64_t duration_ns) {
        uint64_t now = get_time_ns();
        uint64_t expiry = now + duration_ns;

        // CAS: 리스가 만료되었거나 자신이 소유한 경우만 갱신
        uint64_t current_expiry = header_->leader_lease_expiry_ns.load();
        if (current_expiry > now && header_->leader_node_id != node_id) {
            return false;  // 다른 노드가 리스 보유 중
        }

        header_->leader_node_id = node_id;
        header_->leader_lease_expiry_ns.store(expiry);
        return true;
    }

    bool renewLease(int node_id) {
        return acquireLease(node_id, 30'000'000'000ULL);  // 30초
    }

    bool isLeaseValid() const {
        return header_->leader_lease_expiry_ns.load() > get_time_ns();
    }
};
```

**Alternatives Considered**:
- **Network partition detection** 단독: 감지는 가능하나 자동 복구 어려움
  - **단점**: 감지 후 조치가 명확하지 않음
- **Fencing** 단독: Network 문제가 아닌 경우 비효율적
  - **단점**: Lease 갱신 오버헤드
- **STONITH (Shoot The Other Node In The Head)**: 물리적 전원 차단
  - **단점**: 하드웨어 지원 필요, 과도한 조치

**Decision Reason**: Quorum + Fencing 조합이 가장 안전하고 MXRC 규모에 적합

---

### Decision 2.5: Distributed Consensus Library

**Decision**: 초기에는 자체 구현 (Bully), 확장 시 `etcd-cpp-apiv3` 사용

**Rationale**:
- **초기 (3-5 노드)**:
  - 자체 Bully algorithm으로 충분
  - 외부 의존성 최소화 (embedded system 원칙)
  - 코드 제어 가능
- **확장 시 (10+ 노드)**:
  - `etcd-cpp-apiv3`: Official etcd C++ client
  - Raft 기반 검증된 분산 합의
  - Production-ready

**etcd-cpp-apiv3 장점**:
- gRPC 기반 (높은 성능)
- Watch API로 실시간 변경 감지
- Lease 기반 TTL 자동 관리
- Transaction 지원

**etcd-cpp-apiv3 CMake 설정**:
```cmake
find_package(etcd-cpp-api REQUIRED)
target_link_libraries(rt PRIVATE etcd-cpp-api)
```

**Alternatives Considered**:
- **consul-cpp**: HashiCorp Consul 클라이언트
  - **단점**: HTTP API만 제공 (gRPC보다 느림), C++ 지원 약함
  - **장점**: Service discovery 통합
- **Zookeeper C++ client**: Apache ZooKeeper
  - **단점**: 오래된 기술, C++ 지원 약함
  - **장점**: 검증된 시스템
- **자체 Raft 구현**: 완전한 제어
  - **단점**: 구현 비용 매우 높음, 버그 위험
  - **장점**: 외부 의존성 없음

**Decision Reason**: 단계적 접근 (자체 구현 → etcd)이 리스크와 비용 측면에서 최적

---

### Decision 2.6: Health Check 프로토콜

**Decision**: HTTP REST API (기존 Prometheus 서버 확장)

**Rationale**:
- **기존 인프라 활용**: MetricsServer가 이미 HTTP 서버 운영 중
  - `/metrics` (Prometheus)
  - `/health` (새로 추가)
  - `/ready` (새로 추가)
- **표준 프로토콜**: Kubernetes, Docker 등 표준 health check 호환
- **간단한 구현**: HTTP GET 요청만으로 상태 확인
- **성능**: Health check는 1초에 1번 정도로 충분 (gRPC 오버킬)

**API 스펙**:
```yaml
# GET /health
# Response: 200 OK / 503 Service Unavailable
{
  "status": "healthy",  # healthy | degraded | unhealthy
  "timestamp": 1700000000,
  "checks": {
    "rt_deadline": "ok",
    "event_bus": "ok",
    "shared_memory": "ok"
  }
}

# GET /ready
# Response: 200 OK / 503 Service Unavailable
{
  "ready": true,
  "reason": ""
}
```

**구현** (기존 MetricsServer 확장):
```cpp
class HealthCheckHandler {
public:
    std::string handleHealth() {
        auto status = health_check_->check();

        nlohmann::json response;
        response["status"] = statusToString(status);
        response["timestamp"] = time(nullptr);
        response["checks"]["rt_deadline"] = "ok";
        response["checks"]["event_bus"] = "ok";

        return response.dump();
    }

    int getStatusCode(HealthStatus status) {
        return (status == HealthStatus::HEALTHY) ? 200 : 503;
    }
};
```

**Alternatives Considered**:
- **gRPC**: 고성능 RPC 프레임워크
  - **단점**: 오버킬 (health check는 간단), 추가 의존성
  - **장점**: 타입 안전성, 양방향 스트리밍
- **Custom IPC (Unix socket)**: 프로세스 간 통신
  - **단점**: 외부 모니터링 도구 연동 어려움
  - **장점**: 약간 더 빠름 (차이 미미)
- **Shared memory flag**: 가장 빠름
  - **단점**: 프로세스 crash 시 감지 불가, 외부 접근 불가
  - **장점**: RT 성능 최고

**Decision Reason**: HTTP가 표준이고 기존 인프라와 통합 용이

---

## 3. Structured Logging Integration

### Decision 3.1: spdlog JSON Formatter 구현

**Decision**: Custom formatter 구현 (spdlog API 활용)

**Rationale**:
- **Zero 외부 의존성**: spdlog 자체 기능만 사용
- **성능**: Pre-formatted JSON 문자열 생성 (파싱 불필요)
- **유연성**: MXRC 특화 필드 추가 가능
- **기존 코드 호환**: 기존 spdlog 사용 코드 변경 최소화

**구현 예시**:
```cpp
#include <spdlog/spdlog.h>
#include <spdlog/pattern_formatter.h>

class JsonFormatter : public spdlog::formatter {
public:
    void format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) override {
        // JSON 필드 구성
        fmt::format_to(std::back_inserter(dest),
            R"({{"timestamp":"{}","level":"{}","process":"{}","thread":"{}","message":"{}","context":{{}}}})",
            formatTimestamp(msg.time),
            spdlog::level::to_string_view(msg.level),
            process_name_,
            msg.thread_id,
            escapeJson(std::string(msg.payload.begin(), msg.payload.end()))
        );
        dest.push_back('\n');
    }

    std::unique_ptr<formatter> clone() const override {
        return std::make_unique<JsonFormatter>(*this);
    }

private:
    std::string process_name_;

    std::string formatTimestamp(const spdlog::log_clock::time_point& tp) {
        // ISO 8601 format: 2025-01-21T12:34:56.789Z
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            tp.time_since_epoch());
        return fmt::format("{:%Y-%m-%dT%H:%M:%S}.{:03d}Z", tp, ms.count() % 1000);
    }

    std::string escapeJson(const std::string& str) {
        std::string result;
        for (char c : str) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c;
            }
        }
        return result;
    }
};

// 사용법
auto logger = spdlog::default_logger();
logger->set_formatter(std::make_unique<JsonFormatter>());
```

**Alternatives Considered**:
- **spdlog-json** (3rd-party): GitHub의 비공식 JSON formatter
  - **단점**: 유지보수 불확실, 추가 의존성
  - **장점**: 즉시 사용 가능
- **fmt 라이브러리 직접 사용**: spdlog 우회
  - **단점**: 기존 코드 대규모 변경 필요
  - **장점**: 더 많은 제어
- **nlohmann_json으로 JSON 생성**: 객체 직렬화
  - **단점**: 성능 오버헤드 (객체 생성 + 직렬화)
  - **장점**: 타입 안전성

**Performance Impact**:
- JSON formatting overhead: ~100ns per log
- 기존 spdlog보다 ~20% 느림 (여전히 매우 빠름)

---

### Decision 3.2: Log Field 표준

**Decision**: ECS (Elastic Common Schema) 기반 + MXRC 커스텀 필드

**Rationale**:
- **ECS**: Elasticsearch 표준 스키마로 Kibana와 완벽 호환
- **표준화**: 여러 시스템 로그를 통합 검색 가능
- **확장성**: 커스텀 필드 추가 가능

**JSON Schema**:
```json
{
  "@timestamp": "2025-01-21T12:34:56.789Z",
  "log.level": "info",
  "process.name": "mxrc-rt",
  "process.pid": 1234,
  "process.thread.id": 5678,
  "message": "RT cycle completed",
  "event.kind": "metric",
  "event.category": ["process"],
  "mxrc": {
    "rt.state": "RUNNING",
    "rt.slot": 5,
    "rt.cycle": 1234567,
    "datastore.key": "joint_angle",
    "numa.node": 0
  },
  "tags": ["rt", "performance"]
}
```

**ECS 핵심 필드**:
- `@timestamp`: ISO 8601 타임스탬프
- `log.level`: debug, info, warning, error, critical
- `process.name`, `process.pid`: 프로세스 정보
- `message`: 로그 메시지
- `event.kind`: event, metric, state 등

**MXRC 커스텀 필드** (`mxrc` 네임스페이스):
- `mxrc.rt.state`: RT 상태
- `mxrc.rt.slot`: 현재 슬롯
- `mxrc.rt.cycle`: 사이클 카운트
- `mxrc.datastore.key`: DataStore 키
- `mxrc.numa.node`: NUMA 노드

**Alternatives Considered**:
- **완전 커스텀 스키마**: 자유롭지만 표준 도구 연동 어려움
  - **단점**: Elasticsearch, Kibana 기본 대시보드 사용 불가
- **JSON Lines (JSONL)**: 단순한 JSON 배열
  - **단점**: 스키마 없음, 검색 최적화 부족
- **Syslog format**: 전통적인 로그 포맷
  - **단점**: 구조화 부족, 파싱 필요

**Decision Reason**: ECS가 업계 표준이며 Elasticsearch 생태계와 완벽 호환

---

### Decision 3.3: Log Delivery

**Decision**: File-based shipper (Filebeat) 사용

**Rationale**:
- **RT 성능 보호**: 로그를 파일에 쓰고, Filebeat가 비동기로 Elasticsearch 전송
  - Direct network 전송은 네트워크 지연 시 RT 영향 가능
- **신뢰성**: Filebeat가 재시도, 버퍼링 자동 처리
- **간단한 구성**: spdlog async file sink만 설정하면 됨
- **Buffering**: 파일 시스템이 자연스럽게 버퍼 역할

**설정**:
```cpp
// spdlog async rotating file sink
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>

void setupStructuredLogging() {
    // Async logger with rotating file
    auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "/var/log/mxrc/rt.json",
        1024 * 1024 * 10,  // 10MB per file
        3                  // Keep 3 rotated files
    );

    sink->set_formatter(std::make_unique<JsonFormatter>());

    auto logger = std::make_shared<spdlog::async_logger>(
        "mxrc-rt",
        sink,
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::overrun_oldest  // RT: drop old logs if full
    );

    spdlog::set_default_logger(logger);
}
```

**Filebeat 설정**:
```yaml
# /etc/filebeat/filebeat.yml
filebeat.inputs:
  - type: log
    enabled: true
    paths:
      - /var/log/mxrc/*.json
    json.keys_under_root: true
    json.add_error_key: true

output.elasticsearch:
  hosts: ["localhost:9200"]
  index: "mxrc-logs-%{+yyyy.MM.dd}"

setup.ilm.enabled: false
```

**Alternatives Considered**:
- **Direct TCP to Logstash**: spdlog에서 직접 네트워크 전송
  - **단점**: 네트워크 지연 시 RT 영향, 연결 관리 복잡
  - **장점**: 약간 더 낮은 레이턴시
- **Direct Elasticsearch HTTP API**: REST API 직접 호출
  - **단점**: RT 성능 영향 큼, 재시도 로직 직접 구현 필요
  - **장점**: 중간 레이어 없음
- **Kafka**: 분산 메시징
  - **단점**: 오버킬, 인프라 복잡도 증가
  - **장점**: 높은 처리량

**Decision Reason**: Filebeat가 가장 간단하고 RT 성능 보호에 유리

---

### Decision 3.4: Buffer 전략

**Decision**: Ring buffer (spdlog async) + overrun_oldest 정책

**Rationale**:
- **RT 우선**: 버퍼 가득 차도 RT 작업은 멈추지 않음
- **spdlog 내장**: 별도 구현 불필요
- **Lock-free**: spdlog thread pool이 lock-free queue 사용
- **메모리 제한**: Ring buffer 크기 고정 (예: 8192 entries)

**설정**:
```cpp
// spdlog thread pool 설정
spdlog::init_thread_pool(
    8192,  // Queue size (ring buffer)
    1      // Background thread count
);

// Overflow policy: overrun_oldest
auto logger = std::make_shared<spdlog::async_logger>(
    "mxrc-rt",
    sink,
    spdlog::thread_pool(),
    spdlog::async_overflow_policy::overrun_oldest  // Drop old logs
);
```

**Buffer 크기 결정**:
- RT cycle: 10ms
- 평균 로그: 10개/cycle
- Buffer: 8192 entries → ~80초 분량
- Flush 주기: 1초 → 충분한 여유

**Alternatives Considered**:
- **Block policy** (`block`): 버퍼 가득 차면 대기
  - **단점**: RT 블록킹 발생 → UNACCEPTABLE
- **Overrun policy** 없음: 버퍼 무한 증가
  - **단점**: 메모리 고갈 위험
- **Custom lock-free queue**: 직접 구현
  - **단점**: 구현 복잡, spdlog가 이미 제공

**Decision Reason**: spdlog의 `overrun_oldest`가 RT 요구사항에 완벽히 부합

---

### Decision 3.5: Async Logging RT 영향 측정

**Decision**: Performance test로 오버헤드 < 1% 검증

**Rationale**:
- **측정 기반 결정**: 추측 대신 실제 측정
- **목표**: RT cycle time overhead < 1% (< 100μs for 10ms cycle)
- **테스트 시나리오**:
  - Baseline: 로그 없음
  - Test 1: 10 logs/cycle (async)
  - Test 2: 100 logs/cycle (async)
  - Test 3: 10 logs/cycle (sync, 비교용)

**측정 코드**:
```cpp
TEST(AsyncLoggingPerformance, RTImpact) {
    setupStructuredLogging();

    constexpr int NUM_CYCLES = 10000;
    constexpr int LOGS_PER_CYCLE = 10;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < NUM_CYCLES; ++i) {
        for (int j = 0; j < LOGS_PER_CYCLE; ++j) {
            spdlog::info("RT cycle {} log {}", i, j);
        }

        // Simulate RT work
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(end - start).count();

    // Expected: ~1 second (100μs * 10000)
    // With logging: < 1.01 seconds (< 1% overhead)
    EXPECT_LT(duration, 1.01);
}
```

**예상 결과**:
- Async logging: < 1% overhead (spdlog이 백그라운드 스레드에서 처리)
- Sync logging: 10-20% overhead (파일 I/O 대기)

**Alternatives Considered**:
- **추측 기반 결정**: 위험함
- **프로덕션에서 측정**: 너무 늦음
- **Benchmark 도구 (Google Benchmark)**: 더 정밀하지만 오버킬

**Decision Reason**: GoogleTest로 충분히 측정 가능

---

### Decision 3.6: Log Correlation (Request ID Propagation)

**Decision**: Thread-local storage + Context 객체

**Rationale**:
- **자동 전파**: Request ID를 명시적으로 전달하지 않아도 됨
- **Thread 안전성**: Thread-local이므로 동기화 불필요
- **RT 호환**: Thread-local 접근은 매우 빠름 (no lock)

**구현**:
```cpp
class LogContext {
private:
    static thread_local std::string request_id_;
    static thread_local std::map<std::string, std::string> context_;

public:
    static void setRequestId(const std::string& id) {
        request_id_ = id;
    }

    static std::string getRequestId() {
        return request_id_;
    }

    static void set(const std::string& key, const std::string& value) {
        context_[key] = value;
    }

    static std::string get(const std::string& key) {
        auto it = context_.find(key);
        return (it != context_.end()) ? it->second : "";
    }

    static void clear() {
        request_id_.clear();
        context_.clear();
    }
};

// Custom formatter에서 context 사용
class JsonFormatterWithContext : public JsonFormatter {
    void format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) override {
        // ... 기본 필드 ...
        fmt::format_to(std::back_inserter(dest),
            R"(,"request_id":"{}","context":{{}})",
            LogContext::getRequestId()
        );
    }
};
```

**사용 예시**:
```cpp
void handleRequest(const Request& req) {
    LogContext::setRequestId(req.id);
    LogContext::set("user_id", req.user_id);

    spdlog::info("Processing request");
    // 로그에 자동으로 request_id, user_id 포함

    processTask();

    LogContext::clear();  // RAII guard로 자동화 가능
}
```

**RAII Guard**:
```cpp
class LogContextGuard {
public:
    LogContextGuard(const std::string& request_id) {
        LogContext::setRequestId(request_id);
    }

    ~LogContextGuard() {
        LogContext::clear();
    }
};

// 사용
void handleRequest(const Request& req) {
    LogContextGuard guard(req.id);
    // ...
}  // 자동 clear
```

**Alternatives Considered**:
- **명시적 parameter 전달**: 모든 함수에 context 파라미터
  - **단점**: 코드 침투적, 리팩토링 대규모
- **Global 변수**: Thread 안전하지 않음
  - **단점**: 동기화 필요, RT 성능 영향
- **Logging macro에 포함**: 매 로그마다 context 전달
  - **단점**: 사용법 복잡

**Decision Reason**: Thread-local이 가장 간단하고 RT 성능에 영향 없음

---

## 4. Distributed Tracing

### Decision 4.1: OpenTelemetry C++ SDK

**Decision**: OpenTelemetry C++ SDK 사용

**Rationale**:
- **업계 표준**: CNCF 프로젝트, Jaeger, Zipkin, Datadog 등 모두 지원
- **Vendor-neutral**: 여러 백엔드 선택 가능
- **Rich API**: Trace, Metrics, Logs 통합 (향후 확장)
- **Active development**: 지속적인 업데이트

**설치**:
```cmake
# CMakeLists.txt
find_package(opentelemetry-cpp CONFIG REQUIRED)

target_link_libraries(rt PRIVATE
    opentelemetry-cpp::api
    opentelemetry-cpp::sdk
    opentelemetry-cpp::ext
    opentelemetry-cpp::exporters_otlp
)
```

**초기화 코드**:
```cpp
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter.h>

void initTracing() {
    namespace trace = opentelemetry::trace;
    namespace otlp = opentelemetry::exporter::otlp;

    // OTLP Exporter 설정
    otlp::OtlpHttpExporterOptions exporter_options;
    exporter_options.url = "http://localhost:4318/v1/traces";

    auto exporter = std::make_unique<otlp::OtlpHttpExporter>(exporter_options);
    auto processor = std::make_unique<trace::SimpleSpanProcessor>(std::move(exporter));

    // TracerProvider 설정
    auto provider = std::make_shared<trace::TracerProvider>(std::move(processor));
    trace::Provider::SetTracerProvider(provider);
}
```

**성능 고려사항**:
- **Span 생성**: ~100-500ns (in-memory)
- **Export**: 비동기 백그라운드 스레드
- **Sampling**: 1-10%로 설정하여 오버헤드 최소화

**Alternatives Considered**:
- **Jaeger C++ client**: Jaeger 전용
  - **단점**: Vendor lock-in, 다른 백엔드 사용 불가
  - **장점**: Jaeger에 최적화
- **Zipkin C++ (zipkin-cpp-opentracing)**: 오래된 프로젝트
  - **단점**: 유지보수 중단, OpenTracing (구버전 표준)
- **자체 구현**: 완전한 제어
  - **단점**: 구현 비용 매우 높음, 표준 호환성 없음

**Decision Reason**: OpenTelemetry가 미래 표준이며 가장 많은 지원 받음

---

### Decision 4.2: Trace Context Propagation

**Decision**: W3C Trace Context 표준 사용

**Rationale**:
- **표준**: W3C 권고안, 모든 주요 트레이싱 시스템 지원
- **OpenTelemetry 기본**: SDK가 자동 지원
- **Inter-process**: HTTP header로 RT → Non-RT 전파 가능

**W3C Trace Context Format**:
```
traceparent: 00-<trace-id>-<parent-id>-<trace-flags>
tracestate: vendor1=value1,vendor2=value2
```

**EventBus를 통한 전파**:
```cpp
struct RTEvent {
    std::string event_type;
    std::string trace_id;
    std::string span_id;
    // ... other fields
};

// RT 프로세스에서 이벤트 발행
void publishEvent() {
    auto span = tracer_->StartSpan("publish_event");
    auto ctx = span->GetContext();

    RTEvent event;
    event.trace_id = ctx.trace_id().ToHex();
    event.span_id = ctx.span_id().ToHex();

    event_bus_->publish(event);
}

// Non-RT 프로세스에서 이벤트 수신
void onEvent(const RTEvent& event) {
    // Context 복원
    auto parent_ctx = createContextFromIds(event.trace_id, event.span_id);
    auto span = tracer_->StartSpan("handle_event", {.parent = parent_ctx});

    // 작업 수행
    span->End();
}
```

**Alternatives Considered**:
- **Custom header**: 자체 정의 포맷
  - **단점**: 비표준, 다른 시스템과 호환 불가
- **Jaeger native format**: Uber-trace-id
  - **단점**: Jaeger 전용, W3C 표준 아님
- **Zipkin B3 propagation**: `X-B3-TraceId` 등
  - **단점**: 구버전 표준

**Decision Reason**: W3C가 최신 표준이며 OpenTelemetry 기본값

---

### Decision 4.3: Span 생성 Overhead - RAII Span Guard

**Decision**: RAII Span Guard 구현

**Rationale**:
- **자동 관리**: Span 시작/종료 자동화, 예외 안전성
- **코드 간결**: Boilerplate 코드 최소화
- **RT 안전**: 스택 할당, no malloc

**구현**:
```cpp
class SpanGuard {
private:
    std::unique_ptr<opentelemetry::trace::Span> span_;

public:
    explicit SpanGuard(const std::string& operation_name) {
        auto tracer = opentelemetry::trace::Provider::GetTracerProvider()
            ->GetTracer("mxrc-rt");
        span_ = tracer->StartSpan(operation_name);
    }

    ~SpanGuard() {
        if (span_) {
            span_->End();
        }
    }

    void setTag(const std::string& key, const std::string& value) {
        if (span_) {
            span_->SetAttribute(key, value);
        }
    }

    void setError(const std::string& message) {
        if (span_) {
            span_->SetStatus(opentelemetry::trace::StatusCode::kError, message);
        }
    }

    // Non-copyable
    SpanGuard(const SpanGuard&) = delete;
    SpanGuard& operator=(const SpanGuard&) = delete;
};

// 사용 예시
void executeRTCycle() {
    SpanGuard span("rt_cycle");
    span.setTag("slot", std::to_string(current_slot_));

    // RT work

    if (deadline_missed) {
        span.setError("Deadline missed");
    }
}  // Span 자동 종료
```

**Pre-allocated Pool** (추가 최적화):
```cpp
class SpanPool {
private:
    std::array<opentelemetry::trace::Span, 1024> pool_;
    std::atomic<int> index_{0};

public:
    opentelemetry::trace::Span* acquire() {
        int idx = index_.fetch_add(1, std::memory_order_relaxed) % 1024;
        return &pool_[idx];
    }
};
```

**Alternatives Considered**:
- **Manual start/end**: 명시적 호출
  - **단점**: 예외 시 leak, 코드 복잡
- **Macro 기반**: `TRACE_SCOPE("name")`
  - **단점**: 디버깅 어려움, 타입 안전성 부족
- **Lambda callback**: `trace("name", []{...})`
  - **단점**: 코드 구조 변경 필요

**Decision Reason**: RAII가 C++ 최고의 관행이며 안전함

---

### Decision 4.4: Sampling Strategy

**Decision**: Head-based sampling (설정 가능한 비율)

**Rationale**:
- **간단함**: Trace 시작 시 샘플링 결정
- **예측 가능**: 일정한 오버헤드
- **설정 유연성**: 1% (프로덕션) ~ 100% (디버깅)

**구현**:
```cpp
#include <opentelemetry/sdk/trace/sampler.h>

void initTracing(double sample_rate = 0.01) {  // 1% 기본값
    // TraceIdRatioBased sampler
    auto sampler = std::make_shared<opentelemetry::sdk::trace::TraceIdRatioBasedSampler>(
        sample_rate
    );

    auto processor = std::make_unique<trace::BatchSpanProcessor>(
        std::move(exporter)
    );

    auto provider = std::make_shared<trace::TracerProvider>(
        std::move(processor),
        sampler
    );

    trace::Provider::SetTracerProvider(provider);
}
```

**동적 조정** (런타임):
```cpp
class DynamicSampler {
private:
    std::atomic<double> sample_rate_{0.01};

public:
    void setSampleRate(double rate) {
        sample_rate_.store(rate, std::memory_order_relaxed);
    }

    bool shouldSample() {
        double random = (double)rand() / RAND_MAX;
        return random < sample_rate_.load(std::memory_order_relaxed);
    }
};
```

**샘플링 비율 가이드**:
- **프로덕션**: 1-10% (오버헤드 최소화)
- **테스트**: 100% (모든 trace 수집)
- **디버깅**: 100% (특정 시간대)

**Alternatives Considered**:
- **Tail-based sampling**: 전체 trace 수집 후 샘플링
  - **단점**: 모든 span을 메모리에 보관해야 함 (메모리 압박)
  - **장점**: 에러 trace만 선택적으로 보관 가능
- **Adaptive sampling**: CPU 부하 기반 동적 조정
  - **단점**: 복잡도 증가, 예측 불가
  - **장점**: 자동 최적화
- **100% 샘플링**: 모든 trace 수집
  - **단점**: 오버헤드 5-10%, 저장 공간 큼

**Decision Reason**: Head-based가 단순하고 RT 오버헤드 예측 가능

---

### Decision 4.5: Exporter 선택

**Decision**: OTLP (OpenTelemetry Protocol) HTTP/gRPC

**Rationale**:
- **표준 프로토콜**: OpenTelemetry 기본
- **Vendor-neutral**: Jaeger, Zipkin, Datadog, New Relic 등 모두 지원
- **Flexible**: HTTP 또는 gRPC 선택 가능
- **Future-proof**: 향후 새 백엔드 추가 용이

**설정 (HTTP)**:
```cpp
#include <opentelemetry/exporters/otlp/otlp_http_exporter.h>

otlp::OtlpHttpExporterOptions options;
options.url = "http://localhost:4318/v1/traces";
options.content_type = otlp::HttpRequestContentType::kJson;  // or kProtobuf
options.timeout = std::chrono::seconds(10);

auto exporter = std::make_unique<otlp::OtlpHttpExporter>(options);
```

**설정 (gRPC)** - 더 빠름:
```cpp
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter.h>

otlp::OtlpGrpcExporterOptions options;
options.endpoint = "localhost:4317";
options.use_ssl_credentials = false;

auto exporter = std::make_unique<otlp::OtlpGrpcExporter>(options);
```

**Jaeger 설정** (OTLP Receiver):
```yaml
# docker-compose.yml
services:
  jaeger:
    image: jaegertracing/all-in-one:latest
    ports:
      - "16686:16686"  # UI
      - "4317:4317"    # OTLP gRPC
      - "4318:4318"    # OTLP HTTP
    environment:
      - COLLECTOR_OTLP_ENABLED=true
```

**Alternatives Considered**:
- **Jaeger Thrift**: Jaeger native 프로토콜
  - **단점**: Jaeger 전용, 다른 백엔드 사용 불가
  - **장점**: Jaeger에 최적화
- **Zipkin format**: Zipkin JSON/Protobuf
  - **단점**: 구버전 포맷, 제한적 기능
- **Direct Jaeger client**: `jaeger-client-cpp`
  - **단점**: 유지보수 중단 (OpenTelemetry로 마이그레이션 권장)

**Decision Reason**: OTLP가 OpenTelemetry 표준이며 가장 미래 지향적

---

### Decision 4.6: EventBus 계측 패턴

**Decision**: Observer pattern (EventBus에 TracingObserver 추가)

**Rationale**:
- **Non-invasive**: EventBus 기존 코드 변경 최소화
- **기존 패턴 활용**: MXRC가 이미 observer pattern 사용 중
- **분리**: Tracing 로직이 비즈니스 로직과 분리
- **Optional**: Tracing 비활성화 시 오버헤드 zero

**구현**:
```cpp
// TracingObserver.h
class TracingObserver : public IEventObserver {
private:
    std::shared_ptr<opentelemetry::trace::Tracer> tracer_;

public:
    void onEventPublished(const Event& event) override {
        auto span = tracer_->StartSpan("event_published");
        span->SetAttribute("event.type", event.type);
        span->SetAttribute("event.id", event.id);
        span->End();
    }

    void onEventProcessed(const Event& event, uint64_t duration_ns) override {
        auto span = tracer_->StartSpan("event_processed");
        span->SetAttribute("event.type", event.type);
        span->SetAttribute("duration_ns", duration_ns);
        span->End();
    }

    void onEventDropped(const Event& event, const std::string& reason) override {
        auto span = tracer_->StartSpan("event_dropped");
        span->SetAttribute("event.type", event.type);
        span->SetAttribute("reason", reason);
        span->SetStatus(opentelemetry::trace::StatusCode::kError);
        span->End();
    }
};

// 사용
event_bus_->addObserver(std::make_shared<TracingObserver>(tracer));
```

**EventBus 수정** (최소한):
```cpp
class EventBus {
private:
    std::vector<std::shared_ptr<IEventObserver>> observers_;

public:
    void addObserver(std::shared_ptr<IEventObserver> observer) {
        observers_.push_back(observer);
    }

    void publish(const Event& event) {
        // 기존 로직

        // Observer 알림
        for (auto& observer : observers_) {
            observer->onEventPublished(event);
        }
    }
};
```

**Alternatives Considered**:
- **Interceptor pattern**: EventBus publish/subscribe를 감쌈
  - **단점**: 코드 구조 변경 필요, 복잡도 증가
- **Decorator pattern**: EventBus 전체를 감쌈
  - **단점**: 인터페이스 전체 재구현 필요
- **직접 instrumentation**: EventBus 내부에 tracing 코드 추가
  - **단점**: 코드 침투적, tracing 의존성 강제

**Decision Reason**: Observer가 기존 아키텍처와 가장 잘 맞고 비침투적

---

## 요약

### 우선순위별 결정사항

#### P1: CPU Isolation & NUMA Optimization
1. ✅ **CPU Affinity**: `pthread_setaffinity_np`
2. ✅ **CPU Isolation**: `isolcpus` + `cgroups`
3. ✅ **NUMA API**: `libnuma`
4. ✅ **NUMA 정책**: `NUMA_LOCAL` + `NUMA_PREFERRED`
5. ✅ **CPU Hotplug**: Production에서 비활성화
6. ✅ **NUMA 모니터링**: `/proc/[pid]/numa_maps` + Prometheus

#### P2: High Availability & Failover
1. ✅ **Process Monitoring**: `systemd` watchdog + custom health check
2. ✅ **State Checkpoint**: Selective JSON serialization
3. ✅ **Leader Election**: Bully algorithm (초기) → etcd (확장)
4. ✅ **Split-Brain 방지**: Quorum + Fencing
5. ✅ **Consensus Library**: 자체 구현 → `etcd-cpp-apiv3`
6. ✅ **Health Check**: HTTP REST API

#### P3: Structured Logging Integration
1. ✅ **JSON Formatter**: Custom spdlog formatter
2. ✅ **Log Schema**: ECS + MXRC 커스텀 필드
3. ✅ **Log Delivery**: Filebeat (file-based)
4. ✅ **Buffer**: Ring buffer + `overrun_oldest`
5. ✅ **성능 측정**: < 1% overhead 목표
6. ✅ **Correlation**: Thread-local storage

#### P4: Distributed Tracing
1. ✅ **SDK**: OpenTelemetry C++
2. ✅ **Context Propagation**: W3C Trace Context
3. ✅ **Span 관리**: RAII Span Guard
4. ✅ **Sampling**: Head-based (1-10%)
5. ✅ **Exporter**: OTLP (HTTP/gRPC)
6. ✅ **EventBus 계측**: Observer pattern

---

## 다음 단계

이 research를 바탕으로 다음 Phase 1 design artifacts를 생성합니다:

1. **data-model.md**: 8개 entity 상세 정의
2. **contracts/**: API spec (health-check-api.yaml, metrics-api.yaml)
3. **quickstart.md**: 설정 및 테스트 가이드

모든 결정은 다음 원칙을 따릅니다:
- ✅ RT 성능 최우선 (deadline miss < 0.01%, jitter < 10μs)
- ✅ RAII 및 메모리 안전성 (AddressSanitizer 항상 활성화)
- ✅ 검증된 솔루션 우선 (production-tested)
- ✅ 외부 의존성 최소화 (embedded system 고려)
- ✅ 기존 아키텍처 존중 (최소 변경)
