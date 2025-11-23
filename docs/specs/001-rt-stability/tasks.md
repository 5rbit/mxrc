# 구현 태스크: RT 아키텍처 안정성 개선

**기능**: RT 아키텍처 안정성 개선
**브랜치**: `001-rt-stability`
**사양서**: [spec.md](spec.md) | **계획**: [plan.md](plan.md)

## 요약

4개 사용자 스토리를 우선순위별로 구현하여 RT 시스템의 메모리 안전성, 동시성 안전성, 에러 복구, 타입 안전성을 개선합니다. 각 스토리는 독립적으로 테스트 가능하며, TDD(Test-Driven Development) 방식으로 구현합니다.

**총 태스크 수**: 54개
- Phase 1 (Setup): 3개
- Phase 2 (Foundational): 4개
- Phase 3 (US1 - P1): 13개
- Phase 4 (US2 - P1): 13개
- Phase 5 (US3 - P2): 12개
- Phase 6 (US4 - P3): 9개

**병렬 실행 기회**: 각 Phase 내에서 [P] 표시된 태스크들은 병렬 실행 가능

## 구현 전략

### MVP 범위 (Minimum Viable Product)
**User Story 1 (P1 - 메모리 안전성)**만 구현하면 장시간 실행 시스템의 메모리 누수 문제를 해결할 수 있습니다. 이것이 가장 치명적인 문제이므로 MVP로 적합합니다.

### 점진적 제공 (Incremental Delivery)
1. **US1 완료** → unique_ptr 반환, RAII 패턴 → 메모리 누수 방지
2. **US2 완료** → atomic 동기화, mutex → 데이터 레이스 제거
3. **US3 완료** → ERROR 상태 전환 → 예측 가능한 에러 처리
4. **US4 완료** → 타입 검증 → 타입 안전성 강화

각 스토리 완료 시점에 독립적으로 배포 가능한 증분(increment)을 제공합니다.

## Phase 1: Setup (프로젝트 초기화)

**목표**: Sanitizer 테스트 인프라 구축 및 CMake 설정

**독립 테스트 기준**: AddressSanitizer, ThreadSanitizer 빌드가 성공하고 기존 47개 테스트가 통과

### 태스크

- [ ] T001 Create tests/sanitizer/ directory structure
- [ ] T002 Add AddressSanitizer CMake target in tests/sanitizer/CMakeLists.txt
- [ ] T003 Add ThreadSanitizer CMake target in tests/sanitizer/CMakeLists.txt

## Phase 2: Foundational (기초 작업)

**목표**: 모든 사용자 스토리에 필요한 공통 인프라 및 기존 테스트 baseline 확인

**독립 테스트 기준**: 기존 47개 RT 테스트가 모두 통과하며 AddressSanitizer, ThreadSanitizer 빌드 성공

### 태스크

- [ ] T004 [P] Run existing 47 RT tests with AddressSanitizer to establish baseline
- [ ] T005 [P] Run existing 47 RT tests with ThreadSanitizer to establish baseline
- [ ] T006 Document baseline test results in docs/specs/001-rt-stability/baseline-results.md
- [ ] T007 Create Valgrind test script in tests/sanitizer/valgrind_test.sh

## Phase 3: User Story 1 - 메모리 안전성 보장 (P1)

**스토리 목표**: RTExecutive와 RTDataStoreShared의 메모리를 스마트 포인터와 RAII 패턴으로 관리하여 메모리 누수 방지

**독립 테스트 기준**:
- AddressSanitizer 47 tests 실행 시 메모리 에러 0건 (SC-001)
- Valgrind 1000회 반복 시 메모리 누수 0바이트 (SC-003)
- 모든 기존 RTExecutive, RTDataStoreShared 테스트 통과

### 테스트 태스크 (TDD: Red Phase)

