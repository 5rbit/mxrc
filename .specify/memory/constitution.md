<!--
Sync Impact Report - Constitution v1.1.0
========================================

Version Change: 1.0.0 → 1.1.0 (MINOR)
Rationale: Expanded Principle VII to clarify that AI assistant work output must be in Korean

Modified Principles:
- Principle VII: "문서화 및 한글 사용" → "문서화 및 한글 사용 (AI Assistant Output)"
  - Added explicit requirement that AI assistants must display work content in Korean
  - Clarified that technical terms remain in English
  - Added examples for assistant communication (good vs bad)

Added Sections:
- None

Removed Sections:
- None

Template Updates:
✅ .specify/templates/spec-template.md - Already includes Korean language guidelines (lines 14-21)
✅ .specify/templates/plan-template.md - Already includes Korean language guidelines (lines 15-22)
✅ .specify/templates/tasks-template.md - Not checked but inherits from spec/plan patterns
✅ Agent guidance files - No update needed (constitution itself is the authority)

Follow-up TODOs:
- None (all placeholders filled)

Last Amendment: 2025-01-22
-->

# MXRC Constitution

본 Constitution은 MXRC 프로젝트의 핵심 원칙과 개발 규칙을 정의합니다.

---

## Core Principles

### I. 계층적 아키텍처 원칙

**설명**: 시스템은 명확한 3계층 구조를 유지합니다
- **Action Layer**: 기본 동작 실행 및 타임아웃 관리
- **Sequence Layer**: Action 조합 및 조건/병렬 실행
- **Task Layer**: 실행 모드 관리 (단일/주기적/트리거)

**규칙**:
- 상위 계층은 하위 계층에만 의존합니다
- 계층 간 역의존성은 금지됩니다
- 계층별 책임을 명확히 분리합니다

### II. 인터페이스 기반 설계

**설명**: 모든 확장 지점은 인터페이스를 통해 정의됩니다

**규칙**:
- 모든 인터페이스는 I-prefix를 사용합니다 (예: IAction, ITask)
- 순수 가상 클래스로 인터페이스를 정의합니다
- 의존성 주입(Dependency Injection)을 통한 느슨한 결합을 유지합니다

### III. RAII 원칙 (NON-NEGOTIABLE)

**설명**: 모든 리소스는 생성자에서 할당, 소멸자에서 해제됩니다

**규칙**:
- 스마트 포인터(shared_ptr, unique_ptr) 사용 필수
- 수동 메모리 관리(new/delete) 금지
- 리소스 누수 절대 불가

### IV. 메모리 안전성 (NON-NEGOTIABLE)

**설명**: 모든 빌드에서 메모리 안전성을 보장합니다

**규칙**:
- AddressSanitizer 항상 활성화 (`-fsanitize=address`)
- 테스트 실행 시 메모리 누수 자동 감지 필수
- 메모리 누수가 발견되면 즉시 수정

### V. 테스트 주도 개발 (TDD)

**설명**: 모든 기능은 테스트와 함께 개발됩니다

**규칙**:
- 새로운 기능은 테스트 작성 → 테스트 실패 확인 → 구현 순서로 진행
- 단위 테스트와 통합 테스트 모두 작성
- 모든 테스트는 항상 통과해야 함

### VI. 실시간 성능

**설명**: PREEMPT_RT 커널에서 실시간 성능을 보장합니다

**목표**:
- Task 실행 오버헤드 < 1ms
- 로깅 성능: 평균 0.111μs (비동기 로깅)
- 처리량: 5,000,000 msg/sec
- 저지연 설계 필수

### VII. 문서화 및 한글 사용 (AI Assistant Output)

**설명**: 모든 문서는 한글로 작성되며, 기술 용어만 영어로 표기합니다

**규칙**:
- Specification, Plan, Tasks, Architecture 문서는 모두 한글 작성
- 기술 용어(Task, Action, Sequence, API, JSON 등)는 영어 표기
- 코드 주석은 영어 사용 가능
- `docs/onboarding/` 디렉토리의 온보딩 자료 참조

**AI Assistant 작업 출력 규칙** (MANDATORY):
- AI assistant는 모든 작업 내용을 **한글로 설명**해야 합니다
- 작업 진행 상황, 구현 계획, 오류 분석 등 모든 커뮤니케이션은 한글 사용
- 기술 용어(class, function, API, JSON, EventBus, PriorityQueue 등)는 영어 유지
- 코드 스니펫과 로그 출력은 원문 유지

