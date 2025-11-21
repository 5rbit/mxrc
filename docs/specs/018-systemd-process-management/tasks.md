---
description: "systemd 기반 프로세스 관리 고도화 작업 목록"
---

# Tasks: systemd 기반 프로세스 관리 고도화

**Input**: Design documents from `/docs/specs/018-systemd-process-management/`
**Status**: Completed Phase 4 - MVP Complete (RT Performance + Watchdog)
**Progress**: 49/115 tasks completed (43%)
**Last Updated**: 2025-01-21
**Prerequisites**: plan.md (required), spec.md (required for user stories)

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 작업 설명은 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Task, Action, test, model, service 등)
- 파일 경로와 코드는 원래대로 표기합니다

**예시**:
- ✅ 좋은 예: "Task 모델 생성 in src/models/task.cpp"
- ❌ 나쁜 예: "Create Task model in src/models/task.cpp"

---

## Phase 1: Setup (공통 인프라)

**Purpose**: 프로젝트 초기화 및 기본 구조 설정

- [x] T001 프로젝트 디렉토리 구조 생성 src/core/systemd/{interfaces,impl,util,dto}
- [x] T002 [P] systemd 서비스 파일 디렉토리 생성 systemd/
- [x] T003 [P] 설정 파일 디렉토리 생성 config/systemd/
- [x] T004 [P] 테스트 디렉토리 생성 tests/{unit,integration}/systemd/
- [x] T005 CMakeLists.txt에 libsystemd-dev 의존성 추가
- [x] T006 [P] CMakeLists.txt에 systemd 모듈 소스 추가
- [x] T007 [P] pkg-config로 libsystemd 링크 설정 확인
- [x] T008 [P] 문서 디렉토리 확인 docs/specs/018-systemd-process-management/
- [x] T009 빌드 시스템 테스트 및 검증

---

## Phase 2: Foundational (블로킹 필수 요소)

**Purpose**: 모든 User Story가 의존하는 핵심 인프라

**⚠️ CRITICAL**: 이 Phase가 완료되기 전까지 User Story 작업 불가

- [x] T010 [P] SystemdMetric DTO 정의 in src/core/systemd/dto/SystemdMetric.h
- [x] T011 [P] JournaldEntry DTO 정의 in src/core/systemd/dto/JournaldEntry.h
- [x] T012 [P] SystemdUtil 유틸리티 클래스 기본 구조 in src/core/systemd/util/SystemdUtil.h
- [x] T013 SystemdUtil::checkSystemdAvailable() 구현 in src/core/systemd/util/SystemdUtil.cpp
- [x] T014 SystemdUtil::getServiceProperty() 구현 in src/core/systemd/util/SystemdUtil.cpp
- [x] T015 [P] 기본 에러 처리 및 예외 클래스 정의 SystemdException
- [x] T016 [P] 기본 로깅 설정 (spdlog 통합)
- [x] T017 [P] 단위 테스트 프레임워크 설정 GoogleTest in tests/unit/systemd/
- [x] T018 SystemdUtil 단위 테스트 작성 in tests/unit/systemd/systemd_util_test.cpp

**Checkpoint**: ✅ Foundation ready - User Story 구현을 병렬로 시작할 수 있음

---

## Phase 3: User Story 1 - RT 프로세스 실시간 성능 보장 (Priority: P1) 🎯 MVP

**Goal**: RT 프로세스를 FIFO 스케줄링(우선순위 80)으로 CPU 코어 2-3에 바인딩하고 jitter < 50μs 보장

**Independent Test**: `systemctl start mxrc-rt`로 시작 후 cyclictest로 jitter 측정하여 50μs 이하 확인

### Tests for User Story 1

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation**

- [x] T019 [P] [US1] RT 프로세스 스케줄링 정책 테스트 in tests/integration/systemd/rt_scheduling_test.cpp
- [x] T020 [P] [US1] CPU affinity 검증 테스트 in tests/integration/systemd/cpu_affinity_test.cpp
- [x] T021 [P] [US1] jitter 측정 통합 테스트 in tests/integration/systemd/jitter_test.cpp

