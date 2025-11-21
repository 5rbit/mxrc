---
description: 아키텍처 문서를 생성하고 관리합니다.
---

## User Input

```text
$ARGUMENTS
```

You **MUST** consider the user input before proceeding (if not empty).

## Outline

아키텍처 문서를 생성하여 `docs/architecture/` 디렉토리에 저장합니다.

### 실행 순서

1. **아키텍처 정보 수집**:
   - 사용자가 제공한 아키텍처 컴포넌트 설명 분석
   - 아키텍처 ID 자동 생성 (ARCH-###)
   - 문서 상태 결정 (Draft/Review/Approved)

2. **아키텍처 번호 결정**:
   ```bash
   # docs/architecture/ 디렉토리에서 가장 높은 아키텍처 번호 찾기
   find docs/architecture -name "ARCH-*.md" | sort | tail -1
   ```
   - 기존 아키텍처 문서 중 가장 높은 번호 + 1
   - 없으면 ARCH-001부터 시작

3. **템플릿 로드**:
   - `.specify/templates/architecture-template.md` 로드
   - 필요한 섹션 파악

4. **참고 자료**:
   - Constitution: `.specify/memory/constitution.md` - 아키텍처 원칙 확인
   - 온보딩 자료: `docs/onboarding/` - 프로젝트 구조 이해
   - 기존 아키텍처: `docs/architecture/` - 일관성 유지
   - 관련 Spec: `docs/specs/` (해당되는 경우)

5. **언어 사용 규칙**:
   - 모든 문서는 **한글**로 작성합니다
   - **기술 용어만 영어로 표기**합니다 (예: Layer, Interface, Pattern 등)
   - 다이어그램 레이블과 코드는 원래대로 표기

6. **아키텍처 문서 작성**:

   a. **개요 섹션**:
   - 목적, 범위, 주요 목표 기술

   b. **아키텍처 원칙**:
   - Constitution의 Core Principles 준수 확인
   - 계층적 아키텍처, 인터페이스 기반 설계 등 반영

   c. **시스템 구조**:
   - 고수준 아키텍처 다이어그램
   - 컴포넌트 다이어그램
   - 데이터 흐름

   d. **주요 컴포넌트**:
   - 각 컴포넌트의 책임, 인터페이스, 구현 클래스
   - 의존성 관계

   e. **상호작용 및 시퀀스**:
   - 주요 시나리오별 시퀀스 다이어그램

   f. **설계 결정**:
   - 중요한 아키텍처 결정 사항 및 근거
   - 고려했던 대안과 선택 이유

   g. **성능 고려사항**:
   - 성능 목표 및 최적화 전략

   h. **확장성 및 유지보수**:
   - 확장 포인트 및 플러그인 아키텍처

7. **파일 저장**:
   - 경로: `docs/architecture/ARCH-###-[component-name].md`
   - 예시: `docs/architecture/ARCH-001-task-execution-layer.md`

8. **진행도 업데이트**:

   a. **아키텍처 문서 상태 설정**:
   ```markdown
   **Status**: Draft
   **Version**: 1.0.0
   **Last Updated**: [현재 날짜]
   ```

   b. **관련 Spec 업데이트** (해당하는 경우):
   - spec.md에 아키텍처 문서 링크 추가
   - 예시:
     ```markdown
     **Architecture**: [ARCH-###](../../../architecture/ARCH-###-component.md)
     ```

   c. **Agent 파일 업데이트** (`dev/agent/CLAUDE.md`):
   - **원칙**: 컴팩트하게 유지 (링크 목록만)
   - "## 시스템 아키텍처" 섹션의 "Architecture Documents" 부분 업데이트
   - 새로운 아키텍처 문서 링크만 추가
   - 예시:
     ```markdown
     **Architecture Documents**: [docs/architecture/](링크)
     - [ARCH-###: Component Name](링크)

     상세: 각 아키텍처 문서 참조
     ```
   - **주의**: 아키텍처 다이어그램이나 상세 설명은 포함하지 않음

9. **완료 보고**:
   - 아키텍처 ID 및 파일 경로 출력
   - 관련 문서 링크 제공
   - Constitution 준수 여부 확인

## 아키텍처 문서 유형

### Component Architecture (컴포넌트 아키텍처)
- 특정 컴포넌트의 상세 설계
- 인터페이스 및 구현 클래스

### Layer Architecture (계층 아키텍처)
- 특정 계층(Action/Sequence/Task)의 전체 구조
- 계층 간 상호작용

### System Architecture (시스템 아키텍처)
- 전체 시스템 구조
- 고수준 설계 결정

### Integration Architecture (통합 아키텍처)
- 외부 시스템과의 통합 방식
- 통신 프로토콜 및 데이터 포맷

## Constitution 준수 사항

문서 작성 시 다음 원칙들을 반드시 준수:
- ✅ 계층적 아키텍처 원칙
- ✅ 인터페이스 기반 설계
- ✅ RAII 원칙
- ✅ 메모리 안전성
- ✅ 실시간 성능 목표

## 주의사항

- 아키텍처 문서는 한글로 작성하되, 기술 용어는 영어 유지
- 다이어그램은 ASCII art 또는 Mermaid 형식 사용
- Constitution의 원칙과 충돌하지 않는지 확인
- 기존 아키텍처와의 일관성 유지
- 변경 이력 관리
