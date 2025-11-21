---
description: 이슈 문서를 생성하고 추적합니다.
---

## User Input

```text
$ARGUMENTS
```

You **MUST** consider the user input before proceeding (if not empty).

## Outline

이슈 문서를 생성하여 `docs/issues/` 디렉토리에 저장합니다.

### 실행 순서

1. **이슈 정보 수집**:
   - 사용자가 제공한 이슈 설명 분석
   - 이슈 ID 자동 생성 (ISS-###)
   - 우선순위 결정 (High/Medium/Low)

2. **이슈 번호 결정**:
   ```bash
   # docs/issues/ 디렉토리에서 가장 높은 이슈 번호 찾기
   find docs/issues -name "ISS-*.md" | sort | tail -1
   ```
   - 기존 이슈 중 가장 높은 번호 + 1
   - 없으면 ISS-001부터 시작

3. **템플릿 로드**:
   - `.specify/templates/issue-template.md` 로드
   - 필요한 섹션 파악

4. **참고 자료**:
   - Constitution: `.specify/memory/constitution.md`
   - 관련 Spec: `docs/specs/` (해당되는 경우)
   - 아키텍처: `docs/architecture/` (관련 있는 경우)

5. **언어 사용 규칙**:
   - 모든 문서는 **한글**로 작성합니다
   - **기술 용어만 영어로 표기**합니다 (예: Task, Action, bug, error 등)

6. **이슈 문서 작성**:
   - 문제 설명 작성
   - 재현 방법 기술
   - 환경 정보 포함
   - 분석 및 해결 방안 제시
   - 관련 자료 링크

7. **파일 저장**:
   - 경로: `docs/issues/ISS-###-[short-description].md`
   - 예시: `docs/issues/ISS-001-task-timeout-error.md`

8. **진행도 업데이트**:

   a. **이슈 문서 상태 설정**:
   ```markdown
   **Status**: Open
   **Assigned To**: [담당자]
   **Progress**: 0/9 (작업 체크리스트)
   ```

   b. **관련 Spec 업데이트** (해당하는 경우):
   - spec.md 상단에 이슈 링크 추가
   - 예시:
     ```markdown
     **Related Issues**: [ISS-###](../../../issues/ISS-###-description.md)
     ```

   c. **Agent 파일 업데이트** (`dev/agent/CLAUDE.md`):
   - **원칙**: 컴팩트하게 유지 (최대 10개 이슈)
   - "## 활성 이슈" 섹션에 새로운 이슈 추가 (섹션이 없으면 생성)
   - 해결된 이슈가 있으면 먼저 제거
   - 예시:
     ```markdown
     ## 활성 이슈

     ### ISS-### [Issue Title]
     - **Priority**: [High/Medium/Low]
     - **Link**: [docs/issues/ISS-###.md](링크)
     - 상세: 이슈 문서 참조
     ```
   - **주의**: 상세 설명은 포함하지 않고 링크만 제공

9. **완료 보고**:
   - 이슈 ID 및 파일 경로 출력
   - 관련 문서 링크 제공

## 이슈 유형

### Bug Report (버그 리포트)
- 현상, 재현 방법, 예상/실제 동작 명확히 기술
- 로그 및 오류 메시지 포함

### Feature Request (기능 요청)
- 요청 배경 및 목적
- 제안된 솔루션

### Technical Debt (기술 부채)
- 현재 상태 및 문제점
- 리팩토링 제안

### Performance Issue (성능 이슈)
- 성능 측정 결과
- 병목 지점 분석

## 주의사항

- 이슈 문서는 한글로 작성하되, 기술 용어는 영어 유지
- 관련 코드나 로그는 원본 그대로 포함
- 관련 Spec/Architecture 문서와 연결
- 진행 상황을 체크리스트로 관리
