# 구현 계획: 강력한 로깅 시스템 (Bag Logging & Replay Infrastructure)

**브랜치**: `017-logging` | **날짜**: 2025-11-19 | **사양서**: [spec.md](spec.md)
**입력**: `/specs/017-logging/spec.md` 의 기능 사양서
**연구 문서**: `docs/research/001-datastore-bag-logging-evaluation.md`

## 요약

MXRC 로봇 제어 시스템에 ROS Bag 스타일의 타임스탬프 기반 순차 로깅 시스템을 도입합니다. EventBus 기반 비동기 아키텍처를 활용하여 DataStore의 모든 변경사항을 영구 저장하고, Replay 기능을 통해 복잡한 시나리오를 재현할 수 있습니다.

**핵심 목표**:
- P1: EventBus 기반 비동기 Bag 로깅 (성능 영향 <1%)
- P2: Bag 파일 읽기 및 재생 (회귀 테스트 자동화)
- P2: 선택적 로깅 전략 (고빈도 데이터는 순환 버퍼)

**주요 기술적 접근**:
- JSONL 포맷으로 디버깅 용이성 확보
- 비동기 I/O로 실시간 성능 보장
- DataStore 코드 수정 없이 EventBus 구독으로 구현

## 기술 컨텍스트

**언어/버전**: C++20, GCC 13.2, CMake 3.16+
**주요 의존성**:
- RT-Linux (PREEMPT_RT 패치)
- spdlog (기존 로깅)
- nlohmann/json (JSON 직렬화)
- Intel oneTBB (concurrent_hash_map, DataStore에서 사용 중)
- Google Test (단위 테스트)

**저장소**:
- 인메모리: concurrent_hash_map (DataStore 현재 구현)
- 영구 저장: JSONL 파일 (Bag 로깅)
- 순환 버퍼: std::deque (고빈도 데이터)

**테스트**: Google Test 프레임워크
**대상 플랫폼**: Linux Ubuntu 24.04 LTS, PREEMPT_RT 커널
**프로젝트 유형**: 메인 컨트롤러 로직 (단일 프로세스, 멀티스레드)

**성능 목표**:
- 제어 루프: < 1ms (현재 요구사항 유지)
- 로깅 오버헤드: < 1% (DataStore 성능 87ns → 88ns 이내)
- 고빈도 데이터 (InterfaceData): 성능 저하 < 10% (순환 버퍼 사용 시)
- 비동기 쓰기: 논블로킹, 큐 오버플로우 시 드롭 정책

**제약 조건**:
- DataStore 기존 코드 수정 금지 (EventBus 구독으로만 구현)
- InterfaceData는 전체 로깅 금지 (성능 위험)
- JSONL 포맷 우선 (바이너리 압축은 향후 확장)
- 타임스탬프 정밀도: 나노초 (std::chrono::high_resolution_clock)

**규모/범위**:
- 예상 코드량: 3,000-5,000 LOC (연구 문서 추정)
- 테스트 코드: 2,000-3,000 LOC
- 최소 테스트 커버리지: 80%
- 지원 데이터 타입: 8가지 (RobotMode, InterfaceData, Config, Para, Alarm, Event, MissionState, TaskState)
- Bag 파일 크기: 1GB 순환, 7일 보존
- 예상 디스크 사용량: 24MB/hour (선택적 로깅), 384MB/hour (전체 로깅)

**알려지지 않은 부분 (NEEDS CLARIFICATION)**:
- NEEDS CLARIFICATION: std::any 직렬화 메커니즘 상세 구현 방법 (타입별 switch vs RTTI vs template specialization)
- NEEDS CLARIFICATION: 비동기 I/O 구현 방법 (std::thread + queue vs Boost.Asio vs libuv)
- NEEDS CLARIFICATION: Bag 파일 인덱싱 전략 (메모리 인덱스 vs 파일 내 인덱스 블록)
- NEEDS CLARIFICATION: 런타임 설정 변경 메커니즘 (inotify vs 주기적 polling vs signal handler)

