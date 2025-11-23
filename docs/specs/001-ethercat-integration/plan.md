# Implementation Plan: EtherCAT 센서/모터 데이터 수신 인프라

**Branch**: `001-ethercat-integration` | **Date**: 2025-11-23 | **Spec**: [spec.md](./spec.md)
**Status**: Ready for Implementation
**Progress**: Phase 0 ✅ (Research) → Phase 1 ✅ (Design) → Phase 2 ✅ (Tasks)
**Last Updated**: 2025-11-23
**Input**: Feature specification from `/docs/specs/001-ethercat-integration/spec.md`

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

**주요 요구사항**: RT Executive가 EtherCAT을 통해 10ms minor cycle마다 센서 데이터를 읽고 모터 명령을 전송하여 실시간 제어 루프를 구성합니다.

**기술 접근**:
- **EtherCAT Master**: IgH EtherCAT Master (kernel-space, PREEMPT_RT 호환)
- **설정 관리**: YAML 파일 기반 slave configuration (yaml-cpp 라이브러리)
- **PDO 매핑**: 정적 PDO 매핑 우선 (예측 가능성 및 성능)
- **데이터 통합**: RTDataStore를 통한 RT/Non-RT 데이터 공유
- **동기화**: Distributed Clock (DC)은 P3 우선순위로 구현 (선택적 활성화)

**성능 목표**:
- RT cycle latency < 8ms (10ms cycle 내)
- EtherCAT 통신 에러율 < 0.1%
- 초기화 시간 < 5초
- 24시간 연속 안정적 동작

## Technical Context

**Language/Version**: C++20 (GCC 11+)
**Primary Dependencies**:
- IgH EtherCAT Master 1.5.2+ (kernel module)
- yaml-cpp 0.7.0+ (YAML 파싱)
- spdlog 1.x (비동기 로깅)
- GoogleTest (단위/통합 테스트)
- Existing: RTDataStore, RTExecutive, EventBus

**Storage**: RTDataStore (versioned in-memory key-value), SharedMemory (RT/Non-RT 통신)
**Testing**: GoogleTest framework, EtherCAT 시뮬레이터 (Virtual EtherCAT Master)
**Target Platform**: Linux Ubuntu 24.04 LTS with PREEMPT_RT kernel
**Project Type**: Real-time embedded system (C++ single project)
**Performance Goals**:
- RT cycle: 10ms (latency < 8ms)
- EtherCAT frame rate: 100 Hz (10ms cycle)
- Throughput: 센서 데이터 읽기/모터 명령 전송 > 99.9% 성공률
- DC synchronization: ±1μs clock offset (P3)

**Constraints**:
- Real-time: PREEMPT_RT, SCHED_FIFO, memory lock (mlockall)
- Memory allocation: No dynamic allocation in RT path
- Latency: < 8ms 센서 읽기 + 제어 + 모터 쓰기
- Jitter: < 100μs (Free-Run mode), < 10μs (DC mode, P3)

**Scale/Scope**:
- Slave devices: 최대 64개 EtherCAT slaves
- Data rate: ~1KB/cycle PDO data (센서 + 모터)
- Configuration: ~10KB YAML 설정 파일
- Codebase: ~5K LOC (EtherCAT 통합 부분만)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### ✅ I. 계층적 아키텍처 원칙
- EtherCAT 통합은 기존 3계층 아키텍처를 변경하지 않음
- RT Executive 내에서 cyclic action으로 동작
- Action/Sequence/Task 계층과 독립적으로 센서/모터 데이터 제공

### ✅ II. 인터페이스 기반 설계
- `IEtherCATMaster`: EtherCAT master 추상화 인터페이스
- `ISensorData`, `IMotorCommand`: 센서/모터 데이터 인터페이스
- `ISlaveConfig`: Slave 설정 인터페이스
- 의존성 주입을 통한 테스트 가능성 확보

### ✅ III. RAII 원칙 (NON-NEGOTIABLE)
- EtherCAT master/domain/slave 리소스는 RAII wrapper 클래스로 관리
- `std::unique_ptr<EtherCATMaster>` 사용
- 소멸자에서 `ecrt_master_deactivate()`, `ecrt_release_master()` 호출
- 메모리 누수 없음 보장

