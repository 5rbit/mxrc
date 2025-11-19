# 구현 태스크: 강력한 로깅 시스템 (Bag Logging & Replay Infrastructure)

**기능**: Bag Logging & Replay Infrastructure
**브랜치**: `017-logging`
**날짜**: 2025-11-19
**관련 문서**: [spec.md](spec.md), [plan.md](plan.md), [data-model.md](data-model.md)

---

## 태스크 개요

이 문서는 MXRC 로깅 시스템 구현을 위한 모든 태스크를 정의합니다. 태스크는 사용자 스토리별로 구성되어 있으며, 각 스토리는 독립적으로 구현 및 테스트 가능합니다.

**핵심 원칙**:
- ✅ 사용자 스토리별 독립 구현 (US1 → US2 → US3)
- ✅ 각 스토리는 독립적으로 가치를 제공
- ✅ 병렬 작업 기회 최대화 ([P] 마커)
- ✅ TDD 접근 (명시적으로 테스트 작성)

**태스크 레이블**:
- `[P]`: 병렬 실행 가능 (다른 파일, 의존성 없음)
- `[US1]`, `[US2]`, `[US3]`: 사용자 스토리 매핑

---

## Phase 1: 프로젝트 Setup

**목표**: 프로젝트 기본 구조 생성 및 의존성 설정

- [X] T001 src/core/logging/ 디렉토리 생성 및 기본 구조 설정 (interfaces/, core/, dto/, util/)
- [X] T002 CMakeLists.txt에 mxrc_logging 라이브러리 타겟 추가 (nlohmann_json, spdlog, mxrc_event, mxrc_datastore 링크)
- [X] T003 tests/unit/logging/ 디렉토리 생성 및 Google Test 설정
- [X] T004 src/core/logging/dto/DataType.h 열거형 정의 (RobotMode, InterfaceData, Config, Para, Alarm, Event, MissionState, TaskState)
- [X] T005 [P] .gitignore에 logs/ 디렉토리 추가 (Bag 파일 제외)

**완료 기준**:
- ✅ `make -j$(nproc)` 빌드 성공
- ✅ DataType 열거형 8개 값 정의 완료

---

## Phase 2: Foundational Infrastructure (모든 User Story의 전제 조건)

**목표**: 모든 사용자 스토리가 공통으로 사용할 핵심 인프라 구축

### 2.1 BagMessage 및 직렬화 (FR-009, FR-010, FR-011)

- [X] T006 src/core/logging/dto/BagMessage.h 구조체 정의 (timestamp_ns, topic, data_type, serialized_value)
- [X] T007 tests/unit/logging/BagMessage_test.cpp 작성 - toJson/fromJson 테스트 (7 tests)
- [X] T008 src/core/logging/util/Serializer.{h,cpp} 구현 - DataType별 switch문 기반 std::any → JSON 변환
- [X] T009 tests/unit/logging/Serializer_test.cpp 작성 - 8개 DataType 직렬화/역직렬화 테스트 (15 tests)

**완료 기준**:
- ✅ BagMessage 검증 로직 통과 (isValid())
- ✅ 8개 DataType 모두 JSON 왕복 변환 성공
- ✅ 22개 단위 테스트 통과 (BagMessage: 7, Serializer: 15)

### 2.2 파일 I/O 유틸리티 (FR-003, FR-023)

- [X] T010 src/core/logging/util/FileUtils.{h,cpp} 구현 - 파일 존재 확인, 디렉토리 생성, 디스크 공간 체크
- [X] T011 tests/unit/logging/FileUtils_test.cpp 작성 - 디스크 공간 부족 시뮬레이션 (13 tests)

**완료 기준**:
- ✅ FileUtils::getAvailableSpace() 정확한 값 반환
- ✅ 디스크 공간 부족 시 예외 발생
- ✅ 파일 목록 조회 및 정렬 동작
- ✅ 타임스탬프 파일명 생성

---

## Phase 3: User Story 1 - 실시간 시스템 상태 영구 기록 (P1)

**목표**: EventBus 기반 비동기 Bag 로깅 완성

**독립 테스트 기준**: DataStore 변경 시 Bag 파일에 타임스탬프와 함께 기록, 파일 회전 및 보관 정책 동작, 성능 저하 <1%

