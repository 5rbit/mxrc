# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 프로젝트 개요

MXRC는 어떤 로봇도 제어 할 수 있는 범용 로봇 제어 컨트롤러 입니다. C++20으로 개발되며, CMake 빌드 시스템을 사용하고, Linux Ubuntu 24.04 LTS PREEMPT_RT 환경에서 실시간 성능을 목표로 합니다.

**핵심 성능 요구사항:**
- OOP/인터페이스 기반 설계로 모듈 간 상호 의존성(Chaining) 최소화.
- 구동 중심이 아닌, 고성능 아키텍처에 초점.
- RIIA 원칙 필수 적용으로 리소스 누수 방지 및 견고성 확보.
- 구성 요소는 추적 가능한 상세 로그 기록.
- 모든 모듈에 대한 철저한 단위 테스트 수행.
- 실패의 경우도 단위 테스트에 포함하여 진행.
- RTOS 기반 다중 Task 동시 실행 구조화.
- Task 간 공유 상태/이벤트 시스템을 통한 상태 감지 및 변경.
- Task는 템플릿 상속을 통해 확장, 유연을 도모.
- Task는 시작, 완료, 인터록, 타임아웃 조건 명확히 정의 및 실시간 검사.
- Task는 재시도 횟수/지연 시간 설정을 통한 재시도 로직 내장.
- 실패 시 자체 처리 및 안전 상태 전환 로직 포함.
- 명확한 상태 머신 구현 및 실시간 진행률(Progress) 보고 기능.



## 빌드 및 테스트

### 빌드 명령어

```bash
# 빌드 설정 및 빌드
mkdir -p build
cd build
cmake ..
make

# 메인 실행 파일 실행
./mxrc

# 모든 테스트 실행
./run_tests

# 상세 출력과 함께 테스트 실행
./run_tests --gtest_verbose
```

### 단일 테스트 실행

특정 테스트 파일 또는 테스트 케이스를 실행하려면:

```bash
# 특정 테스트 스위트 실행
./run_tests --gtest_filter=TaskManagerTest.*

# 특정 테스트 케이스 실행
./run_tests --gtest_filter=TaskManagerTest.RegisterTaskDefinition
```

## 시스템 개념 계층

MXRC는 네 가지 계층으로 구성된 명확한 개념 구조를 따릅니다:

### 개념 계층 정의

```
┌─────────────────────────────────────────────┐
│        MISSION (미션)                       │
│   상위 시스템이 지시한 작업의 큰 범주      │
│   예: "Pick & Place", "Assembly"            │
│   - 여러 Task들의 조합으로 구성             │
│   - 선형 또는 조건부 실행 시퀀스            │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│        TASK (태스크)                        │
│   로봇의 작업 단위, Sequence로 구성        │
│   예: "PickObject", "MoveToLocation"       │
│   - 원자적(Atomic) 작업 단위               │
│   - 상태 관리, 재시도, 타임아웃 포함       │
│   - TaskManager에 의해 관리                │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│    SEQUENCE (시퀀스)                        │
│   작업 내부의 순차적 Action 조성            │
│   예: "GripperOpen → Move → GripperClose"  │
│   - 순차/조건부/병렬 실행 지원             │
│   - SequenceEngine에 의해 실행             │
│   - 진행률 추적, 모니터링 기능             │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│      ACTION (액션)                         │
│   로봇의 단위 동작                          │
│   예: "MoveMotor", "SetGripper", "Delay"  │
│   - 가장 낮은 수준의 실행 단위             │
│   - ActionExecutor에 의해 실행             │
│   - 결과 저장, 에러 처리                   │
└─────────────────────────────────────────────┘
```

### 계층 간 관계

- **Mission → Task**: Mission은 여러 Task의 조합으로 정의됨
- **Task → Sequence**: Task는 내부적으로 Sequence로 구성됨
- **Sequence → Action**: Sequence는 여러 Action의 순서를 정의함
- **Action**: 최종 실행 단위, 로봇 제어 명령으로 변환됨

