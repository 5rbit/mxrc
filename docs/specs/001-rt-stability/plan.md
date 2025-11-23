# 구현 계획: RT 아키텍처 안정성 개선

**브랜치**: `001-rt-stability` | **날짜**: 2025-11-20 | **사양서**: [spec.md](spec.md)
**입력**: `specs/001-rt-stability/spec.md` 의 기능 사양서

## 요약

RT 아키텍처의 안정성을 개선하여 장시간 실행되는 실시간 시스템에서 메모리 누수, 데이터 레이스, 예측 불가능한 에러 처리 문제를 해결합니다. 4개 우선순위별 개선 사항을 단계적으로 구현합니다:

1. **P1 - 메모리 안전성**: 스마트 포인터와 RAII 패턴으로 메모리 누수 방지
2. **P1 - 동시성 안전성**: atomic 연산과 동기화로 데이터 레이스 제거
3. **P2 - 에러 복구**: ERROR 상태 전환과 복구 메커니즘으로 예측 가능한 에러 처리
4. **P3 - 타입 안전성**: 타입 메타데이터로 런타임 타입 검증

**검증 방법**: AddressSanitizer, ThreadSanitizer, Valgrind로 모든 개선사항 자동 검증

## 기술 컨텍스트

**언어/버전**: C++17 (std::unique_ptr, std::atomic, RAII 패턴)
**주요 의존성**:
- POSIX real-time extensions (SCHED_FIFO, clock_nanosleep)
- POSIX shared memory (shm_open, mmap)
- spdlog 1.x (logging)

**저장소**:
- 프로세스 내: RTDataStore (stack-allocated)
- 프로세스 간: RTDataStoreShared (POSIX shared memory)

**테스트**:
- Google Test (unit, integration tests)
- AddressSanitizer (메모리 안전성)
- ThreadSanitizer (데이터 레이스)
- Valgrind (메모리 누수)

**대상 플랫폼**: Linux Ubuntu 24.04 LTS, PREEMPT_RT
**프로젝트 유형**: Real-Time Controller (RT process + Non-RT process)

**성능 목표**:
- 제어 루프 < 1ms jitter
- 메모리 누수 0바이트
- 데이터 레이스 0건

**제약 조건**:
- 기존 API 호환성 유지 (기존 코드 최소 영향)
- RT 성능 저하 없음 (lock-free 연산 우선)
- 동작 중인 시스템 중단 불가 (단계적 적용)

**규모/범위**:
- 영향받는 파일: 6개 (RTExecutive.{h,cpp}, RTDataStoreShared.{h,cpp}, RTStateMachine.{h,cpp})
- 영향받는 테스트: 47개 (전체 RT 테스트 스위트)
- 새로운 테스트: ~15개 (메모리, 동시성, 에러 복구, 타입 검증)

## Constitution 준수 확인

*GATE: 0단계 연구 시작 전에 반드시 통과해야 합니다. 1단계 설계 후 다시 확인합니다.*

✅ **실시간성 보장**:
- 스마트 포인터는 컴파일 타임 오버헤드만 존재 (런타임 영향 없음)
- atomic 연산은 lock-free로 구현 (SCHED_FIFO 스케줄링 유지)
- mutex는 state machine 전환 시에만 사용 (critical section 최소화)
- **결론**: 실시간 제약 조건 위반 없음

✅ **신뢰성 및 안전성**:
- RAII 패턴으로 리소스 누수 방지 (C++ Core Guidelines 준수)
- std::atomic으로 데이터 레이스 제거 (C++17 표준)
- ERROR 상태 전환으로 부분 초기화 방지
- AddressSanitizer, ThreadSanitizer로 메모리/동시성 안전성 검증
- **결론**: 코드 안전성 표준 준수, 잠재적 위험 식별 및 완화

✅ **테스트 주도 개발**:
- Phase별 테스트 우선 작성 (Red-Green-Refactor)
- 단위 테스트: 각 클래스별 메모리/동시성 검증
- 통합 테스트: RTExecutive + RTDataStore + StateMachine 조합
- Sanitizer 테스트: CI/CD에 AddressSanitizer, ThreadSanitizer 통합
- **결론**: 모든 기능에 대한 포괄적인 테스트 계획 포함

✅ **모듈식 설계**:
- 변경은 각 클래스 내부로 캡슐화 (public API 변경 최소)
- createFromPeriods() 반환 타입 변경이 유일한 breaking change
- RTDataStoreShared RAII는 기존 API 유지
- 타입 메타데이터는 선택적 기능으로 추가
- **결론**: 명확한 API 유지, 기존 시스템 복잡성 증가 없음

✅ **한글 문서화**:
- 모든 설계 결정 한글로 문서화 (spec.md, plan.md)
- API 변경 사항 한글 주석 추가
- 새로운 기능 사용 예제 한글 작성 (quickstart.md)
- **결론**: 한글 문서화 원칙 준수

