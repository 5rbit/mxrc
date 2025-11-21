# Feature Specification: systemd 기반 프로세스 관리 고도화

**Feature Branch**: `018-systemd-process-management`
**Created**: 2025-01-21
**Status**: Planning
**Progress**: 2/5 (Spec → Plan → Tasks → Implementation → Completed)
**Last Updated**: 2025-01-21
**Input**: User description: "systemd 기반 프로세스 관리 고도화"

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Task, Action, Sequence, API, JSON 등)
- 일반 설명, 요구사항, 시나리오는 모두 한글로 작성합니다

**예시**:
- ✅ 좋은 예: "사용자는 Task를 생성할 수 있어야 합니다"
- ❌ 나쁜 예: "User can create a task"

---

## User Scenarios & Testing

### User Story 1 - RT 프로세스 실시간 성능 보장 (Priority: P1)

시스템 운영자가 RT 프로세스의 실시간 성능을 보장받아야 합니다. RT 프로세스는 전용 CPU 코어에서 FIFO 스케줄링 정책으로 실행되며, jitter < 50μs를 유지해야 합니다.

**Why this priority**: 로봇 제어의 핵심 기능으로, 실시간 성능이 보장되지 않으면 안전사고 및 오작동이 발생할 수 있습니다.

**Independent Test**: RT 프로세스를 systemd로 시작하고 `cyclictest`로 jitter를 측정하여 50μs 이하인지 확인합니다. `systemctl status mxrc-rt`로 RT 우선순위와 CPU affinity가 올바르게 설정되었는지 검증합니다.

**Acceptance Scenarios**:

1. **Given** RT 프로세스가 systemd 서비스로 등록되어 있고, **When** `systemctl start mxrc-rt`로 프로세스를 시작하면, **Then** 프로세스는 FIFO 스케줄링 정책(우선순위 80)으로 실행되고 CPU 코어 2-3에 바인딩됩니다.

2. **Given** RT 프로세스가 실행 중이고, **When** cyclictest로 10,000 사이클 동안 jitter를 측정하면, **Then** 최대 jitter는 50μs 이하입니다.

3. **Given** RT 프로세스가 실행 중이고, **When** `systemctl status mxrc-rt`로 상태를 확인하면, **Then** CPUSchedulingPolicy=fifo, CPUSchedulingPriority=80, CPUAffinity=2,3이 표시됩니다.

---

### User Story 2 - Watchdog 기반 장애 감지 및 자동 재시작 (Priority: P1)

시스템 운영자가 프로세스 장애를 자동으로 감지하고 재시작할 수 있어야 합니다. Watchdog 메커니즘을 통해 프로세스가 정상 동작 중임을 주기적으로 확인하고, 응답이 없으면 자동으로 재시작합니다.

**Why this priority**: 무인 운영 환경에서 프로세스 장애 시 자동 복구가 필수입니다. 사람의 개입 없이 서비스 가용성을 유지해야 합니다.

**Independent Test**: 프로세스에서 의도적으로 watchdog 알림을 중단하고, 30초 이내에 systemd가 프로세스를 재시작하는지 확인합니다. `journalctl -u mxrc-rt`로 재시작 로그를 검증합니다.

**Acceptance Scenarios**:

1. **Given** RT 프로세스가 WatchdogSec=30s로 설정되어 실행 중이고, **When** 프로세스가 정상적으로 `sd_notify("WATCHDOG=1")`를 전송하면, **Then** systemd는 프로세스를 정상으로 간주하고 재시작하지 않습니다.

2. **Given** RT 프로세스가 실행 중이고, **When** 프로세스가 30초 이상 watchdog 알림을 전송하지 않으면, **Then** systemd는 프로세스를 비정상으로 판단하고 자동으로 재시작합니다.

3. **Given** 프로세스가 watchdog 타임아웃으로 재시작되었고, **When** `systemctl status mxrc-rt`로 상태를 확인하면, **Then** "watchdog timeout" 메시지가 표시되고 RestartCount가 증가합니다.

---

### User Story 3 - 리소스 제어 및 격리 (Priority: P2)

시스템 운영자가 프로세스별 CPU, 메모리, I/O 리소스 사용량을 제한할 수 있어야 합니다. cgroups를 통해 RT 프로세스가 과도한 리소스를 사용하여 시스템 전체에 영향을 주는 것을 방지합니다.