### 모듈 및 책임

| 계층 | 모듈 | 책임 | 상태 관리 |
|------|------|------|---------|
| Task | `taskmanager` | Task 정의, 등록, 실행 생명주기 | TaskStatus |
| Sequence | `sequence` | Action 순서 정의, 조건/분기/병렬 제어 | SequenceStatus |
| Action | `sequence/executor` | 개별 Action 실행 | ActionStatus |

---

## 아키텍처

### 핵심 모듈

시스템은 `src/core/` 하위에 기능 기반 모듈로 구성되어 있습니다. 각 모듈은 `specs/`에 상세한 사양을 가진 사양 주도 개발 방식을 따릅니다.

**TaskManager 모듈** (`src/core/taskmanager/`)
태스크 관리 시스템은 관심사를 분리한 리팩터링된 아키텍처를 사용합니다:

- **TaskDefinitionRegistry**: 태스크 타입 정의 및 태스크 인스턴스 생성을 위한 팩토리 함수 관리
- **TaskExecutor**: 스레드 풀을 사용한 태스크 실행 생명주기(제출, 실행, 취소) 처리
- **TaskManager**: Command 패턴을 통해 Registry와 Executor에 위임하여 시스템 조율
- **OperatorInterface**: 외부 시스템을 위한 단순화된 API를 제공하는 Facade

**사용 중인 디자인 패턴:**
- Command Pattern: 모든 태스크 작업(StartTaskCommand, CancelTaskCommand, PauseTaskCommand)이 커맨드 객체로 캡슐화됨
- Factory Pattern: TaskDefinitionRegistry가 팩토리 함수를 사용하여 태스크 인스턴스 생성
- Facade Pattern: OperatorInterface가 TaskManager와의 상호작용 단순화
- Dependency Injection: 컴포넌트가 생성자를 통해 의존성 주입 받음 (예: TaskManager가 Registry와 Executor를 주입받음)

### 핵심 아키텍처 원칙

1. **인터페이스 기반 설계**: 핵심 기능이 인터페이스(ITask, ITaskManager, ICommand, IOperatorInterface)를 통해 정의되며, `interfaces/` 디렉토리에 위치
2. **태스크 캡슐화**: 태스크는 ITask를 상속받고 자신의 상태를 내부적으로 관리. 외부 코드는 태스크 상태를 직접 수정할 수 없으며, 액션(execute, cancel, pause)만 요청 가능
3. **작업을 위한 Command Pattern**: 모든 태스크 작업이 커맨드 객체를 통해 수행되어, 향후 로깅, undo/redo, 트랜잭션 확장 가능
4. **정의와 실행의 분리**: TaskDefinitionRegistry가 태스크 메타데이터를 관리하고, TaskExecutor가 런타임 실행을 처리

### 디렉토리 구조

```
src/core/taskmanager/
├── interfaces/          # 핵심 인터페이스 (ITask, ITaskManager, ICommand)
├── commands/            # 커맨드 구현 (Start, Cancel, Pause)
├── tasks/              # 구체적인 태스크 구현 (DriveForward, LiftPallet 등)
├── operator_interface/ # 외부 상호작용을 위한 Facade
├── TaskManager.*       # 메인 조율자
├── TaskDefinitionRegistry.* # 태스크 타입 레지스트리
├── TaskExecutor.*      # 실행 생명주기 관리자
└── TaskManagerInit.*   # 모듈 초기화

tests/unit/task/        # taskmanager 모듈 단위 테스트
specs/                  # 기능 사양 및 계획 문서
```

**Sequence 모듈** (`src/core/sequence/`)
Action 시퀀스 오케스트레이션 시스템으로, Task 내부의 Action 실행을 관리합니다:

- **SequenceRegistry**: 시퀀스 정의(순차, 조건부, 병렬 실행) 저장 및 조회
- **SequenceEngine**: 시퀀스 정의를 로드하여 순차/조건부/병렬 실행을 조율
- **ActionExecutor**: 개별 Action 실행, 타임아웃, 재시도 정책 관리
- **ExecutionMonitor**: 실행 진행률, 상태, 로그 추적
- **ExecutionContext**: Action들 간의 상태 공유를 위한 변수 및 결과 저장소
- **ConditionalBranch**: IF-THEN-ELSE 조건부 분기 정의

**주요 기능:**
- 순차 실행: Action이 순서대로 실행, 이전 결과는 컨텍스트에 저장
- 조건부 분기: 런타임 조건 평가 후 다른 실행 경로 선택
- 병렬 실행: 독립적인 Action들을 병렬로 실행
- 재시도 정책: 실패한 Action에 대한 자동 재시도
- 진행률 추적: 전체 진행 상황 실시간 모니터링
- 취소/일시정지: 실행 중인 시퀀스의 제어

**디렉토리 구조:**
```
src/core/sequence/
├── core/               # 핵심 컴포넌트
│   ├── SequenceEngine.*
│   ├── SequenceRegistry.*
│   ├── ActionExecutor.*
│   ├── ConditionEvaluator.*
│   ├── ExecutionMonitor.*
│   ├── ExecutionContext.*
│   ├── ConditionalBranch.h
│   └── RetryPolicy.h
├── dto/                # 데이터 전송 객체
│   └── SequenceDto.h
├── interfaces/         # 추상 인터페이스
│   ├── IAction.h
│   └── IActionFactory.h
└── ...

tests/unit/sequence/    # sequence 모듈 단위 테스트
tests/integration/sequence/ # 시퀀스 통합 테스트
```

## 코드 스타일 및 규칙

### 네임스페이스

모든 코드는 중첩된 네임스페이스를 사용: `mxrc::core::<모듈명>`

예시:
```cpp
namespace mxrc::core::taskmanager {
    // 구현
}
```

### 헤더 의존성

- 헤더 파일에서 forward declaration을 사용하여 컴파일 의존성 최소화
- `.h` 파일에는 필요한 헤더만 포함; `.cpp` 파일에 포함하는 것을 선호
- 가능한 경우 구현 헤더가 아닌 인터페이스 헤더를 포함

### 태스크 구현

새로운 태스크 타입을 구현할 때:

1. `ITask` 인터페이스를 상속
2. 필수 메서드 구현: `execute()`, `cancel()`, `getStatus()`, `getProgress()`, `getId()`, `toDto()`
3. 내부 상태(`status_`, `progress_`) 관리 - setter를 절대 노출하지 않음
4. `TaskManagerInit.cpp`에서 `TaskDefinitionRegistry::registerDefinition()`을 사용하여 태스크 타입 등록

예시:
```cpp
class MyNewTask : public ITask {
public:
    void execute() override {
        status_ = TaskStatus::RUNNING;
        // ... 작업 수행 ...
        status_ = TaskStatus::COMPLETED;
    }

    void cancel() override {
        if (status_ == TaskStatus::RUNNING) {
            status_ = TaskStatus::CANCELLING;
        }
    }

    // ... 기타 필수 메서드
private:
    TaskStatus status_;
    float progress_;
};
```

### 시퀀스 및 액션 구현

**Action 구현:**

새로운 액션 타입을 구현할 때:

1. `IAction` 인터페이스를 상속
2. 필수 메서드 구현: `execute()`, `cancel()`, `getStatus()`, `getId()`
3. 결과는 ExecutionContext에 저장하여 다른 Action에서 접근 가능
4. 에러 메시지는 ActionExecutor를 통해 캡처됨

