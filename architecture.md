# Action, Sequence, Task 통합 시스템 아키텍처 설계

## 개요

이 시스템은 Action, Sequence, Task 세 가지 계층을 단계적으로 구현합니다.
각 계층은 독립적으로 테스트 가능하며, 명확한 책임 분리를 통해 유지보수성을 보장합니다.

**구현 순서**: Action → Sequence → Task
**통합 전략**: 각 계층 완료 후 다음 계층 시작

---

## 아키텍처 계층

```
┌─────────────────────────────────────────────────────────┐
│         Task Management Layer (Phase 3)                 │
│  Task 생명주기 관리 및 실행 모드 제어                   │
├─────────────────────────────────────────────────────────┤
│ • TaskExecutor (Task 실행: 단일/주기적/트리거)          │
│ • TaskRegistry (Task 정의 관리)                         │
│ • PeriodicScheduler (주기적 실행 스케줄러)              │
│ • TriggerManager (이벤트 트리거 관리)                   │
│ • TaskMonitor (Task 상태 모니터링)                      │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│         Sequence Orchestration Layer (Phase 2)          │
│  여러 Action의 순차/조건부/병렬 실행 조율                │
├─────────────────────────────────────────────────────────┤
│ • SequenceEngine (시퀀스 실행 엔진)                     │
│ • SequenceRegistry (시퀀스 정의 관리)                   │
│ • ConditionEvaluator (조건 평가)                       │
│ • ParallelExecutor (병렬 실행)                         │
│ • RetryHandler (재시도 처리)                           │
│ • ExecutionMonitor (실행 모니터링)                      │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│         Action Execution Layer (Phase 1)                │
│  로봇 동작의 기본 정의 및 실행                           │
├─────────────────────────────────────────────────────────┤
│ • IAction (동작 인터페이스)                             │
│ • ActionExecutor (개별 동작 실행)                       │
│ • ActionFactory (동작 생성 팩토리)                      │
│ • ActionRegistry (동작 타입 등록)                       │
│ • ExecutionContext (실행 컨텍스트)                     │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│              Common Interfaces & DTOs                    │
│  공통 인터페이스 및 데이터 전송 객체                     │
├─────────────────────────────────────────────────────────┤
│ • ActionStatus, SequenceStatus, TaskStatus              │
│ • ExecutionResult, ActionDefinition, SequenceDefinition │
│ • TaskDefinition, ExecutionContext                      │
└─────────────────────────────────────────────────────────┘
```

---

## 핵심 컴포넌트

### Phase 1: Action Layer (동작 계층)

#### IAction (동작 인터페이스)
```cpp
namespace mxrc::core::action {

class IAction {
public:
    virtual ~IAction() = default;
    virtual std::string getId() const = 0;
    virtual std::string getType() const = 0;
    virtual void execute(ExecutionContext& context) = 0;
    virtual void cancel() = 0;
    virtual ActionStatus getStatus() const = 0;
    virtual float getProgress() const = 0;
};

}
```

#### ActionExecutor
- 개별 Action 실행
- 타임아웃 관리
- 실행 결과 수집
- 에러 처리

#### ActionFactory
- Action 타입별 생성 함수 등록
- 파라미터 기반 Action 인스턴스 생성
- 플러그인 방식 확장 지원

#### ActionRegistry
- Action 타입 등록 및 관리
- 사용 가능한 Action 목록 조회
- Action 정의 검증

### Phase 2: Sequence Layer (시퀀스 계층)

#### SequenceEngine
- 시퀀스 실행 제어 (시작, 일시정지, 재개, 취소)
- 상태 관리 (PENDING, RUNNING, PAUSED, COMPLETED, FAILED)
- 실행 흐름 조정 (순차, 병렬, 조건부)
- Action 간 데이터 전달 관리

#### SequenceRegistry
- 시퀀스 정의 등록 및 관리
- 시퀀스 조회, 목록 반환
- 버전 관리

#### ConditionEvaluator
- 조건식 평가 (==, !=, <, >, <=, >=)
- 논리 연산 (AND, OR, NOT)
- 이전 Action 결과 참조
- 조건 평가 에러 처리

#### ParallelExecutor
- 여러 Action 병렬 실행
- 모든 Action 완료 대기 (Join)
- 병렬 그룹 타임아웃 관리

#### RetryHandler
- 재시도 정책 관리
- 지수 백오프 계산
- 최대 재시도 횟수 제한

#### ExecutionMonitor
- 진행률 추적
- 로그 기록
- 상태 조회 인터페이스

### Phase 3: Task Layer (Task 계층)

#### TaskExecutor
- Task 실행 모드 관리 (단일/주기적/트리거)
- Task 생명주기 제어
- TaskManager와 통합

#### TaskRegistry
- Task 정의 등록 및 관리
- Task 타입 구분 (SINGLE_ACTION, SEQUENCE_BASED)
- Task 파라미터 검증

#### PeriodicScheduler
- 주기적 Task 스케줄링
- Interval 기반 실행
- 다음 실행 시간 계산

#### TriggerManager
- 이벤트 기반 Task 실행
- 트리거 조건 평가
- 이벤트 리스너 관리

#### TaskMonitor
- Task 실행 상태 추적
- 실행 횟수 기록
- 성능 메트릭 수집

---

## 디렉토리 구조

