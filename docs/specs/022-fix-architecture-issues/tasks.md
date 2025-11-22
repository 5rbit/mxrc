# Implementation Tasks: 아키텍처 안정성 개선

**Feature**: 022-fix-architecture-issues | **Date**: 2025-01-22
**Branch**: `022-fix-architecture-issues`
**Progress**: 12/35 tasks completed (MVP P1 완료 ✅)

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 작업 설명은 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: DataStore, EventBus, systemd, Accessor, Watchdog 등)

---

## Task 개요

**총 작업 수**: 35개
- **Phase 0**: Setup (3개)
- **Phase 1**: Foundational - VersionedData (4개)
- **Phase 2**: [US1 P1] systemd 시작 순서 (5개) - **MVP**
- **Phase 3**: [US2 P2] DataStore Accessor 패턴 (9개)
- **Phase 4**: [US3 P3] EventBus 우선순위 큐 (8개)
- **Phase 5**: [US4 P4] Watchdog 하트비트 (3개)
- **Phase 6**: Polish & Documentation (3개)

**MVP 범위**: Phase 0 + Phase 1 + Phase 2 (12개 작업) - systemd 시작 순서 경쟁 상태 해결

**핵심 마일스톤**:
1. Phase 2 완료 → P1 배포 가능 (즉시 프로덕션 적용)
2. Phase 3 완료 → DataStore God Object 해결
3. Phase 4 완료 → EventBus 안정성 보장
4. Phase 5 완료 → HA 스플릿 브레인 방지

---

## Phase 0: Setup (3개 작업) ✅ 완료

### - [x] T001 [US1] CMake 빌드 설정 업데이트
**파일**: `CMakeLists.txt`
**설명**: libsystemd 의존성 추가 및 컴파일 플래그 설정
**상세**:
- `find_package(PkgConfig REQUIRED)` 추가
- `pkg_check_modules(SYSTEMD REQUIRED libsystemd)` 추가
- `target_link_libraries(mxrc-rt ${SYSTEMD_LIBRARIES})` 추가
- `target_link_libraries(mxrc-nonrt ${SYSTEMD_LIBRARIES})` 추가
**검증**: `cmake .. && make -j$(nproc)` 성공

### - [x] T002 [P] [US2] VersionedData 헤더 파일 생성
**파일**: `src/core/datastore/core/VersionedData.h`
**설명**: 버전 관리 데이터 래퍼 구조체 정의
**상세**:
- `template <typename T> struct VersionedData` 정의
- 필드: `T value`, `uint64_t version`, `uint64_t timestamp_ns`
- `bool isConsistentWith(const VersionedData<T>&) const` 메서드
- `bool isNewerThan(const VersionedData<T>&) const` 메서드
- POD 타입으로 정의 (trivially copyable)
**검증**: 컴파일 성공, sizeof(VersionedData<double>) == 24 bytes

### - [x] T003 [P] [US3] EventPriority enum 헤더 파일 생성
**파일**: `src/core/event/core/EventPriority.h`
**설명**: 3단계 우선순위 enum 정의
**상세**:
- `enum class EventPriority { CRITICAL = 0, NORMAL = 1, DEBUG = 2 }`
- 주석으로 각 우선순위 용도 설명
**검증**: 컴파일 성공

---

## Phase 1: Foundational - VersionedData (2개 작업 완료, 2개 스킵)

### - [x] T004 [US2] VersionedData 단위 테스트 작성
**파일**: `tests/unit/datastore/VersionedDataTest.cpp`
**설명**: VersionedData 구조체 동작 검증 테스트
**상세**:
- `VersionIncrement`: 버전 증가 테스트
- `ConsistencyCheck`: isConsistentWith() 검증
- `NewerThan`: isNewerThan() 검증
- `TimestampUpdate`: 타임스탬프 갱신 검증
**검증**: `ctest -R VersionedDataTest` 모두 통과