### 3.1 AsyncWriter (FR-007, FR-023, FR-025)

- [X] T012 [P] [US1] src/core/logging/core/AsyncWriter.{h,cpp} 구현 - std::thread + std::queue 비동기 I/O
- [X] T013 [US1] tests/unit/logging/AsyncWriter_test.cpp 작성 - 비동기 쓰기, 큐 오버플로우 드롭 정책 (8 tests)

### 3.2 RotationPolicy & RetentionPolicy (FR-004, FR-005, FR-006)

- [X] T014 [P] [US1] src/core/logging/dto/RotationPolicy.h 구조체 정의 (SIZE/TIME 기반)
- [X] T015 [P] [US1] src/core/logging/dto/RetentionPolicy.h 구조체 정의 (TIME/COUNT 기반)
- [X] T016 [P] [US1] src/core/logging/util/RetentionManager.{h,cpp} 구현 - 오래된 파일 삭제, emergencyDeleteOldest()
- [X] T017 [US1] tests/unit/logging/RetentionManager_test.cpp 작성 - 7일 보존, 디스크 공간 부족 (5 tests)

### 3.3 IBagWriter 인터페이스 및 구현 (FR-001 ~ FR-008)

- [X] T018 [P] [US1] src/core/logging/interfaces/IBagWriter.h 인터페이스 정의 (15개 메서드, contracts/ 기반)
- [X] T019 [US1] src/core/logging/core/SimpleBagWriter.{h,cpp} 구현 - JSONL 쓰기, 회전, flush, stats, 성능 최적화 (>10,000배 개선)
- [X] T020 [US1] tests/unit/logging/SimpleBagWriter_test.cpp 작성 - append, flush, rotation, stats, 고볼륨 테스트 (8 tests, 1000-2000 messages)

### 3.4 DataStoreBagLogger (EventBus 통합)

- [X] T021 [US1] src/core/logging/core/DataStoreBagLogger.{h,cpp} 구현 - EventBus 구독, IBagWriter 사용
- [X] T022 [US1] tests/unit/logging/DataStoreBagLogger_test.cpp 작성 - 이벤트 수신, Bag 기록 (8 tests, 전부 통과)

### 3.5 통합 테스트 (US1 완성)

- [X] T023 [US1] tests/integration/logging/bag_logging_integration_test.cpp 작성 - DataStore → EventBus → BagLogger → Bag 파일 (3 tests, 전부 통과)
- [ ] T024 [US1] 성능 벤치마크 실행 - DataStore 성능 저하 <1% 검증 (SC-001)

**완료 기준**:
- ✅ DataStore.set() 호출 시 Bag 파일에 자동 기록
- ✅ 1GB 파일 회전 동작
- ✅ 7일 보존 정책 동작
- ✅ 성능 저하 <1% (87ns → 88ns)
- ✅ 30개 단위 테스트 + 3개 통합 테스트 통과

**MVP 범위**: User Story 1만 구현하면 프로덕션 디버깅 가치 제공 가능

---

## Phase 4: User Story 2 - 복잡한 시나리오 재현 및 회귀 테스트 (P2)

**목표**: Bag 파일 읽기 및 Replay 기능 완성

**독립 테스트 기준**: Bag 파일을 읽어서 DataStore에 순서대로 재생, 타임스탬프 탐색 <10초, 재생 정확도 99%

### 4.1 인덱싱 (FR-013)

- [ ] T025 [P] [US2] src/core/logging/dto/IndexEntry.h 구조체 정의 (timestamp_ns, file_offset, 16 bytes packed)
- [ ] T026 [P] [US2] src/core/logging/dto/BagFooter.h 구조체 정의 (magic, data_size, index_offset, checksum, 64 bytes packed)
- [ ] T027 [US2] src/core/logging/util/Indexer.{h,cpp} 구현 - 인덱스 블록 생성/로드, 이진 탐색
- [ ] T028 [US2] tests/unit/logging/Indexer_test.cpp 작성 - 이진 탐색, CRC32 체크섬 (3 tests)

### 4.2 BagReader (FR-012, FR-013, FR-024)

- [ ] T029 [US2] src/core/logging/core/BagReader.{h,cpp} 구현 (contracts/BagReader.h 기반)
- [ ] T030 [US2] tests/unit/logging/BagReader_test.cpp 작성 - open, seekTime, next, recovery mode (6 tests)