```
src/core/
├── action/                          # Phase 1: Action Layer
│   ├── interfaces/
│   │   ├── IAction.h
│   │   └── IActionFactory.h
│   ├── core/
│   │   ├── ActionExecutor.h
│   │   ├── ActionExecutor.cpp
│   │   ├── ActionFactory.h
│   │   ├── ActionFactory.cpp
│   │   ├── ActionRegistry.h
│   │   └── ActionRegistry.cpp
│   ├── dto/
│   │   ├── ActionStatus.h
│   │   ├── ActionDefinition.h
│   │   └── ExecutionResult.h
│   └── impl/                       # 기본 Action 구현
│       ├── DelayAction.h
│       ├── DelayAction.cpp
│       ├── MoveAction.h
│       └── MoveAction.cpp
│
├── sequence/                        # Phase 2: Sequence Layer
│   ├── interfaces/
│   │   ├── ISequenceEngine.h
│   │   └── IConditionProvider.h
│   ├── core/
│   │   ├── SequenceEngine.h
│   │   ├── SequenceEngine.cpp
│   │   ├── SequenceRegistry.h
│   │   ├── SequenceRegistry.cpp
│   │   ├── ConditionEvaluator.h
│   │   ├── ConditionEvaluator.cpp
│   │   ├── ParallelExecutor.h
│   │   ├── ParallelExecutor.cpp
│   │   ├── RetryHandler.h
│   │   ├── RetryHandler.cpp
│   │   ├── ExecutionMonitor.h
│   │   └── ExecutionMonitor.cpp
│   ├── dto/
│   │   ├── SequenceStatus.h
│   │   ├── SequenceDefinition.h
│   │   ├── ConditionalBranch.h
│   │   └── RetryPolicy.h
│   └── util/
│       └── ExecutionContext.h
│
├── task/                            # Phase 3: Task Layer
│   ├── interfaces/
│   │   ├── ITask.h
│   │   ├── ITaskExecutor.h
│   │   └── ITriggerProvider.h
│   ├── core/
│   │   ├── TaskExecutor.h
│   │   ├── TaskExecutor.cpp
│   │   ├── TaskRegistry.h
│   │   ├── TaskRegistry.cpp
│   │   ├── PeriodicScheduler.h
│   │   ├── PeriodicScheduler.cpp
│   │   ├── TriggerManager.h
│   │   ├── TriggerManager.cpp
│   │   ├── TaskMonitor.h
│   │   └── TaskMonitor.cpp
│   └── dto/
│       ├── TaskStatus.h
│       ├── TaskDefinition.h
│       ├── TaskExecutionMode.h
│       └── TaskExecution.h
│
└── metrics/                         # Phase 7: Metrics & Monitoring
    ├── interfaces/
    │   ├── IMetricsCollector.h
    │   └── IHealthCheckProvider.h
    ├── core/
    │   ├── MetricsCollector.h
    │   ├── MetricsCollector.cpp
    │   ├── PerformanceMonitor.h
    │   ├── PerformanceMonitor.cpp
    │   ├── ResourceMonitor.h
    │   ├── ResourceMonitor.cpp
    │   ├── ExecutionTracer.h
    │   ├── ExecutionTracer.cpp
    │   ├── HealthCheckService.h
    │   └── HealthCheckService.cpp
    ├── dto/
    │   ├── PerformanceMetrics.h
    │   ├── ResourceUsage.h
    │   ├── ExecutionTrace.h
    │   └── HealthStatus.h
    └── exporters/
        ├── JsonExporter.h
        ├── JsonExporter.cpp
        ├── PrometheusExporter.h
        └── PrometheusExporter.cpp

tests/
├── unit/
│   ├── action/                      # Phase 1 테스트 (15 tests)
│   │   ├── ActionExecutor_test.cpp
│   │   ├── ActionFactory_test.cpp
│   │   └── ActionRegistry_test.cpp
│   ├── sequence/                    # Phase 2 테스트 (84 tests)
│   │   ├── SequenceEngine_test.cpp
│   │   ├── SequenceRegistry_test.cpp
│   │   ├── ConditionEvaluator_test.cpp
│   │   ├── ParallelExecutor_test.cpp
│   │   ├── RetryHandler_test.cpp
│   │   └── ExecutionMonitor_test.cpp
│   ├── task/                        # Phase 3 테스트 (62 tests)
│   │   ├── TaskExecutor_test.cpp
│   │   ├── TaskRegistry_test.cpp
│   │   ├── PeriodicScheduler_test.cpp
│   │   ├── TriggerManager_test.cpp
│   │   └── TaskMonitor_test.cpp
│   └── metrics/                     # Phase 7 테스트 (계획됨)
│       ├── MetricsCollector_test.cpp
│       ├── PerformanceMonitor_test.cpp
│       ├── ResourceMonitor_test.cpp
│       ├── ExecutionTracer_test.cpp
│       ├── HealthCheckService_test.cpp
│       ├── JsonExporter_test.cpp
│       └── PrometheusExporter_test.cpp
└── integration/
    ├── action_integration_test.cpp
    ├── sequence_integration_test.cpp
    ├── task_integration_test.cpp
    ├── metrics_integration_test.cpp  # Phase 7
    └── full_system_test.cpp
```

---

## 설계 원칙

### 1. 단계적 구현 (Incremental Development)
- **Phase 1**: Action Layer 완성 및 테스트
- **Phase 2**: Sequence Layer 완성 및 테스트 (Action 기반)
- **Phase 3**: Task Layer 완성 및 테스트 (Sequence 기반)
- 각 Phase 완료 후 다음 Phase 시작

### 2. 인터페이스 기반 설계
- 모든 확장 지점에 인터페이스 사용
- 의존성 주입(Dependency Injection)
- 느슨한 결합(Loose Coupling)
- 각 계층 간 명확한 경계

### 3. 독립적 테스트 가능성
- 각 계층은 독립적으로 테스트 가능
- Mock 객체 사용 가능
- 의존성 최소화
- 단위 테스트 + 통합 테스트