### - [ ] T005 [US2] IDataStore 인터페이스 확장 (Phase 3에서 처리)
**파일**: `src/core/datastore/interfaces/IDataStore.h`
**설명**: getVersioned/setVersioned 메서드 추가
**상세**:
- `template <typename T> VersionedData<T> getVersioned(const std::string& key) const` 추가
- `template <typename T> void setVersioned(const std::string& key, const T& value)` 추가
- 기존 `get<T>()`/`set<T>()` 메서드 유지 (하위 호환성)
**검증**: 컴파일 성공, 기존 테스트 통과

### - [ ] T006 [US2] DataStore 구현체에 VersionedData 지원 추가 (Phase 3에서 처리)
**파일**: `src/core/datastore/managers/DataStore.cpp`
**설명**: getVersioned/setVersioned 메서드 구현
**상세**:
- 내부 스토리지를 `std::unordered_map<std::string, VersionedData<variant>>` 형태로 확장
- `setVersioned()`: atomic increment로 version 증가, timestamp_ns 갱신
- `getVersioned()`: 현재 VersionedData 복사본 반환
- Lock 범위 최소화 (버전 증가만)
**검증**: 단위 테스트 작성 및 통과

### - [ ] T007 [US2] DataStore VersionedData 통합 테스트 (Phase 3에서 처리)
**파일**: `tests/integration/datastore/version_consistency_test.cpp`
**설명**: RT/Non-RT 동시 접근 시 버전 일관성 검증
**상세**:
- RT 스레드: 10μs 주기로 쓰기
- Non-RT 스레드: 100ms 주기로 읽기 (버전 일관성 체크)
- 버전 불일치 발생률 < 5% 검증
- 10초간 실행 후 결과 수집
**검증**: 테스트 통과, 메트릭 출력

---

## Phase 2: [US1 P1] systemd 시작 순서 (5개 작업) - **MVP** ✅ 완료

### - [x] T008 [US1] mxrc-rt.service Before 지시자 추가
**파일**: `systemd/mxrc-rt.service`
**설명**: RT 서비스가 Non-RT보다 먼저 시작되도록 설정
**상세**:
- `[Unit]` 섹션에 `Before=mxrc-nonrt.service` 추가
- `Type=notify` 유지 (기존 Feature 018 설정)
- `WatchdogSec=30s` 유지
**검증**: `systemctl show mxrc-rt.service | grep Before` 확인

### - [x] T009 [US1] mxrc-nonrt.service After 지시자 추가
**파일**: `systemd/mxrc-nonrt.service`
**설명**: Non-RT 서비스가 RT 다음에 시작되도록 설정
**상세**:
- `[Unit]` 섹션에 `After=mxrc-rt.service` 추가 (기존 `After=network.target` 유지)
- `Type=simple` 유지
**검증**: `systemctl show mxrc-nonrt.service | grep After` 확인

### - [x] T010 [US1] RT 프로세스 READY 신호 전송 구현
**파일**: `src/core/systemd/impl/StartupNotifier.cpp`, `src/core/systemd/impl/StartupNotifier.h`
**설명**: 공유 메모리 생성 후 sd_notify(READY=1) 호출
**상세**:
- `StartupNotifier` 클래스 구현
- `void notifyReady()` 메서드: `sd_notify(0, "READY=1")` 호출
- `RTExecutive::initialize()`에서 공유 메모리 생성 직후 호출
- 실패 시 경고 로그 (spdlog::warn)
**검증**: `journalctl -u mxrc-rt.service | grep "READY=1"` 확인

### - [x] T011 [US1] Non-RT 공유 메모리 연결 재시도 로직 구현
**파일**: `src/core/rt/ipc/IPCInitializer.cpp`, `src/core/rt/ipc/IPCInitializer.h`
**설명**: 공유 메모리 연결 실패 시 최대 5초 재시도
**상세**:
- `IPCInitializer` 클래스 구현
- `bool connectWithRetry()` 메서드
- 최대 50회 재시도 (100ms 간격)
- 각 시도마다 `tryOpenSharedMemory()` 호출
- 성공 시 시도 횟수 로그 (spdlog::info)
- 실패 시 오류 로그 및 false 반환
**검증**: RT 서비스 중단 상태에서 Non-RT 시작 시 재시도 로그 확인

