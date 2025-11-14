# 구현 계획: TaskManager 모듈 리팩터링

**브랜치**: `016-refactor-taskmanager` | **날짜**: 2025-11-13 | **사양서**: [spec.md](./spec.md)
**입력**: `/specs/016-refactor-taskmanager/spec.md` 의 기능 사양서

**참고**: 이 템플릿은 `/speckit.plan` 명령어로 채워집니다. 실행 워크플로우는 `.specify/templates/commands/plan.md` 를 참조하세요.

## 요약

본 계획은 `TaskManager` 모듈의 아키텍처를 개선하기 위한 리팩터링을 다룹니다. 주요 목표는 `TaskManager`의 과도한 책임을 분리하고, `Task`와 `ITask`의 역할을 명확히 하며, 상태 관리 로직을 캡슐화하여 시스템의 유지보수성, 확장성, 안정성을 향상시키는 것입니다. 이를 위해 `TaskManager`를 `TaskDefinitionRegistry`와 `TaskExecutor`로 분리하고, `Task`가 `ITask`를 직접 구현하도록 변경하며, Command Pattern을 다른 Task 조작에도 확대 적용합니다.

## 기술 컨텍스트

**언어/버전**: C++20, GCC 13.2, CMAKE
**주요 의존성**: RT-Linux, Eigen, spdlog
**저장소**: 내부 메모리 및 저장소
**테스트**: Google Test
**대상 플랫폼**: Linux Ubuntu 24.04 LTS, PREEMPT_RT
**프로젝트 유형**: 메인 컨트롤러 로직
**성능 목표**: 제어 루프 < 1ms 및 시스템 다운율 0%에 수렴
**제약 조건**: 실시간 성능 제약 조건 준수
**규모/범위**: `taskmanager` 모듈 내부 리팩터링

## Constitution 준수 확인

*GATE: 0단계 연구 시작 전에 반드시 통과해야 합니다. 1단계 설계 후 다시 확인합니다.*

- **실시간성 보장**: **[통과]** 제안된 리팩터링은 코드 구조 개선에 중점을 두며, 예측 불가능한 지연을 유발하는 로직을 추가하지 않습니다. 실시간 제약 조건을 준수합니다.
- **신뢰성 및 안전성**: **[통과]** 상태 관리 캡슐화 강화, 역할 분리를 통해 코드의 예측 가능성과 안전성이 향상됩니다. C++20 표준과 모범 사례를 따릅니다.
- **테스트 주도 개발**: **[통과]** 책임이 분리된 클래스(`TaskDefinitionRegistry`, `TaskExecutor`)는 단위 테스트 작성을 용이하게 하여 TDD를 더욱 강력하게 지원합니다.
- **모듈식 설계**: **[통과]** 본 리팩터링의 핵심 목표는 `TaskManager`라는 거대 모듈을 더 작고 응집력 있는 모듈로 분리하는 것으로, 모듈식 설계 원칙을 직접적으로 구현합니다.
- **한글 문서화**: **[통과]** 새로 생성되거나 변경되는 모든 클래스와 인터페이스의 API는 한글로 명확하게 문서화될 것입니다.
- **버전 관리**: **[통과]** 모든 변경 사항은 기능 브랜치(`016-refactor-taskmanager`) 내에서 관리되며, 최종 병합 시 Semantic Versioning을 따를 것입니다.

## 프로젝트 구조

### 문서 (이 기능)

```text
specs/016-refactor-taskmanager/
├── plan.md              # 이 파일 (`/speckit.plan` 명령어 출력)
├── research.md          # 0단계 출력 (`/speckit.plan` 명령어)
├── data-model.md        # 1단계 출력 (`/speckit.plan` 명령어)
├── quickstart.md        # 1단계 출력 (`/speckit.plan` 명령어)
├── contracts/           # 1단계 출력 (`/speckit.plan` 명령어)
│   └── interfaces.md
└── tasks.md             # 2단계 출력 (`/speckit.tasks` 명령어 - `/speckit.plan`으로 생성되지 않음)
```

### 소스 코드 (리포지토리 루트)

리팩터링은 주로 `src/core/taskmanager/` 디렉토리 내에서 이루어집니다.

- **수정될 파일**:
    - `src/core/taskmanager/TaskManager.h`
    - `src/core/taskmanager/TaskManager.cpp`
    - `src/core/taskmanager/Task.h`
    - `src/core/taskmanager/Task.cpp`
    - `src/core/taskmanager/interfaces/ITask.h`
    - `src/core/taskmanager/interfaces/ITaskManager.h`
    - `src/core/taskmanager/factory/TaskFactory.h`
    - `src/core/taskmanager/factory/TaskFactory.cpp`
    - `tests/unit/task/TaskManager_test.cpp`
- **생성될 파일**:
    - `src/core/taskmanager/TaskDefinitionRegistry.h`
    - `src/core/taskmanager/TaskDefinitionRegistry.cpp`
    - `src/core/taskmanager/TaskExecutor.h`
    - `src/core/taskmanager/TaskExecutor.cpp`
    - `src/core/taskmanager/commands/CancelTaskCommand.h`
    - `src/core/taskmanager/commands/CancelTaskCommand.cpp`
    - `src/core/taskmanager/commands/PauseTaskCommand.h`
    - `src/core/taskmanager/commands/PauseTaskCommand.cpp`
    - `tests/unit/task/TaskDefinitionRegistry_test.cpp`
    - `tests/unit/task/TaskExecutor_test.cpp`

**구조 결정**: 기존 `taskmanager`의 역할 기반 디렉토리 구조를 유지하면서, `TaskManager`의 책임을 분리하기 위한 새로운 클래스들을 추가합니다. 테스트 코드 또한 병렬적으로 추가하여 코드 변경에 따른 안정성을 검증합니다.

## 복잡성 추적

> **Constitution 준수 확인에 정당화가 필요한 위반 사항이 있는 경우에만 작성하십시오**

| 위반 사항 | 필요한 이유 | 더 간단한 대안이 거부된 이유 |
|-----------|------------|-------------------------------------|
| 없음      | -          | -                                   |
