# MXRC 개발 가이드라인 (Gemini AI)

마지막 업데이트: 2025-11-15

## 프로젝트 개요

MXRC는 범용 로봇 제어 컨트롤러로, C++20으로 개발되며 계층적 아키텍처를 따릅니다.

## 활성 기술 스택

### 언어 및 빌드
- **언어**: C++20
- **빌드 시스템**: CMake 3.16+
- **컴파일러**: GCC 11+ or Clang 14+

### 라이브러리
- **로깅**: spdlog
- **테스트**: Google Test (GTest)
- **메모리 관리**: C++ STL (shared_ptr, unique_ptr)

### 개발 환경
- **OS**: Ubuntu 24.04 LTS PREEMPT_RT
- **IDE**: 자유 (VS Code, CLion 등)

## 프로젝트 구조

```text
mxrc/
├── src/core/
│   ├── action/              # Action Layer (기본 동작)
│   │   ├── interfaces/      # IAction, IActionFactory
│   │   ├── core/            # ActionExecutor, ActionFactory, ActionRegistry
│   │   ├── dto/             # ActionStatus, ActionDefinition
│   │   └── impl/            # DelayAction, MoveAction
│   │
│   ├── sequence/            # Sequence Layer (동작 조합)
│   │   ├── core/            # SequenceEngine, SequenceRegistry
│   │   │                    # ConditionEvaluator, RetryHandler
│   │   └── dto/             # SequenceDefinition, ConditionalBranch
│   │
│   └── task/                # Task Layer (실행 관리)
│       ├── interfaces/      # ITask, ITaskExecutor
│       ├── core/            # TaskExecutor, TaskRegistry
│       │                    # PeriodicScheduler, TriggerManager
│       └── dto/             # TaskDefinition, TaskExecution
│
├── tests/
│   ├── unit/                # 단위 테스트 (112 tests)
│   │   ├── action/          # 26 tests
│   │   ├── sequence/        # 33 tests
│   │   └── task/            # 53 tests
│   └── integration/         # 통합 테스트
│
├── specs/                   # 사양 문서
│   └── 017-action-sequence-orchestration/
│       ├── spec.md          # 기능 사양
│       ├── architecture.md  # 아키텍처 설계
│       ├── plan.md          # 구현 계획
│       └── tasks.md         # Task 목록
│
├── CMakeLists.txt
├── README.md               # 사용자 가이드
└── CLAUDE.md              # 상세 개발 가이드
```

## 빌드 및 테스트 명령어

### 빌드
```bash
# 빌드 디렉토리 생성 및 빌드
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### 테스트
```bash
# 전체 테스트 실행
./run_tests

# 계층별 테스트
./run_tests --gtest_filter=ActionExecutor*
./run_tests --gtest_filter=SequenceEngine*
./run_tests --gtest_filter=TaskExecutor*

# 간략한 출력
./run_tests --gtest_brief=1
```

### 실행
```bash
# 메인 프로그램 실행
./mxrc
```

## 코드 스타일 (C++)

### 네임스페이스
```cpp
// 계층별 네임스페이스
namespace mxrc::core::action { }
namespace mxrc::core::sequence { }
namespace mxrc::core::task { }
```

### 네이밍 규칙
- **클래스**: PascalCase (예: `ActionExecutor`, `SequenceEngine`)
- **함수/메서드**: camelCase (예: `execute()`, `getStatus()`)
- **변수**: snake_case (예: `action_id_`, `execution_time_`)
- **상수**: UPPER_SNAKE_CASE (예: `MAX_RETRIES`)
- **인터페이스**: I 접두사 (예: `IAction`, `ITask`)

### 헤더 가드
```cpp
#ifndef MXRC_CORE_ACTION_ACTION_EXECUTOR_H
#define MXRC_CORE_ACTION_ACTION_EXECUTOR_H
// ...
#endif
```

### RAII 원칙
```cpp
// Good: 스마트 포인터 사용
auto action = std::make_shared<DelayAction>(id, duration);

// Bad: 수동 메모리 관리
// DelayAction* action = new DelayAction(id, duration);
```

### 인터페이스 정의
```cpp
class IAction {
public:
    virtual ~IAction() = default;
    virtual void execute(ExecutionContext& context) = 0;
    virtual void cancel() = 0;
    virtual ActionStatus getStatus() const = 0;
};
```

## 아키텍처 개념

### 3계층 구조
```
Task Layer      → 실행 모드 관리 (ONCE, PERIODIC, TRIGGERED)
    ↓
Sequence Layer  → Action 조합 (순차, 조건부, 병렬)
    ↓
