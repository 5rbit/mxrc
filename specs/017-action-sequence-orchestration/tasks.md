# 구현 작업 목록: 동작 시퀀스 관리 시스템

**특성**: Action Sequence Orchestration (동작 시퀀스 관리 시스템)
**계획**: `specs/017-action-sequence-orchestration/plan.md`
**사양**: `specs/017-action-sequence-orchestration/spec.md`
**기본 브랜치**: `017-action-sequence-orchestration`

---

## 작업 실행 순서 및 의존성

```
Phase 1: Setup (기초 설정)
    ↓
Phase 2: Foundational (핵심 기초 모듈)
    ↓
Phase 3: US1 - Basic Sequential Execution (순차 실행 - P1)
    ↓
Phase 4: US2 - Conditional Flow Control (조건부 분기 - P1)
    ├─→ Phase 5: US3 - Parallel Execution (병렬 실행 - P2)
    └─→ Phase 6: US4 - Retry & Error Handling (재시도/에러 처리 - P2)
    ↓
Phase 7: US5 - Sequence Templates (시퀀스 템플릿 - P3)
    ↓
Phase 8: US6 - Monitoring & Control (모니터링 제어 - P3)
    ↓
Phase 9: Polish & Integration (마무리 및 통합)

**병렬 실행 가능**:
- Phase 3과 Phase 5는 독립적 (다른 파일)
- Phase 5와 Phase 6는 독립적 (병렬 실행 가능)
- Phase 7과 Phase 8은 Phase 6 완료 후 병렬 가능
```

---

## Phase 1: 설정 (Setup)

### 목표
프로젝트 구조, 빌드 설정, 기본 로깅/테스트 프레임워크 구성

### 작업

- [ ] T001 Create CMakeLists.txt configuration for sequence module with test discovery in `CMakeLists.txt`
- [ ] T002 Set up spdlog logging infrastructure for sequence system in `src/core/sequence/core/Logger.h`
- [ ] T003 Create base test fixtures and utilities for sequence tests in `tests/unit/sequence/SequenceTestFixtures.h`

---

## Phase 2: 기초 (Foundational)

### 목표
모든 사용자 스토리가 의존하는 핵심 컴포넌트 구현

### 작업

- [x] T004 Fix and enhance ActionStatus enum with all 7 states in `src/core/sequence/dto/ActionStatus.h`
- [x] T005 Create RetryPolicy struct for retry configuration in `src/core/sequence/core/RetryPolicy.h`
- [x] T006 Implement RetryHandler for managing retry logic in `src/core/sequence/core/RetryHandler.h` and `.cpp`
- [x] T007 Implement SequenceRegistry for sequence definition management in `src/core/sequence/core/SequenceRegistry.h` and `.cpp`
- [x] T008 Create SequenceRegistry unit tests in `tests/unit/sequence/SequenceRegistry_test.cpp`
- [x] T009 Implement ActionExecutor for individual action execution in `src/core/sequence/core/ActionExecutor.h` and `.cpp`
- [x] T010 Create ActionExecutor unit tests in `tests/unit/sequence/ActionExecutor_test.cpp`
- [x] T011 Implement ConditionEvaluator for condition expression evaluation in `src/core/sequence/core/ConditionEvaluator.h` and `.cpp`
- [x] T012 Create ConditionEvaluator unit tests in `tests/unit/sequence/ConditionEvaluator_test.cpp`
- [x] T013 Implement ExecutionMonitor for tracking sequence execution progress in `src/core/sequence/core/ExecutionMonitor.h` and `.cpp`
- [x] T014 Create ExecutionMonitor unit tests in `tests/unit/sequence/ExecutionMonitor_test.cpp`

---

## Phase 3: US1 - 순차 동작 실행 (Sequential Actions)

### 사용자 스토리
사용자가 동작 시퀀스를 정의하고 순차적으로 실행할 수 있습니다. 각 동작 완료 후 다음 동작이 시작됩니다.

### 성공 기준
1. 시퀀스가 정의된 순서대로 모든 동작을 완료
2. 각 동작 결과가 다음 동작에서 참조 가능
3. 모든 동작 완료 시 시퀀스 상태가 COMPLETED로 변경
4. 메모리 누수 없음 (RAII 원칙 준수)