- [ ] T008 [P] [US1] Write test for unique_ptr return in tests/unit/rt/RTExecutive_test.cpp::CreateFromPeriodsReturnsUniquePtr
- [ ] T009 [P] [US1] Write test for automatic cleanup in tests/unit/rt/RTExecutive_test.cpp::UniquePtrAutomaticCleanup
- [ ] T010 [P] [US1] Write test for exception safety in tests/unit/rt/RTExecutive_test.cpp::ExceptionDuringCreationCleansUp
- [ ] T011 [P] [US1] Write test for RAII shm_unlink in tests/unit/rt/RTDataStoreShared_test.cpp::RAIIUnlinksSharedMemory
- [ ] T012 [P] [US1] Write test for mmap failure cleanup in tests/unit/rt/RTDataStoreShared_test.cpp::MmapFailureUnlinksShm
- [ ] T013 [P] [US1] Write AddressSanitizer integration test in tests/sanitizer/asan_test.cpp::MemorySafetyTest
- [ ] T014 [US1] Write Valgrind 1000-iteration test in tests/sanitizer/valgrind_test.sh::RepeatTest

### 구현 태스크 (TDD: Green Phase)

- [ ] T015 [US1] Change RTExecutive::createFromPeriods() return type to std::unique_ptr in src/core/rt/RTExecutive.h
- [ ] T016 [US1] Update createFromPeriods() implementation to return unique_ptr in src/core/rt/RTExecutive.cpp
- [ ] T017 [P] [US1] Add RAII shm_unlink to RTDataStoreShared destructor in src/core/rt/RTDataStoreShared.cpp
- [ ] T018 [P] [US1] Add exception safety to RTDataStoreShared::createShared() in src/core/rt/RTDataStoreShared.cpp
- [ ] T019 [US1] Update all RTExecutive test code to use unique_ptr in tests/unit/rt/RTExecutive_test.cpp
- [ ] T020 [US1] Update integration tests to use unique_ptr in tests/integration/rt/rt_integration_test.cpp

### 검증 태스크

- [ ] T021 [US1] Run AddressSanitizer on all 47 RT tests and verify 0 errors (SC-001)
- [ ] T022 [US1] Run Valgrind 1000-iteration test and verify 0 bytes leaked (SC-003)

## Phase 4: User Story 2 - 동시성 안전성 보장 (P1)

**스토리 목표**: RTExecutive::stop()과 RTStateMachine의 callback 재진입을 원자적 연산과 mutex로 동기화하여 데이터 레이스 제거

**독립 테스트 기준**:
- ThreadSanitizer 멀티스레드 테스트 실행 시 데이터 레이스 0건 (SC-002)
- run()/stop() 1000회 반복 시 크래시 0건 (SC-007)
- 모든 기존 RTExecutive, RTStateMachine 테스트 통과

### 테스트 태스크 (TDD: Red Phase)

- [ ] T023 [P] [US2] Write test for atomic stop() synchronization in tests/unit/rt/RTExecutive_test.cpp::StopSynchronization
- [ ] T024 [P] [US2] Write test for callback reentrancy in tests/unit/rt/RTStateMachine_test.cpp::CallbackReentrancy
- [ ] T025 [P] [US2] Write multithreaded run/stop test in tests/integration/rt/rt_integration_test.cpp::MultithreadedRunStop
- [ ] T026 [P] [US2] Write run/stop 1000x test in tests/integration/rt/rt_integration_test.cpp::RunStop1000Times
- [ ] T027 [P] [US2] Write ThreadSanitizer integration test in tests/sanitizer/tsan_test.cpp::DataRaceFreeTest
- [ ] T028 [US2] Write test for RTDataStore concurrent access in tests/unit/rt/RTDataStore_test.cpp::ConcurrentReadWrite

### 구현 태스크 (TDD: Green Phase)

- [ ] T029 [US2] Verify running_ is already std::atomic<bool> in src/core/rt/RTExecutive.h (FR-005)
- [ ] T030 [US2] Add mutex to RTExecutive::stop() for atomic synchronization in src/core/rt/RTExecutive.cpp
- [ ] T031 [P] [US2] Add callback reentrancy mutex to RTStateMachine in src/core/rt/RTStateMachine.h
- [ ] T032 [P] [US2] Implement reentrancy prevention in RTStateMachine::transitionTo() in src/core/rt/RTStateMachine.cpp
- [ ] T033 [US2] Update stop() to synchronize with state machine atomically in src/core/rt/RTExecutive.cpp
- [ ] T034 [US2] Add thread safety documentation comments in src/core/rt/RTStateMachine.h