### Implementation for User Story 1

- [x] T022 [P] [US1] mxrc-rt.service 파일 생성 in systemd/mxrc-rt.service
- [x] T023 [US1] CPUSchedulingPolicy=fifo 설정 추가 in systemd/mxrc-rt.service
- [x] T024 [US1] CPUSchedulingPriority=80 설정 추가 in systemd/mxrc-rt.service
- [x] T025 [US1] CPUAffinity=2,3 설정 추가 in systemd/mxrc-rt.service
- [x] T026 [US1] ExecStart 경로 설정 in systemd/mxrc-rt.service
- [x] T027 [P] [US1] RT 프로세스 초기화 코드 수정 (sched_setscheduler 제거, systemd에 위임)
- [x] T028 [P] [US1] 설치 스크립트 작성 scripts/install-systemd-services.sh
- [x] T029 [US1] isolcpus 커널 파라미터 검증 로직 추가 in scripts/install-systemd-services.sh
- [x] T030 [US1] RLIMIT_RTPRIO 설정 확인 로직 추가 in scripts/install-systemd-services.sh

**Checkpoint**: ✅ US1 완료 - RT 프로세스가 systemd로 실시간 성능 보장되어 독립적으로 동작

---

## Phase 4: User Story 2 - Watchdog 기반 장애 감지 및 자동 재시작 (Priority: P1) 🎯 MVP

**Goal**: Watchdog 메커니즘으로 프로세스 정상 동작 확인 및 타임아웃 시 자동 재시작

**Independent Test**: 의도적으로 watchdog 알림 중단 후 30초 이내 자동 재시작 확인

### Tests for User Story 2

- [x] T031 [P] [US2] Watchdog 알림 전송 단위 테스트 in tests/unit/systemd/watchdog_notifier_test.cpp
- [x] T032 [P] [US2] Watchdog 타이머 단위 테스트 in tests/unit/systemd/watchdog_timer_test.cpp
- [x] T033 [P] [US2] Watchdog 타임아웃 통합 테스트 in tests/integration/systemd/watchdog_timeout_test.cpp

### Implementation for User Story 2

- [x] T034 [P] [US2] IWatchdogNotifier 인터페이스 정의 in src/core/systemd/interfaces/IWatchdogNotifier.h
- [x] T035 [P] [US2] WatchdogTimer 클래스 정의 in src/core/systemd/util/WatchdogTimer.h
- [x] T036 [US2] WatchdogTimer::start() 구현 in src/core/systemd/util/WatchdogTimer.cpp
- [x] T037 [US2] WatchdogTimer::stop() 구현 in src/core/systemd/util/WatchdogTimer.cpp
- [x] T038 [US2] WatchdogTimer::notify() 구현 (주기적 알림) in src/core/systemd/util/WatchdogTimer.cpp
- [x] T039 [P] [US2] SdNotifyWatchdog 클래스 정의 in src/core/systemd/impl/SdNotifyWatchdog.h
- [x] T040 [US2] SdNotifyWatchdog::sendWatchdog() 구현 (sd_notify API) in src/core/systemd/impl/SdNotifyWatchdog.cpp
- [x] T041 [US2] SdNotifyWatchdog::sendReady() 구현 in src/core/systemd/impl/SdNotifyWatchdog.cpp
- [x] T042 [US2] WatchdogSec=30s 설정 추가 in systemd/mxrc-rt.service
- [x] T043 [US2] Restart=on-failure 설정 추가 in systemd/mxrc-rt.service
- [x] T044 [US2] RestartSec=5s 설정 추가 in systemd/mxrc-rt.service
- [x] T045 [US2] StartLimitBurst=5 설정 추가 in systemd/mxrc-rt.service
- [x] T046 [US2] StartLimitIntervalSec=60s 설정 추가 in systemd/mxrc-rt.service
- [x] T047 [US2] RT 프로세스 main()에 Watchdog 통합 코드 추가
- [x] T048 [P] [US2] watchdog.json 설정 파일 생성 in config/systemd/watchdog.json
- [x] T049 [US2] 설정 파일 로딩 로직 추가 (nlohmann_json 사용)

