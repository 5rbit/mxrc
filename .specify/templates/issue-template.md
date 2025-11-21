# Issue: [ISSUE_TITLE]

**Issue ID**: ISS-[###]
**Created**: [DATE]
**Status**: Open
**Priority**: [High/Medium/Low]
**Progress**: 0/9 (체크리스트 기준)
**Last Updated**: [DATE]
**Assigned To**: [담당자]
**Related Spec**: [Link to spec.md if applicable]

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 이슈 설명은 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Task, Action, API, bug, error 등)
- 코드 스니펫과 로그는 원래대로 표기합니다

**예시**:
- ✅ 좋은 예: "Task 실행 시 timeout 오류가 발생합니다"
- ❌ 나쁜 예: "Task execution fails with timeout error"

---

## 문제 설명

### 현상
[무엇이 잘못되었는지 또는 무엇이 필요한지 간단히 설명]

### 재현 방법
1. [첫 번째 단계]
2. [두 번째 단계]
3. [세 번째 단계]

### 예상 동작
[어떻게 동작해야 하는지 설명]

### 실제 동작
[실제로 어떻게 동작하는지 설명]

---

## 환경 정보

- **OS**: Ubuntu 24.04 LTS (PREEMPT_RT)
- **C++ 버전**: C++20
- **컴파일러**: [GCC 11+ / Clang 14+]
- **빌드 설정**: [Release/Debug]
- **관련 컴포넌트**: [Action/Sequence/Task/DataStore/Logger 등]

---

## 로그 및 오류 메시지

```
[오류 로그나 스택 트레이스를 여기에 붙여넣기]
```

---

## 분석

### 근본 원인 (Root Cause)
[문제의 근본 원인 분석]

### 영향 범위 (Impact)
- [ ] Action Layer
- [ ] Sequence Layer
- [ ] Task Layer
- [ ] DataStore
- [ ] Logger
- [ ] 기타: [명시]

### 발생 빈도
- [ ] 항상 발생
- [ ] 간헐적 발생
- [ ] 특정 조건에서만 발생: [조건 명시]

---

## 해결 방안

### 제안된 솔루션
[문제를 해결하기 위한 제안]

### 대안
1. [대안 1]
2. [대안 2]

### 고려 사항
- [구현 시 고려해야 할 사항들]
- [성능 영향]
- [호환성 문제]

---

## 관련 자료

### 관련 이슈
- [ISS-###]: [관련 이슈 제목]

### 관련 Spec
- [docs/specs/###-feature/spec.md]

### 참고 문서
- [docs/architecture/...]
- [docs/onboarding/...]

### 참고 코드
```cpp
// 문제가 발생하는 코드나 관련 코드
```

---

## 작업 체크리스트

- [ ] 문제 재현 확인
- [ ] 근본 원인 분석 완료
- [ ] 해결 방안 결정
- [ ] 테스트 케이스 작성
- [ ] 구현 완료
- [ ] 코드 리뷰
- [ ] 테스트 통과
- [ ] 문서 업데이트
- [ ] 이슈 종료

---

## 주석

[추가 노트나 코멘트]

---

**Last Updated**: [DATE]
**Assigned To**: [이름]
**Reviewed By**: [이름]
