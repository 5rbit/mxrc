# Implementation Plan: MXRC 아키텍처 개선 및 고도화

**Branch**: `019-architecture-improvements` | **Date**: 2025-01-23 | **Spec**: [spec.md](./spec.md)
**Status**: Planning Complete
**Progress**: Phase 0 (Research) ✅ → Phase 1 (Design) → Phase 2 (Tasks)
**Last Updated**: 2025-01-23
**Input**: Feature specification from `/docs/specs/019-architecture-improvements/spec.md`

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

MXRC 시스템의 아키텍처를 개선하여 타입 안전성, 성능, 안정성을 향상시키는 프로젝트입니다. docs/007-* 문서에서 제안된 개선 사항들을 6개의 독립적인 User Story로 구조화하여 구현합니다.

**핵심 개선 영역**:
1. IPC 계약 명시화 (YAML 스키마 + 코드 자동 생성)
2. DataStore Hot Key 성능 최적화 (Folly AtomicHashMap)
3. EventBus 우선순위 및 정책 강화 (Priority Queue + TTL + Backpressure)
4. 필드버스 추상화 계층 (IFieldbus 인터페이스)
5. Monitoring 강화 (Prometheus + Grafana)
6. 고가용성 고도화 (HA State Machine)

**기술적 접근**: 기존 DataStore, EventBus, EtherCAT 모듈을 유지하면서 점진적으로 개선합니다. Constitution 원칙(RAII, 메모리 안전성, RT 성능)을 준수하며 각 User Story를 독립적으로 테스트 가능하도록 설계합니다.

## Technical Context

**Language/Version**: C++20 (기존 MXRC 표준)
**Primary Dependencies**:
- **yaml-cpp >= 0.7.0**: C++ 런타임 설정 파일 로딩 (HA 정책, Hot Key 설정)
- **PyYAML (Python)**: 빌드 타임 스키마 파싱
- **Jinja2 (Python)**: 코드 생성 템플릿 엔진
- **Folly >= 2023.x**: Lock-free AtomicHashMap
- **prometheus-cpp >= 1.1.0**: 메트릭 수집
- **CivetWeb >= 1.15**: 내장 HTTP 서버
- spdlog >= 1.x (기존)
- GoogleTest (기존)
- TBB (기존)

**Storage**: 공유 메모리 (기존 DataStore), 파일 시스템 (스키마 YAML, 설정 파일)
**Testing**: GoogleTest + 벤치마크 테스트 (Hot Key 성능, Priority Queue 처리량)
**Target Platform**: Ubuntu 24.04 LTS (PREEMPT_RT 권장)
**Project Type**: 단일 프로젝트 (듀얼 프로세스 아키텍처)

**Performance Goals**:
- Hot Key 읽기 < 60ns (평균), < 100ns (99 percentile)
- Hot Key 쓰기 < 110ns (평균), < 150ns (99 percentile)
- EventBus 우선순위 이벤트 처리 지연 50% 개선
- Monitoring 오버헤드 < 1% (RT 프로세스)
- HA 복구 시간 < 10초

**Constraints**:
- RT 프로세스 성능 영향 최소화 (메트릭 수집은 비동기)
- 메모리 증가 < 10MB (Hot Key 캐시)
- 기존 코드와의 하위 호환성 유지
- 안전 우선 원칙 (데이터 손실 방지 > 성능)

**Scale/Scope**:
- IPC Schema: 50+ DataStore 키, 30+ EventBus 이벤트 타입
- Hot Keys: 최대 32개 (현재 14개 사용, 18개 여유)
  - 64축 모터 지원 (위치, 속도, 토크: array<double, 64>)
  - 64개 IO 모듈 지원 (디지털 IO: array<uint64_t, 64>, 아날로그 IO: array<double, 64>)
  - Hot Key 총 메모리: ~17 KB (목표 10MB 이내 충족)
- Monitoring Metrics: 20+ 지표
- HA States: 6개 주요 상태 (NORMAL, DEGRADED, SAFE_MODE, RECOVERY_IN_PROGRESS, MANUAL_INTERVENTION, SHUTDOWN)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Phase 0 Check (Before Research) ✅