**Checkpoint**: ✅ US1 + US2 완료 - RT 프로세스가 실시간 성능 + 자동 장애 복구 기능 보유

---

## Phase 5: User Story 3 - 리소스 제어 및 격리 (Priority: P2)

**Goal**: cgroups를 통한 CPU, 메모리, I/O 리소스 제한으로 시스템 안정성 보장

**Independent Test**: RT 프로세스에 리소스 제한 설정 후 systemd-cgtop으로 사용량 모니터링

### Tests for User Story 3

- [ ] T050 [P] [US3] CPU quota 제한 테스트 in tests/integration/systemd/cpu_quota_test.cpp
- [ ] T051 [P] [US3] 메모리 제한 테스트 in tests/integration/systemd/memory_limit_test.cpp
- [ ] T052 [P] [US3] I/O 가중치 테스트 in tests/integration/systemd/io_weight_test.cpp

### Implementation for User Story 3

- [ ] T053 [P] [US3] CPUQuota=200% 설정 추가 in systemd/mxrc-rt.service
- [ ] T054 [P] [US3] MemoryMax=2G 설정 추가 in systemd/mxrc-rt.service
- [ ] T055 [P] [US3] IOWeight=500 설정 추가 in systemd/mxrc-rt.service
- [ ] T056 [P] [US3] mxrc-nonrt.service 파일 생성 in systemd/mxrc-nonrt.service
- [ ] T057 [US3] Non-RT 프로세스 리소스 제한 설정 (CPUQuota=100%, MemoryMax=1G, IOWeight=100)
- [ ] T058 [US3] CPUAffinity=0,1 설정 추가 (Non-RT) in systemd/mxrc-nonrt.service
- [ ] T059 [P] [US3] cgroups 설정 검증 스크립트 작성 scripts/verify-cgroups.sh
- [ ] T060 [US3] systemd-cgtop 통합 모니터링 스크립트 작성 scripts/monitor-cgroups.sh

**Checkpoint**: US1 + US2 + US3 완료 - 리소스 격리로 시스템 전체 안정성 보장

---

## Phase 6: User Story 4 - 서비스 의존성 관리 및 순서 제어 (Priority: P2)

**Goal**: Non-RT → RT 순서로 서비스 시작하여 초기화 실패 방지

**Independent Test**: `systemctl start mxrc.target`로 일괄 시작 후 systemd-analyze critical-chain으로 순서 검증

### Tests for User Story 4

- [ ] T061 [P] [US4] 서비스 시작 순서 테스트 in tests/integration/systemd/service_order_test.cpp
- [ ] T062 [P] [US4] 의존성 체인 검증 테스트 in tests/integration/systemd/dependency_chain_test.cpp
- [ ] T063 [P] [US4] mxrc.target 일괄 시작 테스트 in tests/integration/systemd/target_test.cpp

### Implementation for User Story 4

- [ ] T064 [P] [US4] mxrc.target 파일 생성 in systemd/mxrc.target
- [ ] T065 [US4] mxrc.target 설명 및 Wants 설정 추가
- [ ] T066 [US4] After=network.target 설정 추가 in systemd/mxrc-nonrt.service
- [ ] T067 [US4] WantedBy=mxrc.target 설정 추가 in systemd/mxrc-nonrt.service
- [ ] T068 [US4] After=mxrc-nonrt.service 설정 추가 in systemd/mxrc-rt.service
- [ ] T069 [US4] Requires=mxrc-nonrt.service 설정 추가 in systemd/mxrc-rt.service
- [ ] T070 [US4] WantedBy=mxrc.target 설정 추가 in systemd/mxrc-rt.service
- [ ] T071 [P] [US4] 의존성 순환 참조 검증 스크립트 작성 scripts/verify-dependencies.sh
- [ ] T072 [US4] systemd-analyze critical-chain 통합 in scripts/verify-dependencies.sh

**Checkpoint**: US1-4 완료 - 서비스 의존성 관리로 안정적인 시작 순서 보장

---

## Phase 7: User Story 5 - 통합 모니터링 및 Prometheus 메트릭 노출 (Priority: P3)