**Why this priority**: 멀티 프로세스 환경에서 리소스 경쟁을 방지하고, 시스템 안정성을 보장하기 위해 필요합니다.

**Independent Test**: RT 프로세스에 CPU와 메모리 제한을 설정하고, 의도적으로 제한을 초과하는 작업을 수행하여 cgroups가 올바르게 동작하는지 확인합니다. `systemd-cgtop`로 리소스 사용량을 모니터링합니다.

**Acceptance Scenarios**:

1. **Given** RT 프로세스가 CPUQuota=200%로 설정되어 있고, **When** 프로세스가 실행 중일 때, **Then** 프로세스는 최대 2개의 CPU 코어만 사용합니다.

2. **Given** RT 프로세스가 MemoryMax=2G로 설정되어 있고, **When** 프로세스가 2GB 이상의 메모리를 할당하려고 하면, **Then** OOM killer가 작동하거나 할당이 실패합니다.

3. **Given** Non-RT 프로세스가 IOWeight=100, RT 프로세스가 IOWeight=500으로 설정되어 있고, **When** 두 프로세스가 동시에 디스크 I/O를 수행하면, **Then** RT 프로세스가 5배 더 많은 I/O 대역폭을 사용합니다.

---

### User Story 4 - 서비스 의존성 관리 및 순서 제어 (Priority: P2)

시스템 운영자가 MXRC 서비스들의 시작 순서를 제어할 수 있어야 합니다. Non-RT 프로세스(DataStore, EventBus)가 먼저 시작된 후 RT 프로세스가 시작되어야 합니다.

**Why this priority**: RT 프로세스가 의존하는 서비스가 준비되지 않으면 초기화 실패가 발생합니다. 올바른 시작 순서가 시스템 안정성에 필수적입니다.

**Independent Test**: 모든 MXRC 서비스를 중지한 후 `systemctl start mxrc.target`로 일괄 시작하고, `systemd-analyze critical-chain mxrc-rt`로 시작 순서를 검증합니다.

**Acceptance Scenarios**:

1. **Given** mxrc-nonrt 서비스가 After=network.target으로 설정되어 있고, **When** 시스템 부팅 시, **Then** network.target이 활성화된 후 mxrc-nonrt가 시작됩니다.

2. **Given** mxrc-rt 서비스가 After=mxrc-nonrt.service Requires=mxrc-nonrt.service로 설정되어 있고, **When** `systemctl start mxrc-rt`를 실행하면, **Then** mxrc-nonrt가 먼저 시작되고 활성 상태가 된 후 mxrc-rt가 시작됩니다.

3. **Given** mxrc-rt 서비스가 실행 중이고 mxrc-nonrt가 Requires로 설정되어 있으며, **When** `systemctl stop mxrc-nonrt`를 실행하면, **Then** mxrc-rt도 함께 중지됩니다.

---

### User Story 5 - 통합 모니터링 및 Prometheus 메트릭 노출 (Priority: P3)

시스템 운영자가 systemd 메트릭을 Prometheus로 수집하여 Grafana 대시보드에서 시각화할 수 있어야 합니다. 프로세스의 재시작 횟수, CPU 사용량, 메모리 사용량 등을 실시간으로 모니터링합니다.

**Why this priority**: 프로덕션 환경에서 시스템 상태를 실시간으로 파악하고, 문제 발생 시 빠르게 대응하기 위해 필요합니다.

**Independent Test**: `systemctl show mxrc-rt` 명령으로 메트릭을 수집하고, Prometheus exporter가 이를 올바르게 노출하는지 확인합니다. Grafana에서 메트릭이 표시되는지 검증합니다.

**Acceptance Scenarios**:

1. **Given** Prometheus exporter가 실행 중이고, **When** `curl http://localhost:9558/metrics`로 메트릭을 조회하면, **Then** `systemd_unit_restart_total{name="mxrc-rt.service"}`, `systemd_unit_cpu_seconds_total{name="mxrc-rt.service"}`, `systemd_unit_memory_bytes{name="mxrc-rt.service"}` 메트릭이 포함됩니다.

2. **Given** mxrc-rt 서비스가 watchdog 타임아웃으로 재시작되었고, **When** Prometheus에서 메트릭을 조회하면, **Then** `systemd_unit_restart_total{name="mxrc-rt.service"}`이 1 증가합니다.