### 4.3 BagReplayer (FR-014, FR-015, FR-016, FR-025)

- [ ] T031 [US2] src/core/logging/core/BagReplayer.{h,cpp} 구현 (contracts/BagReplayer.h 기반)
- [ ] T032 [US2] tests/unit/logging/BagReplayer_test.cpp 작성 - replay, setSpeedFactor, setTimeRange, onMismatch (6 tests)

### 4.4 통합 테스트 (US2 완성)

- [ ] T033 [US2] tests/integration/logging/replay_integration_test.cpp 작성 - Bag 기록 → 읽기 → Replay → 검증 (3 tests)
- [ ] T034 [US2] 성능 벤치마크 실행 - 1GB 파일 탐색 <10초 검증 (SC-004)
- [ ] T035 [US2] 회귀 테스트 자동화 예제 작성 - tests/integration/logging/regression_test_example.cpp

**완료 기준**:
- ✅ Bag 파일 → DataStore 재생 성공
- ✅ 1GB 파일 타임스탬프 탐색 <10초
- ✅ 재생 정확도 99% (SC-005)
- ✅ 18개 단위 테스트 + 3개 통합 테스트 통과

---

## Phase 5: User Story 3 - 선택적 로깅 전략 설정 (P2)

**목표**: DataType별 차별화된 로깅 전략 구현

**독립 테스트 기준**: LoggingStrategy 설정에 따라 Bag 기록/순환 버퍼/무시 동작, 디스크 사용량 90% 감소

### 5.1 CircularBuffer (FR-019, FR-022)

- [ ] T036 [P] [US3] src/core/logging/util/CircularBuffer.h 템플릿 구현 - push, getLast, clear (FIFO)
- [ ] T037 [US3] tests/unit/logging/CircularBuffer_test.cpp 작성 - FIFO, 오버플로우 (4 tests)

### 5.2 LoggingStrategy (FR-017, FR-018, FR-020)

- [ ] T038 [P] [US3] src/core/logging/dto/LoggingStrategy.h 열거형 정의 (NONE, MEMORY_ONLY, EVENT_DRIVEN, FULL_BAG)
- [ ] T039 [US3] src/core/logging/core/StrategyManager.{h,cpp} 구현 - DataType별 전략 설정, predicate 평가
- [ ] T040 [US3] tests/unit/logging/StrategyManager_test.cpp 작성 - 전략 전환, EVENT_DRIVEN predicate (4 tests)

### 5.3 DataStoreBagLogger 전략 통합

- [ ] T041 [US3] src/core/logging/core/DataStoreBagLogger.{h,cpp} 수정 - StrategyManager 통합, 전략별 분기
- [ ] T042 [US3] tests/unit/logging/DataStoreBagLogger_strategy_test.cpp 작성 - MEMORY_ONLY, EVENT_DRIVEN (4 tests)

### 5.4 설정 파일 관리 (FR-021)

- [ ] T043 [P] [US3] src/core/logging/util/ConfigLoader.{h,cpp} 구현 - JSON 설정 파일 읽기, SIGHUP 리로드
- [ ] T044 [US3] tests/unit/logging/ConfigLoader_test.cpp 작성 - 설정 로드, 런타임 리로드 (3 tests)
- [ ] T045 [US3] examples/logging_config.json 예제 파일 작성 (DataType별 전략 설정)

### 5.5 통합 테스트 (US3 완성)

- [ ] T046 [US3] tests/integration/logging/strategy_integration_test.cpp 작성 - InterfaceData MEMORY_ONLY, MissionState FULL_BAG (2 tests)
- [ ] T047 [US3] 디스크 사용량 벤치마크 - 384MB/hour → 24MB/hour 검증 (SC-003)

**완료 기준**:
- ✅ InterfaceData는 순환 버퍼 사용 (Bag 기록 안 함)
- ✅ MissionState는 전체 Bag 기록
- ✅ 런타임 설정 변경 동작
- ✅ 디스크 사용량 90% 감소 (SC-003)
- ✅ 15개 단위 테스트 + 2개 통합 테스트 통과

---

## Phase 6: Polish & Cross-Cutting Concerns