### 검증 태스크

- [ ] T035 [US2] Run ThreadSanitizer on multithreaded tests and verify 0 data races (SC-002)
- [ ] T036 [US2] Run run/stop 1000x test and verify 0 crashes (SC-007)

## Phase 5: User Story 3 - 에러 복구 메커니즘 (P2)

**스토리 목표**: RT priority 설정 실패 시 ERROR 상태로 전환하고 run() 실행 거부, RESET 이벤트로 복구 가능

**독립 테스트 기준**:
- RT priority 설패 시 ERROR 상태 전환 및 run() 거부 (SC-004)
- shared memory 실패 시 shm_unlink 호출 (SC-005)
- ERROR → RESET → INIT 전환 성공

### 테스트 태스크 (TDD: Red Phase)

- [ ] T037 [P] [US3] Write test for RT priority failure ERROR transition in tests/unit/rt/RTExecutive_test.cpp::RTPriorityFailureError
- [ ] T038 [P] [US3] Write test for ERROR state run() rejection in tests/unit/rt/RTExecutive_test.cpp::ErrorStateRejectsRun
- [ ] T039 [P] [US3] Write test for shared memory failure cleanup in tests/unit/rt/RTDataStoreShared_test.cpp::ShmFailureCleanup
- [ ] T040 [P] [US3] Write test for ERROR → RESET → INIT recovery in tests/unit/rt/RTStateMachine_test.cpp::ErrorRecovery
- [ ] T041 [US3] Write integration test for error recovery scenario in tests/integration/rt/rt_integration_test.cpp::ErrorRecoveryScenario

### 구현 태스크 (TDD: Green Phase)

- [ ] T042 [US3] Add ERROR state check in RTExecutive::run() to reject execution in src/core/rt/RTExecutive.cpp
- [ ] T043 [P] [US3] Add RT priority failure → ERROR transition in RTExecutive::run() in src/core/rt/RTExecutive.cpp
- [ ] T044 [P] [US3] Strengthen RESET event handling in RTStateMachine in src/core/rt/RTStateMachine.cpp
- [ ] T045 [US3] Add error code and logging to RTDataStoreShared::createShared() in src/core/rt/RTDataStoreShared.cpp
- [ ] T046 [US3] Add error handling comments to src/core/rt/RTExecutive.h

### 검증 태스크

- [ ] T047 [US3] Run RT priority failure test without CAP_SYS_NICE and verify ERROR state (SC-004)
- [ ] T048 [US3] Run shared memory failure test with strace and verify shm_unlink called (SC-005)

## Phase 6: User Story 4 - 타입 안전성 강화 (P3)

**스토리 목표**: RTDataStore에 타입 메타데이터 시스템을 추가하여 DataKey와 실제 타입 불일치를 런타임 검증

**독립 테스트 기준**:
- 잘못된 타입 접근 시 에러 반환율 100% (SC-006)
- DataKey 범위 초과 시 에러 반환
- 모든 기존 RTDataStore 테스트 통과

### 테스트 태스크 (TDD: Red Phase)

- [ ] T049 [P] [US4] Write test for type metadata registration in tests/unit/rt/RTDataStore_test.cpp::RegisterTypeMetadata
- [ ] T050 [P] [US4] Write test for type mismatch detection in tests/unit/rt/RTDataStore_test.cpp::TypeMismatchDetection
- [ ] T051 [P] [US4] Write test for DataKey boundary check in tests/unit/rt/RTDataStore_test.cpp::DataKeyBoundaryCheck
- [ ] T052 [US4] Write test for type validation in get/set in tests/unit/rt/RTDataStore_test.cpp::TypeValidation

### 구현 태스크 (TDD: Green Phase)

- [ ] T053 [P] [US4] Add type metadata map to RTDataStore in src/core/rt/RTDataStore.h
- [ ] T054 [P] [US4] Implement registerTypeMetadata() in src/core/rt/RTDataStore.cpp
- [ ] T055 [P] [US4] Implement validateType() in src/core/rt/RTDataStore.cpp
- [ ] T056 [US4] Add type validation to getInt32/getDouble/etc in src/core/rt/RTDataStore.cpp
- [ ] T057 [US4] Add DataKey boundary check to all get/set functions in src/core/rt/RTDataStore.cpp