| Principle | Compliance | Notes |
|-----------|------------|-------|
| I. 계층적 아키텍처 | ✅ PASS | 계층 구조 유지 (IPC 개선은 인프라 레벨) |
| II. 인터페이스 기반 설계 | ✅ PASS | IFieldbus 인터페이스 도입 |
| III. RAII 원칙 | ✅ PASS | unique_ptr, shared_ptr 사용 |
| IV. 메모리 안전성 | ✅ PASS | AddressSanitizer 활성화 유지 |
| V. 테스트 주도 개발 | ✅ PASS | 각 User Story별 독립 테스트 |
| VI. 실시간 성능 | ✅ PASS | RT 성능 목표 명시(<60ns/<110ns) |
| VII. 문서화 및 한글 사용 | ✅ PASS | 모든 문서 한글 작성 |

**Gate Result**: ✅ PASS - 모든 원칙 준수

### Phase 1 Check (After Design) - Pending

Phase 1 설계 완료 후 재평가 예정

## Project Structure

### Documentation (this feature)

```text
docs/specs/019-architecture-improvements/
├── spec.md              # Feature specification
├── plan.md              # This file (implementation plan)
├── research.md          # Phase 0 research (completed)
├── data-model.md        # Phase 1 output (pending)
├── quickstart.md        # Phase 1 output (pending)
├── contracts/           # Phase 1 output (pending)
│   ├── ipc-schema.yaml  # IPC 스키마 정의
│   └── ha-policy.yaml   # HA 정책 설정
└── tasks.md             # Phase 2 output (/speckit.tasks)
```

### Source Code (repository root)

```text
# IPC Schema & Code Generation (US1)
scripts/codegen/
├── generate_ipc_schema.py        # 스키마 → C++ 코드 생성 스크립트
├── templates/
│   ├── datastore_keys.h.j2       # DataStore 키 템플릿
│   ├── eventbus_events.h.j2      # EventBus 이벤트 템플릿
│   └── accessor_impl.cpp.j2      # Accessor 구현 템플릿
└── schemas/
    ├── datastore_schema.yaml     # DataStore 키 정의
    └── eventbus_schema.yaml      # EventBus 이벤트 정의

# DataStore Hot Key Optimization (US2)
src/core/datastore/
├── hotkey/
│   ├── HotKeyCache.h             # Folly AtomicHashMap 래퍼
│   ├── HotKeyCache.cpp
│   └── HotKeyConfig.h            # Hot Key 설정
└── DataStore.{h,cpp}             # Hot Key 통합

# EventBus Priority & Policies (US3)
src/core/event/
├── priority/
│   ├── PriorityQueue.h           # 우선순위 큐
│   ├── PriorityQueue.cpp
│   ├── PrioritizedEvent.h        # 우선순위 + TTL 이벤트
│   └── BackpressurePolicy.h     # Backpressure 정책
└── EventBus.{h,cpp}              # Priority Queue 통합

# Fieldbus Abstraction (US4)
src/core/fieldbus/
├── interfaces/
│   └── IFieldbus.h               # 공통 필드버스 인터페이스
├── factory/
│   └── FieldbusFactory.{h,cpp}   # 드라이버 팩토리
└── drivers/
    ├── EtherCATDriver.{h,cpp}    # EtherCAT 구현 (리팩토링)
    └── MockDriver.{h,cpp}        # 테스트용 Mock

# Monitoring & Observability (US5)
src/core/monitoring/
├── MetricsCollector.{h,cpp}      # Prometheus 메트릭 수집
├── PrometheusExporter.{h,cpp}    # Prometheus HTTP 엔드포인트
└── metrics/
    ├── RTMetrics.h               # RT 프로세스 메트릭
    └── NonRTMetrics.h            # Non-RT 프로세스 메트릭

config/grafana/
├── dashboards/
│   └── mxrc_overview.json        # Grafana 대시보드 템플릿
└── alerting/
    └── rules.yaml                # Prometheus AlertManager 규칙

# High Availability (US6)
src/core/ha/
├── HAStateMachine.{h,cpp}        # HA 상태 머신
├── RecoveryPolicy.{h,cpp}        # 복구 정책
└── config/
    └── ha_policy.yaml            # HA 정책 설정

# Tests
tests/
├── unit/
│   ├── datastore/
│   │   └── HotKeyCache_test.cpp
│   ├── event/
│   │   ├── PriorityQueue_test.cpp
│   │   └── Backpressure_test.cpp
│   ├── fieldbus/
│   │   └── FieldbusFactory_test.cpp
│   ├── monitoring/
│   │   └── MetricsCollector_test.cpp
│   └── ha/
│       └── HAStateMachine_test.cpp
├── integration/
│   ├── ipc_schema_integration_test.cpp
│   ├── hotkey_performance_test.cpp
│   ├── eventbus_priority_test.cpp
│   ├── fieldbus_abstraction_test.cpp
│   ├── monitoring_e2e_test.cpp
│   └── ha_recovery_test.cpp
└── benchmark/
    ├── hotkey_benchmark.cpp      # <60ns/<110ns 검증
    └── priority_queue_benchmark.cpp
```

