# Implementation Plan: 팔렛 셔틀 제어 시스템

**Branch**: `016-pallet-shuttle-control` | **Date**: 2025-01-25 | **Spec**: [spec.md](./spec.md)
**Status**: Planning
**Progress**: Phase 0 (Research) → Phase 1 (Design) → Phase 2 (Tasks)
**Last Updated**: 2025-01-25
**Input**: Feature specification from `/docs/specs/016-pallet-shuttle-control/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/commands/plan.md` for the execution workflow.

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

MXRC의 기존 3계층 아키텍처(Action → Sequence → Task)를 활용하여 팔렛 셔틀 로봇의 실제 제어 로직을 구현합니다. 이 기능은 팔렛 픽업-운반-배치 작업, Alarm 관리 시스템, 행동 의사 결정(Behavior Arbitration), 작업 큐 관리를 포함합니다. 특히 고수준 제어/작업 관리를 위해 작업 분할 및 시퀀싱, 오류 처리, 우선순위 기반 행동 선택 메커니즘을 제공합니다.

핵심 기술 접근:
- Mock Driver를 통한 시뮬레이션 환경에서 먼저 테스트
- DataStore 및 Event System을 활용한 상태 관리 및 Alarm 처리
- 설정 파일 기반 Alarm 규칙 및 행동 우선순위 관리
- 플러그인 구조로 새로운 Action/Sequence 확장 가능
- Task Suspend/Resume 기능으로 우선순위 행동 지원

## Technical Context

**Language/Version**: C++20 (GCC 11+ 또는 Clang 14+)
**Primary Dependencies**:
- spdlog >= 1.x (비동기 로깅)
- GTest (테스트 프레임워크)
- TBB (concurrent_hash_map, concurrent_queue)
- nlohmann_json >= 3.11.0 (Alarm 설정 파일 처리)

**Storage**:
- RTDataStore (in-memory key-value, 버전 관리 지원) - 로봇 상태, Alarm 정보 저장
- 설정 파일 (JSON/YAML) - Alarm 규칙, 행동 우선순위, Sequence 정의

**Testing**:
- GoogleTest framework
- Mock Driver를 통한 통합 테스트
- AddressSanitizer로 메모리 안전성 검증

**Target Platform**: Ubuntu 24.04 LTS (PREEMPT_RT)

**Project Type**: Single project (로봇 제어 시스템)

**Performance Goals**:
- Critical Alarm 발생 시 100ms 이내 로봇 정지
- Alarm 발생부터 DataStore 기록까지 50ms 이내
- Task 실행 오버헤드 < 1ms (기존 목표 유지)
- 운영자 상태 조회 응답 < 1초

**Constraints**:
- 실시간 제약: Critical 동작(비상 정지)은 RT 스레드에서 처리
- 메모리 안전성: AddressSanitizer로 검증, 메모리 누수 절대 불가
- RAII 원칙 준수: 모든 리소스는 스마트 포인터로 관리
- 기존 Action/Sequence/Task Layer 아키텍처와의 호환성 유지 (단, 요구사항 충족을 위해 대규모 개편 허용)

**Scale/Scope**:
- 단일 팔렛 셔틀 로봇 제어 (다중 로봇은 범위 외)
- 작업 큐: 최소 10개 이상 작업 관리
- Alarm 유형: 초기 10~15종 (동적 추가/수정/제거 지원)
- User Stories: 6개 (P1: 3개, P2: 2개, P3: 1개)
- Functional Requirements: 40개

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Principle I: 계층적 아키텍처 원칙
- ✅ **PASS**: 기존 3계층 구조(Action → Sequence → Task)를 활용
- ⚠️ **NEEDS EVALUATION**: Behavior Arbitration 및 Task Suspend/Resume 기능이 기존 Task Layer 설계로 지원 가능한지 확인 필요
- **Note**: Spec에서 Task, Action, Sequence Layer의 대규모 아키텍처 개편이 허용되었으므로, 요구사항 충족을 위해 필요 시 개편 진행

### Principle II: 인터페이스 기반 설계
- ✅ **PASS**: 모든 새로운 컴포넌트는 I-prefix 인터페이스로 정의
- **New Interfaces**:
  - `IAlarmManager`: Alarm 생성, 조회, 리셋
  - `IAlarmConfiguration`: Alarm 규칙 관리
  - `IBehaviorArbiter`: 행동 의사 결정
  - `ITaskQueue`: 작업 큐 관리