**예시**:
- ✅ 좋은 예: "PriorityQueue 클래스를 구현했습니다. 4단계 우선순위와 backpressure 정책을 적용했습니다."
- ❌ 나쁜 예: "Implemented PriorityQueue class with 4-level priority and backpressure policy."
- ✅ 좋은 예: "테스트 21개가 모두 통과했습니다. build가 성공적으로 완료되었습니다."
- ❌ 나쁜 예: "All 21 tests passed. Build completed successfully."

---

## 문서 구조

### docs/ 디렉토리 구조
```
docs/
├── specs/              # Feature Specifications
│   └── [###-feature]/
│       ├── spec.md
│       ├── plan.md
│       ├── tasks.md
│       ├── research.md
│       └── contracts/
├── architecture/       # 시스템 아키텍처 문서
├── onboarding/         # 온보딩 및 프로젝트 가이드
├── issues/             # 이슈 추적 및 분석
├── templates/          # 문서 템플릿
└── research/           # 기술 조사 자료
```

### 템플릿 위치
- **Specification 템플릿**: `.specify/templates/spec-template.md`
- **Plan 템플릿**: `.specify/templates/plan-template.md`
- **Tasks 템플릿**: `.specify/templates/tasks-template.md`
- **Issue 템플릿**: `.specify/templates/issue-template.md`
- **Architecture 템플릿**: `.specify/templates/architecture-template.md`

### 소스 코드 구조
```
src/core/<layer>/
├── interfaces/     # I-prefix 인터페이스
├── core/          # 핵심 구현체
├── dto/           # Data Transfer Objects
├── impl/          # 구체적 구현체
└── util/          # 유틸리티
```

### 테스트 구조
```
tests/
├── unit/           # 단위 테스트
│   ├── action/
│   ├── sequence/
│   ├── task/
│   └── datastore/
└── integration/    # 통합 테스트
```

---

## 기술 스택

### 언어 및 표준
- **언어**: C++20
- **컴파일러**: GCC 11+ or Clang 14+
- **OS**: Ubuntu 24.04 LTS (PREEMPT_RT)

### 빌드 시스템
- **빌드 도구**: CMake 3.16+
- **병렬 빌드**: `make -j$(nproc)`
- **테스트**: GoogleTest framework

### 핵심 의존성
- **spdlog >= 1.x**: 비동기 로깅 시스템
- **GTest**: 테스트 프레임워크
- **TBB**: 스레드 안전한 데이터 구조
- **nlohmann_json >= 3.11.0**: JSON 처리

---

## 코드 스타일

### 네이밍 규약
- **클래스**: PascalCase (ActionExecutor, TaskRegistry)
- **인터페이스**: I-prefix (IAction, ITaskExecutor)
- **파일**: 클래스 이름과 동일 (ActionExecutor.h, ActionExecutor.cpp)
- **네임스페이스**: 소문자 (mxrc::core::action)

### 로깅 규약
- **trace**: 상세한 실행 흐름
- **debug**: 디버깅 정보
- **info**: 일반 정보
- **warn**: 경고
- **error**: 오류
- **critical**: 치명적 오류 (즉시 플러시됨)

---

## 개발 워크플로우

### 1. 기능 개발 프로세스
1. `/specify` - Specification 작성 (`docs/specs/[###-feature]/spec.md`)
   - **진행도 업데이트**: spec.md 상단 Status를 "Draft" → "Review" 업데이트
   - Agent 파일 (`dev/agent/CLAUDE.md`) 업데이트
2. `/plan` - 구현 계획 수립 (`docs/specs/[###-feature]/plan.md`)
   - **진행도 업데이트**: spec.md Status를 "Review" → "Planning"으로 업데이트
   - plan.md에 진행 상황 기록
3. `/tasks` - 작업 목록 생성 (`docs/specs/[###-feature]/tasks.md`)
   - **진행도 업데이트**: spec.md Status를 "Planning" → "In Progress"로 업데이트
   - tasks.md에서 작업 체크리스트 관리
4. 구현 및 테스트
   - **진행도 업데이트**: tasks.md에서 각 작업 완료 시 체크 표시
   - 주요 마일스톤 완료 시 spec.md에 기록
5. 코드 리뷰 및 병합
   - **진행도 업데이트**: spec.md Status를 "In Progress" → "Completed"로 업데이트
   - Agent 파일에 완료된 기능 반영