**Goal**: systemd 메트릭을 Prometheus로 수집하여 Grafana 대시보드에서 시각화

**Independent Test**: Prometheus exporter가 메트릭을 올바르게 노출하는지 curl로 확인

### Tests for User Story 5

- [ ] T073 [P] [US5] systemctl show 파싱 단위 테스트 in tests/unit/systemd/metrics_collector_test.cpp
- [ ] T074 [P] [US5] Prometheus 메트릭 포맷 테스트 in tests/unit/systemd/prometheus_format_test.cpp
- [ ] T075 [P] [US5] 메트릭 수집 통합 테스트 in tests/integration/systemd/metrics_integration_test.cpp

### Implementation for User Story 5

- [ ] T076 [P] [US5] ISystemdMetricsCollector 인터페이스 정의 in src/core/systemd/interfaces/ISystemdMetricsCollector.h
- [ ] T077 [P] [US5] SystemdMetricsCollector 클래스 정의 in src/core/systemd/impl/SystemdMetricsCollector.h
- [ ] T078 [US5] SystemdMetricsCollector::collectMetrics() 구현 (systemctl show 파싱) in src/core/systemd/impl/SystemdMetricsCollector.cpp
- [ ] T079 [US5] Prometheus 메트릭 포맷 변환 로직 구현 in src/core/systemd/impl/SystemdMetricsCollector.cpp
- [ ] T080 [US5] RestartCount 메트릭 수집 (systemd_unit_restart_total)
- [ ] T081 [US5] CPU 사용량 메트릭 수집 (systemd_unit_cpu_seconds_total)
- [ ] T082 [US5] 메모리 사용량 메트릭 수집 (systemd_unit_memory_bytes)
- [ ] T083 [P] [US5] mxrc-monitor.service 파일 생성 in systemd/mxrc-monitor.service
- [ ] T084 [US5] mxrc-monitor 프로세스 구현 (메트릭 수집 및 HTTP 서버)
- [ ] T085 [US5] Prometheus exporter HTTP endpoint 구현 (/metrics)
- [ ] T086 [P] [US5] metrics.json 설정 파일 생성 in config/systemd/metrics.json
- [ ] T087 [US5] 메트릭 수집 주기 설정 (기본 1초)

**Checkpoint**: US1-5 완료 - Prometheus 메트릭 노출로 실시간 모니터링 가능

---

## Phase 8: User Story 6 - journald 통합 및 구조화 로깅 (Priority: P3)

**Goal**: journald를 통한 중앙 집중식 로그 관리 및 구조화된 로그 출력

**Independent Test**: `journalctl -u mxrc-rt --output=json`으로 JSON 형식 로그 조회

### Tests for User Story 6

- [ ] T088 [P] [US6] journald 로깅 단위 테스트 in tests/unit/systemd/journald_logger_test.cpp
- [ ] T089 [P] [US6] 구조화된 로그 포맷 테스트 in tests/unit/systemd/structured_log_test.cpp
- [ ] T090 [P] [US6] journalctl 필터링 통합 테스트 in tests/integration/systemd/journald_integration_test.cpp

### Implementation for User Story 6

- [ ] T091 [P] [US6] IJournaldLogger 인터페이스 정의 in src/core/systemd/interfaces/IJournaldLogger.h
- [ ] T092 [P] [US6] JournaldLogger 클래스 정의 in src/core/systemd/impl/JournaldLogger.h
- [ ] T093 [US6] JournaldLogger::log() 구현 (sd_journal_send API) in src/core/systemd/impl/JournaldLogger.cpp
- [ ] T094 [US6] ECS 필드 매핑 (timestamp, log_level, message, trace_id) in src/core/systemd/impl/JournaldLogger.cpp
- [ ] T095 [US6] 로그 레벨 변환 로직 (spdlog → journald priority)
- [ ] T096 [US6] spdlog와 journald 통합 (custom sink)
- [ ] T097 [US6] StandardOutput=journal 설정 추가 in systemd/mxrc-rt.service
- [ ] T098 [US6] StandardError=journal 설정 추가 in systemd/mxrc-rt.service
- [ ] T099 [US6] SyslogIdentifier=mxrc-rt 설정 추가 in systemd/mxrc-rt.service