### Principle III: RAII 원칙 (NON-NEGOTIABLE)
- ✅ **PASS**: 모든 리소스는 스마트 포인터로 관리
- **Enforcement**: shared_ptr<IAction>, unique_ptr<Alarm>, shared_ptr<Task> 사용

### Principle IV: 메모리 안전성 (NON-NEGOTIABLE)
- ✅ **PASS**: AddressSanitizer 활성화, 모든 테스트에서 메모리 누수 검증

### Principle V: 테스트 주도 개발 (TDD)
- ✅ **PASS**: 모든 새로운 Action, Sequence, Task, AlarmManager에 대해 단위 테스트 및 통합 테스트 작성
- **Test Coverage**:
  - Alarm 관리: 생성, 심각도 상향, 리셋
  - Behavior Arbitration: 우선순위 선택, 모드 전환
  - Task Suspend/Resume: 중단 및 재개 동작
  - 작업 큐: 우선순위 정렬, 긴급 작업 삽입

### Principle VI: 실시간 성능
- ✅ **PASS**: Critical Alarm 처리 및 비상 정지는 RT 스레드에서 100ms 이내 처리
- **Performance Targets**:
  - Alarm 발생 → DataStore 기록: < 50ms
  - 로봇 상태 조회: < 1초
  - Task 실행 오버헤드: < 1ms (기존 목표 유지)

### Principle VII: 문서화 및 한글 사용
- ✅ **PASS**: 모든 계획, 설계, 문서는 한글로 작성 (기술 용어는 영어)

### 종합 평가
- **Status**: ✅ PASS WITH NOTES
- **Notes**:
  - Behavior Arbitration 및 Task Suspend/Resume 기능이 기존 Task Layer 아키텍처로 지원 가능한지 Phase 0 Research에서 평가 필요
  - 필요 시 Task, Action, Sequence Layer의 아키텍처 개편 진행 (Spec에서 명시적으로 허용)
  - 개편 시 기존 테스트와의 호환성 및 전체 시스템 안정성 확인 필요

## Project Structure

### Documentation (this feature)