**목표**: 문서화, 에러 처리, 성능 최적화

### 6.1 에러 처리 강화 (FR-023, FR-024, FR-026)

- [ ] T048 [P] src/core/logging/util/ErrorHandler.{h,cpp} 구현 - 로깅 실패 시 시스템 정상 동작 유지
- [ ] T049 tests/unit/logging/ErrorHandler_test.cpp 작성 - 디스크 공간 부족, 파일 손상, 큐 오버플로우 (3 tests)

### 6.2 BagStats 및 모니터링 (FR-008)

- [ ] T050 [P] src/core/logging/dto/BagStats.h 구조체 정의 (messagesWritten, messagesDropped, bytesWritten, writeLatencyUs)
- [ ] T051 src/core/logging/util/StatsCollector.{h,cpp} 구현 - 통계 수집 및 스냅샷
- [ ] T052 tests/unit/logging/StatsCollector_test.cpp 작성 - atomic 카운터, 스냅샷 (2 tests)

### 6.3 문서화

- [ ] T053 [P] src/core/logging/README.md 작성 - 모듈 개요, 빌드 방법, 사용 예제
- [ ] T054 [P] examples/bag_logging_example.cpp 작성 - quickstart.md 기반 완전한 예제
- [ ] T055 [P] examples/bag_replay_example.cpp 작성 - Replay 사용 예제

### 6.4 성능 최적화

- [ ] T056 AsyncWriter 큐 크기 최적화 - 10,000 메시지 용량 검증
- [ ] T057 BagReader 인덱스 캐싱 최적화 - 메모리 사용량 <3MB 검증
- [ ] T058 전체 성능 벤치마크 실행 - SC-001, SC-004, SC-009 검증

### 6.5 최종 통합 테스트

- [ ] T059 tests/integration/logging/end_to_end_test.cpp 작성 - 8시간 운영 시뮬레이션 (SC-002)
- [ ] T060 메모리 누수 검사 - Valgrind 또는 AddressSanitizer 실행

**완료 기준**:
- ✅ 모든 성공 기준 (SC-001 ~ SC-009) 달성
- ✅ 단위 테스트 커버리지 80% 이상 (SC-007)
- ✅ 메모리 누수 없음
- ✅ 문서화 완료

---

## 의존성 그래프

사용자 스토리 완료 순서:

```
Setup (Phase 1) → Foundational (Phase 2)
                        ↓
                    US1 (Phase 3) ← MVP
                        ↓
                    US2 (Phase 4)
                        ↓
                    US3 (Phase 5)
                        ↓
                    Polish (Phase 6)
```

**사용자 스토리 간 의존성**:
- US2는 US1의 BagWriter가 생성한 파일을 읽음 (약한 의존성)
- US3는 US1의 DataStoreBagLogger를 확장 (강한 의존성)
- **권장**: US1 완료 후 US2/US3 병렬 진행 가능 (US2는 테스트 Bag 파일만 필요)

---

## 병렬 실행 예제

### Phase 3 (US1) 병렬 작업

```bash
# 3명의 개발자가 병렬로 작업
Developer A: T012-T013 (AsyncWriter)
Developer B: T014-T017 (RotationPolicy, RetentionPolicy)
Developer C: T018-T020 (IBagWriter, SimpleBagWriter)

# 이후 순차 작업
All: T021-T024 (DataStoreBagLogger, 통합 테스트)
```

### Phase 4 (US2) 병렬 작업

```bash
# 인덱싱과 Reader 병렬 작업
Developer A: T025-T028 (Indexing)
Developer B: T029-T030 (BagReader)

# Replayer는 BagReader 완료 후
Developer A+B: T031-T035 (BagReplayer, 통합 테스트)
```

### Phase 5 (US3) 병렬 작업

```bash
# CircularBuffer와 Strategy 병렬 작업
Developer A: T036-T037 (CircularBuffer)
Developer B: T038-T040 (LoggingStrategy, StrategyManager)
Developer C: T043-T045 (ConfigLoader)

# 통합은 순차
All: T041-T047 (DataStoreBagLogger 수정, 통합 테스트)
```

---

## 구현 전략

### MVP 범위 (최소 기능 제품)