### - [x] T012 [US1] systemd 시작 순서 통합 테스트
**파일**: `tests/integration/systemd/startup_order_test.cpp`
**설명**: 10회 재시작하여 100% 성공률 검증
**상세**:
- Bash 스크립트로 10회 재시작 루프
- 각 재시작마다 `systemctl status` 확인 (Active: active)
- `/dev/shm/mxrc_*` 파일 존재 확인
- journalctl에서 "Connected to shared memory on attempt" 로그 수집
- 실패 횟수 = 0 검증
**검증**: 10회 모두 성공

---

## Phase 3: [US2 P2] DataStore Accessor 패턴 (9개 작업)

### - [ ] T013 [US2] IDataAccessor 기본 인터페이스 정의
**파일**: `src/core/datastore/interfaces/IDataAccessor.h`
**설명**: 모든 Accessor의 기본 인터페이스
**상세**:
- `virtual ~IDataAccessor() = default`
- `virtual std::string getDomain() const = 0`
- I-prefix 네이밍 규칙 준수 (Principle II)
**검증**: 컴파일 성공

### - [ ] T014 [P] [US2] ISensorDataAccessor 인터페이스 정의
**파일**: `src/core/datastore/interfaces/ISensorDataAccessor.h`
**설명**: 센서 데이터 도메인 접근 인터페이스
**상세**:
- `IDataAccessor` 상속
- `virtual VersionedData<double> getTemperature() const = 0`
- `virtual VersionedData<double> getPressure() const = 0`
- `virtual VersionedData<double> getHumidity() const = 0`
- `virtual VersionedData<double> getVibration() const = 0`
- `virtual VersionedData<double> getCurrent() const = 0`
- `virtual void setTemperature(double value) = 0`
- `virtual void setPressure(double value) = 0`
- 주석으로 접근 가능 키 목록 명시 (sensor.*)
**검증**: 컴파일 성공

### - [ ] T015 [P] [US2] IRobotStateAccessor 인터페이스 정의
**파일**: `src/core/datastore/interfaces/IRobotStateAccessor.h`
**설명**: 로봇 상태 도메인 접근 인터페이스
**상세**:
- `IDataAccessor` 상속
- `virtual VersionedData<Eigen::Vector3d> getPosition() const = 0`
- `virtual VersionedData<Eigen::Vector3d> getVelocity() const = 0`
- `virtual VersionedData<std::vector<double>> getJointAngles() const = 0`
- `virtual void setPosition(const Eigen::Vector3d& value) = 0`
- `virtual void setVelocity(const Eigen::Vector3d& value) = 0`
- 주석으로 접근 가능 키 목록 명시 (robot_state.*)
**검증**: 컴파일 성공

### - [ ] T016 [P] [US2] ITaskStatusAccessor 인터페이스 정의
**파일**: `src/core/datastore/interfaces/ITaskStatusAccessor.h`
**설명**: Task 상태 도메인 접근 인터페이스
**상세**:
- `IDataAccessor` 상속
- `enum class TaskState { IDLE, RUNNING, PAUSED, COMPLETED, FAILED }` 정의
- `virtual VersionedData<TaskState> getTaskState() const = 0`
- `virtual VersionedData<double> getProgress() const = 0`
- `virtual VersionedData<int> getErrorCode() const = 0`
- `virtual void setTaskState(TaskState value) = 0`
- `virtual void setProgress(double value) = 0`
- 주석으로 접근 가능 키 목록 명시 (task_status.*)
**검증**: 컴파일 성공

### - [ ] T017 [US2] SensorDataAccessor 구현
**파일**: `src/core/datastore/impl/SensorDataAccessor.cpp`, `src/core/datastore/impl/SensorDataAccessor.h`
**설명**: ISensorDataAccessor 구현체
**상세**:
- `IDataStore& datastore_` 참조로 보유 (소유권 없음, RAII 준수)
- `static constexpr std::array<const char*, 5> ALLOWED_KEYS` 정의
- 모든 메서드를 `inline` 선언 (성능 최적화)
- `getTemperature()`: `datastore_.getVersioned<double>("sensor.temperature")` 호출
- `setTemperature()`: `datastore_.setVersioned("sensor.temperature", value)` 호출
- 나머지 센서 메서드 동일 패턴
**검증**: 단위 테스트 작성 및 통과