```text
docs/specs/016-pallet-shuttle-control/
├── spec.md              # Feature Specification (완료)
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (Research findings)
├── data-model.md        # Phase 1 output (Entity design)
├── quickstart.md        # Phase 1 output (Developer guide)
├── contracts/           # Phase 1 output (API/Interface contracts)
│   ├── IAlarmManager.h
│   ├── IBehaviorArbiter.h
│   ├── ITaskQueue.h
│   └── alarm-config-schema.json
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
src/
├── core/                              # MXRC 핵심 인프라 (범용)
│   ├── action/                        # 범용 Action Layer (기존)
│   │   ├── interfaces/
│   │   │   └── IAction.h
│   │   ├── core/
│   │   │   └── ActionExecutor.{h,cpp}
│   │   └── registry/
│   │       └── ActionRegistry.{h,cpp}
│   ├── sequence/                      # 범용 Sequence Layer (기존)
│   │   ├── interfaces/
│   │   │   └── ISequenceEngine.h
│   │   ├── core/
│   │   │   └── SequenceEngine.{h,cpp}
│   │   └── registry/
│   │       └── SequenceRegistry.{h,cpp}
│   ├── task/                          # 범용 Task Layer (기존)
│   │   ├── interfaces/
│   │   │   └── ITaskExecutor.h
│   │   ├── core/
│   │   │   └── TaskExecutor.{h,cpp}
│   │   └── registry/
│   │       └── TaskRegistry.{h,cpp}
│   ├── control/                       # 범용 제어 인터페이스 (새로 추가)
│   │   ├── interfaces/
│   │   │   ├── IRobotController.h        # 최상위 로봇 컨트롤러 인터페이스
│   │   │   ├── IBehaviorArbiter.h        # 행동 의사 결정 인터페이스
│   │   │   └── ITaskQueue.h              # 작업 큐 인터페이스
│   │   └── impl/
│   │       ├── BehaviorArbiter.{h,cpp}   # 범용 구현 (기본 알고리즘)
│   │       └── TaskQueue.{h,cpp}         # 범용 구현
│   ├── alarm/                         # 범용 Alarm System (새로 추가)
│   │   ├── interfaces/
│   │   │   ├── IAlarmManager.h
│   │   │   └── IAlarmConfiguration.h
│   │   ├── impl/
│   │   │   ├── AlarmManager.{h,cpp}
│   │   │   ├── AlarmConfiguration.{h,cpp}
│   │   │   └── Alarm.{h,cpp}
│   │   └── dto/
│   │       └── AlarmDto.h
│   ├── event/                         # Event System (기존)
│   ├── datastore/                     # DataStore (기존)
│   └── fieldbus/                      # Fieldbus drivers (기존)
│
└── robot/                             # 로봇별 구현 (도메인 특화)
    └── pallet_shuttle/                # 팔렛 셔틀 로봇
        ├── control/                   # 팔렛 셔틀 제어 구현
        │   └── PalletShuttleController.{h,cpp}  # IRobotController 구현체
        ├── actions/                   # 팔렛 셔틀용 Action 구현
        │   ├── MoveToPositionAction.{h,cpp}
        │   ├── PickPalletAction.{h,cpp}
        │   └── PlacePalletAction.{h,cpp}
        ├── sequences/                 # 팔렛 셔틀용 Sequence 구현
        │   ├── PalletTransportSequence.{h,cpp}
        │   ├── SafetyCheckSequence.{h,cpp}
        │   └── RecoverySequence.{h,cpp}
        ├── tasks/                     # 팔렛 셔틀용 Task 구현
        │   └── PalletTransportTask.{h,cpp}
        ├── state/                     # 로봇 상태 관리
        │   └── PalletShuttleState.{h,cpp}
        └── config/                    # 팔렛 셔틀 설정
            └── PalletShuttleConfig.{h,cpp}

tests/
├── unit/
│   ├── core/
│   │   ├── control/                   # 범용 제어 로직 테스트
│   │   │   ├── BehaviorArbiterTest.cpp
│   │   │   └── TaskQueueTest.cpp
│   │   └── alarm/                     # Alarm System 테스트
│   │       ├── AlarmManagerTest.cpp
│   │       ├── AlarmConfigurationTest.cpp
│   │       └── AlarmTest.cpp
│   └── robot/
│       └── pallet_shuttle/
│           ├── control/               # 팔렛 셔틀 컨트롤러 테스트
│           │   └── PalletShuttleControllerTest.cpp
│           ├── actions/
│           │   ├── MoveToPositionActionTest.cpp
│           │   ├── PickPalletActionTest.cpp
│           │   └── PlacePalletActionTest.cpp
│           ├── sequences/
│           │   ├── PalletTransportSequenceTest.cpp
│           │   ├── SafetyCheckSequenceTest.cpp
│           │   └── RecoverySequenceTest.cpp
│           └── tasks/
│               └── PalletTransportTaskTest.cpp
└── integration/
    └── robot/
        └── pallet_shuttle/
            ├── basic_transport_test.cpp
            ├── alarm_handling_test.cpp
            ├── behavior_arbitration_test.cpp
            └── end_to_end_test.cpp
```

**Structure Decision**:

이 프로젝트는 **계층적 Single project 구조**를 사용합니다:

1. **`src/core/`**: MXRC 핵심 인프라 (범용, 모든 로봇에서 재사용 가능)
   - Action/Sequence/Task Layer (기존)
   - Alarm System (새로 추가, 범용)
   - TaskQueue, BehaviorArbiter (새로 추가, 범용)

2. **`src/robot/pallet_shuttle/`**: 팔렛 셔틀 로봇 전용 구현 (도메인 특화)
   - 팔렛 셔틀용 Action, Sequence, Task 구현
   - 고수준 제어 로직 (PalletShuttleController)
   - 로봇별 상태 및 설정 관리

**설계 원칙**:
- **인터페이스 vs 구현 분리**:
  - 범용 인터페이스(`IRobotController`, `IBehaviorArbiter`, `ITaskQueue`)는 `core/control/interfaces/`
  - 범용 기본 구현은 `core/control/impl/`
  - 로봇별 커스터마이징은 `robot/pallet_shuttle/control/`