**Structure Decision**:
MXRC는 단일 프로젝트 구조(듀얼 프로세스 아키텍처)를 유지합니다. 새로운 기능들은 기존 `src/core/` 디렉토리 하위에 모듈별로 구성하며, 각 User Story별로 독립적인 디렉토리를 생성하여 책임을 명확히 분리합니다.

IPC 스키마 및 코드 생성 도구는 `scripts/codegen/`에 배치하여 빌드 시스템과 분리합니다. Grafana 대시보드 및 AlertManager 규칙은 `config/grafana/`에 배치하여 인프라 설정과 코드를 구분합니다.

테스트는 기존 구조(`tests/unit`, `tests/integration`)를 따르며, 성능 벤치마크를 위한 `tests/benchmark/` 디렉토리를 추가합니다.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

해당 없음 - 모든 Constitution 원칙을 준수합니다.

## Research Summary

Phase 0 연구 완료 ([research.md](./research.md) 참조)

**주요 기술 결정**:

1. **IPC Schema**: YAML + Jinja2 코드 생성
   - PyYAML (빌드 타임): 스키마 파싱 및 검증
   - yaml-cpp (런타임): 설정 파일 동적 로딩
   - CMake 통합 용이, 가독성 우수

2. **Hot Key Cache**: Folly AtomicHashMap
   - <60ns 읽기, <110ns 쓰기 목표 달성 가능
   - Facebook에서 검증된 안정성

3. **Priority Queue**: std::priority_queue + 4-레벨 우선순위
   - 단순성, 충분한 성능, 표준 라이브러리

4. **Fieldbus Abstraction**: Pure Virtual Interface + Factory Pattern
   - 명확성, 유지보수성, 테스트 용이성

5. **Monitoring**: prometheus-cpp + CivetWeb
   - 공식 지원, 풍부한 메트릭 타입, 업계 표준

6. **HA State Machine**: Enum + Switch + YAML 설정
   - 단순성, 성능, 유연성

## Phase 1: Design Artifacts

### Data Model

[data-model.md](./data-model.md) 참조 - Phase 1에서 생성 예정

**핵심 엔티티**:
- IPC Schema Definition (DataStore 키, EventBus 이벤트)
- Hot Key Entry (Lock-free 캐시 항목)
- Prioritized Event (우선순위, TTL, Coalescing 정책)
- Fieldbus Driver (IFieldbus 구현체)
- Metric (Prometheus 메트릭)
- HA State (상태 머신 노드)

### Contracts

[contracts/](./contracts/) 디렉토리 참조 - Phase 1에서 생성 예정

**스키마 파일**:
- `ipc-schema.yaml`: DataStore 키 및 EventBus 이벤트 정의
- `ha-policy.yaml`: HA 복구 정책 설정

### Quick Start Guide

[quickstart.md](./quickstart.md) 참조 - Phase 1에서 생성 예정

## Implementation Phases

### Phase 0: Research (Completed ✅)