### 독립 테스트 기준
- 3-5개 동작으로 구성된 시퀀스 정상 실행
- 동작 결과 검증 및 전달
- 시퀀스 상태 전이 검증

### 작업

- [ ] T015 [US1] Implement SequenceEngine core for sequential execution in `src/core/sequence/core/SequenceEngine.h`
- [ ] T016 [US1] Implement SequenceEngine sequential execution logic in `src/core/sequence/core/SequenceEngine.cpp`
- [ ] T017 [US1] Create mock action implementations for testing in `tests/unit/sequence/MockActions.h`
- [ ] T018 [US1] Create SequenceEngine unit tests for sequential execution in `tests/unit/sequence/SequenceEngine_test.cpp`
- [ ] T019 [US1] Create integration test for simple 3-action sequence in `tests/integration/sequence/SequenceIntegration_test.cpp`

---

## Phase 4: US2 - 조건부 분기 (Conditional Flow Control)

### 사용자 스토리
조건식을 기반으로 다양한 실행 경로를 선택할 수 있습니다. 이전 동작의 결과를 조건식에서 참조합니다.

### 성공 기준
1. IF-THEN-ELSE 조건문 지원
2. 조건식에서 이전 동작 결과 참조 가능
3. 비교 연산자 (==, !=, <, >, <=, >=) 지원
4. 논리 연산자 (AND, OR, NOT) 지원
5. 조건 평가 에러 처리

### 독립 테스트 기준
- 단순 조건 (가중치 > 10) 평가
- 복합 조건 (A AND B OR C) 평가
- 조건에 따른 경로 선택 검증

### 작업

- [ ] T020 [P] [US2] Enhance SequenceEngine with conditional branch support in `src/core/sequence/core/SequenceEngine.cpp`
- [ ] T021 [US2] Create condition expression structure in `src/core/sequence/core/ConditionalBranch.h`
- [ ] T022 [US2] Implement condition evaluation in SequenceEngine in `src/core/sequence/core/SequenceEngine.cpp`
- [ ] T023 [US2] Create unit tests for conditional evaluation in `tests/unit/sequence/SequenceEngine_test.cpp`
- [ ] T024 [US2] Create integration test for branching sequence in `tests/integration/sequence/SequenceIntegration_test.cpp`

---

## Phase 5: US3 - 병렬 동작 실행 (Parallel Execution)

### 사용자 스토리
여러 동작을 동시에 실행할 수 있습니다. 모든 병렬 동작이 완료될 때까지 다음 단계로 진행하지 않습니다.

### 성공 기준
1. 여러 동작을 병렬로 실행 가능
2. 모든 동작 완료 대기 (Join semantics)
3. 병렬 그룹 내 최대 32개 동작 지원
4. 병렬 동작 간 리소스 충돌 처리

### 독립 테스트 기준
- 2-4개 동작 병렬 실행
- 모든 동작 완료 확인
- 병렬 실행 성능 검증

### 작업

- [ ] T025 [P] [US3] Create ParallelGroup structure in `src/core/sequence/core/ParallelGroup.h`
- [ ] T026 [P] [US3] Implement parallel execution in SequenceEngine in `src/core/sequence/core/SequenceEngine.cpp`
- [ ] T027 [US3] Implement thread pool based execution for parallel groups in `src/core/sequence/core/ParallelExecutor.h` and `.cpp`
- [ ] T028 [US3] Create unit tests for parallel execution in `tests/unit/sequence/SequenceEngine_test.cpp`
- [ ] T029 [US3] Create integration test for parallel sequence in `tests/integration/sequence/SequenceIntegration_test.cpp`

---

## Phase 6: US4 - 재시도 및 에러 처리 (Retry & Error Handling)

### 사용자 스토리
동작 실패 시 자동으로 재시도할 수 있으며, 최종 실패 시 에러 핸들러를 실행합니다.