### 4. RAII 원칙 준수
- 자동 리소스 관리 (스마트 포인터)
- 메모리 누수 방지
- 예외 안전성 보장
- 모든 리소스는 생성자/소멸자에서 관리

### 5. TaskManager 통합
- Phase 3에서 TaskManager와 통합
- Adapter 패턴 사용
- 기존 TaskManager에 영향 없음
- 선택적 통합 가능

---

## 단위 테스트 전략

### Phase 1: Action Layer 테스트

#### 단위 테스트
```
✓ ActionExecutor: Action 실행, 타임아웃, 에러 처리
✓ ActionFactory: Action 생성, 파라미터 전달
✓ ActionRegistry: Action 타입 등록, 조회
✓ 기본 Action 구현: DelayAction, MoveAction 등
```

#### 통합 테스트
```
✓ Action 실행 및 결과 확인
✓ 여러 Action 순차 실행
✓ Action 에러 처리 시나리오
```

### Phase 2: Sequence Layer 테스트

#### 단위 테스트
```
✓ SequenceRegistry: 시퀀스 등록, 조회, 업데이트
✓ SequenceEngine: 상태 관리, 흐름 제어
✓ ConditionEvaluator: 조건식 평가, 참조
✓ ParallelExecutor: 병렬 실행, Join 동작
✓ RetryHandler: 재시도 정책, 백오프
✓ ExecutionMonitor: 진행률, 로깅
```

#### 통합 테스트
```
✓ 순차 실행 시나리오 (3-5개 Action)
✓ 조건부 분기 시나리오
✓ 병렬 실행 시나리오 (2-4개 Action)
✓ 재시도 및 에러 처리
✓ 일시정지/재개
```

### Phase 3: Task Layer 테스트

#### 단위 테스트
```
✓ TaskExecutor: Task 실행 모드 관리
✓ TaskRegistry: Task 정의 등록, 조회
✓ PeriodicScheduler: 주기적 실행 스케줄링
✓ TriggerManager: 이벤트 기반 실행
✓ TaskMonitor: 상태 추적, 메트릭 수집
```

#### 통합 테스트
```
✓ 단일 Action 포함 Task 실행
✓ Sequence 포함 Task 실행
✓ 주기적 Task 실행 (100ms interval)
✓ 트리거 기반 Task 실행
✓ TaskManager 통합
```

### Phase 7: Metrics & Monitoring 테스트

#### 단위 테스트 (계획됨)
```
⏳ MetricsCollector: 실행 시간 기록, 통계 계산
⏳ PerformanceMonitor: Percentile 계산 (p95, p99)
⏳ ResourceMonitor: 메모리/스레드 풀 모니터링
⏳ ExecutionTracer: DAG 생성, 타임라인 추적
⏳ HealthCheckService: 상태 API, 진단
⏳ JsonExporter: 메트릭 JSON 변환
⏳ PrometheusExporter: Prometheus 포맷 변환
```

#### 통합 테스트 (계획됨)
```
⏳ 메트릭 수집 오버헤드 측정 (< 5%)
⏳ 실시간 성능 모니터링
⏳ 리소스 한계 경고 시나리오
⏳ 실행 추적 및 시각화
⏳ Health Check API 통합
```

### 전체 시스템 테스트
```
✓ Action → Sequence → Task 통합 시나리오
✓ 복잡한 작업 시나리오 (10+ Action, 3+ Sequence, 2+ Task)
✓ 성능 테스트 (1000+ Action)
✓ 메모리 누수 테스트 (RAII 원칙 준수)
⏳ 메트릭 기반 성능 분석 (Phase 7)
```

---

## 계층별 구현 및 통합 시나리오

### Phase 1: Action Layer (독립 개발)
```
┌─────────────────────────────────┐
│     Action System               │
│  (독립적 개발 및 테스트)        │
├─────────────────────────────────┤
│ • IAction 인터페이스            │
│ • ActionExecutor                │
│ • ActionFactory & Registry      │
│ • 기본 Action 구현              │
└─────────────────────────────────┘
```

**완료 기준**:
- 모든 Action 컴포넌트 단위 테스트 통과
- Action 실행 통합 테스트 통과
- 메모리 누수 없음

### Phase 2: Sequence Layer (Action 기반)
```
┌─────────────────────────────────┐
│   Sequence System               │
│  (Action Layer 위에 구축)       │
├─────────────────────────────────┤
│ • SequenceEngine                │
│ • ConditionEvaluator            │
│ • ParallelExecutor              │
│ • RetryHandler                  │
└─────────────────────────────────┘
         ↓ 사용
┌─────────────────────────────────┐
│     Action System               │
└─────────────────────────────────┘
```

**완료 기준**:
- 모든 Sequence 컴포넌트 단위 테스트 통과
- Sequence 실행 통합 테스트 통과 (Action 기반)
- 순차/조건부/병렬 실행 검증

### Phase 3: Task Layer (Sequence 기반)
```
┌─────────────────────────────────┐
│    TaskManager Integration      │
├─────────────────────────────────┤
│ • TaskManagerAdapter            │
└─────────────────────────────────┘
         ↓
┌─────────────────────────────────┐
│      Task System                │
│ (Sequence/Action Layer 위에)    │
├─────────────────────────────────┤
│ • TaskExecutor                  │
│ • PeriodicScheduler             │
│ • TriggerManager                │
│ • TaskMonitor                   │
└─────────────────────────────────┘
         ↓ 사용
┌─────────────────────────────────┐
│   Sequence System               │
│   (Action 포함)                 │
└─────────────────────────────────┘
```

**완료 기준**:
- 모든 Task 컴포넌트 단위 테스트 통과
- Task 실행 모드 통합 테스트 통과
- TaskManager 통합 검증

