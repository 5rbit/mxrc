# 작업 목록: DataStore 성능 및 안정성 개선

**브랜치**: `017-datastore-performance` | **날짜**: 2025-11-19 | **사양서**: [spec.md](spec.md)

이 문서는 기능 구현을 위한 세부 작업 목록을 정의합니다.

## 구현 전략

**MVP 범위**: User Story 1 (Lock-free 메트릭 수집)만 구현하면 즉시 배포 가능합니다.
- 가장 큰 성능 병목 해결
- 독립적으로 테스트 및 검증 가능
- 다른 스토리와 의존성 없음

**점진적 배포**: 각 사용자 스토리가 독립적으로 배포 가능하도록 설계되었습니다.

---

## Phase 1: Setup (프로젝트 초기화)

이 단계에서는 개발 환경을 설정하고 기존 코드를 분석합니다.

### 작업 목록

- [ ] T001 기존 DataStore 코드 분석 및 issue #005 검토 (src/core/datastore/DataStore.h, src/core/datastore/DataStore.cpp)
- [ ] T002 개발 환경 검증 (C++17, oneTBB, Google Test, Valgrind, AddressSanitizer)
- [ ] T003 [P] research.md, data-model.md, quickstart.md 숙지
- [ ] T004 기존 DataStore 테스트 실행 및 baseline 성능 측정 (tests/unit/datastore/)

**완료 기준**:
- 기존 코드 이해 완료
- 개발 환경 ready
- Baseline 성능 데이터 수집 (현재 처리량, 지연 시간)

---

## Phase 2: Foundational (기반 인프라)

이 단계에서는 모든 사용자 스토리에 공통으로 필요한 기반을 구축합니다.

### 작업 목록

- [ ] T005 PerformanceMetrics 구조체 정의 (src/core/datastore/DataStore.h)
- [ ] T006 DataStore.h에 PerformanceMetrics 멤버 변수 추가 (private 섹션)
- [ ] T007 [P] 기존 performance_mutex_ 및 performance_metrics_ 제거 준비 (주석 처리)

**완료 기준**:
- 새로운 데이터 구조 정의 완료
- 컴파일 성공

---

## Phase 3: User Story 1 - Lock-Free 메트릭 수집 (P1)

**스토리 목표**: 전역 뮤텍스를 제거하여 메트릭 수집 시 락 경합 없이 성능 향상

**독립 테스트 기준**:
- 100개 동시 스레드가 1000회 get/set 작업 수행
- 평균 처리 시간 기존 대비 80% 이상 단축
- 전체 처리량이 스레드 수에 선형 비례 (R² > 0.95)
- 메트릭 조회가 즉시 반환 (다른 스레드 차단 없음)

### 작업 목록

#### 구현

- [ ] T008 [US1] get() 메서드에서 성능 뮤텍스 제거 및 atomic 카운터 사용 (src/core/datastore/DataStore.cpp)
- [ ] T009 [P] [US1] set() 메서드에서 성능 뮤텍스 제거 및 atomic 카운터 사용 (src/core/datastore/DataStore.cpp)
- [ ] T010 [P] [US1] remove() 메서드에서 성능 뮤텍스 제거 및 atomic 카운터 사용 (src/core/datastore/DataStore.cpp)
- [ ] T011 [P] [US1] has() 메서드에서 성능 뮤텍스 제거 및 atomic 카운터 사용 (src/core/datastore/DataStore.cpp)
- [ ] T012 [US1] getPerformanceMetrics() 메서드 수정 (atomic load 사용) (src/core/datastore/DataStore.cpp)

#### 테스트

- [ ] T013 [US1] 단위 테스트: atomic 연산 정확성 검증 (tests/unit/datastore/DataStorePerformance_test.cpp - 새 파일)
- [ ] T014 [US1] 단위 테스트: 메트릭 오버플로우 처리 검증 (tests/unit/datastore/DataStorePerformance_test.cpp)
- [ ] T015 [US1] 성능 벤치마크: 1/10/50/100 스레드 처리량 측정 (tests/unit/datastore/DataStorePerformance_test.cpp)
- [ ] T016 [US1] 성능 벤치마크: 선형성 계수 R² 계산 및 검증 (tests/unit/datastore/DataStorePerformance_test.cpp)
- [ ] T017 [US1] 리그레션 테스트: 기존 DataStore 테스트 100% 통과 확인 (tests/unit/datastore/DataStore_test.cpp)

#### 검증

- [ ] T018 [US1] 메트릭 수집 오버헤드 <1% 확인 (성능 프로파일링)
- [ ] T019 [US1] Valgrind 메모리 검사 (메모리 누수 없음)
- [ ] T020 [US1] AddressSanitizer 빌드 및 테스트 실행 (메모리 오류 없음)

**Phase 3 완료 기준**:
- ✅ SC-001 달성: 100 스레드 환경에서 80% 성능 개선
- ✅ SC-002 달성: 선형 확장성 R² > 0.95
- ✅ SC-005 달성: 기존 테스트 100% 통과

---

## Phase 4: User Story 2 - 안전한 Notifier 생명주기 관리 (P1)

**스토리 목표**: Dangling pointer 문제를 해결하여 크래시 없이 안전한 알림 처리

**독립 테스트 기준**:
- 여러 스레드가 동시에 Notifier 등록/해제
- 알림 처리 중 unsubscribe 시도해도 크래시 없음
- 10,000회 구독/해제/알림 사이클 수행해도 메모리 누수 없음
- 콜백 예외가 다른 구독자에게 영향 없음

### 작업 목록

#### 구현

- [ ] T021 [US2] DataStore.h에서 notifiers_ 타입 변경 (unique_ptr → shared_ptr) (src/core/datastore/DataStore.h)
- [ ] T022 [US2] subscribe() 메서드 수정 (shared_ptr 인자 받기) (src/core/datastore/DataStore.cpp)
- [ ] T023 [US2] unsubscribe() 메서드 검증 (기존 로직 유지) (src/core/datastore/DataStore.cpp)
- [ ] T024 [US2] notifySubscribers() 메서드 수정 (shared_ptr 복사로 생명주기 보장) (src/core/datastore/DataStore.cpp)
- [ ] T025 [US2] notify 콜백 try-catch 감싸기 (예외 격리) (src/core/datastore/DataStore.cpp)

#### 테스트

- [ ] T026 [US2] 단위 테스트: shared_ptr 참조 카운트 검증 (tests/integration/datastore/DataStoreConcurrency_test.cpp - 새 파일)
- [ ] T027 [US2] 통합 테스트: 동시 구독/해제 스트레스 테스트 (tests/integration/datastore/DataStoreConcurrency_test.cpp)
- [ ] T028 [US2] 통합 테스트: 알림 처리 중 unsubscribe 안전성 검증 (tests/integration/datastore/DataStoreConcurrency_test.cpp)
- [ ] T029 [US2] 통합 테스트: 콜백 예외 처리 검증 (tests/integration/datastore/DataStoreConcurrency_test.cpp)
- [ ] T030 [US2] 장기 실행 테스트: 10,000회 사이클 무중단 실행 (tests/integration/datastore/DataStoreConcurrency_test.cpp)

#### 검증

- [ ] T031 [US2] Valgrind 메모리 누수 검사 (1시간 스트레스 테스트)
- [ ] T032 [US2] AddressSanitizer 동시성 오류 검증
- [ ] T033 [US2] Notifier 알림 전달 시간 O(N) 검증 (구독자 수에 선형 비례)

**Phase 4 완료 기준**:
- ✅ SC-003 달성: 10,000회 사이클 메모리 누수/크래시 없음
- ✅ FR-002, FR-005 달성: Dangling pointer 방지, 예외 격리

---

## Phase 5: User Story 3 - 최적화된 읽기 접근 (P2)

**스토리 목표**: shared_mutex를 사용하여 읽기 병렬성 지원

**독립 테스트 기준**:
- 100개 읽기 스레드가 동시에 메타데이터 조회
- 읽기 스레드들이 서로 차단되지 않음
- 읽기 대기 시간 <1μs
- 쓰기 요청이 무한정 대기하지 않음 (기아 방지)

### 작업 목록

#### 구현

- [ ] T034 [US3] DataStore.h에서 metadata_mutex_ 타입 변경 (mutex → shared_mutex) (src/core/datastore/DataStore.h)
- [ ] T035 [P] [US3] 읽기 전용 메서드에 shared_lock 적용 (hasAccess 등) (src/core/datastore/DataStore.cpp)
- [ ] T036 [P] [US3] 쓰기 메서드에 unique_lock 적용 (updatePolicy 등) (src/core/datastore/DataStore.cpp)

#### 테스트

- [ ] T037 [US3] 성능 테스트: 읽기 병렬성 검증 (100 읽기 스레드 동시 진행) (tests/unit/datastore/DataStorePerformance_test.cpp)
- [ ] T038 [US3] 성능 테스트: 읽기 대기 시간 <1μs 검증 (tests/unit/datastore/DataStorePerformance_test.cpp)
- [ ] T039 [US3] 통합 테스트: 읽기/쓰기 혼합 워크로드 (100:1 비율) (tests/integration/datastore/DataStoreConcurrency_test.cpp)
- [ ] T040 [US3] 통합 테스트: 쓰기 기아 방지 검증 (tests/integration/datastore/DataStoreConcurrency_test.cpp)

#### 검증

- [ ] T041 [US3] shared_mutex 획득 시간 평균 1μs 이하 확인 (벤치마크)
- [ ] T042 [US3] 읽기 처리량 향상 측정 및 비교 (개선 전후)

**Phase 5 완료 기준**:
- ✅ SC-004 달성: 100 읽기 스레드 병렬 진행, 읽기 대기 시간 <1μs
- ✅ FR-003 달성: 읽기 병렬성 지원

---

## Phase 6: Polish & Cross-Cutting Concerns (마무리)

이 단계에서는 코드 품질, 문서화, 최종 검증을 수행합니다.

### 작업 목록

- [ ] T043 [P] 코드 리뷰 (CLAUDE.md 규칙 준수 확인)
- [ ] T044 [P] 한글 주석 검증 (기술 용어 제외 모두 한글)
- [ ] T045 성능 프로파일링 종합 보고서 작성
- [ ] T046 Valgrind 72시간 장기 실행 테스트 (메모리 누수 <10MB)
- [ ] T047 모든 테스트 실행 및 통과 확인 (단위 + 통합 + 성능)
- [ ] T048 버전 번호 PATCH 증가 (예: 1.2.0 → 1.2.1)
- [ ] T049 [P] CHANGELOG.md 업데이트 (변경 사항 기록)
- [ ] T050 최종 커밋 및 main 브랜치 PR 준비

**Phase 6 완료 기준**:
- ✅ 모든 Constitution 원칙 준수
- ✅ 모든 성공 기준 (SC-001 ~ SC-005) 달성
- ✅ 모든 기능적 요구사항 (FR-001 ~ FR-007) 만족

---

## 의존성 그래프

### 사용자 스토리 완료 순서

```
Setup (Phase 1) → Foundational (Phase 2) → 병렬 실행 가능
                                                ├─→ US1 (Phase 3)
                                                ├─→ US2 (Phase 4)
                                                └─→ US3 (Phase 5)
```

**중요**: User Story 1, 2, 3은 서로 **독립적**이므로 병렬로 구현 가능합니다.
- US1: 메트릭 수집 (get/set/remove/has 메서드)
- US2: Notifier 관리 (subscribe/unsubscribe/notify 메서드)
- US3: 메타데이터 접근 (hasAccess/updatePolicy 메서드)

### MVP 배포 전략

**MVP (Minimum Viable Product)**: Phase 1 + Phase 2 + **Phase 3만** 구현하면 배포 가능
- 가장 큰 성능 병목 (전역 뮤텍스) 해결
- 80% 성능 개선 즉시 달성
- 다른 스토리와 독립적

**점진적 배포**:
1. **1차 배포**: US1 완료 후 배포 (성능 개선)
2. **2차 배포**: US2 완료 후 배포 (안정성 개선)
3. **3차 배포**: US3 완료 후 배포 (읽기 성능 추가 개선)

---

## 병렬 실행 기회

### Phase 3 (US1) 병렬 작업

다음 작업들은 서로 다른 파일이거나 독립적인 메서드이므로 병렬 실행 가능:

```
T008 (get 메서드) ║ T009 (set 메서드) ║ T010 (remove 메서드) ║ T011 (has 메서드)
```

테스트 작성도 병렬 가능:
```
T013 (atomic 테스트) ║ T014 (오버플로우 테스트) ║ T015 (벤치마크 1) ║ T016 (벤치마크 2)
```

### Phase 4 (US2) 병렬 작업

```
T022 (subscribe) ║ T023 (unsubscribe) ║ T024 (notifySubscribers) ║ T025 (예외 처리)
```

### Phase 5 (US3) 병렬 작업

```
T035 (읽기 메서드) ║ T036 (쓰기 메서드)
```

### Phase 6 (Polish) 병렬 작업

```
T043 (코드 리뷰) ║ T044 (주석 검증) ║ T049 (CHANGELOG)
```

---

## 작업 요약

| Phase | 사용자 스토리 | 작업 수 | 예상 소요 |
|-------|-------------|---------|---------|
| Phase 1 | Setup | 4 | 4시간 |
| Phase 2 | Foundational | 3 | 2시간 |
| Phase 3 | US1 (Lock-Free 메트릭) | 13 | 16시간 |
| Phase 4 | US2 (Notifier 안전성) | 13 | 12시간 |
| Phase 5 | US3 (읽기 병렬성) | 9 | 8시간 |
| Phase 6 | Polish | 8 | 6시간 |
| **합계** | **3개 스토리** | **50** | **48시간** |

**MVP 범위** (Phase 1-3만): 23 tasks, 약 22시간

---

## 성공 메트릭 추적

각 Phase 완료 시 다음 메트릭을 측정하고 기록:

### User Story 1 (Phase 3)
- [ ] 100 스레드 환경 평균 처리 시간 (목표: 기존 대비 80% 단축)
- [ ] 선형성 계수 R² (목표: > 0.95)
- [ ] 메트릭 수집 오버헤드 (목표: < 1%)

### User Story 2 (Phase 4)
- [ ] 10,000회 사이클 메모리 누수 (목표: 0 bytes)
- [ ] Notifier 알림 시간 (목표: O(N) 선형)

### User Story 3 (Phase 5)
- [ ] 100 읽기 스레드 대기 시간 (목표: < 1μs)
- [ ] shared_mutex 획득 시간 (목표: 평균 < 1μs)

---

## 참고 자료

- [spec.md](spec.md) - 기능 사양서
- [plan.md](plan.md) - 구현 계획
- [research.md](research.md) - 기술 조사 결과
- [data-model.md](data-model.md) - 데이터 모델 상세
- [quickstart.md](quickstart.md) - 개발자 가이드
- [issue/005](../../issue/005-datastore-lock-free-metrics.md) - 원인 분석

---

## 구현 체크리스트

작업 시작 전:
- [ ] 브랜치 확인: `git branch` (017-datastore-performance)
- [ ] 최신 코드 pull: `git pull origin 017-datastore-performance`
- [ ] 빌드 성공 확인: `cd build && cmake .. && make`
- [ ] 기존 테스트 통과 확인: `./run_tests`

각 Phase 완료 후:
- [ ] 모든 Phase 테스트 통과
- [ ] 코드 리뷰 (CLAUDE.md 규칙)
- [ ] 커밋 및 푸시
- [ ] 진행 상황 업데이트 (이 파일의 체크박스)

전체 완료 후:
- [ ] 모든 성공 기준 달성 확인
- [ ] PR 생성 및 리뷰 요청
- [ ] main 브랜치 머지