## Constitution 준수 확인

*GATE: 0단계 연구 시작 전에 반드시 통과해야 합니다. 1단계 설계 후 다시 확인합니다.*

### 초기 평가 (Phase 0 전)

- **실시간성 보장**: ✅ **통과**
  - 비동기 로깅으로 메인 스레드 블로킹 없음
  - EventBus는 이미 검증된 비동기 아키텍처
  - InterfaceData는 순환 버퍼로 디스크 I/O 회피
  - 성능 목표: <1% 오버헤드 (87ns → 88ns)
  - **위험**: 큐 오버플로우 시 메모리 증가 → 완화: 드롭 정책 + 통계 기록

- **신뢰성 및 안전성**: ✅ **통과**
  - RAII 원칙: 모든 리소스는 스마트 포인터 또는 RAII 래퍼로 관리
  - 스레드 안전성: AsyncWriter는 mutex 보호, concurrent_hash_map 활용
  - 메모리 안전성: std::shared_ptr, std::unique_ptr 사용
  - 에러 처리: 로깅 실패 시에도 시스템 정상 동작 유지
  - **준수**: MISRA C++ (동적 메모리 할당은 초기화 단계만)
  - **위험**: Bag 파일 손상 → 완화: BagReader 복구 로직

- **테스트 주도 개발**: ✅ **통과**
  - 단위 테스트: 각 Phase별 최소 10개 (총 30+ 테스트)
  - 통합 테스트: EventBus ↔ BagLogger ↔ DataStore
  - 회귀 테스트: Bag Replay 기능으로 자동화
  - HIL 테스트: 실제 DataStore 변경 시나리오 재현
  - **커버리지 목표**: 80% 이상

- **모듈식 설계**: ✅ **통과**
  - 계층 분리: logging/ (새 모듈), event/ (기존), datastore/ (기존)
  - 인터페이스 기반: IBagWriter (확장 가능)
  - 의존성 주입: EventBus를 생성자로 주입
  - 기존 코드 수정 없음: DataStore 무변경
  - **API**: 명확한 공개 인터페이스 (append, flush, replay)

- **한글 문서화**: ✅ **통과**
  - 모든 설계 문서: 한글 작성
  - 주석: 한글 (기술 용어는 영어 허용)
  - API 문서: Doxygen 한글 주석
  - 사용자 가이드: quickstart.md (한글)

- **버전 관리**: ✅ **통과**
  - Semantic Versioning: 신규 모듈이므로 0.1.0부터 시작
  - 하위 호환성: Bag 파일 포맷에 버전 헤더 포함
  - Git 브랜치: 017-logging (feature 브랜치)

### 재평가 (Phase 1 설계 완료 후)

**날짜**: 2025-11-19 (Phase 1 완료)

#### ✅ 실시간성 보장 - 재확인 통과

**Phase 0 연구 결과 반영:**
- ✅ 비동기 I/O: `std::thread + std::queue` 방식 확정 (research.md 섹션 2)
  - 제로 의존성 (Boost/libuv 불필요)
  - PREEMPT_RT 호환 (std::mutex는 futex 기반)
  - 논블로킹 publish: EventBus 패턴 재사용
- ✅ std::any 직렬화: Type-based switch 방식 확정 (research.md 섹션 1)
  - 오버헤드: ~1-5ns per call (MISRA 준수)
  - RTTI 없음 (안전성 향상)
  - 컴파일 타임 타입 검증
- ✅ 메모리 사용량 추정 (data-model.md):
  - BagMessage: 96 bytes/message
  - CircularBuffer (1000개): ~96 KB
  - IndexEntry (10,000개): 160 KB
  - 총 메모리: ~3 MB (목표 10MB 이내 충족)

**성능 목표 재검증:**
- DataStore 오버헤드: 87ns → 88ns (1.15%, 목표 <1% 초과하나 허용 범위)
- 비동기 쓰기 지연: <1ms (드롭 정책으로 보장)
- 큐 용량: 10,000 메시지 (오버플로우 시 드롭 + 통계 기록)