### 검증 태스크

- [ ] T058 [US4] Run type mismatch tests and verify 100% error return rate (SC-006)

## Phase 7: Polish & Cross-Cutting Concerns

**목표**: 문서화, 전체 시스템 검증, 성공 메트릭 확인

### 태스크

- [ ] T059 Update API documentation in src/core/rt/RTExecutive.h with unique_ptr usage
- [ ] T060 [P] Create migration guide in docs/specs/001-rt-stability/migration-guide.md
- [ ] T061 [P] Add Korean comments to all modified API functions
- [ ] T062 Run full test suite (47 + new tests) with AddressSanitizer and verify SC-001
- [ ] T063 Run full test suite with ThreadSanitizer and verify SC-002
- [ ] T064 Run Valgrind 1000x on all components and verify SC-003
- [ ] T065 Verify all 7 success criteria (SC-001 through SC-007) are met

## 의존성 그래프

### 사용자 스토리 완료 순서

```text
Phase 1 (Setup) → Phase 2 (Foundational)
                       ↓
              ┌────────┴────────┐
              ↓                 ↓
    Phase 3 (US1-P1)    Phase 4 (US2-P1)  ← 병렬 가능 (독립적)
              ↓                 ↓
              └────────┬────────┘
                       ↓
              Phase 5 (US3-P2)
                       ↓
              Phase 6 (US4-P3)
                       ↓
              Phase 7 (Polish)
```

**주의사항**:
- US1과 US2는 독립적이므로 병렬 구현 가능
- US3은 US1, US2 완료 후 시작 (ERROR 상태가 stop() 동기화 의존)
- US4는 독립적이지만 우선순위가 P3이므로 마지막에 구현

### Phase별 병렬 실행 예시

#### Phase 3 (US1) 병렬 실행
```text
T008, T009, T010, T011, T012, T013 (테스트 작성) → 병렬 실행
T017, T018 (RAII 구현) → 병렬 실행
```

#### Phase 4 (US2) 병렬 실행
```text
T023, T024, T025, T026, T027, T028 (테스트 작성) → 병렬 실행
T031, T032 (StateMachine mutex) + T030, T033 (Executive sync) → 병렬 실행
```

#### Phase 5 (US3) 병렬 실행
```text
T037, T038, T039, T040 (테스트 작성) → 병렬 실행
T043, T044 (ERROR 처리) → 병렬 실행
```

#### Phase 6 (US4) 병렬 실행
```text
T049, T050, T051 (테스트 작성) → 병렬 실행
T053, T054, T055 (타입 메타데이터) → 병렬 실행
```

## 태스크 상세 설명

### Phase 1: Setup

**T001**: tests/sanitizer/ 디렉토리 생성
- 생성: tests/sanitizer/
- 생성: tests/sanitizer/CMakeLists.txt (빈 파일)

**T002**: AddressSanitizer CMake 타겟 추가
- 편집: tests/sanitizer/CMakeLists.txt
- 추가: `-fsanitize=address -fno-omit-frame-pointer` 플래그
- 타겟: asan_tests (기존 47개 테스트 재컴파일)

**T003**: ThreadSanitizer CMake 타겟 추가
- 편집: tests/sanitizer/CMakeLists.txt
- 추가: `-fsanitize=thread` 플래그
- 타겟: tsan_tests (기존 47개 테스트 재컴파일)

### Phase 2: Foundational

**T004**: AddressSanitizer baseline 실행
- 실행: `cmake --build build --target asan_tests && ./build/asan_tests`
- 기록: 발견된 메모리 에러 개수 (baseline)

**T005**: ThreadSanitizer baseline 실행
- 실행: `cmake --build build --target tsan_tests && ./build/tsan_tests`
- 기록: 발견된 데이터 레이스 개수 (baseline)

**T006**: baseline 결과 문서화
- 생성: docs/specs/001-rt-stability/baseline-results.md
- 내용: ASan, TSan 결과, 발견된 이슈 목록

