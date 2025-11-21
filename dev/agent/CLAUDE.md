# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

---

## 📋 프로젝트 개요

MXRC는 어떤 로봇도 제어할 수 있는 범용 로봇 제어 컨트롤러입니다.

**기술 스택**: C++20, CMake, GoogleTest, spdlog, TBB
**환경**: Ubuntu 24.04 LTS (PREEMPT_RT)
**아키텍처**: 3계층 구조 (Action → Sequence → Task)

**상세**: [docs/onboarding/](../docs/onboarding/) 참조

---

## 🔨 빌드 및 테스트

### 빌드
```bash
# Linux
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### 테스트
```bash
# 전체 테스트
./run_tests

# 특정 테스트
./run_tests --gtest_filter=ActionExecutor*
```

**상세**: README.md 참조

---

## 🏗️ 시스템 아키텍처

### 계층 구조
```
Task Layer (실행 모드 관리)
    ↓
Sequence Layer (Action 조합)
    ↓
Action Layer (기본 동작 실행)
```

**Architecture Documents**: [docs/architecture/](../docs/architecture/)
- 상세 아키텍처는 각 문서 참조
- 계층별 책임 및 컴포넌트 정보

**현재 상태**:
- Phase 017 완료: Action/Sequence/Task Layer (195 tests)
- Phase 019 진행: Event System & DataStore

---

## ✨ 활성 기능

현재 개발 중이거나 최근 완료된 기능들 (최대 5개)

> 기능 완료 후 30일이 지나면 자동으로 제거됩니다.

### 018-systemd-process-management
- **Status**: Review
- **Spec**: [docs/specs/018-systemd-process-management/spec.md](../docs/specs/018-systemd-process-management/spec.md)
- 상세: Spec 문서 참조

**Feature Specifications**: [docs/specs/](../docs/specs/)
- 상세: 각 Spec 문서 참조

---

## 🐛 활성 이슈

현재 해결 중인 이슈들 (최대 10개)

> 해결된 이슈는 즉시 제거됩니다.

**Issue Tracking**: [docs/issues/](../docs/issues/)
- 상세: 각 이슈 문서 참조

---

## 🔬 최근 조사

**Research Documents**: [docs/research/](../docs/research/)
- 상세: 조사 문서 참조

> 90일 이상 된 조사는 자동으로 제거됩니다.

---

## 📚 주요 문서

### Constitution (프로젝트 헌법)
[.specify/memory/constitution.md](.specify/memory/constitution.md)
- 7가지 Core Principles
- 개발 워크플로우
- 진행도 추적 규칙
- Agent 파일 동기화 규칙

### 온보딩 자료
[docs/onboarding/](../docs/onboarding/)
- 프로젝트 구조
- 개발 가이드
- 코드 스타일

### 아키텍처
[docs/architecture/](../docs/architecture/)
- 시스템 아키텍처
- 계층별 설계
- 컴포넌트 상세

---

## 💻 코드 작성 가이드

### 핵심 원칙

**1. RAII (NON-NEGOTIABLE)**
```cpp
// ✅ 스마트 포인터 사용
auto action = std::make_shared<DelayAction>("id", 100);

// ❌ 수동 메모리 관리 금지
Action* action = new DelayAction("id", 100);  // 금지!
```

**2. 인터페이스 기반 설계**
```cpp
class IAction {
    virtual ~IAction() = default;
    virtual void execute(ExecutionContext& ctx) = 0;
};
```

**3. 주석은 한글, 기술 용어는 영어**
```cpp
// ✅ concurrent_hash_map으로 스레드 안전한 데이터 접근
// ❌ Thread-safe data access using concurrent_hash_map (영어 금지)
```

**상세**: [docs/onboarding/code_style.md](../docs/onboarding/code_style.md) 참조

---

## 🧪 테스트 규칙

### 테스트 커버리지
- Action Layer: 12 tests
- Sequence Layer: 14 tests
- Task Layer: 67 tests
- Event Layer: 42+ tests
- DataStore: 66 tests
- **전체**: 195+ tests

### 메모리 안전성
```bash
# AddressSanitizer (항상 활성화)
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Valgrind
valgrind --leak-check=full ./run_tests
```

**상세**: [docs/onboarding/testing.md](../docs/onboarding/testing.md) 참조

---

## 📝 Git 커밋 가이드

### 형식
```
<타입>(<범위>): <제목>