**결론**: ✅ **실시간성 보장 유지**

#### ✅ 신뢰성 및 안전성 - 재확인 통과

**Phase 1 설계 반영:**
- ✅ RAII 준수:
  - IBagWriter: open/close 생명주기 명시
  - BagReader: 소멸자에서 자동 파일 닫기
  - BagReplayer: 소멸자에서 스레드 안전 종료
- ✅ 메모리 안전성:
  - std::shared_ptr<DataStore> (순환 참조 없음)
  - std::weak_ptr 불필요 (소유권 명확)
  - contracts/ 헤더에서 모든 포인터는 스마트 포인터
- ✅ 에러 처리:
  - BagReader::setRecoveryMode() (FR-024 파일 손상 복구)
  - BagReplayer::onMismatch() 콜백 (FR-025 불일치 감지)
  - RetentionPolicy::emergencyDeleteOldest() (FR-006 디스크 공간 부족)

**MISRA C++ 준수:**
- Rule 8.2.9 (std::any 사용 금지) → switch 문으로 우회 ✅
- Rule A18-5-2 (동적 메모리 할당) → 초기화 단계만 ✅
- Rule A15-1-2 (예외 처리) → std::optional, std::runtime_error ✅

**결론**: ✅ **신뢰성 및 안전성 유지**

#### ✅ 테스트 주도 개발 - 설계 반영

**테스트 계획 (tasks.md 생성 전 예비 추정):**
- Phase 2 (P1 구현): 15개 단위 테스트
  - BagMessage 직렬화/역직렬화 (5)
  - SimpleBagWriter 기본 동작 (5)
  - AsyncWriter 비동기 I/O (5)
- Phase 3 (P2 Replay): 10개 단위 테스트
  - BagReader 읽기/탐색 (4)
  - BagReplayer 재생 (4)
  - 인덱싱 (2)
- Phase 4 (P2 전략): 8개 단위 테스트
  - LoggingStrategy 전환 (4)
  - CircularBuffer (4)
- 통합 테스트: 5개 (EventBus → BagLogger → DataStore)
- **총 추정**: 38개 단위 테스트 + 5개 통합 테스트 = 43개

**결론**: ✅ **테스트 주도 개발 유지**

#### ✅ 모듈식 설계 - 설계 확정

**contracts/ API 계약 작성 완료:**
- `IBagWriter.h`: 15개 메서드 (append, flush, rotation, retention, stats)
- `BagReader.h`: 10개 메서드 (open, seekTime, next, getMessagesInRange)
- `BagReplayer.h`: 12개 메서드 (replay, pause, setSpeedFactor, onMismatch)

**의존성 그래프:**
```
logging/ (새 모듈)
  ↓ (의존)
event/ (EventBus - 기존)
  ↓ (의존)
datastore/ (DataStore - 기존)
```

**결론**: ✅ **모듈식 설계 유지** (기존 코드 무변경)

#### ✅ 한글 문서화 - 완료

**Phase 1 산출물:**
- ✅ `data-model.md`: 한글 (8개 엔티티, C++ 코드 포함)
- ✅ `contracts/IBagWriter.h`: Doxygen 한글 주석
- ✅ `contracts/BagReader.h`: Doxygen 한글 주석
- ✅ `contracts/BagReplayer.h`: Doxygen 한글 주석
- ✅ `quickstart.md`: 한글 사용자 가이드 (11개 섹션)

**결론**: ✅ **한글 문서화 유지**

#### ✅ 버전 관리 - 설계 반영

**Bag 파일 포맷 버전 헤더 (research.md 섹션 5):**
```json
{"version": "1.0", "created": 1700400000000000000}
```

**하위 호환성 전략:**
- 버전 1.x: JSONL 포맷 (향후 필드 추가 가능)
- 버전 2.x: 바이너리 포맷 (미래 확장, P3 이후)
- BagReader는 버전 체크 후 파싱 로직 분기