3. **Given** Grafana 대시보드가 설정되어 있고, **When** 대시보드를 열면, **Then** 모든 MXRC 서비스의 상태(Active/Failed), CPU 사용률, 메모리 사용량, 재시작 횟수가 실시간으로 표시됩니다.

---

### User Story 6 - journald 통합 및 구조화 로깅 (Priority: P3)

시스템 운영자가 모든 MXRC 서비스 로그를 journald를 통해 중앙 집중식으로 관리할 수 있어야 합니다. 구조화된 로그를 통해 필터링 및 검색이 용이해야 합니다.

**Why this priority**: 분산된 로그 파일을 관리하는 것보다 journald를 통한 중앙 집중식 로그 관리가 더 효율적이고, 문제 해결 시간을 단축할 수 있습니다.

**Independent Test**: 프로세스가 로그를 출력하고, `journalctl -u mxrc-rt --output=json`으로 JSON 형식의 구조화된 로그를 조회합니다. 필터링 기능을 테스트합니다.

**Acceptance Scenarios**:

1. **Given** mxrc-rt 서비스가 실행 중이고, **When** 프로세스가 로그를 출력하면, **Then** `journalctl -u mxrc-rt`로 모든 로그를 조회할 수 있습니다.

2. **Given** 프로세스가 구조화된 로그(JSON)를 출력하고, **When** `journalctl -u mxrc-rt --output=json`로 조회하면, **Then** timestamp, log_level, message, trace_id 등의 필드가 JSON 형식으로 표시됩니다.

3. **Given** journald에 로그가 저장되어 있고, **When** `journalctl -u mxrc-rt -p err`로 에러 레벨 로그만 필터링하면, **Then** error 및 critical 레벨 로그만 표시됩니다.

---

### User Story 7 - 보안 강화 및 샌드박싱 (Priority: P3)

시스템 운영자가 프로세스의 보안을 강화하여 시스템 파일 및 다른 프로세스를 보호할 수 있어야 합니다. PrivateTmp, ProtectSystem, Seccomp 등을 통해 공격 표면을 최소화합니다.

**Why this priority**: 프로덕션 환경에서 보안 사고를 예방하고, 규제 요구사항(예: 산업용 로봇 안전 표준)을 충족하기 위해 필요합니다.

**Independent Test**: `systemd-analyze security mxrc-rt`로 보안 점수를 확인하고, 프로세스가 제한된 syscall만 호출하는지 `strace`로 검증합니다.

**Acceptance Scenarios**:

1. **Given** mxrc-rt 서비스가 PrivateTmp=yes로 설정되어 있고, **When** 프로세스가 `/tmp` 디렉토리에 파일을 생성하면, **Then** 다른 프로세스에서는 해당 파일을 볼 수 없습니다(격리된 tmpfs).

2. **Given** mxrc-rt 서비스가 ProtectSystem=strict로 설정되어 있고, **When** 프로세스가 `/usr` 또는 `/etc` 디렉토리에 파일을 쓰려고 하면, **Then** 쓰기 작업이 실패합니다(읽기 전용).

3. **Given** mxrc-rt 서비스가 SystemCallFilter=@system-service로 설정되어 있고, **When** `systemd-analyze security mxrc-rt`를 실행하면, **Then** 보안 점수가 8.0 이상입니다(10점 만점).

---

### User Story 8 - 부팅 최적화 및 병렬 시작 (Priority: P3)

시스템 운영자가 시스템 부팅 시간을 단축할 수 있어야 합니다. DefaultDependencies=no를 통해 불필요한 의존성을 제거하고, 병렬로 서비스를 시작하여 부팅 시간을 최적화합니다.

**Why this priority**: 로봇 시스템의 빠른 기동 시간은 생산성 향상에 직결됩니다. 부팅 시간 단축은 사용자 경험 개선에 기여합니다.

**Independent Test**: `systemd-analyze blame`와 `systemd-analyze critical-chain`으로 부팅 시간을 분석하고, 최적화 전후를 비교합니다.

**Acceptance Scenarios**:

1. **Given** MXRC 서비스들이 최적화 전 상태이고, **When** `systemd-analyze blame`를 실행하면, **Then** 각 서비스의 초기화 시간이 표시됩니다.