- **확장성**: 향후 다른 로봇(`robot/agv/`, `robot/robotic_arm/`) 추가 시 동일한 인터페이스 구현
- **응집도**: 팔렛 셔틀 관련 모든 코드가 `robot/pallet_shuttle/` 아래에 모여 있어 이해 및 유지보수 용이
- **의존성 역전(DIP)**: 상위 모듈이 하위 구현에 의존하지 않고 인터페이스에 의존
- **MXRC 철학 부합**: "어떤 로봇도 제어할 수 있는 범용 컨트롤러"

핵심 디렉토리:
- `src/core/control/`: 범용 제어 인터페이스 및 기본 구현
- `src/core/alarm/`: 범용 Alarm 관리 시스템
- `src/robot/pallet_shuttle/`: 팔렛 셔틀 로봇 전용 구현 전체

**구현 예시**:
```cpp
// src/core/control/interfaces/IRobotController.h
class IRobotController {
public:
    virtual ~IRobotController() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void submitTask(std::shared_ptr<Task> task) = 0;
};

// src/robot/pallet_shuttle/control/PalletShuttleController.h
class PalletShuttleController : public IRobotController {
public:
    void start() override;
    void stop() override;
    void submitTask(std::shared_ptr<Task> task) override;
private:
    std::shared_ptr<IBehaviorArbiter> arbiter_;  // core의 인터페이스 사용
    std::shared_ptr<ITaskQueue> queue_;          // core의 인터페이스 사용
};
```

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

해당 없음 - 모든 Constitution 원칙을 준수합니다. Task Layer 아키텍처 개편이 필요할 경우 Spec에서 명시적으로 허용되었습니다.

---

## Phase 0: Research & Clarifications

### Research Tasks

Phase 0에서는 다음 항목들에 대한 조사를 수행하여 NEEDS CLARIFICATION을 해결합니다:

#### 1. Task Layer 아키텍처 평가 (CRITICAL)
**Question**: 기존 Task Layer가 Behavior Arbitration 및 Suspend/Resume 기능을 효율적으로 지원할 수 있는가?

**Research Items**:
- 현재 ITaskExecutor 인터페이스 분석 (Suspend/Resume API 존재 여부)
- 기존 TaskExecutor 구현에서 작업 중단 및 재개 메커니즘 확인
- 여러 Task 간 우선순위 전환 시나리오 평가
- TBB concurrent_queue를 활용한 우선순위 큐 구현 가능성

**Expected Output**:
- 기존 Task Layer 활용 가능 여부 결정
- 필요 시 아키텍처 개편 범위 정의 (Task Layer만 or Action/Sequence 포함)
- Suspend/Resume API 설계 제안

#### 2. Alarm System 설계 패턴
**Question**: Alarm 관리를 위한 최적의 설계 패턴은 무엇인가?

**Research Items**:
- Event System과의 통합 방식 (pub/sub vs direct call)
- Alarm 재발 빈도 추적을 위한 데이터 구조 (sliding window? circular buffer?)
- DataStore에 Alarm 정보 저장 시 키 구조 (namespace/alarm_id?)
- Alarm 심각도 자동 상향 조정 알고리즘

**Expected Output**:
- AlarmManager 설계 결정
- Alarm 이력 관리 전략
- Event System 통합 방식

#### 3. Behavior Arbitration 알고리즘
**Question**: 우선순위 기반 행동 선택을 위한 구체적인 알고리즘은?

**Research Items**:
- Priority Queue 구현: TBB concurrent_priority_queue vs custom implementation
- 동일 우선순위 행동의 부가 규칙 처리 (FIFO? distance-based?)
- 제어 모드 전환(Boot/Init/Standby/Manual/Ready/Auto/Fault/Maint/Charging) 시 상태 머신 설계
- 행동 전환 시 현재 작업 처리 전략 (즉시 중단 vs 안전하게 완료)

**Expected Output**:
- BehaviorArbiter 알고리즘 설계
- 우선순위 규칙 정의
- 상태 전환 다이어그램

#### 4. Mock Driver 통합
**Question**: Mock Driver를 통한 팔렛 셔틀 시뮬레이션 방식은?

**Research Items**:
- 기존 Mock Driver 인터페이스 분석
- 팔렛 픽업/배치 동작 시뮬레이션 방법 (상태 변경? 지연 시간?)
- 센서 트리거 및 오류 상황 주입 방법
- Mock Driver를 통한 통합 테스트 시나리오