### 최종 통합 시스템
```
┌─────────────────────────────────┐
│   OperatorInterface             │
├─────────────────────────────────┤
│   TaskManager                   │
├─────────────────────────────────┤
│   Task Layer                    │
│   (주기적/트리거 실행)          │
├─────────────────────────────────┤
│   Sequence Layer                │
│   (순차/조건부/병렬)            │
├─────────────────────────────────┤
│   Action Layer                  │
│   (기본 동작)                   │
└─────────────────────────────────┘
```

---

## 코드 스타일 및 규칙

### 네임스페이스
```cpp
// Phase 1: Action Layer
namespace mxrc::core::action {
    // Action 시스템 코드
}

// Phase 2: Sequence Layer
namespace mxrc::core::sequence {
    // Sequence 시스템 코드
}

// Phase 3: Task Layer
namespace mxrc::core::task {
    // Task 시스템 코드
}
```

### 헤더 의존성
- 인터페이스 헤더만 포함 (구현 X)
- Forward declaration 활용
- 컴파일 시간 최소화
- 하위 계층만 의존 (Task → Sequence → Action)

### 테스트 명명
```cpp
// <ComponentName>_test.cpp
// TEST(<ComponentName>Test, <Scenario>)

// Action: 간략한 설명
TEST(ActionExecutorTest, ExecuteActionSuccessfully)
TEST(ActionFactoryTest, CreateActionWithParameters)

// Seq: 간략한 설명
TEST(SequenceEngineTest, StartSequenceChangesStateToRunning)
TEST(ConditionEvaluatorTest, EvaluateSimpleCondition)

// Task: 간략한 설명 
TEST(TaskExecutorTest, ExecutePeriodicTask)
TEST(TriggerManagerTest, TriggerTaskOnEvent)
```

---

## 구현 순서

### Phase 1: Action Layer 구현
1. **공통 인터페이스 및 DTO** (dto/)
   - ActionStatus.h
   - ActionDefinition.h
   - ExecutionResult.h
2. **Action 인터페이스** (interfaces/)
   - IAction.h
   - IActionFactory.h
3. **핵심 컴포넌트** (core/)
   - ActionExecutor
   - ActionFactory
   - ActionRegistry
4. **기본 Action 구현** (impl/)
   - DelayAction
   - MoveAction
5. **단위 및 통합 테스트** (tests/unit/action/, tests/integration/)

### Phase 2: Sequence Layer 구현
1. **Sequence DTO** (dto/)
   - SequenceStatus.h
   - SequenceDefinition.h
   - ConditionalBranch.h
   - RetryPolicy.h
2. **Sequence 인터페이스** (interfaces/)
   - ISequenceEngine.h
   - IConditionProvider.h
3. **핵심 컴포넌트** (core/)
   - SequenceRegistry
   - ConditionEvaluator
   - ParallelExecutor
   - RetryHandler
   - ExecutionMonitor
4. **통합 컴포넌트** (core/)
   - SequenceEngine
5. **단위 및 통합 테스트** (tests/unit/sequence/, tests/integration/)

### Phase 3: Task Layer 구현 ✅ 완료
1. **Task DTO** (dto/) ✅ 완료
   - TaskStatus.h
   - TaskDefinition.h
   - TaskExecutionMode.h
   - TaskExecution.h
2. **Task 인터페이스** (interfaces/) ✅ 완료
   - ITask.h
   - ITaskExecutor.h
   - ITriggerProvider.h
3. **핵심 컴포넌트** (core/) ✅ 완료
   - TaskRegistry ✅ 완료
   - PeriodicScheduler ✅ 완료
   - TriggerManager ✅ 완료
   - TaskMonitor ✅ 완료
4. **통합 컴포넌트** (core/) ✅ 완료
   - TaskExecutor ✅ 완료 (ONCE/PERIODIC/TRIGGERED 모두 지원)
5. **TaskManager 통합** (integration/) ⚠️ OBSOLETE
   - TaskManagerAdapter (레거시 taskmanager 모듈 제거로 불필요)
6. **단위 및 통합 테스트** (tests/unit/task/, tests/integration/) ✅ 완료
   - TaskRegistry 테스트 ✅ 완료 (12 tests)
   - TaskCoreExecutor 테스트 ✅ 완료 (14 tests)
   - PeriodicScheduler 테스트 ✅ 완료 (9 tests)
   - TriggerManager 테스트 ✅ 완료 (9 tests)
   - TaskMonitor 테스트 ✅ 완료 (18 tests)

**현재 테스트 현황**: 161 tests passing
- Action Layer: 15 tests ✅
- Sequence Layer: 84 tests ✅
- Task Layer: 62 tests ✅ (Phase 3 완전 완료)

### Phase 4: Logging & Code Cleanup ✅ 완료
1. **Logging Enhancement** (Phase 4A) ✅ 완료
   - ActionExecutor: 실행 시간, 진행률 추적 ✅
   - SequenceEngine: 스텝별 진행, 조건 평가 상세 ✅
   - TaskExecutor: 상태 전환, 실행 모드 상세 ✅
   - TaskRegistry: 등록/제거 작업 로그 ✅
   - 구조화된 로깅 포맷 (타임스탬프, 레벨, 컨텍스트) ✅
2. **Code Cleanup** (Phase 4B) ✅ 완료
   - 레거시 taskmanager 모듈 완전 제거 (commit a2a5460) ✅
   - sequence.old 레거시 코드 제거 확인 ✅
   - 불필요한 include 및 의존성 정리 ✅