**Checkpoint**: US1-6 완료 - journald 통합으로 중앙 집중식 로그 관리

---

## Phase 9: User Story 7 - 보안 강화 및 샌드박싱 (Priority: P3)

**Goal**: PrivateTmp, ProtectSystem, Seccomp 등으로 공격 표면 최소화

**Independent Test**: `systemd-analyze security mxrc-rt`로 보안 점수 8.0 이상 확인

### Tests for User Story 7

- [ ] T100 [P] [US7] 보안 점수 검증 테스트 in tests/integration/systemd/security_score_test.cpp
- [ ] T101 [P] [US7] ProtectSystem 검증 테스트 in tests/integration/systemd/protect_system_test.cpp
- [ ] T102 [P] [US7] Seccomp 필터 검증 테스트 in tests/integration/systemd/seccomp_test.cpp

### Implementation for User Story 7

- [ ] T103 [P] [US7] PrivateTmp=yes 설정 추가 in systemd/mxrc-rt.service
- [ ] T104 [P] [US7] ProtectSystem=strict 설정 추가 in systemd/mxrc-rt.service
- [ ] T105 [P] [US7] ProtectHome=yes 설정 추가 in systemd/mxrc-rt.service
- [ ] T106 [P] [US7] NoNewPrivileges=yes 설정 추가 in systemd/mxrc-rt.service
- [ ] T107 [P] [US7] SystemCallFilter=@system-service 설정 추가 in systemd/mxrc-rt.service
- [ ] T108 [P] [US7] User=mxrc 설정 추가 in systemd/mxrc-rt.service
- [ ] T109 [P] [US7] Group=mxrc 설정 추가 in systemd/mxrc-rt.service
- [ ] T110 [P] [US7] security.json 설정 파일 생성 in config/systemd/security.json
- [ ] T111 [US7] 필요한 syscall 목록 검증 (strace 사용)
- [ ] T112 [US7] AmbientCapabilities 설정 (CAP_SYS_NICE만 허용)

**Checkpoint**: US1-7 완료 - 보안 강화로 프로덕션 환경 안전성 확보

---

## Phase 10: User Story 8 - 부팅 최적화 및 병렬 시작 (Priority: P3)

**Goal**: DefaultDependencies=no 및 병렬 시작으로 부팅 시간 30% 단축

**Independent Test**: `systemd-analyze blame`로 부팅 시간 분석 및 최적화 전후 비교

### Tests for User Story 8

- [ ] T113 [P] [US8] 부팅 시간 측정 테스트 in tests/integration/systemd/boot_time_test.cpp
- [ ] T114 [P] [US8] 병렬 시작 검증 테스트 in tests/integration/systemd/parallel_start_test.cpp

### Implementation for User Story 8

- [ ] T115 [P] [US8] DefaultDependencies=no 설정 추가 in systemd/mxrc-rt.service
- [ ] T116 [P] [US8] 필수 의존성만 명시 (After=sysinit.target basic.target)
- [ ] T117 [US8] systemd-analyze blame 분석 스크립트 작성 scripts/analyze-boot-time.sh
- [ ] T118 [US8] systemd-analyze critical-chain 분석 스크립트 작성
- [ ] T119 [US8] 부팅 시간 최적화 전후 비교 보고서 생성

**Checkpoint**: US1-8 완료 - 모든 User Story 구현 완료

---

## Phase 11: Polish & Cross-Cutting Concerns

**Purpose**: 여러 User Story에 영향을 미치는 개선 작업

