# 동작 시퀀스 시스템 아키텍처 설계

## 개요

동작 시퀀스 관리 시스템은 모듈화된 아키텍처로 설계되어 독립적으로 테스트 가능하면서도,
기존 TaskManager 시스템과 통합될 수 있도록 구성됩니다.

---

## 아키텍처 계층

```
┌─────────────────────────────────────────────────────────┐
│         TaskManager Integration Layer                   │
│  (SequenceTaskAdapter - TaskManager와의 연결)           │
└─────────────────────────────────────────────────────────┘
                          ↑
┌─────────────────────────────────────────────────────────┐
│         Action Sequence Orchestration Layer             │
│  (핵심 시퀀스 실행 엔진 - TaskManager에 독립적)         │
├─────────────────────────────────────────────────────────┤
│ • SequenceEngine (시퀀스 실행 엔진)                     │
│ • SequenceRegistry (시퀀스 정의 관리)                   │
│ • ActionExecutor (동작 실행)                           │
│ • ConditionEvaluator (조건 평가)                       │
│ • RetryHandler (재시도 처리)                           │
│ • ExecutionMonitor (모니터링)                          │
└─────────────────────────────────────────────────────────┘
                          ↑
┌─────────────────────────────────────────────────────────┐
│              Interfaces & DTOs                           │
│  (인터페이스 기반 설계로 확장성 보장)                    │
├─────────────────────────────────────────────────────────┤
│ • IAction (동작 인터페이스)                             │
│ • IConditionProvider (조건 제공자)                      │
│ • IActionFactory (동작 생성 팩토리)                     │
│ • SequenceDefinition (시퀀스 정의 DTO)                 │
│ • ExecutionContext (실행 컨텍스트)                     │
└─────────────────────────────────────────────────────────┘
```

---

## 핵심 컴포넌트

### 1. 인터페이스 계층 (`interfaces/`)

#### IAction
```cpp
namespace mxrc::core::sequence {

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

#### IConditionProvider
```cpp
class IConditionProvider {
public:
    virtual ~IConditionProvider() = default;
    virtual bool evaluate(const std::string& expression, 
                         const ExecutionContext& context) = 0;
};
```

#### IActionFactory
```cpp
class IActionFactory {
public:
    virtual ~IActionFactory() = default;
    virtual std::shared_ptr<IAction> createAction(
        const std::string& type,
        const std::map<std::string, std::string>& params) = 0;
};
```

### 2. 핵심 클래스 (`core/`)

#### SequenceRegistry
- 시퀀스 정의 등록 및 관리
- 시퀀스 조회, 목록 반환
- 버전 관리

#### SequenceEngine
- 시퀀스 실행 제어 (시작, 일시정지, 재개, 취소)
- 상태 관리 (PENDING, RUNNING, PAUSED, COMPLETED, FAILED)
- 실행 흐름 조정 (순차, 병렬, 조건부)

#### ActionExecutor
- 개별 동작 실행
- 타임아웃 관리
- 실행 결과 수집

#### ConditionEvaluator
- 조건식 평가
- 이전 동작 결과 참조
- 논리 연산 (AND, OR, NOT)

#### RetryHandler
- 재시도 정책 관리
- 지수 백오프 계산
- 에러 핸들링

#### ExecutionMonitor
- 진행률 추적
- 로그 기록
- 상태 조회 인터페이스

### 3. TaskManager 통합 계층 (`integration/`)

#### SequenceTaskAdapter
- ITask 인터페이스 구현
- SequenceEngine을 ITask로 래핑
- TaskManager와의 연결

```cpp
class SequenceTaskAdapter : public taskmanager::ITask {
public:
    SequenceTaskAdapter(std::shared_ptr<SequenceEngine> engine);
    
    void execute() override;  // engine->start()
    void cancel() override;   // engine->cancel()
    void pause() override;    // engine->pause()
    
    TaskStatus getStatus() const override;
    float getProgress() const override;
    // ... other ITask methods
    
private:
    std::shared_ptr<SequenceEngine> engine_;
};
```

---

## 디렉토리 구조

```
src/core/sequence/
├── interfaces/
│   ├── IAction.h
│   ├── IConditionProvider.h
│   ├── IActionFactory.h
│   └── ISequenceEngine.h
├── core/
│   ├── SequenceDefinition.h
│   ├── SequenceDefinition.cpp
│   ├── SequenceRegistry.h
│   ├── SequenceRegistry.cpp
│   ├── SequenceEngine.h
│   ├── SequenceEngine.cpp
│   ├── ActionExecutor.h
│   ├── ActionExecutor.cpp
│   ├── ConditionEvaluator.h
│   ├── ConditionEvaluator.cpp
│   ├── RetryHandler.h
│   ├── RetryHandler.cpp
│   ├── ExecutionMonitor.h
│   ├── ExecutionMonitor.cpp
│   └── ExecutionContext.h
├── integration/
│   ├── SequenceTaskAdapter.h
│   ├── SequenceTaskAdapter.cpp
│   └── SequenceManagerInit.cpp
├── dto/
│   ├── ActionStatus.h
│   ├── ExecutionResult.h
│   └── SequenceDto.h
└── impl/
    ├── BasicAction.h
    ├── BasicAction.cpp
    ├── BasicActionFactory.h
    └── BasicActionFactory.cpp

