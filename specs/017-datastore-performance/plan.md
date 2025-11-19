# 구현 계획: DataStore 성능 및 안정성 개선

**브랜치**: `017-datastore-performance` | **날짜**: 2025-11-19 | **사양서**: [spec.md](spec.md)
**입력**: `/specs/017-datastore-performance/spec.md` 의 기능 사양서

**참고**: 이 템플릿은 `/plan` 명령어로 채워집니다.

## 요약

이 구현 계획은 DataStore의 고동시성 환경에서의 성능 병목 현상과 메모리 안전성 문제를 해결합니다. 주요 개선 사항:

1. **Lock-free 메트릭 수집**: 전역 뮤텍스를 `std::atomic`으로 대체하여 메트릭 수집 시 락 경합 제거
2. **안전한 Notifier 생명주기 관리**: Raw pointer를 `std::shared_ptr`로 전환하여 dangling pointer 문제 해결
3. **최적화된 읽기 접근**: `std::shared_mutex`를 사용하여 읽기 병렬성 지원

**기술적 접근 방식**:
- C++17 표준 라이브러리 활용 (atomic, shared_mutex, shared_ptr)
- oneTBB concurrent_hash_map 유지 (기존 자료구조 재활용)
- 기존 API 변경 없이 내부 구현만 개선 (하위 호환성 보장)
- RAII 패턴으로 메모리 안전성 보장

## 기술 컨텍스트

**언어/버전**: C++20, GCC 13.2, CMAKE 3.16+
**주요 의존성**:
- oneTBB (Intel Threading Building Blocks) - concurrent_hash_map
- spdlog - 로깅
- Google Test - 단위/통합 테스트
**저장소**: 인메모리 데이터 저장소 (DataStore 클래스)
**테스트**: Google Test (단위 테스트 + 통합 테스트 + 성능 벤치마크)
**대상 플랫폼**: Linux Ubuntu 24.04 LTS, PREEMPT_RT
**프로젝트 유형**: 범용 로봇 제어 컨트롤러 (MXRC)
**성능 목표**:
- 100개 동시 스레드 환경에서 80% 성능 개선
- 메트릭 수집 오버헤드 <1%
- shared_mutex 획득 시간 <1μs
**제약 조건**:
- 기존 DataStore API 변경 금지 (하위 호환성 필수)
- 헤더 파일 공개 인터페이스 변경 최소화
- 컴파일 시간 증가 최소화
- 실시간 성능 유지 (락 대기 시간 최소화)
**규모/범위**:
- DataStore.h/cpp 파일 수정 (내부 구현만)
- 성능 테스트 추가 (벤치마크 스위트)
- 메모리 안전성 검증 (Valgrind, AddressSanitizer)

## Constitution 준수 확인

*GATE: 0단계 연구 시작 전에 반드시 통과해야 합니다. 1단계 설계 후 다시 확인합니다.*

- ✅ **실시간성 보장**:
  - Lock-free atomic 연산은 예측 가능한 O(1) 시간 복잡도
  - shared_mutex는 읽기 락 획득 시 1μs 미만 목표 (실시간 제약 준수)
  - 전역 뮤텍스 제거로 락 경합 최소화 → 실시간성 향상

- ✅ **신뢰성 및 안전성**:
  - RAII 패턴 (shared_ptr, lock_guard, shared_lock) 사용
  - Dangling pointer 문제 해결 (weak_ptr → shared_ptr 전환)
  - 메모리 안전성 검증 (Valgrind, AddressSanitizer)
  - 예외 안전성 (try-catch로 콜백 예외 격리)

- ✅ **테스트 주도 개발**:
  - 단위 테스트: 개별 메서드 테스트 (atomic 연산, shared_ptr 생명주기)
  - 통합 테스트: 다중 스레드 환경 스트레스 테스트
  - 성능 벤치마크: 1/10/50/100 스레드 처리량 측정
  - 메모리 누수 테스트: Valgrind 72시간 실행