Action Layer    → 기본 동작 실행
```

### 주요 컴포넌트

#### Action Layer
- **ActionExecutor**: Action 실행 및 결과 수집
- **ActionFactory**: Action 인스턴스 생성 (플러그인 방식)
- **ActionRegistry**: Action 타입 등록 및 관리

#### Sequence Layer
- **SequenceEngine**: 시퀀스 실행 조율 (순차/조건/병렬)
- **SequenceRegistry**: 시퀀스 정의 저장 및 조회
- **ConditionEvaluator**: 조건식 평가 (==, !=, <, >, AND, OR)
- **RetryHandler**: 재시도 정책 관리

#### Task Layer
- **TaskExecutor**: Task 실행 및 상태 관리
- **TaskRegistry**: Task 정의 관리
- **PeriodicScheduler**: 주기적 실행 (개발 중)
- **TriggerManager**: 이벤트 트리거 (개발 중)

## 테스트 작성 가이드

### 테스트 구조
```cpp
#include "gtest/gtest.h"

namespace mxrc::core::action {

class ComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 초기화
    }

    void TearDown() override {
        // 정리
    }
};

TEST_F(ComponentTest, TestScenario) {
    // Given - 준비
    // When - 실행
    // Then - 검증
    EXPECT_EQ(expected, actual);
}

}
```

### 테스트 명명
- 파일: `ComponentName_test.cpp`
- 클래스: `ComponentNameTest`
- 케이스: 시나리오 설명 (예: `ExecuteActionSuccessfully`)

## 최근 변경 사항

### Phase 3B-1: Task Single Execution (2025-11-15)
**추가된 기능:**
- TaskRegistry 구현 (Task 정의 등록 및 조회)
- TaskExecutor ONCE 모드 구현 (단일 실행)
- TaskCoreExecutor 단위 테스트 14개 추가
- TaskRegistry 단위 테스트 12개 추가

**테스트 현황:**
- 전체: 112 tests (모두 통과 ✅)
- Action: 26 tests
- Sequence: 33 tests
- Task: 53 tests

### Phase 2A-2G: Sequence Layer (2025-11-14)
**추가된 기능:**
- SequenceEngine 구현 (순차/조건부/병렬 실행)
- ConditionEvaluator 구현 (조건식 평가)
- RetryHandler 구현 (재시도 정책)
- 시퀀스 템플릿 지원

**테스트 현황:**
- 33 tests 추가 (모두 통과)

### Phase 1: Action Layer (2025-11-13)
**추가된 기능:**
- IAction 인터페이스 정의
- ActionExecutor, ActionFactory, ActionRegistry 구현
- DelayAction, MoveAction 기본 구현
- ExecutionContext (Action 간 데이터 공유)

**테스트 현황:**
- 26 tests 추가 (모두 통과)

## 개발 프로세스

### 1. 사양 확인
```bash
# 관련 사양 문서 확인
specs/017-action-sequence-orchestration/spec.md
specs/017-action-sequence-orchestration/architecture.md
```

### 2. 테스트 작성 (TDD)
```bash
# 테스트 작성
tests/unit/<layer>/<Component>_test.cpp
```

### 3. 구현
```bash
# 헤더 및 구현 작성
src/core/<layer>/core/<Component>.h
src/core/<layer>/core/<Component>.cpp
```

### 4. 빌드 및 테스트
```bash
cd build
make -j$(nproc)
./run_tests --gtest_filter=<Component>*
```

### 5. 통합
```bash
# 전체 테스트 확인
./run_tests
```

## 다음 단계

### Phase 3B-2: Periodic Execution
- PeriodicScheduler 구현
- TaskExecutor periodic 모드 확장
- 주기적 실행 테스트

### Phase 3B-3: Triggered Execution
- TriggerManager 구현
- TaskExecutor triggered 모드 확장
- 이벤트 기반 실행 테스트

## 참고 문서

- **README.md**: 프로젝트 개요 및 사용 가이드
- **CLAUDE.md**: 상세 개발 가이드 및 API 문서
- **architecture.md**: 전체 사양 및 아키텍처

## 문제 해결

### 빌드 오류
```bash
# 빌드 디렉토리 삭제 후 재빌드
rm -rf build
mkdir build && cd build
cmake .. && make -j$(nproc)
```

### 테스트 실패
```bash
# 특정 테스트만 실행하여 디버깅
./run_tests --gtest_filter=<FailedTest>

# 상세 출력
./run_tests --gtest_verbose
```

### 의존성 문제
```bash
# Ubuntu에서 의존성 재설치
sudo apt-get update
sudo apt-get install --reinstall libspdlog-dev libgtest-dev
```

<!-- 수동 추가 시작 -->
<!-- 프로젝트별 추가 설정이나 노트를 여기에 작성 -->
<!-- 수동 추가 끝 -->
