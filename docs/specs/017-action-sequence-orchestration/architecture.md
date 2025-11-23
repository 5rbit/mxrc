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