- ✅ **모듈식 설계**:
  - 기존 DataStore API 변경 없음 (내부 구현만 수정)
  - 공개 인터페이스 유지 (하위 호환성 보장)
  - 메트릭 수집/Notifier 관리는 내부 세부 사항

- ✅ **한글 문서화**:
  - 모든 설계 결정 문서화 (이 plan.md)
  - 코드 주석 한글 작성 (CLAUDE.md 규칙 준수)
  - API 변경 없으므로 외부 문서 업데이트 불필요

- ✅ **버전 관리**:
  - 내부 구현 변경으로 PATCH 버전 증가 (예: 1.2.0 → 1.2.1)
  - API 변경 없으므로 MAJOR/MINOR 버전 변경 불필요

## 프로젝트 구조

### 문서 (이 기능)

```text
specs/017-datastore-performance/
├── spec.md              # 기능 사양서 (완료)
├── plan.md              # 이 파일 (구현 계획)
├── research.md          # Phase 0 출력 (기술 리서치)
├── data-model.md        # Phase 1 출력 (데이터 모델)
├── quickstart.md        # Phase 1 출력 (개발자 가이드)
├── contracts/           # Phase 1 출력 (API 계약서 - 해당 없음, 내부 구현만)
├── checklists/          # 품질 체크리스트
│   └── requirements.md  # 사양서 품질 검증 (완료)
└── tasks.md             # Phase 2 출력 (/tasks 명령어로 생성)
```

### 소스 코드 (리포지토리 루트)

```text
src/core/datastore/
├── DataStore.h          # 🔧 수정 필요: 내부 멤버 변수 타입 변경
│                        #   - std::mutex → std::atomic<size_t> (메트릭)
│                        #   - std::mutex → std::shared_mutex (메타데이터)
│                        #   - raw pointer → std::shared_ptr (Notifier)
└── DataStore.cpp        # 🔧 수정 필요: 구현 로직 변경

tests/unit/datastore/
├── DataStore_test.cpp   # ✅ 기존 테스트 (하위 호환성 검증)
└── DataStorePerformance_test.cpp  # ➕ 신규: 성능 벤치마크

tests/integration/datastore/
└── DataStoreConcurrency_test.cpp  # ➕ 신규: 동시성 스트레스 테스트
```

**구조 결정**:
- 기존 DataStore 클래스 내부 구현만 수정 (API 변경 없음)
- 새로운 파일 생성 최소화 (테스트 파일만 추가)
- CLAUDE.md에 명시된 디렉토리 구조 준수

## 복잡성 추적

> **Constitution 준수 확인에 정당화가 필요한 위반 사항이 있는 경우에만 작성하십시오**

**해당 사항 없음**: 모든 Constitution 원칙을 준수하며, 복잡성을 증가시키지 않고 오히려 감소시킵니다.

- 전역 뮤텍스 제거 → 동시성 복잡성 감소
- shared_ptr 사용 → 메모리 관리 복잡성 감소
- 기존 API 유지 → 통합 복잡성 없음

## Phase 0: 연구 및 조사 (Research)

이 단계에서는 구현에 필요한 기술적 세부 사항을 조사합니다.

### 조사 항목

1. **std::atomic 메모리 순서 (Memory Order)**
   - 연구 질문: relaxed, acquire, release, seq_cst 중 어느 것을 사용해야 하는가?
   - 목표: 성능과 정확성 사이의 균형점 찾기

2. **std::shared_mutex 성능 특성**
   - 연구 질문: 읽기/쓰기 비율에 따른 성능 변화는?
   - 목표: 쓰기 기아(write starvation) 방지 전략 수립

3. **oneTBB concurrent_hash_map 내부 락킹 메커니즘**
   - 연구 질문: concurrent_hash_map의 accessor는 어떻게 동작하는가?
   - 목표: 불필요한 이중 락킹 방지

4. **shared_ptr vs weak_ptr 성능 비교**
   - 연구 질문: Notifier 관리에 어느 것이 적합한가?
   - 목표: 메모리 안전성과 성능 최적화