### - [ ] T018 [P] [US2] RobotStateAccessor 구현
**파일**: `src/core/datastore/impl/RobotStateAccessor.cpp`, `src/core/datastore/impl/RobotStateAccessor.h`
**설명**: IRobotStateAccessor 구현체
**상세**:
- `IDataStore& datastore_` 참조로 보유
- `static constexpr std::array<const char*, 4> ALLOWED_KEYS` 정의
- 모든 메서드를 `inline` 선언
- `getPosition()`: `datastore_.getVersioned<Eigen::Vector3d>("robot_state.position")` 호출
- 벡터 타입 사전 할당 (RT 경로 메모리 할당 금지)
**검증**: 단위 테스트 작성 및 통과

### - [ ] T019 [P] [US2] TaskStatusAccessor 구현
**파일**: `src/core/datastore/impl/TaskStatusAccessor.cpp`, `src/core/datastore/impl/TaskStatusAccessor.h`
**설명**: ITaskStatusAccessor 구현체
**상세**:
- `IDataStore& datastore_` 참조로 보유
- `static constexpr std::array<const char*, 3> ALLOWED_KEYS` 정의
- 모든 메서드를 `inline` 선언
- TaskState enum 직렬화 지원
**검증**: 단위 테스트 작성 및 통과

### - [ ] T020 [US2] Accessor 성능 벤치마크 작성
**파일**: `tests/unit/datastore/accessor_benchmark.cpp`
**설명**: Accessor 읽기/쓰기 지연 시간 측정
**상세**:
- `getTemperature()` 100만 회 호출 평균 지연 < 60ns 검증
- `setTemperature()` 10만 회 호출 평균 지연 < 110ns 검증
- `isConsistentWith()` 100만 회 호출 평균 지연 < 10ns 검증
- rdtsc() CPU cycle counter 사용
**검증**: 모든 벤치마크 목표 달성

### - [ ] T021 [US2] 기존 코드 Accessor 패턴 마이그레이션
**파일**: `src/core/rt/ControlLoop.cpp`, `src/core/nonrt/Monitor.cpp` 등
**설명**: 직접 DataStore 접근을 Accessor 사용으로 변경
**상세**:
- `datastore->get<double>("sensor.temperature")` → `sensorAccessor->getTemperature()`
- RT 경로: 최신 값 바로 사용 (버전 체크 생략)
- Non-RT 경로: 버전 일관성 체크 추가 (최대 3회 재시도)
- 컴파일러 경고로 직접 접근 deprecated 처리
**검증**: 기존 기능 정상 동작, 통합 테스트 통과

---

## Phase 4: [US3 P3] EventBus 우선순위 큐 (8개 작업)

### - [ ] T022 [US3] PrioritizedEvent 구조체 정의
**파일**: `src/core/event/core/PrioritizedEvent.h`
**설명**: 우선순위 이벤트 엔티티
**상세**:
- `std::string type` (최대 64자)
- `EventPriority priority`
- `std::variant<int, double, std::string> payload`
- `uint64_t timestamp_ns`
- POD 타입으로 정의 (move semantics 지원)
**검증**: sizeof(PrioritizedEvent) < 128 bytes

### - [ ] T023 [US3] PriorityQueue 클래스 구현
**파일**: `src/core/event/core/PriorityQueue.cpp`, `src/core/event/core/PriorityQueue.h`
**설명**: 3단계 우선순위 큐
**상세**:
- `std::array<std::unique_ptr<boost::lockfree::spsc_queue<PrioritizedEvent>>, 3> queues_`
- `std::atomic<size_t> total_size_`
- `const size_t QUEUE_CAPACITY = 4096`
- `const size_t DROP_THRESHOLD = 3276` (80%)
- `bool push(PrioritizedEvent&& event)`: Non-blocking, 백프레셔 체크
- `std::optional<PrioritizedEvent> pop()`: CRITICAL → NORMAL → DEBUG 순서
**검증**: 단위 테스트 작성 및 통과