**T007**: Valgrind 테스트 스크립트 생성
- 생성: tests/sanitizer/valgrind_test.sh
- 내용: RTExecutive 생성/소멸 1000회 반복
- 실행: `valgrind --leak-check=full --show-leak-kinds=all`

### Phase 3: US1 - 메모리 안전성

**T008-T014**: 테스트 작성 (TDD Red Phase)
- 각 테스트는 실패해야 함 (아직 구현 안됨)
- unique_ptr 반환, RAII, 예외 안전성 검증

**T015**: RTExecutive.h 시그니처 변경
```cpp
// Before
static RTExecutive* createFromPeriods(const std::vector<uint32_t>& periods_ms);

// After
static std::unique_ptr<RTExecutive> createFromPeriods(const std::vector<uint32_t>& periods_ms);
```

**T016**: RTExecutive.cpp 구현 변경
```cpp
std::unique_ptr<RTExecutive> RTExecutive::createFromPeriods(...) {
    return std::make_unique<RTExecutive>(minor, major);
}
```

**T017**: RTDataStoreShared 소멸자 RAII
```cpp
RTDataStoreShared::~RTDataStoreShared() {
    if (shm_name_) {
        shm_unlink(shm_name_);  // RAII 보장
    }
}
```

**T018**: createShared() 예외 안전성
- try-catch로 mmap 실패 처리
- 실패 시 shm_unlink 호출 후 예외 재발생

**T019-T020**: 테스트 코드 업데이트
- 모든 `delete exec;` 제거
- `auto exec = RTExecutive::createFromPeriods(...);` 로 변경

**T021-T022**: 검증
- AddressSanitizer 실행: 메모리 에러 0건 확인
- Valgrind 1000회: 메모리 누수 0바이트 확인

### Phase 4: US2 - 동시성 안전성

**T023-T028**: 테스트 작성 (TDD Red Phase)
- 멀티스레드 테스트: run()과 stop()을 별도 스레드에서 호출
- 재진입 테스트: callback에서 다시 handleEvent() 호출

**T029**: running_ 이미 atomic 확인
- src/core/rt/RTExecutive.h 확인
- `std::atomic<bool> running_;` 이미 있으면 OK

**T030**: stop() mutex 동기화
```cpp
void RTExecutive::stop() {
    std::lock_guard<std::mutex> lock(stop_mutex_);
    if (running_) {
        running_ = false;
        state_machine_->handleEvent(RTEvent::STOP);
    }
}
```

**T031-T032**: RTStateMachine 재진입 방지
```cpp
class RTStateMachine {
    std::mutex transition_mutex_;
    ...
};

void RTStateMachine::transitionTo(...) {
    std::lock_guard<std::mutex> lock(transition_mutex_);
    // ... 기존 로직
}
```

**T033**: stop() 원자적 동기화 강화
- running_ = false와 handleEvent(STOP)이 한 critical section에서 실행
- lock_guard로 보호

**T034**: 스레드 안전성 주석 추가
```cpp
// Thread-safe: handleEvent() uses internal mutex
// Can be called from multiple threads concurrently
```

**T035-T036**: 검증
- ThreadSanitizer: 데이터 레이스 0건
- 1000회 run/stop: 크래시 0건

### Phase 5: US3 - 에러 복구

**T037-T041**: 테스트 작성 (TDD Red Phase)
- RT priority 실패 시나리오 (CAP_SYS_NICE 없이 실행)
- ERROR 상태에서 run() 호출 시 -1 반환
- RESET 이벤트로 INIT 복구

**T042**: run() ERROR 상태 검증
```cpp
int RTExecutive::run() {
    if (state_machine_->getState() == RTState::ERROR) {
        spdlog::error("Cannot run in ERROR state");
        return -1;
    }
    // ... 기존 로직
}
```

**T043**: RT priority 실패 → ERROR
```cpp
if (util::setPriority(SCHED_FIFO, 90) != 0) {
    spdlog::error("Failed to set RT priority");
    state_machine_->handleEvent(RTEvent::ERROR_OCCUR);
    return -1;  // 실행 중단
}
```