- ✅ IPC 스키마 포맷 조사 (YAML vs JSON vs Protobuf)
- ✅ Lock-Free 자료구조 벤치마크 (Folly vs Boost vs std::atomic)
- ✅ Priority Queue 알고리즘 분석 (Heap vs Skip List vs Calendar Queue)
- ✅ Fieldbus 추상화 패턴 연구 (Pure Virtual vs CRTP vs Type Erasure)
- ✅ Prometheus C++ 라이브러리 평가 (prometheus-cpp vs custom)
- ✅ HA 상태 머신 설계 패턴 조사 (Enum vs State Pattern vs Boost.MSM)

**Output**: [research.md](./research.md) (1,969 lines)

### Phase 1: Design & Contracts (Pending)

**Tasks**:
1. data-model.md 작성 (6개 핵심 엔티티 정의)
2. contracts/ 생성
   - ipc-schema.yaml (DataStore 키 50+, EventBus 이벤트 30+ 정의)
   - ha-policy.yaml (HA 복구 정책 설정)
3. quickstart.md 작성 (5분 내 시작 가이드)
4. Agent context 업데이트 (`dev/agent/CLAUDE.md`)

**Output**: data-model.md, contracts/*, quickstart.md

### Phase 2: Tasks Generation (Not Started)

`/speckit.tasks` 명령으로 생성 예정

**Expected Output**: tasks.md with 40-60 tasks organized by User Story

### Phase 3-8: Implementation (Not Started)

`/speckit.implement` 명령으로 실행 예정

## Success Metrics

| Metric | Target | Verification |
|--------|--------|--------------|
| Hot Key 읽기 성능 | <60ns (평균) | benchmark_hotkey_read |
| Hot Key 쓰기 성능 | <110ns (평균) | benchmark_hotkey_write |
| 우선순위 이벤트 처리 | 50% 빠름 | integration_test_priority |
| TTL 만료 정확도 | 100% | unit_test_ttl_expiration |
| Monitoring 오버헤드 | <1% CPU | profiling_rt_process |
| HA 복구 시간 | <10초 | integration_test_ha_recovery |
| 코드 생성 정확도 | 100% 컴파일 | ipc_schema_codegen_test |
| Fieldbus 추상화 재사용률 | >80% | code_coverage_analysis |

## Risks & Mitigation

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Lock-Free 구현 버그 | HIGH | MEDIUM | 광범위한 단위 테스트, Folly 검증된 라이브러리 사용 |
| RT 성능 저하 | CRITICAL | LOW | 벤치마크 지속 측정, 비동기 메트릭 수집 |
| 필드버스 추상화 오버헤드 | MEDIUM | MEDIUM | 인라인 최적화, 가상 함수 최소화 |
| IPC 스키마 호환성 깨짐 | MEDIUM | LOW | 버전 관리, 하위 호환성 검증 도구 |
| HA 복잡한 시나리오 미처리 | HIGH | MEDIUM | 광범위한 시뮬레이션 테스트, 장애 주입 테스트 |
| Prometheus 통합 복잡도 | LOW | LOW | 공식 라이브러리 사용, 예제 참조 |

## Timeline Estimate

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| Phase 0: Research | ✅ 2 days | None |
| Phase 1: Design | 2 days | Phase 0 |
| Phase 2: Tasks | 0.5 days | Phase 1 |
| Phase 3-8: Implementation | 15-20 days | Phase 2 |
| - US1 (IPC Schema) | 3 days | None |
| - US2 (Hot Key) | 4 days | US1 |
| - US3 (EventBus) | 3 days | US1 |
| - US4 (Fieldbus) | 4 days | None |
| - US5 (Monitoring) | 3 days | US2, US3 |
| - US6 (HA) | 3 days | US5 |

**Total Estimate**: 19-24 days (US1-US2-US3 병렬 가능, US4 독립 실행 가능)

## Next Steps

1. ✅ Phase 0 완료: research.md 생성
2. ⏳ Phase 1 진행:
   - data-model.md 작성
   - contracts/ 생성 (ipc-schema.yaml, ha-policy.yaml)
   - quickstart.md 작성
   - Agent context 업데이트
3. ⏳ Phase 2: `/speckit.tasks` 실행하여 tasks.md 생성
4. ⏳ Phase 3-8: `/speckit.implement` 실행하여 구현

**Immediate Action**: Phase 1 설계 artifacts 생성