### 2. 문서화 프로세스
- **Architecture 변경**: `docs/architecture/` 업데이트
  - **진행도 업데이트**: 관련 spec.md에 아키텍처 문서 링크 추가
  - ARCH-### 문서 Status 관리 (Draft → Review → Approved)
- **새로운 이슈**: `docs/issues/` 문서 작성
  - **진행도 업데이트**: 이슈 체크리스트에 진행 상황 표시
  - 관련 spec.md에 이슈 링크 추가
- **기술 조사**: `docs/research/` 자료 추가
  - **진행도 업데이트**: RES-### 문서 Status 관리 (In Progress → Completed)
  - 조사 결과를 plan.md에 반영
- **온보딩 업데이트**: `docs/onboarding/` 개선
  - 새로운 기능/컴포넌트 추가 시 온보딩 자료 업데이트

### 3. Agent 파일 동기화

#### 기본 원칙: 컴팩트 유지 (MANDATORY)
- **Agent 파일은 최소한의 정보만 포함**
- **상세 내용은 링크로 참조**
- **정기적으로 오래된 항목 제거**

#### 자동 업데이트 대상
- **파일**: `dev/agent/CLAUDE.md`
- **업데이트 시점**:
  - Feature Spec 작성 완료 시
  - Architecture 문서 생성/변경 시
  - 주요 기능 구현 완료 시

#### 업데이트 내용 (참조 방식)

**활성 기능** (최대 5개까지만):
```markdown
## 활성 기능

### [###-feature-name]
- **Status**: [상태]
- **Spec**: [docs/specs/###-feature/spec.md](링크)
- 상세: Spec 문서 참조
```

**활성 이슈** (최대 10개까지만):
```markdown
## 활성 이슈

### ISS-### [Issue Title]
- **Priority**: [우선순위]
- **Link**: [docs/issues/ISS-###.md](링크)
- 상세: 이슈 문서 참조
```

**아키텍처 문서**:
```markdown
## 시스템 아키텍처

**Architecture Documents**: [docs/architecture/](링크)
- [ARCH-001: Task Layer](링크)
- 상세: 각 아키텍처 문서 참조
```

**최근 조사**:
```markdown
## 최근 조사

**Research Documents**: [docs/research/](링크)
- 상세: 조사 문서 참조
```

#### 정리 규칙
- **완료된 기능**: 30일 후 "활성 기능"에서 제거
- **해결된 이슈**: 즉시 "활성 이슈"에서 제거
- **오래된 조사**: 90일 후 "최근 조사"에서 제거
- **상세 정보**: 항상 docs/ 문서를 참조하도록 안내

### 4. 진행도 추적 규칙 (MANDATORY)

#### Spec 문서 상태 관리
```markdown
**Status**: Draft → Review → Planning → In Progress → Completed
```

각 상태별 의미:
- **Draft**: 초기 작성 중
- **Review**: 검토 대기 중
- **Planning**: 구현 계획 수립 중
- **In Progress**: 구현 진행 중
- **Completed**: 완료

#### 진행도 업데이트 포맷
모든 spec.md 파일 상단에 다음 정보 유지:
```markdown
**Status**: [현재 상태]
**Progress**: [완료된 작업 / 전체 작업]
**Last Updated**: [최종 업데이트 날짜]
```

#### 체크리스트 관리
- tasks.md: `- [ ]` → `- [x]` 형식으로 완료 표시
- 완료율 자동 계산: (완료된 작업 / 전체 작업) * 100%

### 5. 테스트 게이트
- 모든 테스트는 항상 통과해야 함
- 메모리 누수 없음 검증 필수
- AddressSanitizer 오류 없음 확인

---

## Governance

### 원칙 준수
- 본 Constitution의 원칙은 모든 개발 과정에 적용됩니다
- 원칙 위반 시 코드 리뷰에서 반려됩니다
- 특별한 사유가 있을 경우, 문서화 후 팀 승인 필요

### 수정 절차
- Constitution 수정은 팀 전체 합의가 필요합니다
- 수정 사항은 반드시 문서화되어야 합니다
- 수정 후 관련 문서 및 코드 업데이트 필수

### 참고 자료
- **온보딩 자료**: `docs/onboarding/`
- **아키텍처 문서**: `docs/architecture/`
- **Specifications**: `docs/specs/`
- **이슈 추적**: `docs/issues/`
- **기술 조사**: `docs/research/`

---

**Version**: 1.1.0
**Ratified**: 2025-01-20
**Last Amended**: 2025-01-22