### Phase 5: TaskManager 통합 ⚠️ OBSOLETE
**Note**: 레거시 TaskManager 모듈이 제거되면서 (commit a2a5460) 본 Phase는 더 이상 필요하지 않습니다.
현재 시스템은 Action → Sequence → Task 3계층 구조로 깔끔하게 정리되었습니다.

### Phase 6: System Integration ✅ 완료
1. **전체 시스템 통합 테스트** ✅ 완료
2. **성능 테스트** ✅ 완료 (161 tests)
3. **메모리 누수 검증** ✅ 완료 (RAII 원칙 준수)
4. **문서화** ✅ 진행 중

### Phase 7: Metrics & Performance Monitoring ⏳ 계획됨
1. **Metrics Collection** (Phase 7A)
   - MetricsCollector: Action/Sequence/Task 실행 시간 추적
   - PerformanceMonitor: 통계 (min/max/avg/p95/p99 percentiles)
   - ResourceMonitor: 메모리, 스레드 풀 모니터링
   - 메트릭 내보내기 (JSON/Prometheus)
2. **Execution Tracing** (Phase 7B)
   - ExecutionTracer: 실행 흐름 DAG 생성
   - 타임라인 시각화 (Chrome Trace Event Format)
   - 크리티컬 패스 분석
3. **Health & Diagnostics** (Phase 7C)
   - HealthCheckService: 시스템 상태 API
   - DiagnosticsAPI: 현재 실행 진단
   - 과거 실행 통계 조회

---

## 확장성 설계

### 새로운 동작 타입 추가
1. IAction 상속
2. execute() 구현
3. IActionFactory에 등록
4. 테스트 작성

### 새로운 조건 함수 추가
1. IConditionProvider 상속
2. evaluate() 구현
3. ConditionEvaluator에 등록

### 새로운 에러 핸들러 추가
1. 핸들러 함수 정의
2. RetryHandler에 등록
3. 테스트 작성

---

## 의존성 관리

### 외부 의존성
- spdlog (로깅) - 기존 프로젝트와 동일
- gtest (테스트) - 기존 프로젝트와 동일

### 내부 의존성
```
ActionExecutor ← IAction
ConditionEvaluator ← ExecutionContext
RetryHandler ← ActionExecutor
SequenceEngine → ActionExecutor, ConditionEvaluator, RetryHandler
ExecutionMonitor ← SequenceEngine
SequenceTaskAdapter → SequenceEngine (ITask 구현)
```

---

## 메모리 안전성

### RAII 원칙
- 모든 리소스는 생성자에서 할당, 소멸자에서 해제
- std::shared_ptr, std::unique_ptr 사용
- 수동 메모리 관리 금지

### 스레드 안전성
- Sequence 실행은 TaskExecutor 스레드 풀에서 처리
- 상태 접근은 mutex로 보호
- 로그는 thread-safe

---

## 성능 고려사항

### 1. 메모리 효율성
- 큰 시퀀스도 효율적으로 처리 (1000+ 동작)
- 결과 캐싱으로 재계산 방지

### 2. 실행 효율성
- 불필요한 복사 제거 (이동 의미론)
- 조건 평가 최적화

### 3. 모니터링 오버헤드
- 로그 레벨 제어
- 선택적 상세 추적

---

## 공유 데이터 및 이벤트 시스템

### DataStore: 중앙 집중식 데이터 공유

**목적**: 계층 간 데이터 공유 및 상태 관리

#### 아키텍처 진화

**Phase 1: Singleton → shared_ptr 전환**
```cpp
// 기존 (메모리 누수)
class DataStore {
    static DataStore& getInstance() {
        static DataStore* instance = new DataStore();  // ❌ 해제 불가
        return *instance;
    }
};

// 개선 (안전한 메모리 관리)
class DataStore : public std::enable_shared_from_this<DataStore> {
public:
    static std::shared_ptr<DataStore> create() {
        static std::shared_ptr<DataStore> instance =
            std::make_shared<DataStore>();  // ✅ 자동 해제
        return instance;
    }
};
```

**개선 효과**:
- 메모리 누수 제거
- 자동 생명주기 관리
- 테스트 격리 가능 (createForTest() 메서드)

**Phase 3: concurrent_hash_map 전환**
```cpp
class DataStore {
private:
    // 고성능 thread-safe 데이터 접근
    tbb::concurrent_hash_map<std::string, SharedData> data_map_;

    // 비-크리티컬 경로는 mutex로 보호
    std::map<std::string, std::unique_ptr<Notifier>> notifiers_;
    std::map<std::string, DataExpirationPolicy> expiration_policies_;
    mutable std::mutex mutex_;
};
```

**성능 개선**:
- 전역 mutex 병목 제거
- 세분화된 락 (bucket-level locking)
- 10배 성능 향상 예상 (1000ms → 100ms)

#### 주요 기능

**1. 데이터 접근 메서드**
```cpp
// Set: accessor 패턴으로 thread-safe 쓰기
template<typename T>
void set(const std::string& id, const T& data, DataType type) {
    {
        typename tbb::concurrent_hash_map<std::string, SharedData>::accessor acc;
        if (data_map_.find(acc, id)) {
            // 기존 데이터 업데이트
            if (acc->second.type != type) {
                throw std::runtime_error("Data type mismatch");
            }
        } else {
            // 새로운 데이터 삽입
            data_map_.insert(acc, id);
        }
        acc->second = new_data;
        // accessor는 스코프를 벗어나면 자동 해제 (RAII)
    }

    // 비-크리티컬 경로
    {
        std::lock_guard<std::mutex> lock(mutex_);
        expiration_policies_[id] = policy;
        performance_metrics_["set_calls"]++;
    }

    // 구독자 알림 (락 없이 실행)
    notifySubscribers(new_data);
}

// Get: const_accessor로 읽기 전용 접근
template<typename T>
T get(const std::string& id) {
    T result;
    {
        typename tbb::concurrent_hash_map<std::string, SharedData>::const_accessor acc;
        if (!data_map_.find(acc, id)) {
            throw std::out_of_range("Data not found");
        }
        result = std::any_cast<T>(acc->second.value);
    }
    return result;
}
```