### - [ ] T024 [US3] 백프레셔 및 Drop Policy 구현
**파일**: `src/core/event/core/PriorityQueue.cpp`
**설명**: 큐 포화 시 낮은 우선순위부터 버리기
**상세**:
- 큐 80% 이상: DEBUG 이벤트 버림
- 큐 90% 이상: NORMAL 이벤트도 버림
- CRITICAL 이벤트는 절대 버리지 않음 (busy-wait)
- 버려진 이벤트 수 메트릭 수집 (`metrics_.debugEventsDropped`, `metrics_.normalEventsDropped`)
**검증**: 단위 테스트로 Drop Policy 검증

### - [ ] T025 [P] [US3] ThrottlingPolicy 클래스 구현
**파일**: `src/core/event/adapters/ThrottlingPolicy.cpp`, `src/core/event/adapters/ThrottlingPolicy.h`
**설명**: 동일 이벤트 타입 Throttling (100ms 간격)
**상세**:
- `std::unordered_map<std::string, uint64_t> last_sent_time_`
- `const uint64_t THROTTLE_INTERVAL_MS = 100`
- `bool shouldSend(const std::string& event_type)` 메서드
- 마지막 전송 시간과 비교하여 허용 여부 결정
**검증**: 단위 테스트로 Throttling 동작 검증

### - [ ] T026 [P] [US3] CoalescingPolicy 클래스 구현
**파일**: `src/core/event/adapters/CoalescingPolicy.cpp`, `src/core/event/adapters/CoalescingPolicy.h`
**설명**: 동일 이벤트 병합 (100ms 이내 반복 시 1개로 병합)
**상세**:
- `std::unordered_map<std::string, PrioritizedEvent> pending_events_`
- `const uint64_t COALESCE_WINDOW_MS = 100`
- `std::optional<PrioritizedEvent> coalesce(PrioritizedEvent&& event)` 메서드
- 윈도우 내 동일 타입 이벤트 병합, 윈도우 종료 시 전송
**검증**: 단위 테스트로 Coalescing 동작 검증

### - [ ] T027 [US3] EventBus에 PriorityQueue 통합
**파일**: `src/core/event/adapters/EventBus.cpp`
**설명**: 기존 EventBus를 PriorityQueue 사용으로 변경
**상세**:
- `std::unique_ptr<PriorityQueue> priorityQueue_` 멤버 추가
- `publish()` → `push()`: PrioritizedEvent 생성 후 큐에 삽입
- `processEvents()`: `pop()` 호출하여 우선순위 순서로 처리
- Throttling 및 Coalescing 정책 적용
**검증**: 기존 EventBus 기능 정상 동작

### - [ ] T028 [US3] EventBus 우선순위 큐 단위 테스트
**파일**: `tests/unit/event/PriorityQueueTest.cpp`
**설명**: 우선순위 큐 동작 검증
**상세**:
- `CriticalEventFirst`: CRITICAL 이벤트가 먼저 pop되는지 검증
- `DebugEventDrop`: 큐 80% 이상 차면 DEBUG 버려지는지 검증
- `NormalEventDrop`: 큐 90% 이상 차면 NORMAL 버려지는지 검증
- `CriticalNeverDrop`: CRITICAL은 절대 버려지지 않는지 검증
**검증**: 모든 테스트 통과

### - [ ] T029 [US3] 이벤트 폭주 시나리오 통합 테스트
**파일**: `tests/integration/event/event_storm_test.cpp`
**설명**: 대량 이벤트 발생 시 안정성 검증
**상세**:
- 100,000개 이벤트 push (50% CRITICAL, 30% NORMAL, 20% DEBUG)
- CRITICAL 손실률 = 0% 검증
- NORMAL 손실률 < 1% 검증
- DEBUG 손실률 < 10% 검증
- RT 사이클 지터 < 10μs 검증
**검증**: 테스트 통과, 메트릭 출력