2. **Given** mxrc-rt 서비스가 DefaultDependencies=no로 설정되어 있고, **When** 시스템을 재부팅하면, **Then** mxrc-rt 서비스는 sysinit.target과 basic.target에 대한 의존성 없이 병렬로 시작됩니다.

3. **Given** 부팅 최적화가 완료되었고, **When** `systemd-analyze time`을 실행하면, **Then** MXRC 서비스들의 총 부팅 시간이 최적화 전보다 30% 이상 단축됩니다.

---

### Edge Cases

- **CPU isolation 미설정 시**: RT 프로세스가 격리된 CPU 코어에 바인딩되지만, 커널 파라미터 `isolcpus=2-3`이 설정되지 않으면 다른 프로세스도 해당 코어를 사용할 수 있어 실시간 성능이 저하될 수 있습니다. 설치 스크립트에서 이를 감지하고 경고해야 합니다.

- **Watchdog 타임아웃 값 설정 오류**: WatchdogSec 값이 너무 짧으면(예: 5초) 정상 프로세스도 재시작될 수 있고, 너무 길면(예: 5분) 장애 감지가 지연됩니다. 권장값(30초)을 문서화하고, 프로세스 특성에 맞게 조정할 수 있도록 해야 합니다.

- **MemoryMax 제한 초과 시 OOM**: 프로세스가 MemoryMax를 초과하면 OOM killer가 프로세스를 강제 종료할 수 있습니다. 이 경우 systemd는 Restart=on-failure 정책에 따라 재시작을 시도하지만, 근본 원인(메모리 누수 등)을 해결해야 합니다. journald 로그에 OOM 메시지를 기록해야 합니다.

- **의존성 순환 참조**: mxrc-rt가 mxrc-nonrt를 Requires하고, mxrc-nonrt가 mxrc-rt를 Wants하면 순환 의존성이 발생합니다. systemd는 이를 감지하고 경고하지만, 서비스가 제대로 시작되지 않을 수 있습니다. `systemctl list-dependencies --all mxrc.target`로 의존성을 검증해야 합니다.

- **journald 디스크 공간 부족**: 로그가 과도하게 쌓이면 디스크 공간이 부족해질 수 있습니다. SystemMaxUse 및 RuntimeMaxUse 설정으로 로그 크기를 제한하고, 오래된 로그를 자동으로 삭제하도록 구성해야 합니다.

- **Seccomp 필터로 인한 기능 제한**: SystemCallFilter가 너무 엄격하면 정상적인 작업도 차단될 수 있습니다(예: `@privileged` 차단 시 실시간 우선순위 설정 실패). 테스트 환경에서 충분히 검증한 후 프로덕션에 적용해야 합니다.

- **NUMA 정책 오설정**: 멀티 소켓 시스템에서 NUMAPolicy=bind를 잘못 설정하면 메모리 접근 성능이 저하될 수 있습니다. `numactl --hardware`로 NUMA 토폴로지를 확인하고, RT 프로세스가 사용하는 CPU와 같은 NUMA 노드의 메모리를 바인딩해야 합니다.

## Requirements

### Functional Requirements

