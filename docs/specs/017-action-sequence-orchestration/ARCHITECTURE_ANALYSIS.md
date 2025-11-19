# 아키텍처 분석: TaskManager 통합 전략

**날짜**: 2025-11-15
**Phase**: Phase 4 완료 → Phase 5 준비

---

## 현재 시스템 구조

### 1. 새로운 3계층 아키텍처 (`src/core/`)

```
Action Layer (src/core/action/)
  ├─ IAction 인터페이스
  ├─ ActionExecutor (실행)
  ├─ ActionFactory (생성)
  └─ ExecutionContext (상태 공유)

Sequence Layer (src/core/sequence/)
  ├─ SequenceEngine (조율)
  ├─ SequenceRegistry (정의 관리)
  ├─ ConditionEvaluator (조건 평가)
  └─ RetryHandler (재시도)

Task Layer (src/core/task/)
  ├─ TaskExecutor (실행 - ONCE 모드만)
  ├─ TaskRegistry (정의 관리)
  └─ TaskDefinition, TaskExecution (DTO)

  [미구현]
  ├─ PeriodicScheduler (주기적 실행)
  └─ TriggerManager (트리거 실행)
```

### 2. 레거시 TaskManager 시스템 (`src/core/taskmanager/`)

```
TaskManager (조율자)
  ├─ ITaskManager 인터페이스
  ├─ TaskDefinitionRegistry (레거시 정의)
  ├─ TaskExecutor (레거시 실행기 - 스레드 풀)
  ├─ OperatorInterface (외부 API Facade)
  └─ Command Pattern (StartTask, CancelTask, PauseTask)

ITask 구현체들:
  ├─ SequenceTaskAdapter ★ 새 SequenceEngine 사용!
  ├─ DriveForwardTask
  ├─ LiftPalletTask
  ├─ InspectAreaTask
  ├─ DummyTask
  └─ FailureTypeTask
```

---

## 핵심 발견사항

### SequenceTaskAdapter의 역할

**파일**: `src/core/taskmanager/tasks/SequenceTaskAdapter.h`

```cpp
class SequenceTaskAdapter : public ITask {
    // 레거시 ITask 인터페이스 구현
    void execute() override;
    void cancel() override;
    TaskStatus getStatus() const override;

private:
    std::shared_ptr<SequenceEngine> sequenceEngine_;  // ★ 새 SequenceEngine!
    std::string sequenceId_;
    std::string executionId_;
};
```

**의미**:
- 레거시 TaskManager의 ITask 인터페이스를 구현
- **내부적으로 새로운 SequenceEngine을 사용**
- TaskManager와 새 아키텍처 간의 **브릿지** 역할

### TaskManager가 제공하는 추가 기능

1. **Command Pattern**
   - StartTaskCommand, CancelTaskCommand, PauseTaskCommand
   - Undo/Redo, 로깅, 트랜잭션 확장 가능

2. **스레드 풀 기반 비동기 실행**
   - TaskExecutor가 스레드 풀 관리
   - 다중 Task 동시 실행 지원

3. **OperatorInterface** (Facade)
   - 외부 시스템을 위한 단순화된 API
   - defineNewTask, startTaskExecution, monitorTaskStatus

4. **Factory Pattern**
   - TaskDefinitionRegistry가 Task 타입별 팩토리 함수 관리
   - 런타임 Task 인스턴스 생성

---

## TaskManager 필요성 평가

### 제거 불가능한 이유

✅ **main.cpp에서 실제 사용 중**
- 현재 데모 프로그램으로 사용
- OperatorInterface를 통한 외부 API 제공

✅ **SequenceTaskAdapter가 새 SequenceEngine과 통합**
- 레거시와 신규 아키텍처의 브릿지 역할
- 이미 작동하는 통합 메커니즘

✅ **추가 기능 제공**
- Command Pattern (확장 가능성)
- 스레드 풀 (비동기 실행)
- OperatorInterface (외부 API)

✅ **기존 Task 구현체 보유**
- DriveForwardTask, LiftPalletTask 등
- 실제 로봇 동작 로직 포함

### 문제점

❌ **이중 구조**
- `src/core/taskmanager/` (레거시)
- `src/core/task/` (신규)
- 역할 중복 (TaskRegistry vs TaskDefinitionRegistry)

❌ **일관성 부족**
- 레거시는 ITask 인터페이스 사용
- 신규는 TaskDefinition + TaskExecutor 패턴

❌ **마이그레이션 경로 불명확**
- 장기적으로 어느 쪽을 사용할지 미정
- 개발자 혼란 가능성

---