### 성공 기준
1. 동작 실패 시 자동 재시도 (최대 3회)
2. 재시도 간격 지수 백오프 (1ms, 2ms, 4ms)
3. 최종 실패 시 에러 핸들러 실행
4. 에러 복구 또는 안전한 종료

### 독립 테스트 기준
- 실패 동작 재시도 검증
- 지수 백오프 간격 확인
- 재시도 횟수 제한 검증

### 작업

- [ ] T030 [P] [US4] Create RetryPolicy configuration structure in `src/core/sequence/core/RetryPolicy.h`
- [ ] T031 [P] [US4] Enhance ActionExecutor with retry support in `src/core/sequence/core/ActionExecutor.cpp`
- [ ] T032 [US4] Create ErrorHandler interface in `src/core/sequence/interfaces/IErrorHandler.h`
- [ ] T033 [US4] Implement error recovery in SequenceEngine in `src/core/sequence/core/SequenceEngine.cpp`
- [ ] T034 [US4] Create unit tests for retry logic in `tests/unit/sequence/ActionExecutor_test.cpp`
- [ ] T035 [US4] Create integration test for error handling in `tests/integration/sequence/SequenceIntegration_test.cpp`

---

## Phase 7: US5 - 시퀀스 템플릿 (Sequence Templates)

### 사용자 스토리
공통 패턴을 템플릿으로 정의하고 다양한 파라미터로 재사용할 수 있습니다.

### 성공 기준
1. 템플릿 정의 및 등록 가능
2. 다양한 파라미터로 인스턴스화 가능
3. 각 인스턴스 독립적 실행
4. 결과 집계 가능

### 독립 테스트 기준
- "Pick and Place" 템플릿 생성 및 실행
- 다양한 좌표 파라미터 적용
- 템플릿 재사용성 검증

### 작업

- [ ] T036 [P] [US5] Create template instantiation mechanism in `src/core/sequence/core/SequenceTemplate.h`
- [ ] T037 [US5] Implement parameter substitution in SequenceEngine in `src/core/sequence/core/SequenceEngine.cpp`
- [ ] T038 [US5] Create template registry in SequenceRegistry in `src/core/sequence/core/SequenceRegistry.cpp`
- [ ] T039 [US5] Create unit tests for template system in `tests/unit/sequence/SequenceTemplate_test.cpp`
- [ ] T040 [US5] Create integration test for template instantiation in `tests/integration/sequence/SequenceIntegration_test.cpp`

---

## Phase 8: US6 - 모니터링 및 제어 (Monitoring & Control)

### 사용자 스토리
실행 중인 시퀀스의 상태를 모니터링하고, 필요시 일시정지, 재개, 취소할 수 있습니다.

### 성공 기준
1. 실시간 진행 상황 조회
2. 시퀀스 일시정지 및 재개
3. 시퀀스 취소 (안전한 정리)
4. 실행 로그 기록 및 조회

### 독립 테스트 기준
- 진행률 조회 및 검증
- 일시정지/재개 상태 전이
- 취소 시 안전한 정리 확인

### 작업

- [ ] T041 [P] [US6] Enhance SequenceEngine with pause/resume support in `src/core/sequence/core/SequenceEngine.cpp`
- [ ] T042 [P] [US6] Implement execution log in ExecutionMonitor in `src/core/sequence/core/ExecutionMonitor.cpp`
- [ ] T043 [US6] Create progress tracking in SequenceEngine in `src/core/sequence/core/SequenceEngine.cpp`
- [ ] T044 [US6] Create cancel operation with cleanup in SequenceEngine in `src/core/sequence/core/SequenceEngine.cpp`
- [ ] T045 [US6] Create unit tests for monitoring in `tests/unit/sequence/ExecutionMonitor_test.cpp`
- [ ] T046 [US6] Create integration test for pause/resume in `tests/integration/sequence/SequenceIntegration_test.cpp`

---

## Phase 9: 마무리 및 통합 (Polish & Integration)

### 목표
TaskManager 통합, 성능 최적화, 문서화 완성

### 작업