5. **벤치마크 프레임워크 선정**
   - 연구 질문: Google Benchmark vs 직접 구현?
   - 목표: 정확한 성능 측정 도구 선택
### Phase 0 완료 상태

✅ **모든 조사 항목 완료** - 상세 내용은 [research.md](research.md) 참조

**주요 결정 사항**:
- std::atomic with `memory_order_relaxed`
- std::shared_mutex for read-heavy workloads
- std::shared_ptr for Notifier lifecycle
- Google Test + std::chrono for benchmarking

---

## Phase 1: 설계 및 계약서 (Design & Contracts)

이 단계에서는 데이터 모델과 개발자 가이드를 작성합니다.

### Phase 1 완료 상태

✅ **data-model.md 생성 완료**
- PerformanceMetrics 구조체 정의
- Notifier 생명주기 관리 전략
- SharedMetadata 읽기/쓰기 패턴
- 메모리 레이아웃 및 스레드 안전성

✅ **quickstart.md 생성 완료**
- 단계별 구현 가이드 (Lock-free 메트릭 → Notifier → shared_mutex)
- 테스트 전략 (단위/통합/성능/메모리)
- 디버깅 팁 및 체크리스트

❌ **contracts/ 디렉토리** - 해당 없음 (내부 구현만 변경, API 계약서 불필요)

### Constitution 준수 확인 (재검증)

Phase 1 설계 완료 후 재확인:

- ✅ **실시간성**: atomic/shared_mutex 모두 예측 가능한 시간 복잡도
- ✅ **신뢰성**: RAII 패턴, 메모리 안전성 검증 계획 수립
- ✅ **테스트**: 단위/통합/성능 테스트 전략 문서화
- ✅ **모듈성**: 기존 API 유지, 내부 구현만 변경
- ✅ **문서화**: research.md, data-model.md, quickstart.md 완료
- ✅ **버전 관리**: PATCH 버전 증가 (하위 호환성 유지)

**결론**: 모든 Constitution 원칙을 준수하며, Phase 2 (작업 분해)로 진행 가능합니다.

---

## Phase 2: 작업 분해 (Task Breakdown)

**다음 단계**: `/tasks` 명령어를 실행하여 구현 작업을 세분화된 태스크로 분해합니다.

tasks.md에는 다음 내용이 포함됩니다:
- 각 사용자 스토리별 구현 작업 목록
- 의존성 관계 (어떤 작업이 먼저 완료되어야 하는지)
- 예상 소요 시간 및 우선순위
- 테스트 계획

---

## 산출물 요약

### 완료된 문서

| 파일 | 목적 | 상태 |
|------|------|------|
| [spec.md](spec.md) | 기능 사양서 | ✅ 완료 |
| [plan.md](plan.md) | 구현 계획 (이 파일) | ✅ 완료 |
| [research.md](research.md) | 기술 조사 결과 | ✅ 완료 |
| [data-model.md](data-model.md) | 데이터 모델 상세 | ✅ 완료 |
| [quickstart.md](quickstart.md) | 개발자 가이드 | ✅ 완료 |
| [checklists/requirements.md](checklists/requirements.md) | 사양서 품질 검증 | ✅ 완료 |
| tasks.md | 작업 분해 | ⏳ 대기 (/tasks 명령어) |

### 다음 단계

```bash
# 브랜치 확인
git branch
# 017-datastore-performance

# 작업 분해 실행
/tasks

# 생성된 tasks.md를 검토 후 구현 시작
```

---

## 부록: 참고 자료

- **Issue #005**: [issue/005-datastore-lock-free-metrics.md](../../issue/005-datastore-lock-free-metrics.md)
- **CLAUDE.md**: 프로젝트 코드 작성 가이드
- **C++17 Standard**: [cppreference.com](https://en.cppreference.com/)
- **oneTBB Documentation**: [oneapi-src.github.io/oneTBB](https://oneapi-src.github.io/oneTBB/)

