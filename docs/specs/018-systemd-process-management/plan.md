# Implementation Plan: systemd 기반 프로세스 관리 고도화

**Branch**: `018-systemd-process-management` | **Date**: 2025-01-21 | **Spec**: [spec.md](spec.md)
**Status**: Planning
**Progress**: Phase 0 (Research) → Phase 1 (Design) → Phase 2 (Tasks)
**Last Updated**: 2025-01-21
**Input**: Feature specification from `docs/specs/018-systemd-process-management/spec.md`

**Note**: This plan is created by the `/plan` command.

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Task, Action, Sequence, API, JSON, CMake 등)
- 일반 설명, 구현 계획, 설계 결정은 모두 한글로 작성합니다

**예시**:
- ✅ 좋은 예: "Task 계층에서 실행 모드를 관리합니다"
- ❌ 나쁜 예: "The Task layer manages execution modes"

---

## Summary

MXRC 시스템의 RT/Non-RT 프로세스를 systemd로 관리하기 위한 고급 기능을 구현합니다. Watchdog 통합, Prometheus 메트릭 노출, journald 구조화 로깅, 보안 강화를 통해 프로덕션 환경에서 안정적이고 모니터링 가능한 로봇 제어 시스템을 구축합니다. libsystemd API를 사용하여 watchdog 알림을 전송하고, systemctl show 출력을 파싱하여 Prometheus 메트릭을 노출하며, journald와 통합하여 중앙 집중식 로그 관리를 제공합니다.

## Technical Context

**Language/Version**: C++20 (GCC 11+ 또는 Clang 14+)
**Primary Dependencies**: libsystemd-dev (sd_notify API), spdlog >= 1.x, nlohmann_json >= 3.11.0, Prometheus C++ client (선택적)
**Storage**: N/A (systemd 메트릭 및 로그는 journald/Prometheus에 저장)
**Testing**: GoogleTest framework
**Target Platform**: Ubuntu 24.04 LTS (PREEMPT_RT 커널), systemd v255+
**Project Type**: Single (MXRC 프로젝트 내 systemd 통합 모듈)
**Performance Goals**: Watchdog 알림 오버헤드 < 10μs, Prometheus 메트릭 수집 주기 1초, journald 로깅 지연 < 1ms, RT jitter < 50μs 유지
**Constraints**: RT 프로세스 실시간 성능 유지, Watchdog 타임아웃 30초 이내 감지, 보안 점수 8.0/10.0 이상
**Scale/Scope**: 3개 systemd 서비스 (mxrc-rt, mxrc-nonrt, mxrc-monitor), 약 15개 Prometheus 메트릭, 6개 인터페이스 클래스

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

[Gates determined based on constitution file]

## Project Structure

### Documentation (this feature)

```text
docs/specs/[###-feature]/
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (/speckit.plan command)
├── data-model.md        # Phase 1 output (/speckit.plan command)
├── quickstart.md        # Phase 1 output (/speckit.plan command)
├── contracts/           # Phase 1 output (/speckit.plan command)
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)
<!--
  ACTION REQUIRED: Replace the placeholder tree below with the concrete layout
  for this feature. Delete unused options and expand the chosen structure with
  real paths (e.g., apps/admin, packages/something). The delivered plan must
  not include Option labels.
-->

```text
# [REMOVE IF UNUSED] Option 1: Single project (DEFAULT)
src/
├── models/
├── services/
├── cli/
└── lib/

tests/
├── contract/
├── integration/
└── unit/

# [REMOVE IF UNUSED] Option 2: Web application (when "frontend" + "backend" detected)
backend/
├── src/
│   ├── models/
│   ├── services/
│   └── api/
└── tests/

frontend/
├── src/
│   ├── components/
│   ├── pages/
│   └── services/
└── tests/

# [REMOVE IF UNUSED] Option 3: Mobile + API (when "iOS/Android" detected)
api/
└── [same as backend above]