```cpp
class MyAction : public IAction {
public:
    MyAction(const std::string& id) : id_(id) {}

    void execute(ExecutionContext& context) override {
        // 입력 변수 읽기
        auto input = context.getVariable("input_value");
        if (!input.has_value()) {
            throw std::runtime_error("Missing input_value");
        }

        // 작업 수행
        int value = std::any_cast<int>(input);
        int result = value * 2;

        // 결과 저장
        context.setActionResult(id_, result);
    }

    void cancel() override {
        // 취소 처리 (필요시)
    }

    ActionStatus getStatus() const override {
        return status_;
    }

    std::string getId() const override {
        return id_;
    }

private:
    std::string id_;
    ActionStatus status_ = ActionStatus::PENDING;
};
```

**Sequence 정의:**

시퀀스는 순차, 조건부, 병렬 실행을 지원합니다:

```cpp
// 1. 순차 실행 정의
SequenceDefinition seq;
seq.id = "move_and_pick";
seq.actionIds = {"move_to_location", "gripper_open", "pick_object"};
registry.registerSequence(seq);

// 2. 조건부 분기 정의
ConditionalBranch branch;
branch.id = "check_weight";
branch.condition = "weight > 5";  // 지원 연산자: ==, !=, <, >, <=, >=, AND, OR, NOT
branch.thenActions = {"gripper_strong"};
branch.elseActions = {"gripper_weak"};
engine.registerBranch(branch);

// 3. 병렬 실행 정의 (Phase 5)
ParallelBranch parallel;
parallel.id = "parallel_setup";
parallel.branches = {{"move_arm"}, {"move_legs"}};
engine.registerParallelBranch(parallel);
```

**Execution Context 사용:**

Action들 간에 상태를 공유합니다:

```cpp
// Action 내부에서
context.setVariable("processed_count", 10);  // 상태 저장
auto result = context.getVariable("processed_count");  // 상태 읽기
context.setActionResult(actionId, processedData);  // 결과 저장
auto prevResult = context.getActionResult(prevActionId);  // 이전 결과 읽기
```

### 테스트 규칙

- 테스트는 Google Test (GTest) 프레임워크 사용
- 테스트 파일 이름: `<컴포넌트명>_test.cpp`
- 테스트용 Mock 클래스 이름: `Mock<클래스명>` 또는 `Mock<클래스명>For<목적>`
- 테스트 파일에서 시나리오 설명을 위해 한글 주석을 일반적으로 사용
- 모든 새 기능에는 해당 단위 테스트가 있어야 함

## 사양 주도 개발

이 프로젝트는 사양 우선 접근 방식을 따릅니다:

1. 각 기능은 `specs/<기능번호>-<기능명>/`에 사양을 가짐
2. 사양에는 사용자 스토리, 인수 기준, 기능적 요구사항, 성공 기준 포함
3. 사양은 한글로 작성됨
4. 기능 구현 시 관련 사양 참조 (예: `specs/016-refactor-taskmanager/spec.md`)

## 의존성

- **spdlog**: 로깅 프레임워크
- **GTest**: 단위 테스트 프레임워크
- **CMake**: 빌드 시스템 (최소 버전 3.16)

Ubuntu에서 의존성 설치:
```bash
sudo apt-get install libspdlog-dev libgtest-dev cmake
```

## 현재 작업

현재 저장소는 `016-refactor-taskmanager` 브랜치에 있으며, TaskManager 모듈을 다음과 같이 리팩터링 중입니다:
- 태스크 정의 관리(TaskDefinitionRegistry)와 실행(TaskExecutor) 분리
- 레지스트리 기반 팩토리 함수를 위해 TaskFactory 제거
- 외부 상태 수정 메서드 제거로 캡슐화 강화
- Command 패턴 사용 확대 (CancelTaskCommand, PauseTaskCommand 추가)

이 코드베이스 작업 시, `specs/`의 관련 사양을 참조하고 변경사항이 위에 설명된 아키텍처 원칙과 일치하는지 확인하세요.