✅ **버전 관리**:
- API 호환성 깨짐: createFromPeriods() 반환 타입 변경 → MAJOR 버전 업
- 새로운 기능 추가: RAII, ERROR 상태, 타입 검증 → MINOR 버전 업
- 버그 수정: 없음 (개선만 있음)
- **제안 버전**: 2.0.0 (breaking change due to createFromPeriods())
- **결론**: Semantic Versioning 2.0.0 준수

### Phase 1 설계 후 재확인

*(Phase 1 완료 후 이 섹션을 업데이트)*

- [ ] data-model.md의 엔티티가 실시간 제약 조건을 위반하지 않는지 확인
- [ ] contracts/의 API가 스레드 안전한지 확인
- [ ] 메모리 할당이 RT loop 밖에서만 발생하는지 확인

## 프로젝트 구조

### 문서 (이 기능)

```text
docs/specs/001-rt-stability/
├── spec.md              # 기능 사양서 (완료)
├── plan.md              # 이 파일 (진행 중)
├── research.md          # Phase 0 출력 (다음)
├── data-model.md        # Phase 1 출력
├── quickstart.md        # Phase 1 출력
├── contracts/           # Phase 1 출력
│   ├── RTExecutive.h    # createFromPeriods() 시그니처
│   ├── RTDataStoreShared.h  # RAII 인터페이스
│   └── RTStateMachine.h     # ERROR 상태 처리
├── checklists/
│   └── requirements.md  # 요구사항 체크리스트 (완료)
└── tasks.md             # Phase 2 출력 (/tasks 명령어)
```

### 소스 코드 (리포지토리 루트)

```text
src/core/rt/
├── RTExecutive.h           # [수정] createFromPeriods() 반환 타입
├── RTExecutive.cpp         # [수정] unique_ptr 반환, ERROR 상태 처리
├── RTStateMachine.h        # [수정] ERROR 상태 처리 강화
├── RTStateMachine.cpp      # [수정] 재진입 방지
├── RTDataStore.h           # [수정] 타입 메타데이터
├── RTDataStore.cpp         # [수정] 타입 검증
├── RTDataStoreShared.h     # [수정] RAII 패턴
└── RTDataStoreShared.cpp   # [수정] 소멸자에 shm_unlink

tests/unit/rt/
├── RTExecutive_test.cpp         # [수정] unique_ptr 사용
├── RTStateMachine_test.cpp      # [수정] ERROR 복구 테스트
├── RTDataStore_test.cpp         # [수정] 타입 검증 테스트
└── RTDataStoreShared_test.cpp   # [수정] RAII 테스트

tests/integration/rt/
└── rt_integration_test.cpp      # [수정] 멀티스레드 테스트

tests/sanitizer/  # [신규]
├── asan_test.cpp              # AddressSanitizer 테스트
├── tsan_test.cpp              # ThreadSanitizer 테스트
└── valgrind_test.sh           # Valgrind 스크립트
```

**구조 결정**:
- 기존 파일 수정 중심 (신규 파일 최소화)
- sanitizer 테스트는 별도 디렉토리로 분리 (기존 테스트와 독립 실행)
- contracts/는 API 변경 사항만 명시 (전체 헤더 복사 아님)

## 복잡성 추적

> **Constitution 준수 확인에 정당화가 필요한 위반 사항이 있는 경우에만 작성하십시오**

| 위반 사항 | 필요한 이유 | 더 간단한 대안이 거부된 이유 |
|-----------|------------|-------------------------------------|
| createFromPeriods() breaking change | 메모리 안전성을 위한 std::unique_ptr 반환 필수 | raw pointer + 문서화로는 메모리 누수 방지 불가능. 장시간 실행 시스템에서 메모리 누수는 크래시로 이어짐 |
| RTStateMachine에 mutex 추가 | state transition callback 재진입 방지 필수 | atomic만으로는 callback 실행 중 재진입 방지 불가능. lock-free 큐는 복잡도 증가 |

## Phase 0: 연구 및 기술 선택

### 미해결 기술 질문

1. **std::unique_ptr 반환 vs std::shared_ptr?**
   - 질문: RTExecutive 소유권이 단일인가 공유되는가?
   - 연구 필요: 기존 코드에서 RTExecutive 사용 패턴 분석
   - 결정 기준: 소유권이 단일이면 unique_ptr, 공유되면 shared_ptr

2. **stop() 동기화 메커니즘: mutex vs condition_variable vs atomic ordering?**
   - 질문: running_ = false와 state transition을 어떻게 원자적으로 수행?
   - 연구 필요: atomic memory_order, mutex overhead, condition_variable 필요성
   - 결정 기준: RT 성능 영향 최소화, 정확성 보장