tests/unit/sequence/
├── SequenceRegistry_test.cpp
├── SequenceEngine_test.cpp
├── ActionExecutor_test.cpp
├── ConditionEvaluator_test.cpp
├── RetryHandler_test.cpp
├── ExecutionMonitor_test.cpp
├── SequenceTaskAdapter_test.cpp
└── integration_test.cpp
```

---

## 설계 원칙

### 1. 인터페이스 기반 설계
- 모든 확장 지점에 인터페이스 사용
- 의존성 주입(Dependency Injection)
- 느슨한 결합(Loose Coupling)

### 2. 단위 테스트 우선
- 모든 클래스는 테스트 가능하게 설계
- Mock 객체 사용 가능
- 의존성 최소화

### 3. RAII 원칙 준수
- 자동 리소스 관리 (스마트 포인터)
- 메모리 누수 방지
- 예외 안전성 보장

### 4. TaskManager 통합
- Adapter 패턴 사용
- 기존 TaskManager에 영향 없음
- 선택적 통합 가능

---

## 단위 테스트 전략

### 테스트 레벨

#### Level 1: 단위 테스트 (Unit Tests)
- 각 클래스의 책임별 테스트
- Mock/Stub 사용
- 독립적 테스트

```
✓ SequenceRegistry: 등록, 조회, 업데이트
✓ ActionExecutor: 동작 실행, 타임아웃, 에러
✓ ConditionEvaluator: 조건식 평가, 참조
✓ RetryHandler: 재시도 정책, 백오프
✓ ExecutionMonitor: 진행률, 로깅
✓ SequenceEngine: 상태 관리, 흐름 제어
```

#### Level 2: 통합 테스트 (Integration Tests)
- 컴포넌트 간 상호작용 테스트
- 실제 시퀀스 실행
- 시나리오 기반 테스트

```
✓ 순차 실행 시나리오
✓ 조건부 분기 시나리오
✓ 병렬 실행 시나리오
✓ 재시도 및 에러 처리
✓ 일시정지/재개
```

#### Level 3: TaskManager 통합 테스트 (Integration Tests)
- SequenceTaskAdapter 테스트
- TaskManager와의 상호작용
- 기존 시스템과의 호환성

---

## TaskManager 통합 시나리오

### Phase 1: 독립 개발 (현재)
```
┌──────────────────┐
│ Sequence System  │
│ (독립적 개발)    │
└──────────────────┘
```

### Phase 2: 어댑터 통합 (선택적)
```
┌──────────────────────────────────────┐
│ TaskManager                          │
├──────────────────────────────────────┤
│ SequenceTaskAdapter (ITask 구현)     │
├──────────────────────────────────────┤
│ SequenceEngine                       │
│ (독립적으로 작동)                    │
└──────────────────────────────────────┘
```

### Phase 3: 완전 통합
```
┌──────────────────────────────────────┐
│ OperatorInterface                    │
├──────────────────────────────────────┤
│ TaskManager                          │
├──────────────────────────────────────┤
│ SequenceTaskAdapter ←→ SequenceEngine│
│ (완전히 통합된 시스템)               │
└──────────────────────────────────────┘
```

---

## 코드 스타일 및 규칙

### 네임스페이스
```cpp
namespace mxrc::core::sequence {
    // 모든 시퀀스 시스템 코드
}
```

### 헤더 의존성
- 인터페이스 헤더만 포함 (구현 X)
- Forward declaration 활용
- 컴파일 시간 최소화

### 테스트 명명
```cpp
// <ComponentName>_test.cpp
// TEST(<ComponentName>Test, <Scenario>)

TEST(SequenceEngineTest, StartSequenceChangesStateToRunning)
TEST(ActionExecutorTest, TimeoutCancelsAction)
```

---

## 구현 순서

1. **인터페이스 정의** (interfaces/)
2. **DTO 및 도우미 클래스** (dto/, types)
3. **핵심 컴포넌트** (core/)
   - SequenceRegistry
   - ActionExecutor
   - ConditionEvaluator
   - RetryHandler
4. **통합 컴포넌트** (core/)
   - SequenceEngine
   - ExecutionMonitor
5. **단위 테스트** (tests/)
6. **TaskManager 어댑터** (integration/)
7. **통합 테스트** (tests/)

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