- **FR-001**: 시스템은 RT 프로세스를 FIFO 스케줄링 정책(우선순위 80)으로 실행해야 합니다.
- **FR-002**: 시스템은 RT 프로세스를 CPU 코어 2-3에 바인딩해야 합니다(CPUAffinity=2,3).
- **FR-003**: 시스템은 Non-RT 프로세스를 CPU 코어 0-1에 바인딩해야 합니다(CPUAffinity=0,1).
- **FR-004**: 시스템은 Watchdog 메커니즘을 통해 프로세스 정상 동작을 주기적으로 확인해야 합니다(WatchdogSec=30s).
- **FR-005**: 프로세스는 `sd_notify("WATCHDOG=1")` API를 통해 watchdog 알림을 systemd에 전송해야 합니다.
- **FR-006**: 시스템은 watchdog 타임아웃 발생 시 프로세스를 자동으로 재시작해야 합니다(Restart=on-failure).
- **FR-007**: 시스템은 RT 프로세스의 CPU 사용량을 최대 200%로 제한해야 합니다(CPUQuota=200%).
- **FR-008**: 시스템은 RT 프로세스의 메모리 사용량을 최대 2GB로 제한해야 합니다(MemoryMax=2G).
- **FR-009**: 시스템은 RT 프로세스의 I/O 가중치를 500으로 설정하여 높은 우선순위를 부여해야 합니다(IOWeight=500).
- **FR-010**: 시스템은 Non-RT 프로세스(mxrc-nonrt)가 먼저 시작된 후 RT 프로세스(mxrc-rt)를 시작해야 합니다(After=mxrc-nonrt.service).
- **FR-011**: 시스템은 RT 프로세스가 Non-RT 프로세스에 의존하도록 설정해야 합니다(Requires=mxrc-nonrt.service).
- **FR-012**: 시스템은 mxrc.target을 통해 모든 MXRC 서비스를 그룹화하여 일괄 관리할 수 있어야 합니다.
- **FR-013**: 시스템은 systemd 메트릭(RestartCount, CPUUsageNSec, MemoryCurrent)을 Prometheus 형식으로 노출해야 합니다.
- **FR-014**: 시스템은 모든 서비스 로그를 journald를 통해 중앙 집중식으로 수집해야 합니다.
- **FR-015**: 시스템은 구조화된 로그(JSON 형식)를 journald에 기록하여 필터링 및 검색을 지원해야 합니다.
- **FR-016**: 시스템은 프로세스별로 격리된 임시 디렉토리를 제공해야 합니다(PrivateTmp=yes).
- **FR-017**: 시스템은 프로세스가 시스템 디렉토리(/usr, /etc)를 읽기 전용으로만 접근하도록 제한해야 합니다(ProtectSystem=strict).
- **FR-018**: 시스템은 허용된 syscall 목록만 호출할 수 있도록 Seccomp 필터를 적용해야 합니다(SystemCallFilter=@system-service).
- **FR-019**: 시스템은 프로세스가 새로운 권한을 획득하지 못하도록 제한해야 합니다(NoNewPrivileges=yes).
- **FR-020**: 시스템은 mxrc 사용자 및 그룹으로 프로세스를 실행하여 권한을 격리해야 합니다(User=mxrc, Group=mxrc).
- **FR-021**: 시스템은 불필요한 기본 의존성을 제거하여 부팅 시간을 단축할 수 있어야 합니다(DefaultDependencies=no).
- **FR-022**: 시스템은 서비스 간 의존성 그래프를 분석하여 병렬 시작이 가능한 서비스를 식별해야 합니다.
- **FR-023**: 시스템은 NUMA 정책을 통해 RT 프로세스가 사용하는 CPU와 동일한 NUMA 노드의 메모리를 바인딩해야 합니다(NUMAPolicy=bind, NUMAMask=0).
- **FR-024**: 시스템은 재시작 정책을 설정하여 서비스 장애 시 자동 복구를 지원해야 합니다(Restart=on-failure, RestartSec=5s).
- **FR-025**: 시스템은 재시작 빈도를 제한하여 무한 재시작 루프를 방지해야 합니다(StartLimitBurst=5, StartLimitIntervalSec=60s).

### Key Entities

- **systemd Unit File**: RT/Non-RT 프로세스를 관리하는 서비스 정의 파일. 스케줄링 정책, CPU affinity, 리소스 제한, watchdog 설정 등을 포함합니다.

- **mxrc.target**: 모든 MXRC 서비스를 그룹화하는 타겟 유닛. `systemctl start mxrc.target`으로 모든 서비스를 일괄 시작할 수 있습니다.

- **Watchdog Context**: 프로세스와 systemd 간의 watchdog 통신 상태. 마지막 알림 시간, 타임아웃 값, 재시작 횟수 등을 포함합니다.

- **cgroups Resource Limits**: CPU, 메모리, I/O 등의 리소스 제한 값. systemd가 커널 cgroups를 통해 적용합니다.

- **Prometheus Metric**: systemd 메트릭을 Prometheus 형식으로 변환한 데이터. 메트릭 이름, 값, 레이블(서비스 이름, 타입 등)을 포함합니다.

- **journald Log Entry**: 구조화된 로그 항목. timestamp, 서비스 이름, 로그 레벨, 메시지, 추가 메타데이터(trace_id, span_id 등)를 포함합니다.

## Success Criteria

### Measurable Outcomes