3. **RTDataStore 타입 메타데이터: static map vs constexpr?**
   - 질문: 타입 검증을 컴파일 타임에 할 수 있는가?
   - 연구 필요: C++17 constexpr 한계, static_assert 활용
   - 결정 기준: 컴파일 타임 검증 가능하면 constexpr, 불가능하면 runtime map

4. **Valgrind 실행 오버헤드는 CI/CD에서 허용 가능한가?**
   - 질문: Valgrind로 1000회 반복 테스트 시간은?
   - 연구 필요: CI/CD 타임아웃, Valgrind 캐시 활용
   - 결정 기준: 10분 이내면 CI/CD 통합, 초과하면 nightly build

5. **ERROR 상태에서 run() 재시도 허용 여부?**
   - 질문: ERROR → RESET → INIT → run() 시나리오가 필요한가?
   - 연구 필요: 실제 운영 시나리오, 에러 복구 정책
   - 결정 기준: 자동 복구 필요하면 RESET 지원, 수동 복구만 필요하면 재시작

### 연구 태스크 (research.md에 결과 작성)

- [ ] **Task 1**: 기존 RTExecutive 사용 패턴 분석 (unique_ptr vs shared_ptr 결정)
- [ ] **Task 2**: atomic memory ordering 벤치마크 (stop() 동기화 메커니즘)
- [ ] **Task 3**: C++17 constexpr 한계 조사 (타입 메타데이터)
- [ ] **Task 4**: Valgrind CI/CD 통합 시간 측정
- [ ] **Task 5**: 실제 운영 에러 복구 시나리오 조사

### Best Practices 조사

- [ ] **C++ Core Guidelines**: 스마트 포인터 가이드라인 (R.20-R.37)
- [ ] **Herb Sutter - atomic<> Weapons**: memory_order 사용법
- [ ] **MISRA C++ 2023**: 실시간 시스템 동시성 규칙
- [ ] **Google Test Advanced**: Sanitizer 통합 모범 사례
- [ ] **Linux PREEMPT_RT**: mutex priority inheritance

**Output**: [research.md](research.md) (Phase 0 완료 시 생성)

## Phase 1: 설계 및 계약

### 엔티티 모델 (data-model.md)

핵심 엔티티 4개:

1. **RTExecutive**
   - 변경: createFromPeriods() 반환 타입 `std::unique_ptr<RTExecutive>`
   - 추가: ERROR 상태 검증 (run() 실행 전)
   - RAII: 소멸자에서 stop() 호출 보장 (기존 동작 유지)

2. **RTStateMachine**
   - 추가: callback 재진입 방지 mutex
   - 강화: ERROR 상태 처리 (RESET 이벤트)
   - 검증: handleEvent() 호출 스레드 안전성

3. **RTDataStore**
   - 추가: 타입 메타데이터 (DataKey → std::type_info 매핑)
   - 검증: get/set 시 타입 불일치 에러 반환
   - 경계 검사: DataKey 범위 초과 방지

4. **RTDataStoreShared**
   - RAII: 소멸자에서 shm_unlink 보장
   - 예외 안전성: 생성 실패 시 부분 리소스 정리
   - 검증: mmap 실패 후 shm_unlink 호출

**Output**: [data-model.md](data-model.md) (Phase 1)

### API 계약 (contracts/)

주요 API 변경 3개:

1. **RTExecutive::createFromPeriods()** (BREAKING CHANGE)
   ```cpp
   // Before
   RTExecutive* createFromPeriods(const std::vector<uint32_t>& periods_ms);

   // After
   std::unique_ptr<RTExecutive> createFromPeriods(const std::vector<uint32_t>& periods_ms);
   ```

2. **RTDataStoreShared 소멸자** (NON-BREAKING, 내부 개선)
   ```cpp
   ~RTDataStoreShared();  // 이제 shm_unlink 보장
   ```

3. **RTDataStore 타입 검증** (NEW FEATURE)
   ```cpp
   // 새로운 함수
   int registerTypeMetadata(DataKey key, const std::type_info& type);
   int validateType(DataKey key, const std::type_info& type) const;
   ```

**Output**: [contracts/](contracts/) (Phase 1)

### 빠른 시작 가이드 (quickstart.md)

사용자 관점 변경 사항:

1. **메모리 안전 사용법**
   ```cpp
   // Before (manual delete 필요)
   auto* exec = RTExecutive::createFromPeriods({10, 20});
   // ... use exec
   delete exec;  // 잊으면 메모리 누수!

   // After (자동 정리)
   auto exec = RTExecutive::createFromPeriods({10, 20});
   // ... use exec
   // 자동으로 메모리 해제됨
   ```