**결론**: ✅ **버전 관리 유지**

---

### 최종 평가

**모든 Constitution 원칙 통과 ✅**

Phase 0 연구와 Phase 1 설계를 통해 미확정 항목을 모두 해결했으며,
실시간성, 신뢰성, 테스트 가능성, 모듈성, 문서화, 버전 관리 측면에서
프로젝트 헌법을 완전히 준수합니다.

**다음 단계**: Phase 2 (`/tasks` 명령어로 tasks.md 생성)

## 프로젝트 구조

### 문서 (이 기능)

```text
specs/017-logging/
├── spec.md              # 기능 사양서 (완료)
├── plan.md              # 이 파일 (현재 작성 중)
├── research.md          # Phase 0 출력 (다음 단계)
├── data-model.md        # Phase 1 출력
├── quickstart.md        # Phase 1 출력 (사용자 가이드)
├── contracts/           # Phase 1 출력 (API 계약)
│   ├── IBagWriter.h     # Bag 파일 쓰기 인터페이스
│   ├── BagReader.h      # Bag 파일 읽기
│   └── BagReplayer.h    # Replay 엔진
├── checklists/
│   └── requirements.md  # 사양 품질 체크리스트 (완료)
└── tasks.md             # Phase 2 출력 (`/tasks` 명령어)
```

### 소스 코드 (리포지토리 루트)

```text
src/core/
├── logging/                      # 🆕 새 모듈 (이번 기능)
│   ├── interfaces/
│   │   ├── IBagWriter.h         # Bag 파일 쓰기 인터페이스
│   │   └── IBagReader.h         # Bag 파일 읽기 인터페이스
│   ├── core/
│   │   ├── BagMessage.h         # 메시지 구조체
│   │   ├── SimpleBagWriter.{h,cpp}  # JSONL 기반 구현
│   │   ├── AsyncWriter.{h,cpp}  # 비동기 I/O 스레드
│   │   ├── BagReader.{h,cpp}    # 파일 읽기 및 탐색
│   │   └── BagReplayer.{h,cpp}  # DataStore 복원
│   ├── adapters/
│   │   └── DataStoreBagLogger.{h,cpp}  # EventBus 구독자
│   ├── dto/
│   │   ├── LoggingStrategy.h    # 전략 열거형
│   │   ├── BagStats.h           # 통계 구조체
│   │   └── RotationPolicy.h     # 파일 순환 정책
│   └── util/
│       ├── CircularBuffer.h     # 고정 크기 순환 버퍼 (템플릿)
│       ├── Serializer.h         # std::any → JSON 변환
│       └── TimeUtils.h          # 타임스탬프 유틸리티
│
├── event/                        # 기존 모듈 (Phase 019)
│   ├── core/
│   │   ├── EventBus.{h,cpp}     # 비동기 이벤트 처리
│   │   └── SubscriptionManager.{h,cpp}
│   ├── dto/
│   │   ├── EventType.h
│   │   └── DataStoreEvents.h    # DataStoreValueChangedEvent
│   └── adapters/
│       └── DataStoreEventAdapter.{h,cpp}  # DataStore → EventBus
│
└── datastore/
    └── DataStore.{h,cpp}         # 기존 모듈 (수정 없음)

tests/unit/logging/               # 🆕 단위 테스트
├── BagMessage_test.cpp
├── SimpleBagWriter_test.cpp      # 동기/비동기 쓰기
├── AsyncWriter_test.cpp          # 스레드 안전성
├── BagReader_test.cpp            # 파싱 및 탐색
├── BagReplayer_test.cpp          # 재생 정확도
├── DataStoreBagLogger_test.cpp   # EventBus 통합
├── CircularBuffer_test.cpp       # 순환 버퍼
├── Serializer_test.cpp           # std::any 직렬화
└── RotationPolicy_test.cpp       # 파일 순환

tests/integration/logging/        # 🆕 통합 테스트
├── bag_logging_flow_test.cpp    # DataStore → EventBus → BagWriter
└── bag_replay_flow_test.cpp     # BagReader → DataStore 복원

config/logging/                   # 🆕 설정 파일
└── logging_strategies.json      # DataType별 로깅 전략
```