- **SC-001**: RT 프로세스의 최대 jitter는 50μs 이하여야 합니다(cyclictest로 측정).
- **SC-002**: Watchdog 타임아웃 발생 시 30초 이내에 프로세스가 재시작되어야 합니다.
- **SC-003**: 시스템은 `systemctl start mxrc.target` 명령으로 모든 MXRC 서비스를 10초 이내에 시작할 수 있어야 합니다.
- **SC-004**: Prometheus exporter는 모든 MXRC 서비스의 메트릭을 1초마다 갱신해야 합니다.
- **SC-005**: `systemd-analyze security mxrc-rt` 명령의 보안 점수는 8.0/10.0 이상이어야 합니다.
- **SC-006**: 부팅 시간 최적화 후 MXRC 서비스들의 총 초기화 시간은 최적화 전보다 30% 이상 단축되어야 합니다.
- **SC-007**: journald를 통한 로그 조회 성능은 100,000개의 로그 항목 중 특정 조건(예: 에러 레벨)으로 필터링 시 1초 이내에 결과를 반환해야 합니다.
- **SC-008**: RT 프로세스가 CPUQuota=200% 제한 하에서 최대 2개 코어를 초과하여 사용하지 않아야 합니다(`systemd-cgtop`으로 측정).
- **SC-009**: 시스템은 mxrc-nonrt 서비스가 중지되면 mxrc-rt 서비스도 자동으로 중지되어야 합니다(Requires 의존성).
- **SC-010**: 프로세스는 ProtectSystem=strict 설정 하에서 `/usr` 및 `/etc` 디렉토리에 쓰기 시도 시 100% 실패해야 합니다.

## Dependencies

### Technical Dependencies

- **systemd v249+**: Ubuntu 22.04 LTS 이상에서 제공되는 systemd 버전. WatchdogSec, CPUSchedulingPolicy, ProtectSystem 등의 기능을 지원합니다.
- **libsystemd-dev**: `sd_notify()` API를 사용하기 위한 개발 라이브러리.
- **PREEMPT_RT 커널**: 실시간 성능 보장을 위한 실시간 커널 패치.
- **CPU Isolation 커널 파라미터**: `isolcpus=2-3` 커널 부팅 파라미터를 통해 RT 전용 CPU 코어를 격리해야 합니다.
- **RLIMIT_RTPRIO 설정**: RT 우선순위 설정을 위한 시스템 리소스 제한. `/etc/security/limits.conf`에서 구성합니다.
- **Prometheus**: systemd 메트릭 수집 및 저장을 위한 시계열 데이터베이스.
- **Grafana**: Prometheus 메트릭 시각화를 위한 대시보드 도구.

### Feature Dependencies

- **001-production-readiness**: HA (High Availability) 기능과 통합되어야 합니다. systemd의 OnFailure 이벤트를 FailoverManager와 연동하여 프로세스 장애 시 failover를 트리거합니다.
- **Structured Logging (Phase 5)**: journald와 통합하여 구조화된 로그를 기록하고 조회합니다. ECS 스키마를 journald JSON 형식으로 변환해야 합니다.
- **Monitoring (Phase 4)**: Prometheus exporter가 systemd 메트릭을 수집하여 Grafana 대시보드에 표시합니다. ProcessMonitor와 연동하여 통합 모니터링을 제공합니다.

## Assumptions

- **가정 1**: 시스템은 Ubuntu 24.04 LTS에서 실행되며, systemd v255 이상이 설치되어 있습니다.
- **가정 2**: PREEMPT_RT 커널이 이미 설치 및 구성되어 있으며, RT 우선순위 설정이 가능합니다.
- **가정 3**: CPU 코어 2-3은 RT 전용으로 격리되어 있으며(`isolcpus=2-3`), 다른 프로세스가 사용하지 않습니다.
- **가정 4**: mxrc 사용자 및 그룹이 이미 생성되어 있으며, 필요한 권한(CAP_SYS_NICE 등)이 부여되어 있습니다.
- **가정 5**: 시스템은 최소 4개의 CPU 코어와 4GB 이상의 RAM을 가지고 있습니다.
- **가정 6**: Prometheus와 Grafana는 별도로 설치 및 구성되어 있으며, MXRC 시스템과 네트워크로 연결되어 있습니다.
- **가정 7**: 프로세스는 이미 `sd_notify()` API를 호출할 수 있도록 libsystemd와 링크되어 있습니다.
- **가정 8**: 멀티 소켓 시스템의 경우 NUMA 토폴로지가 확인되어 있으며, NUMAMask가 적절히 설정되어 있습니다.
- **가정 9**: 보안 요구사항(Seccomp, Capabilities 제한)은 테스트 환경에서 충분히 검증된 후 프로덕션에 적용됩니다.
- **가정 10**: journald는 기본 설정으로 동작하며, SystemMaxUse=1G, RuntimeMaxUse=512M로 로그 크기가 제한되어 있습니다.