2. **에러 복구 사용법**
   ```cpp
   auto exec = RTExecutive::createFromPeriods({10, 20});
   int result = exec->run();
   if (result == -1) {
       // RT priority 설정 실패 등
       // state machine은 ERROR 상태
       exec->getStateMachine()->handleEvent(RTEvent::RESET);
       // INIT 상태로 복구, 재시도 가능
   }
   ```

3. **타입 안전 사용법**
   ```cpp
   RTDataStore store;
   store.registerTypeMetadata(DataKey::ROBOT_X, typeid(int32_t));

   store.setInt32(DataKey::ROBOT_X, 100);  // OK
   double val = store.getDouble(DataKey::ROBOT_X);  // ERROR 반환
   ```

**Output**: [quickstart.md](quickstart.md) (Phase 1)

### Agent Context 업데이트

```bash
.specify/scripts/bash/update-agent-context.sh gemini
```

추가할 기술 스택:
- std::unique_ptr, std::shared_ptr (C++11)
- std::atomic, std::mutex (C++11)
- AddressSanitizer (compile flag: -fsanitize=address)
- ThreadSanitizer (compile flag: -fsanitize=thread)
- Valgrind (external tool)

**Output**:
- `dev/agent/CLAUDE.md` 업데이트 (기존 기술 스택 보존)
- 또는 `dev/agent/GEMINI.md` (Gemini 사용 시)

## Phase 2: 태스크 분해

*이 섹션은 `/tasks` 명령어로 자동 생성됩니다. plan.md 작성 시에는 비워둡니다.*

**Output**: [tasks.md](tasks.md) (Phase 2, `/tasks` 명령어)

## 구현 우선순위

### Phase 1: 메모리 안전성 (P1) - 예상 2일
1. RTExecutive::createFromPeriods() 반환 타입 변경
2. 모든 테스트 코드 unique_ptr 업데이트
3. RTDataStoreShared RAII 구현
4. AddressSanitizer 검증

**완료 기준**: SC-001 달성 (AddressSanitizer 47 tests 메모리 에러 0건)

### Phase 2: 동시성 안전성 (P1) - 예상 3일
1. RTStateMachine callback 재진입 방지 mutex 추가
2. RTExecutive::stop() 동기화 강화
3. 멀티스레드 테스트 추가
4. ThreadSanitizer 검증

**완료 기준**: SC-002, SC-007 달성 (데이터 레이스 0건, run()/stop() 1000회 크래시 0건)

### Phase 3: 에러 처리 (P2) - 예상 2일
1. RTExecutive::run() ERROR 상태 전환 로직
2. ERROR 상태 run() 거부 로직
3. RTStateMachine RESET 이벤트 강화
4. 에러 복구 테스트

**완료 기준**: SC-004 달성 (RT priority 실패 시 ERROR 상태 전환)

### Phase 4: 타입 안전성 (P3) - 예상 2일
1. RTDataStore 타입 메타데이터 시스템
2. get/set 타입 검증 로직
3. DataKey 경계 검사
4. 타입 불일치 테스트

**완료 기준**: SC-006 달성 (잘못된 타입 접근 에러 반환율 100%)

## 위험 및 완화

| 위험 | 가능성 | 영향 | 완화 전략 |
|------|--------|------|----------|
| unique_ptr 변경으로 기존 코드 깨짐 | 높음 | 중간 | 컴파일 에러로 조기 발견, 마이그레이션 가이드 제공 |
| mutex 추가로 RT 성능 저하 | 낮음 | 높음 | 벤치마크로 검증, critical section 최소화 |
| Valgrind CI/CD 시간 초과 | 중간 | 낮음 | nightly build로 이동, 샘플링 테스트 |
| 타입 메타데이터 런타임 오버헤드 | 낮음 | 낮음 | 선택적 기능으로 제공, 릴리스 빌드 최적화 |

## 성공 메트릭

- [ ] SC-001: AddressSanitizer 47 tests 메모리 에러 0건
- [ ] SC-002: ThreadSanitizer 데이터 레이스 0건
- [ ] SC-003: Valgrind 1000회 반복 메모리 누수 0바이트
- [ ] SC-004: RT priority 실패 시 ERROR 상태 전환
- [ ] SC-005: shared memory 실패 시 shm_unlink 호출
- [ ] SC-006: 타입 불일치 에러 반환율 100%
- [ ] SC-007: run()/stop() 1000회 반복 크래시 0건

## 다음 단계

1. ✅ spec.md 작성 완료
2. ✅ plan.md 작성 완료
3. ⏳ Phase 0: research.md 작성 (미해결 질문 해결)
4. ⏳ Phase 1: data-model.md, contracts/, quickstart.md 작성
5. ⏳ Phase 1: Agent context 업데이트
6. ⏳ Phase 2: tasks.md 생성 (`/tasks` 명령어)
7. ⏳ Phase 3-6: 구현 (`/implement` 명령어)
