# 018: ActionExecutor Stateful 리팩토링

## 개요

ActionExecutor를 Stateful한 독립적인 모듈로 리팩토링하여 액션의 전체 생명주기를 책임지게 하고, SequenceEngine과의 결합도를 낮춥니다.

## 목표

1. **모듈 독립성 향상**: ActionExecutor가 실행 중인 액션들을 스스로 관리
2. **단일 책임 원칙 (SRP) 준수**: ActionExecutor가 액션 생명주기 전체를 책임
3. **결합도 감소**: SequenceEngine은 actionId만으로 액션 제어 가능
4. **미래 확장성**: 이벤트 기반 아키텍처로의 전환 준비

## 현재 아키텍처의 문제점

### 1. 높은 결합도
```cpp
// SequenceEngine이 currentAction 포인터를 직접 관리
std::shared_ptr<IAction> currentAction;
executor_->cancel(currentAction);  // IAction 포인터 필요
```

- SequenceEngine이 액션 포인터를 직접 관리해야 함
- 두 모듈 간의 책임이 명확하지 않음

### 2. 상태 관리의 분산
- 액션 상태가 SequenceEngine과 ActionExecutor에 분산
- 동기화 문제 발생 가능성

## 개선 방안

### 1. ActionExecutor Stateful 전환

```cpp
class ActionExecutor {
private:
    struct ExecutionState {
        std::shared_ptr<IAction> action;
        std::future<void> future;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::milliseconds timeout;
        std::atomic<bool> cancelRequested{false};
    };

    std::map<std::string, ExecutionState> runningActions_;
    std::mutex actionsMutex_;

public:
    // actionId 기반 인터페이스
    std::string executeAsync(std::shared_ptr<IAction> action,
                              ExecutionContext& context,
                              std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    void cancel(const std::string& actionId);
    bool isRunning(const std::string& actionId) const;
    ExecutionResult getResult(const std::string& actionId);
    void waitForCompletion(const std::string& actionId);
};
```

### 2. SequenceEngine 단순화

```cpp
// Before
{
    std::lock_guard<std::mutex> actionLock(state.currentActionMutex);
    state.currentAction = action;
}
executor_->execute(action, context, step.timeout);

// After
std::string actionId = executor_->executeAsync(action, context, step.timeout);
// 필요시
executor_->cancel(actionId);
```

## 기능 요구사항

### FR1: 액션 상태 관리
- ActionExecutor는 실행 중인 모든 액션의 상태를 관리
- actionId를 키로 하는 내부 맵 사용
- 스레드 안전성 보장 (mutex 사용)

### FR2: 비동기 실행 인터페이스
- `executeAsync()`: 액션을 비동기로 실행하고 actionId 반환
- 반환된 actionId로 액션 제어 가능

### FR3: actionId 기반 제어
- `cancel(actionId)`: actionId로 액션 취소
- `isRunning(actionId)`: 실행 상태 확인
- `getResult(actionId)`: 실행 결과 조회
- `waitForCompletion(actionId)`: 완료 대기

### FR4: 자동 정리
- 완료된 액션은 자동으로 내부 맵에서 제거
- 메모리 누수 방지

### FR5: 기존 동기 API 유지
- 기존 `execute()` 메서드는 호환성을 위해 유지
- 내부적으로 `executeAsync()` + `waitForCompletion()` 사용

## 비기능 요구사항

### NFR1: 성능
- 액션 조회 시간: O(log n)
- 락 경합 최소화

### NFR2: 스레드 안전성
- 모든 public 메서드는 스레드 안전
- 데드락 방지

### NFR3: 하위 호환성
- 기존 테스트 모두 통과
- 기존 API 유지

## 성공 기준

1. ✅ ActionExecutor가 액션 상태를 독립적으로 관리
2. ✅ SequenceEngine에서 currentAction 관련 코드 제거
3. ✅ actionId 기반 인터페이스로 액션 제어
4. ✅ 기존 테스트 164+ 개 모두 통과
5. ✅ 새로운 테스트 5+ 개 추가 (비동기 API 테스트)

## 구현 단계

### Phase 1: ActionExecutor 내부 상태 관리 추가
- ExecutionState 구조체 정의
- runningActions_ 맵 추가
- 스레드 안전성 확보

### Phase 2: 비동기 API 구현
- executeAsync() 구현
- cancel(actionId) 구현
- isRunning(), getResult(), waitForCompletion() 구현

### Phase 3: 기존 execute() 리팩토링
- executeAsync() 사용하도록 변경
- 하위 호환성 유지

### Phase 4: SequenceEngine 리팩토링
- currentAction 관련 코드 제거
- actionId 기반으로 변경

### Phase 5: 테스트 및 검증
- 새로운 비동기 API 테스트 추가
- 기존 테스트 모두 통과 확인

## 테스트 계획

### 단위 테스트
1. `ActionExecutorTest::ExecuteAsyncReturnsActionId`
2. `ActionExecutorTest::CancelByActionId`
3. `ActionExecutorTest::IsRunningCheck`
4. `ActionExecutorTest::GetResultAfterCompletion`
5. `ActionExecutorTest::WaitForCompletionBlocks`
6. `ActionExecutorTest::AutoCleanupCompletedActions`
7. `ActionExecutorTest::BackwardCompatibility`

### 통합 테스트
1. `SequenceEngineTest::AsyncExecutionWithCancellation`
2. `SequenceEngineTest::MultipleActionsManagement`

## 위험 요소 및 대응

### 위험 1: 기존 코드 호환성
**대응**: 기존 execute() API 유지, 내부만 변경

### 위험 2: 성능 저하
**대응**: 벤치마크 테스트로 성능 검증

### 위험 3: 메모리 누수
**대응**: 자동 정리 로직 및 테스트 추가

## 향후 확장 방향

이 리팩토링은 다음 단계로의 기반이 됩니다:

### Phase 6 (미래): 이벤트 기반 아키텍처
- ActionExecutor가 이벤트 발행 (ActionCompleted, ActionFailed)
- SequenceEngine은 이벤트 구독자로 동작
- 완전한 논블로킹 시스템 구현
