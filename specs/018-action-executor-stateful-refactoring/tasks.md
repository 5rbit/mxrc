# 018: ActionExecutor Stateful 리팩토링 - 작업 목록

## Phase 1: ActionExecutor 내부 상태 관리 추가

### US1.1: ExecutionState 구조체 정의
**목표**: 실행 중인 액션의 상태를 저장할 구조체 정의

- [ ] T001: ExecutionState 구조체 정의
  - action 포인터
  - future 객체
  - startTime, timeout
  - cancelRequested 플래그

- [ ] T002: runningActions_ 맵 추가
  - std::map<std::string, ExecutionState>
  - actionsMutex_ 추가

- [ ] T003: 단위 테스트 작성
  - ExecutionState 생성 및 초기화 테스트

**완료 조건**: ExecutionState 구조체와 맵이 정의되고 테스트 통과

---

## Phase 2: 비동기 API 구현

### US2.1: executeAsync() 메서드 구현
**목표**: 액션을 비동기로 실행하고 actionId 반환

- [ ] T004: executeAsync() 시그니처 정의
  ```cpp
  std::string executeAsync(std::shared_ptr<IAction> action,
                            ExecutionContext& context,
                            std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
  ```

- [ ] T005: 비동기 실행 로직 구현
  - std::async로 액션 실행
  - ExecutionState 생성 및 맵에 저장
  - actionId 생성 및 반환

- [ ] T006: 타임아웃 모니터링 스레드 구현
  - 백그라운드에서 타임아웃 체크
  - 타임아웃 시 자동 취소

- [ ] T007: 단위 테스트 작성
  - ExecuteAsyncReturnsActionId
  - AsyncExecutionCompletes

**완료 조건**: executeAsync() 구현 완료 및 테스트 통과

### US2.2: cancel(actionId) 메서드 구현
**목표**: actionId로 실행 중인 액션 취소

- [ ] T008: cancel(actionId) 시그니처 정의
  ```cpp
  void cancel(const std::string& actionId);
  ```

- [ ] T009: 취소 로직 구현
  - runningActions_에서 액션 조회
  - action->cancel() 호출
  - cancelRequested 플래그 설정

- [ ] T010: 단위 테스트 작성
  - CancelByActionId
  - CancelNonExistentAction

**완료 조건**: cancel(actionId) 구현 완료 및 테스트 통과

### US2.3: 상태 조회 메서드 구현
**목표**: 액션 상태 조회 API 제공

- [ ] T011: isRunning() 구현
  ```cpp
  bool isRunning(const std::string& actionId) const;
  ```

- [ ] T012: getResult() 구현
  ```cpp
  ExecutionResult getResult(const std::string& actionId);
  ```

- [ ] T013: waitForCompletion() 구현
  ```cpp
  void waitForCompletion(const std::string& actionId);
  ```

- [ ] T014: 단위 테스트 작성
  - IsRunningCheck
  - GetResultAfterCompletion
  - WaitForCompletionBlocks

**완료 조건**: 모든 조회 메서드 구현 및 테스트 통과

### US2.4: 자동 정리 메커니즘
**목표**: 완료된 액션 자동 제거

- [ ] T015: 완료 콜백 구현
  - 액션 완료 시 맵에서 제거
  - 메모리 누수 방지

- [ ] T016: 단위 테스트 작성
  - AutoCleanupCompletedActions
  - NoMemoryLeak

**완료 조건**: 자동 정리 구현 및 메모리 누수 없음 확인

---

## Phase 3: 기존 execute() 리팩토링

### US3.1: execute() 내부 구현 변경
**목표**: 기존 execute()가 executeAsync() 사용하도록 변경

- [ ] T017: execute() 리팩토링
  - executeAsync() 호출
  - waitForCompletion() 호출
  - 결과 반환

- [ ] T018: 하위 호환성 테스트
  - 기존 ActionExecutor 테스트 모두 통과
  - BackwardCompatibility 테스트

**완료 조건**: 기존 API 호환성 유지 및 모든 테스트 통과

---

## Phase 4: SequenceEngine 리팩토링

### US4.1: currentAction 관련 코드 제거
**목표**: SequenceEngine에서 액션 포인터 관리 코드 제거

- [ ] T019: SequenceState 리팩토링
  - currentAction, currentActionMutex 제거

- [ ] T020: executeSequential() 수정
  - executeAsync() 사용
  - actionId로 취소 처리

- [ ] T021: cancel() 메서드 단순화
  - actionId 기반 취소로 변경
  - 락 중첩 제거

- [ ] T022: handleConditionalBranch() 수정
  - 비동기 실행 적용

**완료 조건**: SequenceEngine 리팩토링 완료

### US4.2: 통합 테스트
**목표**: SequenceEngine과 ActionExecutor 통합 동작 확인

- [ ] T023: 통합 테스트 작성
  - AsyncExecutionWithCancellation
  - MultipleActionsManagement
  - SequenceWithAsyncActions

**완료 조건**: 모든 통합 테스트 통과

---

## Phase 5: 테스트 및 검증

### US5.1: 전체 테스트 실행
**목표**: 모든 테스트 통과 확인

- [ ] T024: 단위 테스트 실행
  - Action layer: 기존 테스트 + 7개 신규
  - Sequence layer: 기존 테스트 + 3개 신규

- [ ] T025: 통합 테스트 실행
  - 전체 시스템 통합 테스트

- [ ] T026: 성능 테스트
  - 액션 조회 시간 측정
  - 메모리 사용량 확인

**완료 조건**: 170+ 테스트 모두 통과

### US5.2: 코드 리뷰 및 문서화
**목표**: 코드 품질 확보 및 문서 업데이트

- [ ] T027: 코드 리뷰
  - 스레드 안전성 검증
  - 메모리 누수 확인

- [ ] T028: 문서 업데이트
  - API 문서 작성
  - architecture.md 업데이트

**완료 조건**: 리뷰 완료 및 문서 업데이트

---

## 진행 상황 요약

| Phase | 사용자 스토리 | 작업 | 완료 | 진행률 |
|-------|--------------|------|------|--------|
| Phase 1 | US1.1 | T001-T003 | 0/3 | 0% |
| Phase 2 | US2.1-US2.4 | T004-T016 | 0/13 | 0% |
| Phase 3 | US3.1 | T017-T018 | 0/2 | 0% |
| Phase 4 | US4.1-US4.2 | T019-T023 | 0/5 | 0% |
| Phase 5 | US5.1-US5.2 | T024-T028 | 0/5 | 0% |
| **전체** | **10개** | **28개** | **0/28** | **0%** |

## 예상 테스트 수

- 기존 테스트: 164개
- 신규 단위 테스트: 10개
- 신규 통합 테스트: 3개
- **총 예상**: 177개 테스트