**2. Observer 패턴 (weak_ptr 기반)**
```cpp
class MapNotifier : public Notifier {
private:
    // weak_ptr로 dangling pointer 방지
    std::vector<std::weak_ptr<Observer>> subscribers_;
    std::mutex mutex_;

public:
    void notify(const SharedData& data) override {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto it = subscribers_.begin(); it != subscribers_.end(); ) {
            if (auto obs = it->lock()) {       // ✅ 자동 NULL 체크
                obs->onDataChanged(data);
                ++it;
            } else {
                it = subscribers_.erase(it);   // ✅ 자동 정리
            }
        }
    }
};
```

**3. 데이터 만료 정책**
```cpp
// TTL (Time To Live) 지원
DataExpirationPolicy policy = {
    ExpirationPolicyType::TTL,
    std::chrono::milliseconds(5000)
};
dataStore->set("temp_data", value, DataType::Para, policy);

// 주기적으로 만료된 데이터 정리
dataStore->cleanExpiredData();
```

**4. 접근 제어**
```cpp
// 모듈별 데이터 접근 권한 관리
dataStore->setAccessPolicy("sensitive_data", "ModuleA", true);

if (dataStore->hasAccess("sensitive_data", "ModuleB")) {
    // ModuleB는 접근 불가
}
```

#### 알려진 이슈

**이슈 #004: 테스트 간 상태 공유**
- **문제**: Singleton 패턴으로 인한 테스트 격리 부족
- **증상**: 전체 테스트 실행 시 pthread mutex 오류
- **해결 방안**: `createForTest()` 메서드로 독립 인스턴스 생성
- **상세**: `issue/004-singleton-test-isolation.md` 참조

### EventBus: 비동기 이벤트 처리

**목적**: 실시간 실행 상태 모니터링 및 느슨한 결합

#### 아키텍처

```
┌──────────────────────────────────────────────────────────┐
│                       EventBus                            │
│  ┌────────────────────────────────────────────────┐     │
│  │         Lock-Free Queue (SPSC)                 │     │
│  │  ┌──────┬──────┬──────┬──────┬──────┬──────┐  │     │
│  │  │ Evt1 │ Evt2 │ Evt3 │ Evt4 │ Evt5 │ ...  │  │     │
│  │  └──────┴──────┴──────┴──────┴──────┴──────┘  │     │
│  └────────────────────────────────────────────────┘     │
│         ↓                                  ↑             │
│  ┌─────────────┐                    ┌──────────┐        │
│  │  Dispatch   │                    │ Publish  │        │
│  │   Thread    │                    │ (Mutex)  │        │
│  └─────────────┘                    └──────────┘        │
│         ↓                                                │
│  ┌────────────────────────────────────────────────┐     │
│  │        SubscriptionManager                     │     │
│  │  ┌──────────────────────────────────────┐     │     │
│  │  │  EventType → Subscribers (weak_ptr)  │     │     │
│  │  └──────────────────────────────────────┘     │     │
│  └────────────────────────────────────────────────┘     │
└──────────────────────────────────────────────────────────┘
         ↓
   ┌──────────────────────────────────────────────┐
   │           Subscribers                        │
   │  • ExecutionTimeCollector                   │
   │  • StateTransitionLogger                    │
   │  • DataStoreEventAdapter                    │
   │  • External Monitoring Systems              │
   └──────────────────────────────────────────────┘
```

#### 주요 특징

**1. SPSC Lock-Free Queue → Mutex 전환**
```cpp
// 이슈 #001에서 발견된 문제
// SPSC (Single-Producer Single-Consumer)는 multi-producer 환경에서 크래시
// 해결: Mutex로 보호된 multi-producer 지원

class EventBus {
private:
    LockFreeQueue<std::shared_ptr<IEvent>> eventQueue_;  // SPSC 큐
    std::mutex publishMutex_;  // ✅ multi-producer 보호

public:
    void publish(std::shared_ptr<IEvent> event) {
        std::lock_guard<std::mutex> lock(publishMutex_);  // ✅ 안전
        if (!eventQueue_.push(event)) {
            stats_.dropped++;
        }
    }
};
```

**2. 이벤트 타입 및 DTO**
```cpp
// Action 이벤트
struct ActionStartedEvent : public IEvent {
    std::string actionId;
    std::string actionType;
    std::chrono::system_clock::time_point timestamp;
};

struct ActionCompletedEvent : public IEvent {
    std::string actionId;
    long duration;  // milliseconds
    std::any result;
};

// Sequence 이벤트
struct SequenceProgressUpdatedEvent : public IEvent {
    std::string sequenceId;
    float progress;  // 0.0 ~ 1.0
    int currentStep;
    int totalSteps;
};

// Task 이벤트
struct TaskScheduledEvent : public IEvent {
    std::string taskId;
    TaskExecutionMode mode;  // ONCE, PERIODIC, TRIGGERED
};
```