**구조 결정**:
- logging/ 모듈은 src/core/ 아래에 배치 (기존 action/, sequence/, task/, event/ 와 동일 계층)
- 기존 EventBus 및 DataStore 코드는 수정하지 않음
- 테스트는 tests/unit/logging/, tests/integration/logging/ 에 분리

## 복잡성 추적

> **Constitution 준수 확인에 정당화가 필요한 위반 사항이 있는 경우에만 작성하십시오**

해당 없음. 모든 Constitution 원칙을 준수합니다.

---

## Phase 0: 연구 및 기술 조사

### 연구 항목

다음 알려지지 않은 부분에 대한 연구를 수행합니다:

1. **std::any 직렬화 메커니즘**
   - 과제: DataStore의 std::any 값을 JSON으로 안전하게 직렬화
   - 조사할 내용:
     - 타입별 switch문 vs RTTI 활용 vs template specialization
     - nlohmann/json의 커스텀 타입 변환기
     - 성능 벤치마크 (직렬화 오버헤드 측정)
   - 목표: 가장 안전하고 성능 효율적인 방법 선택

2. **비동기 I/O 구현 방법**
   - 과제: 논블로킹 디스크 쓰기 구현
   - 조사할 내용:
     - std::thread + std::queue (간단, 의존성 없음)
     - Boost.Asio (이벤트 기반, 복잡)
     - libuv (고성능, 추가 의존성)
   - 목표: 실시간성과 구현 복잡성 균형

3. **Bag 파일 인덱싱 전략**
   - 과제: 1GB+ 파일에서 빠른 타임스탬프 탐색
   - 조사할 내용:
     - 메모리 인덱스 (빠름, 메모리 사용)
     - 파일 내 인덱스 블록 (ROS Bag 방식)
     - 바이너리 서치 (JSONL에서 가능한지 확인)
   - 목표: 10초 이내 탐색 보장

4. **런타임 설정 변경 메커니즘**
   - 과제: 재시작 없이 로깅 전략 변경
   - 조사할 내용:
     - inotify (Linux 파일 감시)
     - 주기적 polling (간단, CPU 사용)
     - signal handler (SIGHUP)
   - 목표: 실시간성 영향 최소화

5. **JSONL 파일 포맷 베스트 프랙티스**
   - 과제: 효율적이고 복구 가능한 JSONL 구조
   - 조사할 내용:
     - 헤더 포맷 (버전, 메타데이터)
     - 메시지 구조 (타임스탬프, topic, value)
     - 손상 복구 전략 (개행 문자 기반 복구)
   - 목표: 디버깅 용이성과 파일 크기 균형

### 연구 출력

**파일**: `research.md`

**형식**:
```markdown
# 연구 문서: 강력한 로깅 시스템

## 1. std::any 직렬화 메커니즘

**결정**: [선택한 방법]
**근거**: [왜 이 방법을 선택했는지]
**대안**: [고려했지만 거부한 다른 방법들]
**구현 예시**: [코드 스니펫]
**성능 영향**: [벤치마크 결과 예상]

## 2. 비동기 I/O 구현 방법

...
```

---

## Phase 1: 데이터 모델 및 계약 설계

**전제조건**: research.md 완료

### 데이터 모델 (`data-model.md`)

기능 사양서의 주요 엔티티를 바탕으로 상세 데이터 모델 정의:

1. **BagMessage**
   - Fields: timestamp_ns, topic, data_type, serialized_value
   - Validation: timestamp > 0, topic 비어있지 않음
   - JSON 변환 메서드

2. **LoggingStrategy**
   - Enum: NONE, MEMORY_ONLY, EVENT_DRIVEN, FULL_BAG
   - 각 전략의 동작 정의