## Phase 5 통합 전략

### 목표

레거시 TaskManager와 새 Task 모듈을 통합하여:
1. 기존 기능 유지 (하위 호환성)
2. 새 아키텍처 활용 (Action → Sequence → Task)
3. 점진적 마이그레이션 경로 제공

### Phase 5A: Task Layer 완성

**작업**:
- PeriodicScheduler 구현 (주기적 실행)
- TriggerManager 구현 (이벤트 기반 실행)
- TaskExecutor periodic/triggered 모드 확장

**결과**: 새 Task 모듈 완성 (ONCE, PERIODIC, TRIGGERED 모두 지원)

### Phase 5B: TaskManager 통합

#### 통합 방식 1: NewTaskAdapter (추천)

```
레거시 TaskManager
  ├─ ITask 인터페이스
  │   ├─ SequenceTaskAdapter (기존)
  │   ├─ NewTaskAdapter (신규) ★
  │   └─ 기타 레거시 Task들
  │
  └─ TaskManager → 모든 ITask 실행 가능

NewTaskAdapter:
  - 새 TaskDefinition을 래핑
  - 내부적으로 새 TaskExecutor 사용
  - ITask 인터페이스 구현
```

**장점**:
- ✅ 기존 TaskManager 인프라 재사용
- ✅ 레거시 코드 변경 최소화
- ✅ 점진적 마이그레이션 가능

**구현**:
```cpp
// src/core/taskmanager/tasks/NewTaskAdapter.h
class NewTaskAdapter : public ITask {
public:
    NewTaskAdapter(
        const TaskDefinition& definition,
        std::shared_ptr<TaskExecutor> executor,
        ExecutionContext& context
    );

    void execute() override;
    void cancel() override;
    TaskStatus getStatus() const override;
    // ...

private:
    TaskDefinition definition_;
    std::shared_ptr<TaskExecutor> executor_;
    ExecutionContext& context_;
    std::string executionId_;
};
```

#### 통합 방식 2: TaskManager 업데이트 (대안)

TaskManager가 직접 새 TaskExecutor를 사용하도록 업데이트

**단점**:
- ❌ 대규모 리팩터링 필요
- ❌ 기존 코드 많이 변경
- ❌ 리스크 큼

### Phase 5C: 아키텍처 문서화

**작업**:
1. 현재 아키텍처 상태 문서화 (ADR)
2. TaskManager 통합 전략 문서화
3. 장기 마이그레이션 계획 수립:
   - Option A: 레거시 TaskManager 유지 (어댑터 패턴 계속 사용)
   - Option B: 레거시 TaskManager 단계적 폐기 (새 Task 모듈로 완전 이전)

---

## 권장 사항

### 단기 (Phase 5)

1. ✅ **NewTaskAdapter 구현**
   - 새 Task 모듈을 레거시 ITask로 래핑
   - SequenceTaskAdapter와 동일한 패턴

2. ✅ **TaskManagerInit 업데이트**
   - 레거시 Task들과 NewTaskAdapter 모두 등록
   - 하위 호환성 유지

3. ✅ **통합 테스트**
   - TaskManager를 통한 새 Task 실행 검증
   - 레거시 Task들과 공존 확인

### 중기 (Phase 6 이후)

1. **레거시 Task들 평가**
   - DriveForwardTask 등을 새 아키텍처로 마이그레이션할지 결정
   - 실제 로봇 동작 로직이 있으면 유지 필요

2. **OperatorInterface 업데이트**
   - 새 Task 모듈 API 노출
   - 레거시 API와 병행 지원

### 장기

**Option A: Hybrid 유지** (추천)
- TaskManager: 외부 API, 스레드 풀, Command Pattern
- 새 Task 모듈: 핵심 실행 로직
- 어댑터 패턴으로 연결

**Option B: 완전 마이그레이션**
- TaskManager 기능을 새 Task 모듈로 통합
- 레거시 코드 단계적 제거
- 더 큰 리팩터링 필요

---

## 결론

**TaskManager는 제거 불가, 통합 필요**

- ✅ 레거시 TaskManager는 유용한 기능 제공 (Command, 스레드 풀, Facade)
- ✅ SequenceTaskAdapter는 이미 새 아키텍처와 통합됨
- ✅ NewTaskAdapter 패턴으로 새 Task 모듈 통합 가능
- ✅ 점진적 마이그레이션 경로 제공 (리스크 최소화)

**Phase 5 우선순위**:
1. Task Layer 완성 (Periodic, Triggered)
2. NewTaskAdapter 구현
3. 통합 테스트 및 문서화