**3. 구독 및 필터링**
```cpp
// 타입 기반 구독
auto subId = eventBus->subscribe(
    [](auto e) { return e->getType() == EventType::ACTION_COMPLETED; },
    [](std::shared_ptr<IEvent> event) {
        auto actionEvent = std::static_pointer_cast<ActionCompletedEvent>(event);
        spdlog::info("Action {} completed in {}ms",
                     actionEvent->actionId, actionEvent->duration);
    }
);

// Predicate 기반 필터링
auto subId2 = eventBus->subscribe(
    [](auto e) {
        if (e->getType() != EventType::SEQUENCE_PROGRESS_UPDATED) return false;
        auto progEvent = std::static_pointer_cast<SequenceProgressUpdatedEvent>(e);
        return progEvent->progress >= 0.5f;  // 50% 이상만
    },
    [](std::shared_ptr<IEvent> event) { /* ... */ }
);
```

**4. 구독자 예외 격리**
```cpp
void EventBus::dispatchLoop() {
    while (running_) {
        std::shared_ptr<IEvent> event;
        if (eventQueue_.pop(event)) {
            for (auto& [id, sub] : subscriptions_) {
                try {
                    if (sub.filter(event)) {
                        sub.callback(event);  // ✅ 예외 격리
                    }
                } catch (const std::exception& e) {
                    stats_.failedCallbacks++;
                    spdlog::error("Subscriber {} threw: {}", id, e.what());
                }
            }
        }
    }
}
```

#### DataStore ↔ EventBus 양방향 연동

**DataStoreEventAdapter**
```cpp
class DataStoreEventAdapter {
public:
    // 1. DataStore 변경 → EventBus 이벤트 발행
    void watchDataStore(const std::string& keyPattern) {
        auto observer = std::make_shared<DataStoreObserver>(
            [this, keyPattern](const SharedData& data) {
                if (matches(data.id, keyPattern)) {
                    auto event = std::make_shared<DataStoreValueChangedEvent>();
                    event->key = data.id;
                    event->value = data.value;
                    eventBus_->publish(event);  // ✅ 이벤트 발행
                }
            }
        );
        dataStore_->subscribe(keyPattern, observer);
    }

    // 2. EventBus 이벤트 → DataStore 자동 저장
    void saveEventToDataStore(EventType type, const std::string& prefix) {
        auto subId = eventBus_->subscribe(
            [type](auto e) { return e->getType() == type; },
            [this, prefix](std::shared_ptr<IEvent> event) {
                if (auto actionEvent = std::dynamic_pointer_cast<ActionCompletedEvent>(event)) {
                    std::string key = prefix + actionEvent->actionId;
                    dataStore_->set(key, actionEvent->result, DataType::Event);  // ✅ 자동 저장
                }
            }
        );
    }

    // 3. 순환 업데이트 방지
    void updateDataStore(const std::string& key, const std::any& value) {
        if (isUpdatingFromEvent_) return;  // ✅ 순환 방지

        isUpdatingFromEvent_ = true;
        dataStore_->set(key, value, DataType::Para);
        isUpdatingFromEvent_ = false;
    }
};
```

#### 성능 특성

**1. 처리량**
- Lock-Free Queue: 10,000+ events/sec
- Mutex 오버헤드: <5%
- 이벤트 지연: <10ms (평균)

**2. 메모리 효율**
```cpp
// 큐 용량 설정
auto eventBus = std::make_shared<EventBus>(10000);  // 10K 이벤트 버퍼

// 오버플로우 시 드롭 정책
EventBusStats stats = eventBus->getStats();
spdlog::warn("Dropped {} events", stats.dropped);
```

**3. 구독자 관리 (weak_ptr)**
```cpp
class SubscriptionManager {
private:
    struct Subscription {
        std::weak_ptr<void> owner;  // ✅ 구독자 생명주기 추적
        std::function<bool(std::shared_ptr<IEvent>)> filter;
        std::function<void(std::shared_ptr<IEvent>)> callback;
    };

    // 자동 정리
    void cleanupExpiredSubscriptions() {
        for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ) {
            if (it->second.owner.expired()) {
                it = subscriptions_.erase(it);  // ✅ 자동 제거
            } else {
                ++it;
            }
        }
    }
};
```

#### 사용 예시

**실시간 실행 모니터링**
```cpp
// 1. EventBus 생성 및 시작
auto eventBus = std::make_shared<EventBus>(10000);
eventBus->start();

// 2. Executor에 EventBus 주입
auto actionExecutor = std::make_shared<ActionExecutor>(eventBus);
auto sequenceEngine = std::make_shared<SequenceEngine>(actionExecutor, eventBus);
auto taskExecutor = std::make_shared<TaskExecutor>(sequenceEngine, eventBus);

// 3. 실행 시간 수집
auto collector = std::make_shared<ExecutionTimeCollector>();
eventBus->subscribe(
    [](auto e) {
        return e->getType() == EventType::ACTION_COMPLETED ||
               e->getType() == EventType::SEQUENCE_COMPLETED;
    },
    [collector](std::shared_ptr<IEvent> event) {
        collector->record(event);
    }
);

// 4. 상태 전이 로깅
eventBus->subscribe(
    [](auto e) { return true; },  // 모든 이벤트
    [](std::shared_ptr<IEvent> event) {
        spdlog::info("[{}] Event: {}",
                     event->getTimestamp(), event->getType());
    }
);

// 5. Task 실행
TaskDefinition taskDef("task1", "My Task");
taskDef.setWork("MyAction").setOnceMode();
taskExecutor->execute(taskDef, context);

// → ActionStarted 이벤트 발행
// → ActionCompleted 이벤트 발행
// → ExecutionTimeCollector 자동 기록
// → 로그 자동 출력

// 6. 종료
eventBus->stop();
```

#### 알려진 이슈

**이슈 #001: SPSC → Mutex 전환**
- **문제**: Multi-producer 환경에서 SPSC 큐 크래시
- **증상**: `std::terminate()` 또는 데이터 손실
- **해결**: Mutex로 보호된 publish 메서드
- **향후**: MPSC Lock-Free Queue 최적화 예정
- **상세**: `TROUBLESHOOTING.md` 참조