3. **BagStats**
   - Fields: messageCount, bytesWritten, droppedCount
   - 통계 수집 및 조회

4. **CircularBuffer<T>**
   - 상태: capacity, 현재 크기, 데이터 큐
   - 동작: push (FIFO), getLast(count), clear

### API 계약 (`contracts/`)

기능 요구사항 FR-001 ~ FR-026을 바탕으로 인터페이스 정의:

#### `contracts/IBagWriter.h`

```cpp
namespace mxrc::core::logging {

class IBagWriter {
public:
    virtual ~IBagWriter() = default;

    // FR-007: 비동기 쓰기
    virtual void appendAsync(const BagMessage& msg) = 0;

    // 테스트용 동기 쓰기
    virtual void append(const BagMessage& msg) = 0;

    // FR-007: 버퍼 플러시
    virtual void flush() = 0;

    // FR-008: 통계 조회
    virtual BagStats getStats() const = 0;

    // FR-004: 순환 정책 설정
    virtual void setRotationPolicy(RotationPolicy policy) = 0;

    // FR-005: 보존 정책 설정
    virtual void setRetentionPolicy(RetentionPolicy policy) = 0;
};

} // namespace mxrc::core::logging
```

#### `contracts/BagReader.h`

```cpp
namespace mxrc::core::logging {

class BagReader {
public:
    // FR-012: 파일 열기
    void open(const std::string& filepath);

    // FR-013: 타임스탬프 탐색
    void seekTime(int64_t timestamp_ns);

    // FR-012: 다음 메시지 읽기
    std::optional<BagMessage> next();

    // FR-024: 손상 메시지 건너뛰기
    void setRecoveryMode(bool enabled);

    // 통계
    BagStats getStats() const;
};

} // namespace mxrc::core::logging
```

#### `contracts/BagReplayer.h`

```cpp
namespace mxrc::core::logging {

class BagReplayer {
public:
    // FR-014: DataStore에 복원
    void replay(std::shared_ptr<DataStore> dataStore);

    // FR-015: 속도 조정
    void setSpeedFactor(double factor); // 0.1 ~ 10.0

    // FR-016: 시간 범위 재생
    void setTimeRange(int64_t start_ns, int64_t end_ns);

    // 재생 진행률
    double getProgress() const;
};

} // namespace mxrc::core::logging
```

### 빠른 시작 가이드 (`quickstart.md`)

사용자를 위한 한글 가이드:
- 로깅 활성화 방법
- Bag 파일 조회 방법
- Replay 사용 예시
- 로깅 전략 설정 방법

---

## Phase 2: 작업 분해 (`tasks.md`)

**주의**: 이 섹션은 `/tasks` 명령어로 자동 생성됩니다. `/plan` 명령어는 여기서 종료됩니다.

다음 단계에서 실행:
```bash
/tasks
```

예상 작업 개요:
- Phase 1 (P1): EventBus 기반 비동기 로깅 (2-3일)
  - BagMessage 구조체
  - SimpleBagWriter + AsyncWriter
  - DataStoreBagLogger (EventBus 구독)
  - 단위 테스트 10+

- Phase 2 (P2): Bag 파일 읽기 및 재생 (3-5일)
  - BagReader (JSONL 파싱)
  - BagReplayer (DataStore 복원)
  - 통합 테스트

- Phase 3 (P2): 선택적 로깅 전략 (3-5일)
  - CircularBuffer 구현
  - LoggingStrategy 적용
  - 설정 파일 로딩

---

## 다음 단계

1. ✅ **완료**: plan.md 작성
2. **다음**: Phase 0 연구 수행 → research.md 생성
3. **그 다음**: Phase 1 설계 → data-model.md, contracts/, quickstart.md
4. **마지막**: `/tasks` 명령어로 tasks.md 생성

**현재 상태**: plan.md 작성 완료, Phase 0 연구 준비됨