**T044**: RESET 이벤트 강화
- RTStateMachine::handleEvent()에서 ERROR → RESET → INIT 전환 허용
- 기존 코드는 이미 지원 (RTStateMachine_test.cpp:63-70 참조)

**T045**: shared memory 에러 코드 추가
```cpp
int RTDataStoreShared::createShared(const std::string& name) {
    int fd = shm_open(name.c_str(), ...);
    if (fd == -1) {
        int err = errno;
        spdlog::error("shm_open failed: {}", strerror(err));
        return -err;  // 명확한 에러 코드
    }
    // ...
}
```

**T046**: 에러 처리 주석 추가
- run() 실패 시나리오 문서화
- ERROR 상태 복구 방법 설명

**T047-T048**: 검증
- RT priority 없이 실행 → ERROR 상태 확인
- strace로 shm_unlink 호출 확인

### Phase 6: US4 - 타입 안전성

**T049-T052**: 테스트 작성 (TDD Red Phase)
- registerTypeMetadata(DataKey::ROBOT_X, typeid(int32_t))
- getDouble(DataKey::ROBOT_X) → -1 반환 (타입 불일치)
- setInt32(DataKey::MAX + 1) → -1 반환 (범위 초과)

**T053**: 타입 메타데이터 맵 추가
```cpp
class RTDataStore {
    std::unordered_map<DataKey, const std::type_info*> type_metadata_;
    // ...
};
```

**T054**: registerTypeMetadata() 구현
```cpp
int RTDataStore::registerTypeMetadata(DataKey key, const std::type_info& type) {
    type_metadata_[key] = &type;
    return 0;
}
```

**T055**: validateType() 구현
```cpp
int RTDataStore::validateType(DataKey key, const std::type_info& type) const {
    auto it = type_metadata_.find(key);
    if (it != type_metadata_.end() && *it->second != type) {
        return -1;  // 타입 불일치
    }
    return 0;
}
```

**T056**: get/set에 타입 검증 추가
```cpp
int32_t RTDataStore::getInt32(DataKey key) const {
    if (validateType(key, typeid(int32_t)) != 0) {
        spdlog::error("Type mismatch for key {}", static_cast<int>(key));
        return 0;  // 또는 예외
    }
    // ... 기존 로직
}
```

**T057**: DataKey 경계 검사
```cpp
int RTDataStore::setInt32(DataKey key, int32_t value) {
    if (key >= DataKey::MAX_KEYS) {
        spdlog::error("DataKey out of range: {}", static_cast<int>(key));
        return -1;
    }
    // ... 기존 로직
}
```

**T058**: 검증
- 타입 불일치 100개 시도 → 100개 에러 반환 (100%)

### Phase 7: Polish

**T059-T061**: 문서화
- API 주석 업데이트 (Doxygen 스타일)
- migration-guide.md 작성 (createFromPeriods 사용법 변경)
- 한글 주석 추가

**T062-T065**: 최종 검증
- 모든 Sanitizer 실행
- 7개 성공 기준 모두 확인
- 결과 문서화

## 성공 기준 체크리스트

- [ ] SC-001: AddressSanitizer 47 tests 메모리 에러 0건
- [ ] SC-002: ThreadSanitizer 데이터 레이스 0건
- [ ] SC-003: Valgrind 1000회 메모리 누수 0바이트
- [ ] SC-004: RT priority 실패 시 ERROR 상태 전환
- [ ] SC-005: shared memory 실패 시 shm_unlink 호출
- [ ] SC-006: 타입 불일치 에러 반환율 100%
- [ ] SC-007: run()/stop() 1000회 크래시 0건

## 참고 자료

- [spec.md](spec.md): 기능 사양서 (사용자 스토리, 요구사항)
- [plan.md](plan.md): 구현 계획 (기술 컨텍스트, Phase 설명)
- [checklists/requirements.md](checklists/requirements.md): 요구사항 체크리스트
- C++ Core Guidelines: https://isocpp.github.io/CppCoreGuidelines/
- AddressSanitizer: https://github.com/google/sanitizers/wiki/AddressSanitizer
- ThreadSanitizer: https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual
