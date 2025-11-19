# 작업 목록: 비동기 로깅 시스템 및 안정성 강화

**브랜치**: `018-async-logging` | **날짜**: 2025-11-19
**사양서**: [spec.md](spec.md) | **계획**: [plan.md](plan.md)

## 개요

이 문서는 비동기 로깅 시스템 구현을 위한 작업 목록을 사용자 스토리별로 정의합니다.

**사용자 스토리 우선순위**:
1. **User Story 1 (P1)**: 실시간 제어 루프에서 블로킹 없는 로깅
2. **User Story 2 (P1)**: 크래시 직전 로그 보존
3. **User Story 3 (P2)**: 중앙화된 로깅 설정 관리

**MVP 범위**: User Story 1 + User Story 2 (P1만 구현)

---

## Phase 1: Setup (프로젝트 초기화)

### 목표
프로젝트 디렉토리 구조 생성 및 빌드 시스템 설정

### 작업 목록

- [ ] T001 src/core/logging/ 디렉토리 생성
- [ ] T002 tests/unit/logging/ 디렉토리 생성
- [ ] T003 tests/integration/logging/ 디렉토리 생성
- [ ] T004 logs/ 디렉토리 생성 및 .gitignore 추가
- [ ] T005 src/core/logging/CMakeLists.txt 생성 (mxrc_logging 라이브러리 정의)
- [ ] T006 src/CMakeLists.txt 수정 (add_subdirectory(core/logging), target_link_libraries(mxrc PRIVATE mxrc_logging))

---

## Phase 2: Foundational (기반 구현)

### 목표
모든 사용자 스토리가 의존하는 핵심 로거 초기화 로직 구현

### 작업 목록

- [ ] T007 src/core/logging/Log.h 기본 구조 작성 (네임스페이스, 함수 선언)
- [ ] T008 [P] [US1+US2+US3] initialize_async_logger() 함수 구현 (콘솔/파일 sink 생성)
- [ ] T009 [US1+US2+US3] spdlog thread pool 초기화 코드 추가 (큐 크기 8192, 스레드 1개)
- [ ] T010 [US1+US2+US3] async_logger 생성 및 기본 로거 설정
- [ ] T011 [US1+US2+US3] 로그 패턴 설정 ("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v")
- [ ] T012 [US1+US2+US3] 에러 핸들러 설정 (stderr 출력, 애플리케이션 계속)
- [ ] T013 [US1+US2+US3] shutdown_logger() 함수 구현 (정리 로직)

**독립 테스트 기준**:
- initialize_async_logger() 호출 시 예외 없이 완료
- spdlog::default_logger()가 비동기 로거로 설정됨
- spdlog::info() 호출 시 로그 파일에 기록됨

---

## Phase 3: User Story 1 - 실시간 제어 루프에서 블로킹 없는 로깅 (P1)

### 목표
비동기 로깅을 통해 로그 호출이 10μs 이내에 반환되고, 1000Hz 제어 루프에 1% 미만의 오버헤드만 발생

### 작업 목록

#### 구현
- [ ] T014 [US1] Log.h에 비동기 큐 설정 확인 (overflow_policy::block)
- [ ] T015 [P] [US1] tests/unit/logging/AsyncLogger_test.cpp 생성 (기본 테스트 프레임워크)
- [ ] T016 [US1] AsyncLogger_test.cpp에 초기화 테스트 추가
- [ ] T017 [US1] AsyncLogger_test.cpp에 기본 로깅 테스트 추가 (info, debug, warn, error)

#### 성능 테스트
- [ ] T018 [P] [US1] tests/unit/logging/LogPerformance_test.cpp 생성
- [ ] T019 [US1] LogPerformance_test에 10μs 지연 테스트 추가 (10,000회 로그 평균)
- [ ] T020 [US1] LogPerformance_test에 1000Hz 제어 루프 오버헤드 테스트 추가 (1% 미만)
- [ ] T021 [US1] LogPerformance_test에 초당 10,000 로그 처리량 테스트 추가 (95% 유지)

#### 통합
- [ ] T022 [US1] src/main.cpp에 initialize_async_logger() 호출 추가 (main() 시작 직후)
- [ ] T023 [US1] src/main.cpp에 shutdown_logger() 호출 추가 (종료 전)

**독립 테스트 기준**:
- ✅ 로그 호출 평균 지연 < 10μs
- ✅ 1000Hz 루프 주기가 1.01ms 이하 (오버헤드 1% 이하)
- ✅ 초당 10,000 로그 시 처리량 95% 이상 유지
- ✅ 모든 단위 테스트 통과 (AsyncLogger_test, LogPerformance_test)

---

## Phase 4: User Story 2 - 크래시 직전 로그 보존 (P1)

### 목표
치명적 시그널 발생 시 크래시 3초 이내의 로그를 99% 이상 파일에 보존

### 작업 목록