- [ ] T120 [P] 운영 매뉴얼 작성 docs/operations/systemd-guide.md
- [ ] T121 [P] Troubleshooting 가이드 작성 docs/operations/systemd-troubleshooting.md
- [ ] T122 [P] 설치 가이드 작성 docs/operations/systemd-installation.md
- [ ] T123 코드 리뷰 및 리팩토링
- [ ] T124 [P] 메모리 누수 검사 (AddressSanitizer)
- [ ] T125 [P] 성능 벤치마크 (Watchdog 알림 오버헤드 < 10μs 검증)
- [ ] T126 [P] RT jitter 최종 검증 (cyclictest 10,000 사이클)
- [ ] T127 모든 단위 테스트 실행 및 커버리지 측정
- [ ] T128 모든 통합 테스트 실행 및 검증
- [ ] T129 [P] CI/CD 파이프라인 업데이트 (systemd 테스트 추가)
- [ ] T130 [P] CHANGELOG.md 업데이트
- [ ] T131 최종 보안 점수 확인 (systemd-analyze security)
- [ ] T132 부팅 시간 최적화 검증 (30% 단축 확인)
- [ ] T133 Prometheus 메트릭 수집 검증 (Grafana 대시보드 테스트)
- [ ] T134 journald 로그 조회 성능 검증 (100,000개 항목 1초 이내)
- [ ] T135 quickstart.md 검증 (신규 사용자 테스트)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 의존성 없음 - 즉시 시작 가능
- **Foundational (Phase 2)**: Setup 완료 후 - 모든 User Story를 블로킹
- **User Stories (Phase 3-10)**: Foundational 완료 후
  - 병렬 진행 가능 (인력이 있는 경우)
  - 우선순위 순서 진행 (P1 → P2 → P3)
- **Polish (Phase 11)**: 원하는 User Story들이 모두 완료된 후

### User Story Dependencies

- **User Story 1 (P1)**: Foundational 완료 후 시작 가능 - 다른 Story 의존성 없음
- **User Story 2 (P1)**: Foundational 완료 후 시작 가능 - US1과 통합되지만 독립 테스트 가능
- **User Story 3 (P2)**: Foundational 완료 후 시작 가능 - US1/US2와 독립적
- **User Story 4 (P2)**: Foundational 완료 후 시작 가능 - US3과 병렬 진행 가능
- **User Story 5 (P3)**: Foundational 완료 후 시작 가능 - 모든 서비스 메트릭 수집
- **User Story 6 (P3)**: Foundational 완료 후 시작 가능 - US5와 병렬 진행 가능
- **User Story 7 (P3)**: Foundational 완료 후 시작 가능 - US5/US6과 병렬 진행 가능
- **User Story 8 (P3)**: Foundational 완료 후 시작 가능 - US4 완료 후 최적화 효과 극대화

### Within Each User Story

- Tests MUST be written and FAIL before implementation
- 인터페이스 정의 → 구현 클래스 → 통합 코드 순서
- 서비스 파일 작성 → 프로세스 통합 → 테스트 검증 순서
- Core implementation before integration
- Story complete before moving to next priority

### Parallel Opportunities

- Setup 단계의 모든 [P] 태스크는 병렬 실행 가능
- Foundational 단계의 모든 [P] 태스크는 병렬 실행 가능 (Phase 2 내)
- Foundational 완료 후 모든 User Story는 병렬 시작 가능 (팀 인력 허용 시)
- 각 User Story 내 [P] 태스크는 병렬 실행 가능
- 서로 다른 User Story는 별도 팀원이 병렬 작업 가능

---

## Parallel Example: User Story 2

```bash
# 모든 테스트를 병렬로 작성 (User Story 2):
Task: "Watchdog 알림 전송 단위 테스트 in tests/unit/systemd/watchdog_notifier_test.cpp"
Task: "Watchdog 타이머 단위 테스트 in tests/unit/systemd/watchdog_timer_test.cpp"
Task: "Watchdog 타임아웃 통합 테스트 in tests/integration/systemd/watchdog_timeout_test.cpp"

# 인터페이스와 유틸리티 클래스를 병렬로 작성:
Task: "IWatchdogNotifier 인터페이스 정의 in src/core/systemd/interfaces/IWatchdogNotifier.h"
Task: "WatchdogTimer 클래스 정의 in src/core/systemd/util/WatchdogTimer.h"
Task: "SdNotifyWatchdog 클래스 정의 in src/core/systemd/impl/SdNotifyWatchdog.h"
```

---

## Implementation Strategy