---

## 통합 시나리오

### 시나리오 1: Action 실행 및 모니터링

```cpp
// 1. 시스템 초기화
auto dataStore = DataStore::create();
auto eventBus = std::make_shared<EventBus>(10000);
eventBus->start();

// 2. DataStore ↔ EventBus 연동
auto adapter = std::make_shared<DataStoreEventAdapter>(dataStore, eventBus);
adapter->watchDataStore("robot.*");  // robot.* 키 변경 감지
adapter->saveEventToDataStore(EventType::ACTION_COMPLETED, "action.results.");

// 3. Action 실행
auto executor = std::make_shared<ActionExecutor>(eventBus);
auto action = std::make_shared<MoveAction>("move1", 10, 20);

executor->executeAsync(action, context);
// → ActionStarted 이벤트 발행
// → EventBus 디스패치 스레드가 구독자에게 전달
// → 로그 출력: "Action move1 started"

// 4. Action 완료
executor->waitForCompletion("move1");
// → ActionCompleted 이벤트 발행
// → DataStore에 자동 저장: "action.results.move1"
// → 로그 출력: "Action move1 completed in 150ms"

// 5. DataStore에서 결과 조회
auto result = dataStore->get<std::any>("action.results.move1");
```

### 시나리오 2: Sequence 실행 및 진행률 추적

```cpp
// 1. Sequence 정의
SequenceDefinition seqDef("seq1", "Move and Rotate");
seqDef.addStep(ActionStep("step1", "Move").addParameter("x", "10"));
seqDef.addStep(ActionStep("step2", "Rotate").addParameter("angle", "90"));

// 2. 진행률 모니터링
eventBus->subscribe(
    [](auto e) { return e->getType() == EventType::SEQUENCE_PROGRESS_UPDATED; },
    [dataStore](std::shared_ptr<IEvent> event) {
        auto progEvent = std::static_pointer_cast<SequenceProgressUpdatedEvent>(event);
        // DataStore에 진행률 저장
        dataStore->set("sequence.progress." + progEvent->sequenceId,
                       progEvent->progress, DataType::Para);
    }
);

// 3. Sequence 실행
auto sequenceEngine = std::make_shared<SequenceEngine>(actionExecutor, eventBus);
sequenceEngine->start(seqDef, context);

// → SequenceStarted 이벤트
// → ActionStarted (step1) 이벤트
// → SequenceProgressUpdated (0.0 → 0.5) 이벤트
// → ActionCompleted (step1) 이벤트
// → ActionStarted (step2) 이벤트
// → SequenceProgressUpdated (0.5 → 1.0) 이벤트
// → ActionCompleted (step2) 이벤트
// → SequenceCompleted 이벤트

// 4. 외부 시스템에서 진행률 조회
float progress = dataStore->get<float>("sequence.progress.seq1");
```

### 시나리오 3: Task 주기적 실행 및 데이터 수집

```cpp
// 1. Task 정의 (PERIODIC 모드)
TaskDefinition taskDef("periodic_task", "Sensor Reading");
taskDef.setWork("ReadSensor")
       .setPeriodicMode(std::chrono::milliseconds(1000));  // 1초마다

// 2. 센서 데이터 자동 저장
adapter->saveEventToDataStore(EventType::ACTION_COMPLETED, "sensor.data.");

// 3. Task 시작
auto taskExecutor = std::make_shared<TaskExecutor>(sequenceEngine, eventBus);
taskExecutor->execute(taskDef, context);

// → 1초마다 TaskScheduled 이벤트
// → ActionStarted (ReadSensor) 이벤트
// → ActionCompleted 이벤트
// → DataStore에 자동 저장: "sensor.data.ReadSensor"

// 4. 데이터 히스토리 조회
auto sensorValue = dataStore->get<int>("sensor.data.ReadSensor");

// 5. 조건 기반 알림
eventBus->subscribe(
    [dataStore](auto e) {
        if (e->getType() != EventType::ACTION_COMPLETED) return false;
        auto actionEvent = std::static_pointer_cast<ActionCompletedEvent>(e);
        if (actionEvent->actionId != "ReadSensor") return false;

        // DataStore에서 임계값 확인
        int threshold = dataStore->get<int>("sensor.threshold");
        int value = std::any_cast<int>(actionEvent->result);
        return value > threshold;  // 임계값 초과 시만
    },
    [](std::shared_ptr<IEvent> event) {
        spdlog::warn("Sensor threshold exceeded!");
        // 알람 발생
    }
);
```

---

## 설계 원칙 재확인

### 1. RAII (Resource Acquisition Is Initialization)
- ✅ DataStore: shared_ptr 기반 자동 메모리 관리
- ✅ EventBus: 디스패치 스레드 자동 종료
- ✅ concurrent_hash_map accessor: RAII 패턴

### 2. 느슨한 결합 (Loose Coupling)
- ✅ EventBus는 선택적 의존성 (nullptr 허용)
- ✅ DataStore는 계층 독립적
- ✅ Observer 패턴으로 Publisher-Subscriber 분리

### 3. 스레드 안전성
- ✅ concurrent_hash_map: 세분화된 락
- ✅ weak_ptr: dangling pointer 방지
- ✅ Mutex: 비-크리티컬 경로 보호

### 4. 성능 최적화
- ✅ 전역 락 제거 (10배 향상)
- ✅ Lock-Free Queue (10,000+ ops/sec)
- ✅ 이벤트 오버헤드 <5%

### 5. 테스트 가능성
- ✅ createForTest(): 테스트 격리
- ✅ 모의 객체 주입 가능
- ✅ 독립적 단위 테스트

---

