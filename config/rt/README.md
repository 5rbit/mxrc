# RT Performance Configuration

이 디렉토리는 실시간(RT) 프로세스의 성능 최적화를 위한 JSON 설정 파일들을 포함합니다.

## 설정 파일

### 1. cpu_affinity.json
CPU affinity와 스케줄러 설정을 정의합니다.

**주요 설정:**
- `cpu_cores`: RT 프로세스를 고정할 CPU 코어 번호 (예: [1, 2])
- `policy`: 스케줄링 정책 (SCHED_FIFO, SCHED_RR, SCHED_OTHER)
- `priority`: RT 우선순위 (1-99, 99가 최고)

**시스템 요구사항:**
```bash
# CPU isolation 설정 (부팅 파라미터)
# /etc/default/grub 수정:
GRUB_CMDLINE_LINUX="isolcpus=1,2 nohz_full=1,2 rcu_nocbs=1,2"

# 적용
sudo update-grub
sudo reboot
```

**권한 요구사항:**
- CAP_SYS_NICE capability 필요 (RT 우선순위 설정용)
```bash
sudo setcap cap_sys_nice=eip ./mxrc_rt
```

### 2. numa_binding.json
NUMA 메모리 바인딩 설정을 정의합니다.

**주요 설정:**
- `numa_node`: 메모리를 할당할 NUMA 노드 번호 (0부터 시작)
- `policy`: 메모리 할당 정책 (BIND, PREFERRED, INTERLEAVE)
- `min_local_access_percent`: 목표 로컬 NUMA 접근 비율 (기본: 95%)

**NUMA 노드 확인:**
```bash
# NUMA 토폴로지 확인
numactl --hardware

# CPU와 NUMA 노드 매핑 확인
lscpu | grep NUMA
```

**CPU 코어와 NUMA 노드 매칭:**
CPU affinity 설정의 CPU 코어들이 동일한 NUMA 노드에 있어야 최적 성능을 얻을 수 있습니다.

예시:
- CPU 0-3: NUMA node 0
- CPU 4-7: NUMA node 1

cpu_affinity.json에서 cores=[1,2]를 사용한다면, numa_binding.json에서 node=0을 설정해야 합니다.

### 3. perf_monitor.json
성능 모니터링 설정을 정의합니다.

**주요 설정:**
- `cycle_time_us`: RT 사이클 주기 (마이크로초)
- `deadline_us`: 목표 완료 시간 (cycle_time의 90% 권장)
- `max_deadline_miss_rate_percent`: 허용 가능한 deadline miss 비율 (0.01% = SC-001)

**성능 목표 (Success Criteria):**
- SC-001: Deadline miss rate < 0.01%
- SC-002: Cycle time jitter 감소 50%
- SC-003: Local NUMA access > 95%
- SC-004: 평균 메모리 접근 지연시간 감소 30%

## 사용 예제

### C++ 코드에서 설정 로드

```cpp
#include "core/rt/RTExecutive.h"
#include "core/rt/perf/CPUAffinityManager.h"
#include "core/rt/perf/NUMABinding.h"
#include "core/rt/perf/PerfMonitor.h"

int main() {
    auto executive = std::make_unique<mxrc::core::rt::RTExecutive>(1, 10);

    // 초기화 hook 등록
    executive->registerInitializationHook("cpu_affinity", []() {
        auto cpu_mgr = std::make_unique<mxrc::rt::perf::CPUAffinityManager>();
        if (!cpu_mgr->loadConfig("config/rt/cpu_affinity.json")) {
            spdlog::error("Failed to load CPU affinity config");
            return;
        }
        if (!cpu_mgr->apply()) {
            spdlog::error("Failed to apply CPU affinity");
        }
    });

    executive->registerInitializationHook("numa_binding", []() {
        auto numa_binding = std::make_unique<mxrc::rt::perf::NUMABinding>();
        if (!numa_binding->loadConfig("config/rt/numa_binding.json")) {
            spdlog::error("Failed to load NUMA binding config");
            return;
        }
        if (!numa_binding->apply()) {
            spdlog::error("Failed to apply NUMA binding");
        }
    });

    // 초기화 hook 실행
    executive->executeInitializationHooks();

    // RT 실행
    executive->run();

    return 0;
}
```

### RTExecutive 통합 사용 (권장)

```cpp
#include "core/rt/RTExecutive.h"
#include "core/rt/RTMetrics.h"
#include "core/monitoring/MetricsCollector.h"

int main() {
    // Metrics 설정
    auto metrics_collector = std::make_shared<mxrc::core::monitoring::MetricsCollector>();
    auto rt_metrics = std::make_unique<mxrc::core::rt::RTMetrics>(metrics_collector);

    auto executive = std::make_unique<mxrc::core::rt::RTExecutive>(1, 10);
    executive->setRTMetrics(rt_metrics.get());

    // 설정 파일 로드 (내부에서 자동으로 초기화 hook 등록)
    executive->configureCPUAffinity("config/rt/cpu_affinity.json");
    executive->configureNUMABinding("config/rt/numa_binding.json");
    executive->configurePerfMonitor("config/rt/perf_monitor.json");

    // 초기화 및 실행
    executive->executeInitializationHooks();
    executive->run();

    return 0;
}
```

## 테스트 및 검증

### 설정 검증

```bash
# JSON 파일 문법 검증
jq . config/rt/cpu_affinity.json
jq . config/rt/numa_binding.json
jq . config/rt/perf_monitor.json

# CPU isolation 확인
cat /proc/cmdline | grep isolcpus

# NUMA 설정 확인
numactl --show

# 프로세스 CPU affinity 확인 (프로세스 실행 중)
taskset -p <pid>

# 프로세스 NUMA 바인딩 확인 (프로세스 실행 중)
cat /proc/<pid>/numa_maps
```

### 성능 측정

```bash
# RT 성능 통합 테스트 실행
./build/run_tests --gtest_filter="*CPUIsolation*"
./build/run_tests --gtest_filter="*NUMAOptimization*"

# Prometheus 메트릭 확인
curl http://localhost:8080/metrics | grep mxrc_rt
```

## 문제 해결

### CPU Affinity 실패
```
Failed to set CPU affinity: Operation not permitted
```
**해결:** CAP_SYS_NICE capability 부여
```bash
sudo setcap cap_sys_nice=eip ./mxrc_rt
```

### NUMA 바인딩 실패
```
NUMA not available on this system
```
**해결:** `numa_binding.json`에서 `allow_non_numa: true` 설정 (테스트용)

### Deadline Miss 과다
```
Deadline miss rate: 0.15% (exceeds threshold 0.01%)
```
**해결 체크리스트:**
1. CPU isolation 확인 (`isolcpus` 커널 파라미터)
2. RT 우선순위 확인 (priority=99)
3. NUMA 노드와 CPU 코어 매칭 확인
4. 시스템 부하 확인 (`top`, `htop`)
5. `deadline_us` 값 조정 (여유 시간 증가)

## 참고 자료

- [Linux Real-Time Wiki](https://wiki.linuxfoundation.org/realtime/start)
- [PREEMPT_RT Patch](https://wiki.linuxfoundation.org/realtime/preempt_rt_versions)
- [NUMA Best Practices](https://www.kernel.org/doc/html/latest/vm/numa.html)
- [CPU Isolation Guide](https://www.kernel.org/doc/html/latest/admin-guide/kernel-parameters.html)
