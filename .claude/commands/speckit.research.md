---
description: 기술 조사 문서를 생성하고 관리합니다.
---

## User Input

```text
$ARGUMENTS
```

You **MUST** consider the user input before proceeding (if not empty).

## Outline

기술 조사 문서를 생성하여 `docs/research/` 디렉토리에 저장합니다.

### 실행 순서

1. **조사 주제 분석**:
   - 사용자가 제공한 조사 주제 파악
   - 조사 ID 자동 생성 (RES-###)
   - 조사 목적 및 범위 결정

2. **조사 번호 결정**:
   ```bash
   # docs/research/ 디렉토리에서 가장 높은 조사 번호 찾기
   find docs/research -name "RES-*.md" | sort | tail -1
   ```
   - 기존 조사 문서 중 가장 높은 번호 + 1
   - 없으면 RES-001부터 시작

3. **조사 유형 결정**:
   - **Technology Research**: 새로운 기술/라이브러리 조사
   - **Performance Analysis**: 성능 측정 및 분석
   - **Algorithm Research**: 알고리즘 비교 및 선택
   - **Architecture Study**: 아키텍처 패턴 연구
   - **Problem Investigation**: 문제 원인 조사

4. **참고 자료**:
   - Constitution: `.specify/memory/constitution.md` - 프로젝트 제약사항 확인
   - 온보딩 자료: `docs/onboarding/` - 프로젝트 컨텍스트
   - 아키텍처: `docs/architecture/` - 현재 시스템 구조
   - 관련 Spec: `docs/specs/` (해당되는 경우)

5. **언어 사용 규칙**:
   - 모든 문서는 **한글**로 작성합니다
   - **기술 용어만 영어로 표기**합니다 (예: library, framework, algorithm 등)
   - 코드, 벤치마크 결과, 로그는 원래대로 표기

6. **조사 문서 작성**:

   a. **개요**:
   - 조사 목적 및 배경
   - 조사 범위
   - 관련 이슈/Spec (있는 경우)

   b. **조사 방법**:
   - 어떤 방식으로 조사했는지
   - 사용한 도구 및 환경

   c. **조사 내용**:

   **Technology Research의 경우**:
   - 기술 개요
   - 장단점 분석
   - 프로젝트 적용 가능성
   - 대안 비교

   **Performance Analysis의 경우**:
   - 성능 측정 결과
   - 병목 지점 분석
   - 개선 방안

   **Algorithm Research의 경우**:
   - 알고리즘 설명
   - 시간/공간 복잡도
   - 실험 결과
   - 비교 분석

   d. **결론 및 권장사항**:
   - 조사 결과 요약
   - 권장 방향
   - 다음 단계

   e. **참고 자료**:
   - 외부 문서/논문/블로그 링크
   - 관련 코드 저장소
   - 벤치마크 결과

7. **파일 저장**:
   - 경로: `docs/research/RES-###-[topic-name].md`
   - 예시: `docs/research/RES-001-async-logging-comparison.md`

8. **Feature Spec과 연결** (해당되는 경우):
   - 특정 Feature의 Phase 0 (Research Phase)인 경우:
     - `docs/specs/[###-feature]/research.md`에도 링크 추가
   - 조사 결과를 Plan에 반영 필요 시 표시

9. **진행도 업데이트**:

   a. **조사 문서 상태 설정**:
   ```markdown
   **Status**: In Progress → Completed
   **Last Updated**: [현재 날짜]
   ```

   b. **관련 Spec 업데이트** (해당하는 경우):
   - spec.md 또는 plan.md에 조사 결과 반영
   - 예시:
     ```markdown
     **Research**: [RES-###](../../../research/RES-###-topic.md)
     ```

   c. **Feature Spec의 research.md와 연결** (Phase 0 Research인 경우):
   - `docs/specs/[###-feature]/research.md`에 조사 내용 요약 추가
   - 상세 내용은 `docs/research/RES-###.md`로 링크

   d. **Agent 파일 업데이트** (`dev/agent/CLAUDE.md`):
   - **원칙**: 컴팩트하게 유지 (링크만)
   - 조사 결과가 기술 스택에 영향을 주면 "## 기술 스택" 업데이트
   - "## 최근 조사" 섹션에 링크 추가 (90일 이상 된 항목은 제거)
   - 예시:
     ```markdown
     ## 최근 조사

     **Research Documents**: [docs/research/](링크)
     - [RES-###: Topic](링크) - [날짜]

     상세: 조사 문서 참조
     ```
   - **주의**: 조사 내용 요약은 포함하지 않고 링크만 제공

10. **완료 보고**:
    - 조사 ID 및 파일 경로 출력
    - 주요 발견 사항 요약
    - 후속 조치 제안

## 조사 문서 템플릿 구조

```markdown
# Research: [TOPIC]

**Research ID**: RES-###
**Created**: [DATE]
**Type**: [Technology/Performance/Algorithm/Architecture/Problem]
**Status**: [In Progress/Completed]
**Related**: [Links to Spec/Issue/Architecture]

---

## 조사 목적

[왜 이 조사를 하는가?]

## 조사 범위

[무엇을 조사하는가?]

## 조사 방법

[어떻게 조사했는가?]

## 조사 내용

### [주제 1]

[조사 내용 상세]

### [주제 2]

[조사 내용 상세]

## 실험 결과

[벤치마크, 테스트 결과 등]

```
[코드나 데이터]
```

## 분석

[결과 분석]

## 결론

[조사 결과 요약]

## 권장사항

[권장 방향]

## 다음 단계

- [ ] [후속 조치 1]
- [ ] [후속 조치 2]

## 참고 자료

- [링크 1]
- [링크 2]
```

## 주의사항

- 조사 문서는 한글로 작성하되, 기술 용어는 영어 유지
- 실험 결과는 재현 가능하도록 상세히 기록
- 벤치마크는 환경 정보와 함께 기록
- 조사 과정에서 발견한 이슈는 별도 이슈로 등록
- Constitution의 원칙을 고려한 결론 도출

## Feature Spec과의 연계

특정 Feature의 Phase 0 Research인 경우:
1. `docs/specs/[###-feature]/research.md`에 링크 추가
2. Plan 작성 시 조사 결과 반영
3. 조사 내용이 Architecture에 영향을 주면 Architecture 문서 업데이트