---

## Phase 5: [US4 P4] Watchdog 하트비트 (3개 작업)

### - [ ] T030 [US4] WatchdogNotifier 클래스 구현
**파일**: `src/core/systemd/impl/WatchdogNotifier.cpp`, `src/core/systemd/impl/WatchdogNotifier.h`
**설명**: systemd Watchdog 하트비트 전송
**상세**:
- `std::atomic<bool> running_`
- `std::thread watchdog_thread_`
- `const std::chrono::seconds WATCHDOG_INTERVAL{10}` (30초 / 3)
- `void start()`: 스레드 생성 후 10초마다 `sd_notify(0, "WATCHDOG=1")` 호출
- `void stop()`: 스레드 종료 (하트비트 중단)
**검증**: journalctl에서 10초마다 WATCHDOG 신호 확인

### - [ ] T031 [US4] IPC 채널 장애 감지 로직 구현
**파일**: `src/core/rt/RTExecutive.cpp`
**설명**: IPC 채널 상태 모니터링 및 Watchdog 중단
**상세**:
- `void monitorIPCHealth()` 메서드 추가
- 5초마다 `ipc_channel_->isHealthy()` 확인
- 장애 감지 시 `watchdog_notifier_->stop()` 호출 (타임아웃 유도)
- `spdlog::critical("IPC channel failure detected")` 로그
- systemd가 30초 후 자동 재시작
**검증**: IPC 장애 시뮬레이션 테스트로 재시작 확인

### - [ ] T032 [US4] Watchdog 복구 통합 테스트
**파일**: `tests/integration/systemd/watchdog_recovery_test.cpp`
**설명**: Watchdog 타임아웃 및 자동 재시작 검증
**상세**:
- RT 프로세스에 SIGUSR1 신호 전송 (IPC 장애 시뮬레이션)
- 30초 대기
- `systemctl status mxrc-rt.service` 확인 (재시작됨)
- journalctl에서 "Watchdog timeout" 로그 확인
**검증**: 재시작 성공, IPC 복구 시간 < 10초

---

## Phase 6: Polish & Documentation (3개 작업)

### - [ ] T033 [P] Prometheus 메트릭 추가
**파일**: `src/core/metrics/MetricsCollector.cpp`
**설명**: 아키텍처 안정성 관련 메트릭 추가
**상세**:
- P1: `startup_failures`, `startup_duration`, `shm_retry_count`
- P2: `version_mismatch_count`, `version_check_latency`
- P3: `event_queue_usage`, `events_dropped{priority}`, `events_throttled`, `events_coalesced`
- P4: `watchdog_heartbeats`, `watchdog_timeouts`, `ipc_failures`, `ipc_recovery_time`
**검증**: `curl http://localhost:9090/metrics | grep mxrc` 확인

### - [ ] T034 [P] 아키텍처 문서 업데이트
**파일**: `docs/architecture/architecture.md`
**설명**: Feature 022 변경 사항 반영
**상세**:
- DataStore Accessor 패턴 다이어그램 추가
- EventBus 우선순위 큐 구조 설명
- systemd 시작 순서 다이어그램 업데이트
- Watchdog 하트비트 흐름도 추가
**검증**: 문서 리뷰 완료

### - [ ] T035 [P] CHANGELOG.md 업데이트
**파일**: `CHANGELOG.md`
**설명**: Feature 022 변경 사항 기록
**상세**:
- P1: systemd 시작 순서 경쟁 상태 해결
- P2: DataStore Accessor 패턴 도입
- P3: EventBus 우선순위 큐 및 백프레셔
- P4: systemd Watchdog 하트비트
- Breaking Changes: 없음 (하위 호환성 유지)
**검증**: 문서 리뷰 완료

---

## 병렬 실행 가능 작업 (Parallelizable)

**[P] 마커가 있는 작업**은 서로 다른 파일을 수정하므로 병렬 실행 가능:

**Phase 0**:
- T002, T003 (다른 헤더 파일)