### MVP First (User Story 1 + 2 Only)

1. Phase 1: Setup 완료
2. Phase 2: Foundational 완료 (CRITICAL - 모든 Story 블로킹)
3. Phase 3: User Story 1 완료 (RT 성능 보장)
4. Phase 4: User Story 2 완료 (Watchdog 자동 복구)
5. **STOP and VALIDATE**: US1 + US2를 독립적으로 테스트
6. MVP 배포/데모

### Incremental Delivery

1. Setup + Foundational → Foundation ready
2. User Story 1 추가 → 독립 테스트 → 배포/데모 (MVP - RT 성능!)
3. User Story 2 추가 → 독립 테스트 → 배포/데모 (MVP + 자동 복구!)
4. User Story 3 추가 → 독립 테스트 → 배포/데모 (리소스 격리!)
5. User Story 4 추가 → 독립 테스트 → 배포/데모 (의존성 관리!)
6. User Story 5-8 순차 추가 → 각각 독립 테스트 → 배포/데모
7. 각 Story는 이전 Story를 깨뜨리지 않고 가치를 추가

### Parallel Team Strategy

여러 개발자가 있는 경우:

1. 팀 전체가 Setup + Foundational을 함께 완료
2. Foundational 완료 후:
   - Developer A: User Story 1 (RT 성능)
   - Developer B: User Story 2 (Watchdog)
   - Developer C: User Story 3 (리소스 격리)
   - Developer D: User Story 4 (의존성 관리)
3. P1 완료 후 P2, P2 완료 후 P3로 진행
4. 각 Story는 독립적으로 완성되고 통합됨

---

## Notes

- [P] 태스크 = 서로 다른 파일, 의존성 없음
- [Story] 레이블로 태스크와 User Story 매핑
- 각 User Story는 독립적으로 완성 및 테스트 가능
- 테스트는 구현 전에 작성하고 실패 확인
- 각 태스크 또는 논리적 그룹 후 커밋
- Checkpoint에서 멈춰 Story 독립 검증 가능
- 피할 것: 모호한 태스크, 동일 파일 충돌, Story 독립성을 깨는 교차 의존성

---

## Key Technical Decisions

### systemd 통합 방식
- **libsystemd API 직접 사용**: `sd_notify()`, `sd_journal_send()` 등을 C++에서 직접 호출
- **systemctl show 파싱**: Prometheus 메트릭 수집 시 systemctl show 출력을 파싱하여 사용
- **이유**: systemd D-Bus API보다 간단하고 직접적이며, RT 성능 영향 최소화

### Watchdog 구현 방식
- **주기적 타이머 기반**: WatchdogTimer 클래스가 주기적으로 sd_notify("WATCHDOG=1") 전송
- **타임아웃 값**: 30초 기본값, 설정 파일로 조정 가능
- **이유**: systemd가 권장하는 방식이며, 프로세스 메인 루프와 독립적으로 동작

### 메트릭 수집 방식
- **systemctl show + 파싱**: systemd 내부 메트릭을 외부에서 수집
- **별도 프로세스 (mxrc-monitor)**: 메트릭 수집 및 HTTP 서버를 독립 프로세스로 분리
- **이유**: RT 프로세스에 HTTP 서버 오버헤드를 추가하지 않기 위함

### 로깅 통합 방식
- **spdlog custom sink**: journald와 통합하는 spdlog sink 구현
- **ECS 필드 매핑**: ECS 스키마를 journald JSON 형식으로 변환
- **이유**: 기존 spdlog 코드를 수정하지 않고 journald 통합 가능

### 보안 강화 수준
- **ProtectSystem=strict**: 시스템 디렉토리 읽기 전용
- **SystemCallFilter=@system-service**: 일반 서비스용 syscall만 허용
- **이유**: 보안 점수 8.0 이상 달성하면서도 RT 기능 유지

### 부팅 최적화 전략
- **DefaultDependencies=no**: 불필요한 의존성 제거
- **명시적 의존성만 설정**: 필수 타겟만 After로 지정
- **이유**: 부팅 시간 30% 단축 목표 달성