#### 기반 구현 (flush 정책)
- [ ] T024 [US2] Log.h의 initialize_async_logger()에 flush_on(critical) 정책 추가
- [ ] T025 [P] [US2] Log.h에 주기적 flush 스레드 구현 (3초 간격, g_flush_thread_running, g_flush_thread)
- [ ] T026 [US2] shutdown_logger()에 flush 스레드 정지 로직 추가

#### 시그널 핸들러 구현
- [ ] T027 [P] [US2] src/core/logging/SignalHandler.h 파일 생성
- [ ] T028 [US2] SignalHandler.h에 signal_handler() 함수 구현 (SIGSEGV, SIGABRT 처리)
- [ ] T029 [US2] signal_handler()에 spdlog::critical() 호출 추가
- [ ] T030 [US2] signal_handler()에 spdlog::shutdown() 호출 추가 (큐 플러시)
- [ ] T031 [US2] signal_handler()에 기본 핸들러 복원 로직 추가 (SIG_DFL, std::raise)
- [ ] T032 [US2] SignalHandler.h에 register_signal_handlers() 함수 구현

#### backward-cpp 통합 (선택적)
- [ ] T033 [P] [US2] src/core/logging/backward.hpp 다운로드 (wget)
- [ ] T034 [US2] CMakeLists.txt에 backward INTERFACE 라이브러리 추가
- [ ] T035 [US2] CMakeLists.txt에 USE_BACKWARD 옵션 추가
- [ ] T036 [US2] signal_handler()에 백트레이스 생성 로직 추가 (#ifdef USE_BACKWARD_CPP)

#### 테스트
- [ ] T037 [P] [US2] tests/unit/logging/SignalHandler_test.cpp 생성
- [ ] T038 [US2] SignalHandler_test에 SIGSEGV 로그 보존 테스트 추가 (fork + 로그 확인)
- [ ] T039 [P] [US2] tests/unit/logging/AsyncLogger_test.cpp에 CRITICAL flush 테스트 추가
- [ ] T040 [US2] AsyncLogger_test에 CRITICAL 로그 100ms 이내 파일 기록 확인 추가

#### 통합 테스트
- [ ] T041 [P] [US2] tests/integration/logging/CrashSafety_test.cpp 생성
- [ ] T042 [US2] CrashSafety_test에 크래시 3초 이내 로그 보존율 99% 테스트 추가

#### 통합
- [ ] T043 [US2] src/main.cpp에 register_signal_handlers() 호출 추가 (선택적)

**독립 테스트 기준**:
- ✅ CRITICAL 로그 100ms 이내 파일 기록
- ✅ SIGSEGV 발생 시 크래시 직전 로그 파일에 저장
- ✅ 크래시 3초 이내 로그 99% 이상 보존
- ✅ 백트레이스 정보 로그에 포함 (backward-cpp 사용 시)
- ✅ 모든 테스트 통과 (SignalHandler_test, CrashSafety_test)

---

## Phase 5: User Story 3 - 중앙화된 로깅 설정 관리 (P2)

### 목표
프로젝트 전체에서 일관된 로깅 동작 유지 및 단일 설정 변경으로 전체 시스템 적용

### 작업 목록

#### 코드베이스 정리
- [ ] T044 [P] [US3] 프로젝트 전체에서 개별 로거 생성 코드 검색 (grep -r "spdlog::register_logger\|make_shared<spdlog::logger>")
- [ ] T045 [US3] 발견된 개별 로거 생성 코드를 spdlog::info() 등 기본 로거 사용으로 교체

#### 설정 파일 지원 (선택적)
- [ ] T046 [P] [US3] config/logging.conf 파일 생성 (로그 레벨, 출력 대상 설정)
- [ ] T047 [US3] Log.h의 initialize_async_logger()에 설정 파일 읽기 로직 추가 (선택적)

#### 검증
- [ ] T048 [US3] 프로젝트 전체에서 spdlog 로거 생성 코드 0건 확인 (grep 재검색)

**독립 테스트 기준**:
- ✅ 프로젝트 전체에서 개별 로거 생성 코드 0건
- ✅ 모든 모듈이 spdlog::default_logger() 사용
- ✅ 설정 파일 수정 시 전체 시스템에 즉시 적용 (재시작 후)

---

## Phase 6: Polish & 메모리 검증

### 목표
메모리 누수 방지 및 코드 품질 검증

### 작업 목록

#### 메모리 검증
- [ ] T049 [P] Valgrind로 메모리 누수 검사 (valgrind --leak-check=full ./run_tests --gtest_filter=AsyncLogger*)
- [ ] T050 [P] AddressSanitizer로 메모리 오류 검사 (cmake -DCMAKE_CXX_FLAGS="-fsanitize=address")
- [ ] T051 메모리 이슈 수정 (발견 시)

#### 코드 리뷰
- [ ] T052 [P] CLAUDE.md 코딩 규칙 준수 확인 (주석 한글, 기술 용어 영어)
- [ ] T053 RAII 패턴 적용 확인 (모든 리소스 자동 정리)
- [ ] T054 [P] 기존 테스트 실행 (./run_tests, 100% 통과 확인)

#### 문서화
- [ ] T055 [P] README.md에 비동기 로깅 사용 방법 추가
- [ ] T056 API 주석 추가 (Log.h, SignalHandler.h)

---

## 의존성 그래프

### User Story 완료 순서

```
Phase 1 (Setup)
    ↓
Phase 2 (Foundational)
    ↓
    ├─→ Phase 3 (US1) - 블로킹 없는 로깅 [독립 테스트 가능]
    ├─→ Phase 4 (US2) - 크래시 로그 보존 [독립 테스트 가능, US1 병렬 가능]
    └─→ Phase 5 (US3) - 중앙화된 설정 [독립 테스트 가능, US1+US2 완료 후 권장]
    ↓
Phase 6 (Polish) - 메모리 검증 및 코드 리뷰
```

### 병렬 실행 기회

**Phase 2 (Foundational) 후 병렬 진행 가능**:
- US1과 US2는 **완전 독립적** - 동시 진행 가능
  - US1: 성능 테스트 (LogPerformance_test.cpp)
  - US2: 시그널 핸들러 (SignalHandler.h, SignalHandler_test.cpp)
- US3는 US1+US2 완료 후 진행 권장 (코드베이스 정리 작업)

**Phase 6 (Polish) 병렬 작업**:
- T049, T050 (메모리 검증) 병렬 실행 가능
- T052, T054 (코드 리뷰, 테스트) 병렬 실행 가능
- T055, T056 (문서화) 병렬 실행 가능

---

## 구현 전략

### MVP (Minimum Viable Product)

**Phase 1 + Phase 2 + Phase 3 + Phase 4** = MVP
- User Story 1 (P1): 비동기 로깅으로 성능 개선
- User Story 2 (P1): 크래시 안전성 확보
- 핵심 가치: 실시간 성능 + 로그 보존

**점진적 배포**:
1. **Week 1**: Phase 1-2 (Setup + Foundational)
2. **Week 2**: Phase 3 (US1) - 비동기 로깅 + 성능 테스트
3. **Week 3**: Phase 4 (US2) - 시그널 핸들러 + 크래시 안전성
4. **Week 4**: Phase 5-6 (US3 + Polish) - 코드 정리 + 메모리 검증

### 테스트 우선순위

1. **필수 (MVP)**:
   - AsyncLogger_test.cpp (US1)
   - LogPerformance_test.cpp (US1)
   - SignalHandler_test.cpp (US2)
   - CrashSafety_test.cpp (US2)

2. **선택적 (US3)**:
   - 코드베이스 검색 스크립트

3. **최종 (Polish)**:
   - Valgrind, AddressSanitizer

---

## 작업 요약

| Phase | 작업 수 | 병렬 가능 | 핵심 산출물 |
|-------|---------|----------|------------|
| Phase 1: Setup | 6 | 6 | 디렉토리 구조, CMakeLists.txt |
| Phase 2: Foundational | 7 | 1 | Log.h (initialize_async_logger) |
| Phase 3: US1 | 10 | 3 | AsyncLogger_test, LogPerformance_test |
| Phase 4: US2 | 20 | 7 | SignalHandler.h, CrashSafety_test |
| Phase 5: US3 | 5 | 2 | 코드베이스 정리 |
| Phase 6: Polish | 8 | 5 | 메모리 검증, 문서화 |
| **총계** | **56** | **24** | 완전한 비동기 로깅 시스템 |

**MVP 범위**: T001~T043 (43개 작업)
**병렬 실행 가능**: 24개 작업 (43% 병렬화 가능)

---

## 체크리스트

### 구현 전
- [ ] 모든 의존성 확인 (spdlog, Google Test, backward-cpp)
- [ ] CLAUDE.md 코딩 규칙 숙지
- [ ] research.md, data-model.md, quickstart.md 검토

### 구현 중
- [ ] 각 Phase 완료 시 독립 테스트 수행
- [ ] RAII 패턴 준수
- [ ] 주석 한글 작성 (기술 용어 영어 허용)

### 구현 후
- [ ] 모든 테스트 통과 (56개 작업)
- [ ] Valgrind 메모리 누수 없음
- [ ] 기존 테스트 100% 통과
- [ ] 성능 목표 달성 (<10μs 지연, <1% 오버헤드, 99% 로그 보존)

---

## 참고 자료

- [spec.md](spec.md) - 기능 사양서
- [plan.md](plan.md) - 구현 계획
- [research.md](research.md) - 기술 조사
- [data-model.md](data-model.md) - 데이터 모델
- [quickstart.md](quickstart.md) - 개발자 가이드
- [contracts/logging-api.md](contracts/logging-api.md) - API 계약