- [ ] T047 Create SequenceTaskAdapter for TaskManager integration in `src/core/sequence/integration/SequenceTaskAdapter.h` and `.cpp`
- [ ] T048 Create integration tests with TaskManager in `tests/integration/sequence/SequenceTaskManagerIntegration_test.cpp`
- [ ] T049 [P] Create performance test for 1000-action sequence in `tests/performance/SequencePerformance_test.cpp`
- [ ] T050 [P] Validate memory usage and RAII compliance with valgrind in `tests/memcheck/`
- [ ] T051 Create API documentation in `docs/api/sequence_api.md`
- [ ] T052 Create user guide for sequence system in `docs/guides/sequence_user_guide.md`
- [ ] T053 Add sequence module examples in `examples/sequence_examples.cpp`
- [ ] T054 Final integration test combining all features in `tests/integration/sequence/FullSystem_test.cpp`
- [ ] T055 Commit and push all implementation changes with detailed commit message

---

## 병렬 실행 계획 (Parallel Execution Plan)

### Wave 1: Phase 3 (순차 실행 기초)
**선행 조건**: Phase 1, 2 완료
**시간**: 2-3일
**담당**: 1명

### Wave 2: Phase 5 & 6 (병렬 + 재시도)
**선행 조건**: Phase 2 완료, Phase 3 진행 중
**병렬 가능**: Phase 5와 Phase 6은 완전히 독립적 (다른 컴포넌트)
**시간**: 3-4일 (병렬로 2-3일)
**담당**: 2명

### Wave 3: Phase 4 (조건 분기)
**선행 조건**: Phase 3 완료
**시간**: 1-2일
**담당**: 1명

### Wave 4: Phase 7 & 8 (템플릿 + 모니터링)
**선행 조건**: Phase 4 완료
**병렬 가능**: Phase 7과 Phase 8은 독립적
**시간**: 3-4일 (병렬로 2-3일)
**담당**: 2명

### Wave 5: Phase 9 (통합 및 마무리)
**선행 조건**: 모든 Phase 완료
**시간**: 2-3일
**담당**: 1명

---

## 구현 전략 (Implementation Strategy)

### MVP Scope (최소 기능 집합)
**범위**: US1 + 기본 US2 (단순 조건)
**시간**: 3-5일
**목표**: 순차 실행과 기본 조건 분기 지원

**포함 작업**: T001-T024
**제외 작업**: T025-T055 (고급 기능)

### Phase 1 - 증분 배포 계획

**Week 1-2**: 기초 (T001-T014)
- 빌드 설정, 레지스트리, 실행기 구현
- 모든 기초 모듈 단위 테스트 통과

**Week 3-4**: US1 순차 실행 (T015-T019)
- 기본 SequenceEngine 구현
- 순차 실행 통합 테스트 통과

**Week 5**: US2 조건 분기 (T020-T024)
- 조건 평가 엔진 추가
- 분기 통합 테스트 통과

**Week 6-7**: US3, US4 병렬 + 재시도 (T025-T035)
- 병렬 실행기 구현
- 재시도 로직 구현

**Week 8-9**: US5, US6 템플릿 + 모니터링 (T036-T046)
- 템플릿 시스템 구현
- 모니터링 및 제어 기능

**Week 10**: 통합 및 마무리 (T047-T055)
- TaskManager 통합
- 성능 검증 및 문서화

---

## 형식 검증 (Format Validation)

✅ 모든 작업이 다음 형식을 따릅니다:
- `- [ ]` 체크박스
- `T###` 작업 ID
- `[P]` 병렬화 가능 표시 (해당 시)
- `[US#]` 사용자 스토리 레이블 (Phase 3+)
- 명확한 설명 및 파일 경로

✅ 작업 개수: 총 55개
- Phase 1 (Setup): 3개
- Phase 2 (Foundational): 11개
- Phase 3 (US1): 5개
- Phase 4 (US2): 5개
- Phase 5 (US3): 5개
- Phase 6 (US4): 6개
- Phase 7 (US5): 5개
- Phase 8 (US6): 6개
- Phase 9 (Polish): 9개

✅ 병렬화 기회: 9개 작업 `[P]` 표시 가능

✅ 독립 테스트 기준: 각 사용자 스토리에 정의됨