**Phase 1 + Phase 2 + Phase 3 (User Story 1)**만 구현:
- EventBus 기반 비동기 Bag 로깅
- 파일 회전 및 보관 정책
- 프로덕션 디버깅 가능

**가치 제공**:
- 간헐적 오류 디버깅 (타임스탬프 기반 상태 추적)
- 8시간 운영 이력 영구 저장
- 디스크 공간 자동 관리

**예상 작업량**: 24개 태스크, ~2-3주 (1인 기준)

### 증분 배포 (Incremental Delivery)

1. **v0.1.0 (MVP)**: User Story 1 (Phase 1-3)
2. **v0.2.0**: User Story 2 추가 (Phase 4) - Replay 기능
3. **v0.3.0**: User Story 3 추가 (Phase 5) - 선택적 전략
4. **v1.0.0**: Polish (Phase 6) - 안정화, 성능 최적화

---

## 검증 체크리스트

### User Story 1 (P1) 검증

- [ ] DataStore.set("MissionState.status", "RUNNING") 호출 시 Bag 파일에 기록됨
- [ ] Bag 파일 크기 1GB 도달 시 자동 회전
- [ ] 7일 이상 파일 자동 삭제
- [ ] DataStore 성능 저하 <1% (87ns → 88ns)
- [ ] 30개 단위 테스트 + 3개 통합 테스트 통과

### User Story 2 (P2) 검증

- [ ] BagReader로 Bag 파일 순차 읽기
- [ ] 타임스탬프 기반 탐색 <10초 (1GB 파일)
- [ ] BagReplayer로 DataStore 상태 재현
- [ ] 재생 속도 2배속 동작
- [ ] 18개 단위 테스트 + 3개 통합 테스트 통과

### User Story 3 (P2) 검증

- [ ] InterfaceData MEMORY_ONLY 전략 동작 (Bag 기록 안 함)
- [ ] MissionState FULL_BAG 전략 동작 (전체 기록)
- [ ] Alarm EVENT_DRIVEN 전략 동작 (severity > HIGH만)
- [ ] 런타임 설정 변경 (SIGHUP) 동작
- [ ] 디스크 사용량 90% 감소 (384MB/hour → 24MB/hour)
- [ ] 15개 단위 테스트 + 2개 통합 테스트 통과

### 전체 시스템 검증

- [ ] 단위 테스트 커버리지 80% 이상 (SC-007)
- [ ] 8시간 운영 시 모든 변경사항 기록 (SC-002)
- [ ] 큐 오버플로우 시 시스템 크래시 없음 (SC-006)
- [ ] Bag 파일 손상 시 복구 가능한 메시지 읽기 (FR-024)
- [ ] 메모리 사용량 <10MB (계획: 3MB)
- [ ] 메모리 누수 없음 (Valgrind 통과)

---

## 태스크 통계

**총 태스크 수**: 60개

**Phase별 분포**:
- Phase 1 (Setup): 5개
- Phase 2 (Foundational): 6개
- Phase 3 (US1 - P1): 13개
- Phase 4 (US2 - P2): 11개
- Phase 5 (US3 - P2): 12개
- Phase 6 (Polish): 13개

**병렬 작업 기회**: 20개 태스크에 [P] 마커 (전체의 33%)

**예상 테스트 수**: 63개 단위 테스트 + 8개 통합 테스트 = **71개**

**예상 작업량**:
- MVP (Phase 1-3): ~2-3주 (1인 기준)
- Full (Phase 1-6): ~5-7주 (1인 기준)
- 병렬 작업 시 (3인): ~2-3주 (전체 완료)

---

## 다음 단계

1. **Phase 1 Setup 시작**: T001-T005 태스크 실행
2. **Foundational 구축**: T006-T011 (BagMessage, Serializer, FileUtils)
3. **MVP 구현**: T012-T024 (User Story 1)
4. **테스트 및 검증**: 단위 테스트 80% 커버리지 달성
5. **Incremental Delivery**: User Story 2, 3 순차 추가

**시작 명령어**:
```bash
# Phase 1 Setup
mkdir -p src/core/logging/{interfaces,core,dto,util}
mkdir -p tests/unit/logging
mkdir -p tests/integration/logging
mkdir -p logs examples

# 첫 번째 태스크 시작
# T001: 디렉토리 구조 생성 완료 ✅
```