<본문>
```

### 규칙
- ✅ **한글 사용 필수**
- ❌ AI 언급 금지 ("Claude가 작성" 등)
- ❌ 영어 커밋 메시지 금지

**예시**:
```
fix(action): ActionExecutor 소멸자 데드락 해결

RAII 패턴으로 스레드 안전하게 정리

관련 이슈: #003
```

**상세**: [docs/onboarding/git_guide.md](../docs/onboarding/git_guide.md) 참조

---

## 🚀 개발 워크플로우

### 기능 개발
1. `/speckit.specify` - Specification 작성 
2. `/speckit.plan` - 구현 계획 수립
3. `/speckit.tasks` - 작업 목록 생성 
4. `/speckit.implement` - 구현
5. `/speckit.tests` - 테스트 작성 및 실행
6. `/speckit.taskstoissues` - 문제 발생시 이슈 생성
5. 코드 리뷰 및 병합

### 문서 작성
- `/speckit.issue` - 이슈 문서 생성
- `/speckit.architecture` - 아키텍처 문서 생성
- `/speckit.research` - 기술 조사 문서 생성

### 진행도 업데이트 (MANDATORY)
각 단계마다 다음 업데이트:
- Spec 문서 Status
- dev/agent/CLAUDE.md (이 파일)
- 관련 문서 링크

**상세**: [.specify/memory/constitution.md](.specify/memory/constitution.md) 참조

---

## 🔗 디렉토리 구조

```
src/core/
├── action/         # Action Layer
├── sequence/       # Sequence Layer
├── task/           # Task Layer
├── event/          # Event System
└── datastore/      # Data Management

docs/
├── specs/          # Feature Specifications
├── architecture/   # Architecture Documents
├── issues/         # Issue Tracking
├── research/       # Research Documents
└── onboarding/     # Onboarding Materials

tests/
├── unit/           # Unit Tests
└── integration/    # Integration Tests
```

---

## ⚡ 성능 목표

- Task 실행 오버헤드 < 1ms
- 로깅 성능: 평균 0.111μs
- 처리량: 5,000,000 msg/sec
- 메모리 누수 없음 (AddressSanitizer 검증)

---

## 📖 참고 자료

### 필수 문서
- **Constitution**: [.specify/memory/constitution.md](.specify/memory/constitution.md)
- **README**: [README.md](../README.md)
- **온보딩**: [docs/onboarding/](../docs/onboarding/)

### 문서 위치
- Specifications: `docs/specs/[###-feature]/`
- Architecture: `docs/architecture/`
- Issues: `docs/issues/`
- Research: `docs/research/`

### 이슈 해결
- Troubleshooting: `docs/TROUBLESHOOTING.md`
- 크리티컬 이슈: `docs/debugging_with_lldb.md`

---

**Last Updated**: 2025-01-20
**Total Tests**: 195+ passing ✅
**Memory Safe**: AddressSanitizer enabled 🔒

---

> 💡 **중요**: 이 파일은 컴팩트하게 유지됩니다. 상세 내용은 항상 docs/ 디렉토리의 문서를 참조하세요.

## Active Technologies
- C++20 (GCC 11+ 또는 Clang 14+) + libsystemd-dev (sd_notify API), spdlog >= 1.x, nlohmann_json >= 3.11.0, Prometheus C++ client (선택적) (018-systemd-process-management)
- N/A (systemd 메트릭 및 로그는 journald/Prometheus에 저장) (018-systemd-process-management)

## Recent Changes
- 018-systemd-process-management: Added C++20 (GCC 11+ 또는 Clang 14+) + libsystemd-dev (sd_notify API), spdlog >= 1.x, nlohmann_json >= 3.11.0, Prometheus C++ client (선택적)