ios/ or android/
└── [platform-specific structure: feature modules, UI flows, platform tests]
```

**Structure Decision**: [Document the selected structure and reference the real
directories captured above]

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| [e.g., 4th project] | [current need] | [why 3 projects insufficient] |
| [e.g., Repository pattern] | [specific problem] | [why direct DB access insufficient] |

### I. 계층적 아키텍처 원칙
✅ **PASS** - systemd 통합은 기존 3계층 구조(Action, Sequence, Task)를 유지하며, 별도의 systemd 모듈로 구현됩니다.

### II. 인터페이스 기반 설계
✅ **PASS** - 모든 systemd 통합 컴포넌트는 인터페이스로 정의됩니다:
- `IWatchdogNotifier`: watchdog 알림 인터페이스
- `ISystemdMetricsCollector`: systemd 메트릭 수집 인터페이스
- `IJournaldLogger`: journald 로깅 인터페이스

### III. RAII 원칙
✅ **PASS** - 모든 리소스는 스마트 포인터로 관리되며, RAII 패턴을 따릅니다.

### IV. 메모리 안전성
✅ **PASS** - AddressSanitizer로 메모리 안전성을 검증하고, 메모리 누수가 없도록 합니다.

### V. 테스트 주도 개발
✅ **PASS** - 단위 테스트 및 통합 테스트를 작성하여 모든 기능을 검증합니다.

### VI. 실시간 성능
✅ **PASS** - Watchdog 알림 오버헤드 < 10μs, RT jitter < 50μs 유지를 목표로 합니다.

### VII. 문서화 및 한글 사용
✅ **PASS** - 모든 문서는 한글로 작성하며, 기술 용어만 영어로 표기합니다.

**결론**: 모든 Constitution 원칙을 준수합니다. 추가 정당화 불필요.

## Project Structure

### Documentation (this feature)

```text
docs/specs/018-systemd-process-management/
├── plan.md              # This file (/plan command output)
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (interface definitions)
│   ├── IWatchdogNotifier.md
│   ├── ISystemdMetricsCollector.md
│   └── IJournaldLogger.md
└── tasks.md             # Phase 2 output (/tasks command - NOT created by /plan)
```

### Source Code (repository root)

```text
# systemd 통합 모듈
src/core/systemd/
├── interfaces/
│   ├── IWatchdogNotifier.h
│   ├── ISystemdMetricsCollector.h
│   └── IJournaldLogger.h
├── impl/
│   ├── SdNotifyWatchdog.h/cpp        # sd_notify() 기반 watchdog
│   ├── SystemdMetricsCollector.h/cpp # systemctl show 파싱
│   └── JournaldLogger.h/cpp          # sd_journal_send 통합
├── util/
│   ├── SystemdUtil.h/cpp             # systemd 유틸리티
│   └── WatchdogTimer.h/cpp           # Watchdog 타이머
└── dto/
    ├── SystemdMetric.h               # 메트릭 DTO
    └── JournaldEntry.h               # 로그 항목 DTO

# systemd 서비스 파일 (배포용)
systemd/
├── mxrc-rt.service                   # RT 프로세스 서비스
├── mxrc-nonrt.service                # Non-RT 프로세스 서비스
├── mxrc-monitor.service              # 모니터링 서비스
└── mxrc.target                       # 전체 서비스 그룹

# 설정 파일
config/systemd/
├── watchdog.json                     # Watchdog 설정
├── metrics.json                      # 메트릭 수집 설정
└── security.json                     # 보안 정책

# 테스트
tests/
├── unit/systemd/
│   ├── watchdog_notifier_test.cpp
│   ├── metrics_collector_test.cpp
│   └── journald_logger_test.cpp
└── integration/systemd/
    ├── systemd_integration_test.cpp
    └── failover_integration_test.cpp
```

**Structure Decision**: 단일 프로젝트 구조를 사용하며, `src/core/systemd/` 디렉토리에 systemd 통합 모듈을 배치합니다. 기존 MXRC 프로젝트의 계층 구조를 유지하고, systemd 관련 컴포넌트만 새로운 네임스페이스 `mxrc::systemd`로 분리합니다.

## Complexity Tracking

**모든 Constitution Check 통과** - 복잡도 정당화 불필요.