### ✅ IV. 메모리 안전성 (NON-NEGOTIABLE)
- AddressSanitizer 활성화 (`-fsanitize=address`)
- 모든 테스트에서 메모리 누수 자동 감지
- RT path에서 dynamic allocation 금지 (pre-allocated buffers 사용)

### ✅ V. 테스트 주도 개발 (TDD)
- 단위 테스트: EtherCAT master wrapper, PDO mapping, YAML parser
- 통합 테스트: Virtual EtherCAT master를 사용한 end-to-end 테스트
- 시뮬레이터: SOEM 기반 EtherCAT slave 시뮬레이터

### ✅ VI. 실시간 성능
- RT cycle latency < 8ms (목표)
- EtherCAT PDO 읽기/쓰기: < 1ms
- No dynamic allocation in RT path
- SCHED_FIFO, CPU isolation, memory locking 적용

### ✅ VII. 문서화 및 한글 사용
- 모든 plan, tasks, architecture 문서는 한글 작성
- 기술 용어(EtherCAT, PDO, DC, IgH 등)는 영어 표기
- 코드 주석은 영어 사용

**결과**: 모든 constitution 원칙 준수. 구현 진행 가능.

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

```text
src/core/ethercat/
├── interfaces/               # 인터페이스 정의 (I-prefix)
│   ├── IEtherCATMaster.h
│   ├── ISensorData.h
│   ├── IMotorCommand.h
│   └── ISlaveConfig.h
├── core/                     # 핵심 구현체
│   ├── EtherCATMaster.h
│   ├── EtherCATMaster.cpp
│   ├── EtherCATDomain.h
│   ├── EtherCATDomain.cpp
│   ├── SensorDataManager.h
│   ├── SensorDataManager.cpp
│   ├── MotorCommandManager.h
│   └── MotorCommandManager.cpp
├── dto/                      # Data Transfer Objects
│   ├── SensorData.h
│   ├── MotorCommand.h
│   ├── SlaveConfig.h
│   └── PDOMapping.h
├── util/                     # 유틸리티
│   ├── YAMLConfigParser.h
│   ├── YAMLConfigParser.cpp
│   ├── EtherCATLogger.h
│   └── PDOHelper.h
└── adapters/                 # RT Executive 통합
    ├── RTEtherCATCycle.h
    └── RTEtherCATCycle.cpp

config/
└── ethercat/                 # YAML 설정 파일
    ├── slaves.yaml           # Slave 구성 정의
    ├── pdo_mappings.yaml     # PDO 매핑
    └── dc_config.yaml        # DC 동기화 설정 (P3)

tests/
├── unit/ethercat/
│   ├── EtherCATMaster_test.cpp
│   ├── YAMLConfigParser_test.cpp
│   ├── PDOMapping_test.cpp
│   └── SensorDataManager_test.cpp
└── integration/ethercat/
    ├── ethercat_cycle_test.cpp
    ├── rt_datastore_integration_test.cpp
    └── virtual_slave_test.cpp  # 시뮬레이터 사용
```

**Structure Decision**:

MXRC의 기존 `src/core/<layer>/` 구조를 따라 `src/core/ethercat/` 디렉토리를 생성합니다. EtherCAT은 RT Executive의 확장 기능이므로 별도 계층이 아닌 `core` 하위에 위치합니다.

- **interfaces/**: I-prefix 인터페이스로 테스트 가능성 확보
- **core/**: IgH EtherCAT Master API를 감싸는 RAII wrapper 클래스
- **dto/**: 센서/모터 데이터 구조체 (타입 안전성)
- **util/**: YAML 파싱, PDO helper 등
- **adapters/**: RT Executive에 등록할 cyclic action
- **config/**: YAML 설정 파일 (코드와 분리)

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| [e.g., 4th project] | [current need] | [why 3 projects insufficient] |
| [e.g., Repository pattern] | [specific problem] | [why direct DB access insufficient] |
