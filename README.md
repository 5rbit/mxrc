# MXRC - Universal Robot Control Controller

MXRC는 어떤 로봇도 제어할 수 있는 범용 로봇 제어 컨트롤러입니다. C++20으로 개발되며, Linux Ubuntu 24.04 LTS PREEMPT_RT 환경에서 실시간 성능을 목표로 합니다.

## 주요 특징

- **계층적 아키텍처**: Action → Sequence → Task의 명확한 3계층 구조
- **인터페이스 기반 설계**: 확장 가능하고 유지보수가 쉬운 구조
- **RAII 원칙 준수**: 자동 리소스 관리로 메모리 누수 방지
- **철저한 테스트**: 112개의 단위 테스트로 품질 보증
- **사양 주도 개발**: 명확한 요구사항과 검증 가능한 구현

## 빌드 환경

### 필수 요구사항

- **OS**: Ubuntu 24.04 LTS (PREEMPT_RT)
- **컴파일러**: C++20 지원 (GCC 11+ or Clang 14+)
- **빌드 시스템**: CMake 3.16+
- **의존성**:
  - spdlog (로깅)
  - GTest (테스트)

### 빌드 방법

```bash
# 의존성 설치 (Ubuntu)
sudo apt-get install libspdlog-dev libgtest-dev cmake

# 빌드
mkdir -p build
cd build
cmake ..
make -j$(nproc)

# 테스트 실행
./run_tests

# 메인 실행 파일
./mxrc
```

## 시스템 아키텍처

MXRC는 3개의 주요 계층으로 구성됩니다:

```
┌────────────────────────────────┐
│      Task Layer                │  실행 모드 관리 (단일/주기적/트리거)
├────────────────────────────────┤
│      Sequence Layer            │  Action 조합 (순차/조건부/병렬)
├────────────────────────────────┤
│      Action Layer              │  기본 동작 실행
└────────────────────────────────┘
```

### Action Layer

로봇 동작의 가장 기본적인 단위입니다.

- **ActionExecutor**: 개별 Action 실행 및 관리
- **ActionFactory**: 플러그인 방식의 Action 생성
- **ActionRegistry**: Action 타입 등록 및 조회

**예시:**
```cpp
// Action 생성 및 실행
auto action = factory->createAction("Delay", {{"duration", "100"}});
auto result = executor->execute(action, context);
```

### Sequence Layer

여러 Action을 조합하여 복잡한 작업을 정의합니다.

- **SequenceEngine**: 순차/조건부/병렬 실행 조율
- **SequenceRegistry**: 시퀀스 정의 관리
- **ConditionEvaluator**: 런타임 조건 평가
- **RetryHandler**: 자동 재시도 정책

**예시:**
```cpp
// Sequence 정의
SequenceDefinition seq("pick_and_place");
seq.addStep(ActionStep("move", "Move").addParameter("x", "10"));
seq.addStep(ActionStep("grip", "Grip").addParameter("force", "50"));
registry->registerDefinition(seq);

// Sequence 실행
auto result = engine->execute(seq, context);
```

### Task Layer

Action 또는 Sequence를 Task로 패키징하여 다양한 실행 모드를 제공합니다.

- **TaskExecutor**: Task 실행 및 상태 관리
- **TaskRegistry**: Task 정의 저장 및 조회
- **PeriodicScheduler**: 주기적 실행 (개발 중)
- **TriggerManager**: 이벤트 기반 실행 (개발 중)

**실행 모드:**
- **ONCE**: 단일 실행 ✅ (완료)
- **PERIODIC**: 주기적 실행 (개발 중)
- **TRIGGERED**: 이벤트 트리거 실행 (개발 중)

**예시:**
```cpp
// Task 정의
TaskDefinition task("inspection_task");
task.setWorkSequence("inspection_seq")
    .setOnceMode();

// Task 실행
auto result = taskExecutor->execute(task, context);
```

## 디렉토리 구조

```
mxrc/
├── src/core/
│   ├── action/              # Action Layer (26 tests)
│   ├── sequence/            # Sequence Layer (33 tests)
│   └── task/                # Task Layer (53 tests)
│
├── tests/
│   ├── unit/                # 단위 테스트
│   └── integration/         # 통합 테스트
│
├── specs/                   # 사양 및 계획 문서
│   └── 017-action-sequence-orchestration/
│       ├── spec.md
│       ├── architecture.md
│       └── tasks.md
│
├── CMakeLists.txt
├── README.md
└── CLAUDE.md               # 개발자 가이드
```

## 테스트 현황

### 전체 테스트: 112개 (모두 통과 ✅)