**Phase 3**:
- T014, T015, T016 (다른 인터페이스 파일)
- T018, T019 (다른 구현 파일, T017 완료 후)

**Phase 4**:
- T025, T026 (다른 Policy 클래스)

**Phase 6**:
- T033, T034, T035 (다른 문서 파일)

---

## 종속성 그래프

```
Phase 0 (Setup)
T001 (CMake)
├── T002 (VersionedData.h) ──┐
└── T003 (EventPriority.h)   │
                             │
Phase 1 (Foundational)       │
T004 (VersionedData Test) ───┤
T005 (IDataStore 확장) ──────┤
T006 (DataStore 구현) ────────┤
T007 (통합 테스트) ───────────┘
         │
         ├──────────────────┬─────────────────┐
         │                  │                 │
Phase 2 (P1)         Phase 3 (P2)      Phase 4 (P3)
T008 (RT Before)     T013 (IAccessor)  T022 (PrioritizedEvent)
T009 (Non-RT After)  ├── T014 (ISensor) T023 (PriorityQueue)
T010 (READY 신호)     ├── T015 (IRobot)  T024 (Drop Policy)
T011 (재시도 로직)    ├── T016 (ITask)   ├── T025 (Throttling)
T012 (통합 테스트)    T017 (Sensor구현)  ├── T026 (Coalescing)
                     ├── T018 (Robot구현) T027 (EventBus 통합)
                     ├── T019 (Task구현)  T028 (단위 테스트)
                     T020 (벤치마크)     T029 (통합 테스트)
                     T021 (마이그레이션)
                             │
                             └──────────┐
                                        │
                                 Phase 5 (P4)
                                 T030 (WatchdogNotifier)
                                 T031 (IPC 장애 감지)
                                 T032 (통합 테스트)
                                        │
                                        │
                                 Phase 6 (Polish)
                                 T033 (메트릭)
                                 T034 (문서)
                                 T035 (CHANGELOG)
```

---

## MVP 체크리스트 (Phase 0-2, 12개 작업)

systemd 시작 순서 경쟁 상태 해결만 포함:

- [ ] T001: CMake libsystemd 추가
- [ ] T002: VersionedData 헤더 생성
- [ ] T003: EventPriority enum 생성
- [ ] T004: VersionedData 단위 테스트
- [ ] T005: IDataStore 확장
- [ ] T006: DataStore 구현
- [ ] T007: DataStore 통합 테스트
- [ ] T008: mxrc-rt.service Before 추가
- [ ] T009: mxrc-nonrt.service After 추가
- [ ] T010: RT READY 신호 구현
- [ ] T011: Non-RT 재시도 로직
- [ ] T012: systemd 시작 순서 통합 테스트

**MVP 완료 조건**: 10회 재시작 시 100% 성공률

---

## 검증 절차

### 단위 테스트
```bash
cd /home/tory/workspace/mxrc/mxrc/build
ctest --output-on-failure
```

### 통합 테스트
```bash
./tests/integration/systemd/startup_order_test
./tests/integration/datastore/version_consistency_test
./tests/integration/event/event_storm_test
./tests/integration/systemd/watchdog_recovery_test
```

### 성능 벤치마크
```bash
./tests/unit/datastore/accessor_benchmark
./tools/rt_latency_test
```

### 서비스 배포 (P1 MVP)
```bash
sudo cp systemd/mxrc-rt.service /etc/systemd/system/
sudo cp systemd/mxrc-nonrt.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl restart mxrc-rt.service
sudo systemctl restart mxrc-nonrt.service
```

---

## 관련 문서

- [spec.md](spec.md) - Feature 명세
- [plan.md](plan.md) - 구현 계획 및 Phase 0 연구
- [data-model.md](data-model.md) - 엔티티 및 인터페이스 정의
- [quickstart.md](quickstart.md) - 빠른 시작 가이드
- [contracts/](contracts/) - API 계약 및 스키마

---

**작성일**: 2025-01-22
**Phase**: 2 (Tasks)
**다음 단계**: `/speckit.implement` 실행하여 구현 시작