## Scope

### In Scope

- mxrc-rt.service, mxrc-nonrt.service, mxrc-monitor.service 파일 고도화
- mxrc.target 파일 생성 및 구성
- sd_notify() watchdog 통합 코드 구현 (C++)
- systemd 메트릭을 Prometheus 형식으로 노출하는 exporter 구현
- journald 통합 및 구조화 로그 출력
- CPU affinity, NUMA 정책, 리소스 제한(cgroups) 설정
- 보안 강화(PrivateTmp, ProtectSystem, Seccomp, Capabilities 제한)
- 부팅 최적화(DefaultDependencies=no, 병렬 시작)
- 설치 및 구성 스크립트 작성
- 사용자 가이드 및 운영 매뉴얼 작성

### Out of Scope

- Pacemaker/Corosync를 이용한 멀티 노드 HA 클러스터 구성 (선택적 기능, 향후 구현 가능)
- 소켓 활성화(mxrc-api.socket) 및 타이머 유닛(mxrc-checkpoint.timer) 구현 (선택적 기능, 필요 시 추가)
- Kubernetes 환경에서의 systemd 통합 (별도 feature로 분리)
- Windows 또는 macOS 환경 지원
- systemd 이외의 init 시스템(SysVinit, Upstart) 지원
- RT 커널 설치 및 구성 (별도 인프라 작업으로 가정)
- Grafana 대시보드 템플릿 작성 (모니터링 feature에서 다룸)

## Risks

### Technical Risks

- **위험 1**: CPUIsolation 파라미터가 올바르게 설정되지 않으면 RT 성능이 보장되지 않습니다.
  - **완화 방안**: 설치 스크립트에서 `/proc/cmdline`을 확인하여 `isolcpus` 설정을 검증하고, 없으면 경고 메시지를 출력합니다.

- **위험 2**: Seccomp 필터가 너무 제한적이면 정상 작업도 차단될 수 있습니다.
  - **완화 방안**: 테스트 환경에서 `strace`로 필요한 syscall 목록을 확인하고, SystemCallFilter에 추가합니다. 프로덕션 적용 전 충분한 회귀 테스트를 수행합니다.

- **위험 3**: Watchdog 타임아웃 값이 부적절하면 정상 프로세스가 재시작되거나, 장애 감지가 지연될 수 있습니다.
  - **완화 방안**: 프로세스별 특성에 맞는 기본값(30초)을 제공하고, 운영 중 모니터링을 통해 조정할 수 있도록 문서화합니다.

### Integration Risks

- **위험 4**: systemd와 FailoverManager 간 이벤트 연동이 실패하면 failover가 트리거되지 않을 수 있습니다.
  - **완화 방안**: OnFailure=mxrc-failover@%n.service를 테스트하여 systemd가 failover 스크립트를 올바르게 호출하는지 검증합니다. 통합 테스트를 작성합니다.

- **위험 5**: Prometheus exporter가 systemd 메트릭을 올바르게 파싱하지 못하면 모니터링이 불가능합니다.
  - **완화 방안**: `systemctl show` 출력 형식을 명확히 파싱하고, 단위 테스트로 검증합니다. Prometheus 쿼리 문서를 작성합니다.

## Out of Scope (Clarifications)

- 본 feature는 systemd 기반 프로세스 관리 최적화에 집중하며, RT 커널 설치, Prometheus/Grafana 설치는 사전 준비 사항으로 가정합니다.
- Pacemaker/Corosync 통합은 선택적 기능으로, 향후 별도 feature로 구현할 수 있습니다.
- 소켓/타이머 유닛은 현재 MXRC 아키텍처에 필수가 아니므로, 필요 시 추가합니다.