| 계층 | 테스트 수 | 상태 |
|------|----------|------|
| Action Layer | 26 | ✅ 통과 |
| Sequence Layer | 33 | ✅ 통과 |
| Task Layer | 53 | ✅ 통과 |

### 테스트 실행

```bash
# 전체 테스트
./run_tests

# 계층별 테스트
./run_tests --gtest_filter=ActionExecutor*
./run_tests --gtest_filter=SequenceEngine*
./run_tests --gtest_filter=TaskExecutor*

# 간략한 출력
./run_tests --gtest_brief=1
```

## 개발 가이드

### 새로운 Action 추가

1. `IAction` 인터페이스 상속
2. `execute()`, `cancel()`, `getStatus()` 구현
3. ActionFactory에 팩토리 함수 등록
4. 단위 테스트 작성

```cpp
class MyAction : public IAction {
    void execute(ExecutionContext& context) override {
        // 작업 구현
        context.setActionResult(id_, result);
    }
    // ... 기타 메서드
};

// 등록
factory->registerFactory("MyAction", [](const auto& id, const auto& params) {
    return std::make_shared<MyAction>(id, params);
});
```

### 새로운 Sequence 정의

```cpp
SequenceDefinition seq("my_sequence");
seq.addStep(ActionStep("step1", "Action1"));
seq.addStep(ActionStep("step2", "Action2"));
sequenceRegistry->registerDefinition(seq);
```

### 새로운 Task 정의

```cpp
TaskDefinition task("my_task");
task.setWorkSequence("my_sequence")
    .setOnceMode()
    .setTimeout(std::chrono::seconds(30));
```

## 설계 원칙

### 1. RAII (Resource Acquisition Is Initialization)
- 모든 리소스는 생성자에서 할당, 소멸자에서 해제
- 스마트 포인터 사용 (shared_ptr, unique_ptr)
- 수동 메모리 관리 금지

### 2. 인터페이스 기반 설계
- 모든 확장 지점에 인터페이스 제공
- 의존성 주입 (Dependency Injection)
- 느슨한 결합 (Loose Coupling)

### 3. 단계적 구현
- Phase 1: Action Layer (완료 ✅)
- Phase 2: Sequence Layer (완료 ✅)
- Phase 3: Task Layer (진행 중 🚧)
  - Phase 3B-1: Single Execution (완료 ✅)
  - Phase 3B-2: Periodic Execution (예정)
  - Phase 3B-3: Triggered Execution (예정)

### 4. 테스트 주도 개발
- 모든 컴포넌트에 단위 테스트
- 통합 테스트로 계층 간 상호작용 검증
- 메모리 누수 검증

## 성능 목표

- **실시간 성능**: PREEMPT_RT 커널 활용
- **메모리 효율성**: 1000+ Action 시퀀스 처리 가능
- **저지연**: Task 실행 오버헤드 < 1ms
- **안정성**: 메모리 누수 없음, 예외 안전성 보장

## 로깅

spdlog를 사용한 구조화된 로깅:

```cpp
Logger::get()->info("Task {} completed successfully", taskId);
Logger::get()->error("Action {} failed: {}", actionId, error);
Logger::get()->debug("Executing step {}/{}", current, total);
```

로그 레벨:
- **trace**: 상세한 실행 흐름
- **debug**: 디버깅 정보
- **info**: 일반 정보
- **warn**: 경고
- **error**: 오류

## 기여 가이드

1. **사양 확인**: `specs/` 디렉토리의 관련 사양 검토
2. **브랜치 생성**: `feature/<기능명>` 또는 `fix/<버그명>`
3. **테스트 작성**: 기능 구현 전에 테스트 작성 (TDD)
4. **코드 작성**: CLAUDE.md의 코딩 가이드 준수
5. **테스트 실행**: 모든 테스트 통과 확인
6. **PR 생성**: 명확한 설명과 함께 Pull Request

## 라이선스

[라이선스 정보 추가 필요]

## 참고 문서

- **개발자 가이드**: [CLAUDE.md](CLAUDE.md)
- **아키텍처 문서**: [specs/017-action-sequence-orchestration/architecture.md](specs/017-action-sequence-orchestration/architecture.md)
- **구현 계획**: [specs/017-action-sequence-orchestration/plan.md](specs/017-action-sequence-orchestration/plan.md)
- **Task 목록**: [specs/017-action-sequence-orchestration/tasks.md](specs/017-action-sequence-orchestration/tasks.md)

## 연락처

프로젝트 관련 문의: [연락처 정보 추가 필요]

---

**현재 상태**: Phase 3B-1 완료 (112/112 tests passing)
**마지막 업데이트**: 2025-11-15