**Expected Output**:
- Mock Driver 확장 계획
- 시뮬레이션 테스트 전략

#### 5. 설정 파일 구조
**Question**: Alarm 규칙 및 Sequence 정의를 위한 설정 파일 형식은?

**Research Items**:
- JSON vs YAML 선택 기준
- Alarm 설정 스키마 설계 (심각도, 임계값, 대응 Action/Sequence)
- Sequence 설정 기반 정의 가능성 (코드 재컴파일 없이 추가)
- 설정 파일 변경 시 런타임 리로드 지원 여부

**Expected Output**:
- 설정 파일 스키마 정의
- 런타임 설정 변경 메커니즘

### Research Output Location

모든 조사 결과는 `docs/specs/016-pallet-shuttle-control/research.md`에 통합됩니다.

---

## Phase 1: Design Artifacts

Phase 1에서는 Phase 0의 연구 결과를 바탕으로 다음 설계 문서를 생성합니다:

### 1. Data Model (`data-model.md`)

**Key Entities** (from spec.md):
- PalletTransportTask
- PalletTransportSequence
- Alarm
- RobotState
- TaskQueue
- SafetyCheckSequence
- AlarmConfiguration
- BehaviorArbiter
- RecoverySequence

각 엔티티에 대해:
- 클래스 정의 (인터페이스 + 구현체)
- 속성 및 메서드
- 상태 전이 다이어그램 (해당 시)
- 의존성 관계

### 2. API Contracts (`contracts/`)

**Interfaces to Define**:
- `IAlarmManager.h`: Alarm 생성, 조회, 리셋, 심각도 상향 조정
- `IAlarmConfiguration.h`: Alarm 규칙 추가, 수정, 제거, Export/Import
- `IBehaviorArbiter.h`: 행동 선택, 우선순위 평가, 모드 전환
- `ITaskQueue.h`: 작업 추가, 우선순위 정렬, 긴급 작업 삽입
- `IRobotState.h`: 상태 조회, 업데이트

**Configuration Schemas**:
- `alarm-config-schema.json`: Alarm 설정 파일 스키마
- `sequence-config-schema.json`: Sequence 정의 스키마 (선택적)

### 3. Developer Quickstart (`quickstart.md`)

**Contents**:
- 빌드 방법 (CMake 명령어)
- 테스트 실행 방법 (특정 테스트 필터링)
- Mock Driver를 통한 시뮬레이션 실행
- 예제 코드: 팔렛 운반 Task 생성 및 실행
- Alarm 설정 파일 작성 가이드
- 디버깅 팁 (spdlog 레벨 설정, AddressSanitizer 사용)

### 4. Agent Context Update

Phase 1 완료 후:
- `.specify/scripts/bash/update-agent-context.sh claude` 실행
- `dev/agent/CLAUDE.md`에 다음 추가:
  - 활성 기능: 016-pallet-shuttle-control (Status: Planning → In Progress)
  - Active Technologies 섹션에 새로운 컴포넌트 추가 (alarm/, robot/)
  - Recent Changes 섹션 업데이트

---

## Next Steps

Phase 2는 `/speckit.tasks` 커맨드를 통해 별도로 실행됩니다.

Phase 1 완료 후:
1. `research.md`, `data-model.md`, `quickstart.md`, `contracts/` 생성 확인
2. Agent context 업데이트 확인 (`dev/agent/CLAUDE.md`)
3. spec.md Status를 "Planning" → "In Progress"로 업데이트
4. `/speckit.tasks` 실행하여 tasks.md 생성

---

## Notes

- **아키텍처 개편 가능성**: Phase 0 Research 결과에 따라 Task, Action, Sequence Layer의 아키텍처 개편이 필요할 수 있습니다. Spec에서 명시적으로 허용되었으므로, 요구사항 충족을 위해 필요 시 진행합니다.
- **Mock Driver 우선**: 실제 하드웨어 통합 전에 Mock Driver를 통한 시뮬레이션 환경에서 모든 기능을 먼저 테스트합니다.
- **범용 설계**: Alarm 시스템은 향후 다른 로봇 제어 기능에서도 재사용 가능하도록 범용적으로 설계합니다.
- **테스트 우선**: 모든 새로운 컴포넌트는 단위 테스트 및 통합 테스트와 함께 개발합니다.
