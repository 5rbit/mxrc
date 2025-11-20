# 구현 작업 목록: RT/Non-RT 이중 프로세스 아키텍처

**기능**: 021-rt-nonrt-architecture
**생성일**: 2025-11-20
**총 예상 기간**: 6.5주

---

## Phase 1: RT Executive 기반 구조 (1주)

### TASK-001: RTExecutive 클래스 뼈대 생성
**예상 시간**: 4시간
**우선순위**: P1
**의존성**: 없음

**설명**:
Cyclic Executive의 기본 구조를 구현합니다.

**체크리스트**:
- [ ] src/core/rt/RTExecutive.h 생성
  - [ ] 클래스 선언, MINOR_CYCLE_MS, MAJOR_CYCLE_MS 상수
  - [ ] run(), executeSlot(), waitNextCycle() 메서드 선언
- [ ] src/core/rt/RTExecutive.cpp 생성
  - [ ] 생성자/소멸자 구현
  - [ ] while(running_) 기본 루프 구현
- [ ] CMakeLists.txt에 RTExecutive 파일 추가
- [ ] 빌드 성공 확인

**완료 기준**:
- 컴파일 에러 없음
- 빈 루프가 실행되고 정상 종료됨

---

### TASK-002: SCHED_FIFO 우선순위 및 CPU 고정
**예상 시간**: 3시간
**우선순위**: P1
**의존성**: TASK-001

**설명**:
RT 스레드에 최고 우선순위를 부여하고 전용 CPU 코어에 고정합니다.

**체크리스트**:
- [ ] src/core/rt/util/TimeUtils.h 생성
  - [ ] setPriority(SCHED_FIFO, priority) 함수
  - [ ] pinToCPU(core_id) 함수
- [ ] src/core/rt/util/TimeUtils.cpp 구현
  - [ ] sched_setscheduler() 사용
  - [ ] pthread_setaffinity_np() 사용
  - [ ] mlockall(MCL_CURRENT | MCL_FUTURE) 추가
- [ ] RTExecutive::run()에서 호출
  - [ ] setPriority(SCHED_FIFO, 90)
  - [ ] pinToCPU(1)
- [ ] 권한 체크 (CAP_SYS_NICE)

**완료 기준**:
- `chrt -p <pid>`로 SCHED_FIFO 확인
- `taskset -cp <pid>`로 CPU 1번 고정 확인
- 일반 사용자 실행 시 권한 에러 메시지 출력

---

### TASK-003: clock_nanosleep 절대 시간 대기
**예상 시간**: 5시간
**우선순위**: P1
**의존성**: TASK-001

**설명**:
Drift 없는 정확한 주기를 위해 절대 시간 기준 대기를 구현합니다.

**체크리스트**:
- [ ] TimeUtils.h에 getMonotonicTimeNs() 추가
  - [ ] clock_gettime(CLOCK_MONOTONIC, &ts) 사용
  - [ ] uint64_t nanoseconds 반환
- [ ] TimeUtils.h에 waitUntilNextCycle() 추가
  - [ ] uint64_t cycle_start_ns, uint64_t cycle_duration_ns 인자
  - [ ] uint64_t next_wakeup_ns = cycle_start_ns + cycle_duration_ns
  - [ ] clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, ...)
- [ ] RTExecutive::run() 루프에 통합
  ```cpp
  while(running_) {
      uint64_t cycle_start = getMonotonicTimeNs();
      executeSlot(current_slot_);
      current_slot_ = (current_slot_ + 1) % num_slots_;
      waitUntilNextCycle(cycle_start, minor_cycle_ms_ * 1'000'000ULL);
  }
  ```
- [ ] Drift 누적 방지 확인

**완료 기준**:
- 1000번 사이클 후 총 시간이 1000ms ± 1ms
- sleep_for() 대비 drift 10배 감소

---

### TASK-004: ScheduleCalculator GCD/LCM 구현
**예상 시간**: 3시간
**우선순위**: P2
**의존성**: 없음

**설명**:
주기 배열에서 Minor/Major Cycle을 자동 계산합니다.

**체크리스트**:
- [ ] src/core/rt/util/ScheduleCalculator.h 생성
  - [ ] struct ScheduleParams { minor, major, num_slots }
  - [ ] static ScheduleParams calculate(vector<uint32_t> periods)
- [ ] src/core/rt/util/ScheduleCalculator.cpp 구현
  - [ ] std::gcd() 사용 (C++17)
  - [ ] std::lcm() 사용 (C++17)
  - [ ] MAX_MAJOR_CYCLE_MS = 1000 체크
- [ ] tests/unit/rt/ScheduleCalculator_test.cpp 작성
  - [ ] [1, 5, 10] → minor=1, major=10
  - [ ] [2, 4, 8] → minor=2, major=8
  - [ ] [3, 7, 11] → LCM=231 확인

**완료 기준**:
- 모든 테스트 통과
- 엣지 케이스 (빈 배열, 단일 값) 처리

---

### TASK-005: Jitter 측정 테스트
**예상 시간**: 4시간
**우선순위**: P1
**의존성**: TASK-002, TASK-003

**설명**:
1ms 주기의 정확도를 측정하고 jitter 히스토그램을 생성합니다.

**체크리스트**:
- [ ] tests/unit/rt/RTExecutive_jitter_test.cpp 생성
- [ ] 100,000번 사이클 실행
- [ ] 각 사이클의 시작 시간 기록
- [ ] Delta 계산 (actual - expected)
- [ ] 히스토그램 생성 (±1μs, ±5μs, ±10μs, ±50μs, >50μs)
- [ ] 99.9 퍼센타일 계산
- [ ] 결과를 CSV로 저장 (후처리용)

**완료 기준**:
- 99.9% 사이클이 ±10μs 이내
- 평균 jitter < 2μs
- 최대 jitter < 50μs

---

## Phase 2: RTDataStore (1주)

### TASK-006: RTDataStore 기본 구조
**예상 시간**: 5시간
**우선순위**: P1
**의존성**: 없음

**설명**:
고정 크기 배열 기반 데이터 저장소를 구현합니다.

**체크리스트**:
- [ ] src/core/rt/RTDataStore.h 생성
  - [ ] enum class Key : uint16_t { SENSOR_POSITION_X = 0, ..., MAX_KEYS = 512 }
  - [ ] union RTValue { int32_t i32; float f32; double f64; uint64_t u64; char str[32]; }
  - [ ] struct RTData { RTValue value; uint64_t timestamp_ns; uint32_t sequence; uint8_t valid; }
  - [ ] std::array<RTData, 512> data_
- [ ] src/core/rt/RTDataStore.cpp 생성
  - [ ] 생성자에서 data_.fill({0}) 초기화
  - [ ] 메모리 레이아웃 확인 (alignas(64))
- [ ] CMakeLists.txt 추가

**완료 기준**:
- sizeof(RTDataStore) = 약 32KB
- 정렬 확인 (캐시 라인)

---

### TASK-007: 타입별 set/get 메서드
**예상 시간**: 6시간
**우선순위**: P1
**의존성**: TASK-006

**설명**:
각 데이터 타입에 대한 읽기/쓰기 메서드를 구현합니다.

**체크리스트**:
- [ ] set_i32(Key key, int32_t value) → int
  - [ ] data_[key].value.i32 = value
  - [ ] data_[key].timestamp_ns = getMonotonicTimeNs()
  - [ ] data_[key].sequence.fetch_add(1)
  - [ ] data_[key].valid = 1
  - [ ] return 0 (성공)
- [ ] get_i32(Key key, int32_t* out_value) → int
  - [ ] if (!data_[key].valid) return -1
  - [ ] *out_value = data_[key].value.i32
  - [ ] return 0
- [ ] set_f32(), get_f32() 동일 패턴
- [ ] set_f64(), get_f64() 동일 패턴
- [ ] set_u64(), get_u64() 동일 패턴
- [ ] set_str(), get_str() (strncpy 사용)

**완료 기준**:
- 모든 타입 읽기/쓰기 성공
- 에러 코드 반환 (예외 없음)
- 컴파일러 최적화 -O2에서도 정상 동작

---

### TASK-008: Atomic 연산 및 Sequence Number
**예상 시간**: 4시간
**우선순위**: P1
**의존성**: TASK-007

**설명**:
동시 접근 시 데이터 일관성을 보장합니다.

**체크리스트**:
- [ ] RTData.sequence를 std::atomic<uint32_t>로 변경
- [ ] Write 시 sequence 증가
  ```cpp
  uint32_t seq = data_[key].sequence.fetch_add(1, std::memory_order_release);
  ```
- [ ] Read 시 sequence 일관성 체크
  ```cpp
  uint32_t seq1 = data_[key].sequence.load(std::memory_order_acquire);
  RTValue val = data_[key].value;
  uint32_t seq2 = data_[key].sequence.load(std::memory_order_acquire);
  if (seq1 != seq2) { retry; }
  ```
- [ ] tests/unit/rt/RTDataStore_concurrent_test.cpp
  - [ ] 2개 스레드 동시 읽기/쓰기
  - [ ] Torn read 감지

**완료 기준**:
- 10만 회 동시 접근에서 torn read 0건
- ThreadSanitizer 경고 없음

---

### TASK-009: is_fresh() 메서드
**예상 시간**: 2시간
**우선순위**: P2
**의존성**: TASK-007

**설명**:
데이터가 최신인지 타임스탬프로 확인합니다.

**체크리스트**:
- [ ] bool is_fresh(Key key, uint64_t max_age_ns) 추가
  ```cpp
  if (!data_[key].valid) return false;
  uint64_t age = getMonotonicTimeNs() - data_[key].timestamp_ns;
  return age <= max_age_ns;
  ```
- [ ] 테스트 케이스
  - [ ] 1ms 전 데이터 → is_fresh(key, 2ms) == true
  - [ ] 5ms 전 데이터 → is_fresh(key, 2ms) == false

**완료 기준**:
- 타임스탬프 기반 판단 정확
- RT 루프에서 오래된 데이터 감지 가능

---

### TASK-010: RTDataStore 단위 테스트
**예상 시간**: 5시간
**우선순위**: P1
**의존성**: TASK-006~009

**설명**:
모든 RTDataStore 기능을 검증합니다.

**체크리스트**:
- [ ] tests/unit/rt/RTDataStore_test.cpp
- [ ] CRUD 테스트 (각 타입)
- [ ] 키 범위 테스트 (0~511 유효, 512 무효)
- [ ] 타임스탬프 테스트
- [ ] Sequence number 증가 확인
- [ ] is_fresh() 테스트
- [ ] 성능 테스트 (읽기/쓰기 < 1μs)

**완료 기준**:
- 모든 테스트 통과 (GTest)
- WCET < 1μs (100만 회 평균)

---

## Phase 3: 공유 메모리 통신 (5일)

### TASK-011: SharedMemoryRegion 구조 정의
**예상 시간**: 3시간
**우선순위**: P1
**의존성**: 없음

**설명**:
RT/Non-RT 간 공유 메모리 레이아웃을 정의합니다.

**체크리스트**:
- [ ] src/core/rt/ipc/SharedMemoryRegion.h 생성
- [ ] struct SharedMemoryRegion 정의
  ```cpp
  struct {
      // RT → Non-RT (10ms)
      struct {
          int32_t robot_mode;
          float position_x, position_y, velocity;
          uint64_t timestamp_ns;
          uint32_t sequence;
      } rt_to_nonrt;

      // Non-RT → RT (100ms)
      struct {
          float max_velocity;
          float pid_kp, pid_ki, pid_kd;
          uint64_t timestamp_ns;
          uint32_t sequence;
      } nonrt_to_rt;

      std::atomic<uint64_t> rt_heartbeat_ns;
      std::atomic<uint64_t> nonrt_heartbeat_ns;
  };
  ```
- [ ] 정렬 및 패딩 확인

**완료 기준**:
- sizeof(SharedMemoryRegion) < 1KB
- 캐시 라인 정렬 (alignas(64))

---

### TASK-012: 공유 메모리 생성 및 매핑
**예상 시간**: 4시간
**우선순위**: P1
**의존성**: TASK-011

**설명**:
POSIX 공유 메모리를 생성하고 양쪽 프로세스에서 매핑합니다.

**체크리스트**:
- [ ] src/core/rt/ipc/SharedMemoryManager.{h,cpp} 생성
- [ ] createSharedMemory(const char* name) 구현
  ```cpp
  int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
  ftruncate(fd, sizeof(SharedMemoryRegion));
  void* addr = mmap(NULL, sizeof(SharedMemoryRegion),
                     PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  return reinterpret_cast<SharedMemoryRegion*>(addr);
  ```
- [ ] attachSharedMemory(const char* name) 구현 (client)
- [ ] detachSharedMemory() 구현 (munmap)
- [ ] 에러 처리 (권한, 이미 존재)

**완료 기준**:
- RT 프로세스에서 생성, Non-RT에서 연결 성공
- 양쪽에서 읽기/쓰기 가능
- 프로세스 종료 후 shm_unlink

---

### TASK-013: DataSynchronizer 구현
**예상 시간**: 6시간
**우선순위**: P1
**의존성**: TASK-008, TASK-012

**설명**:
RTDataStore와 공유 메모리 간 양방향 동기화를 구현합니다.

**체크리스트**:
- [ ] src/core/rt/ipc/DataSynchronizer.{h,cpp} 생성
- [ ] syncFromRTDataStore(RTDataStore* rt_ds, SharedMemoryRegion* shm)
  ```cpp
  // RTDataStore → Shared Memory
  shm->rt_to_nonrt.sequence++;
  rt_ds->get_i32(Key::ROBOT_MODE, &shm->rt_to_nonrt.robot_mode);
  rt_ds->get_f32(Key::POSITION_X, &shm->rt_to_nonrt.position_x);
  // ...
  shm->rt_to_nonrt.timestamp_ns = getMonotonicTimeNs();
  ```
- [ ] applyNonRTParams(RTDataStore* rt_ds, SharedMemoryRegion* shm)
  ```cpp
  // Shared Memory → RTDataStore
  uint32_t seq1 = shm->nonrt_to_rt.sequence;
  float max_vel = shm->nonrt_to_rt.max_velocity;
  uint32_t seq2 = shm->nonrt_to_rt.sequence;
  if (seq1 == seq2) {
      rt_ds->set_f32(Key::PARAM_MAX_VELOCITY, max_vel);
  }
  ```
- [ ] applyRTStatus (Non-RT쪽, DataStore에 쓰기)
- [ ] 테스트: RT → Shared → Non-RT 전체 경로

**완료 기준**:
- 데이터 왕복 성공
- Sequence number로 torn read 방지 확인

---

### TASK-014: 주기적 동기화 통합
**예상 시간**: 4시간
**우선순위**: P1
**의존성**: TASK-001, TASK-013

**설명**:
RT Executive에 10ms/100ms 주기 동기화 Action을 등록합니다.

**체크리스트**:
- [ ] RTExecutive에 SharedMemoryRegion* 멤버 추가
- [ ] registerAction(name, period_ms, callback) 메서드 구현
- [ ] rt_main.cpp 에서 동기화 Action 등록
  ```cpp
  rtExec.registerAction("sync_to_nonrt", 10, [&](RTContext& ctx) {
      DataSynchronizer::syncFromRTDataStore(ctx.datastore, ctx.shm);
  });

  rtExec.registerAction("sync_from_nonrt", 100, [&](RTContext& ctx) {
      DataSynchronizer::applyNonRTParams(ctx.datastore, ctx.shm);
  });
  ```
- [ ] Heartbeat 갱신 (1ms마다)
  ```cpp
  shm->rt_heartbeat_ns.store(getMonotonicTimeNs());
  ```

**완료 기준**:
- 10ms마다 RT → Non-RT 데이터 갱신
- 100ms마다 Non-RT → RT 파라미터 반영

---

### TASK-015: 통합 IPC 테스트
**예상 시간**: 6시간
**우선순위**: P1
**의존성**: TASK-012~014

**설명**:
2개 프로세스를 동시 실행하여 IPC를 검증합니다.

**체크리스트**:
- [ ] tests/integration/rt/ipc_sync_test.cpp
- [ ] RT 프로세스: position_x = 10.5 기록
- [ ] Non-RT 프로세스: 20ms 후 읽기
- [ ] 지연 시간 측정 (< 15ms)
- [ ] 역방향 테스트 (Non-RT → RT)
- [ ] Heartbeat 실패 시나리오
  - [ ] Non-RT kill -9
  - [ ] RT에서 500ms 후 감지 확인
- [ ] 장기 안정성 (1000회 동기화)

**완료 기준**:
- 양방향 동기화 성공률 100%
- RT → Non-RT 지연 < 15ms
- Non-RT → RT 지연 < 120ms
- Heartbeat 실패 감지 < 500ms

---

## Phase 4: 동적 주기 설정 (1주)

### TASK-016: JSON 설정 파일 구조 정의
**예상 시간**: 2시간
**우선순위**: P2
**의존성**: 없음

**설명**:
주기 설정을 위한 JSON 스키마를 정의합니다.

**체크리스트**:
- [ ] config/rt_schedule.json 생성
  ```json
  {
    "periods_ms": [1, 5, 10, 20, 50, 100],
    "actions": [
      {
        "name": "sensor_read",
        "period_ms": 1,
        "wcet_us": 50,
        "priority": "HIGH"
      },
      ...
    ]
  }
  ```
- [ ] 주석으로 설명 추가
- [ ] 예시 설정 (manipulator, mobile_robot)

**완료 기준**:
- JSON 파싱 가능 (nlohmann_json)
- 스키마 문서화

---

### TASK-017: schedule_generator 도구 구현
**예상 시간**: 8시간
**우선순위**: P2
**의존성**: TASK-004, TASK-016

**설명**:
JSON에서 C++ 헤더를 자동 생성하는 빌드 도구를 만듭니다.

**체크리스트**:
- [ ] tools/schedule_generator.cpp 생성
- [ ] main(argc, argv) 파라미터: config.json, output.h
- [ ] JSON 파싱 (nlohmann_json)
- [ ] ScheduleCalculator::calculate() 호출
- [ ] C++ 헤더 생성
  ```cpp
  out << "constexpr uint32_t MINOR_CYCLE_MS = " << minor << ";\n";
  out << "constexpr uint32_t MAJOR_CYCLE_MS = " << major << ";\n";
  out << "constexpr ActionSchedule ACTIONS[] = {\n";
  // ...
  ```
- [ ] 에러 처리 (파일 없음, JSON 오류)

**완료 기준**:
- `./schedule_generator config.json out.h` 실행 성공
- 생성된 헤더가 컴파일됨
- 잘못된 JSON 시 에러 메시지

---

### TASK-018: validate_schedule 도구 구현
**예상 시간**: 6시간
**우선순위**: P2
**의존성**: TASK-004, TASK-016

**설명**:
스케줄 유효성을 검증하는 도구를 만듭니다.

**체크리스트**:
- [ ] tools/validate_schedule.cpp 생성
- [ ] CPU 사용률 계산
  ```cpp
  double utilization = 0.0;
  for (action : actions) {
      utilization += (action.wcet_us / 1000.0) / action.period_ms;
  }
  ```
- [ ] 70% 초과 시 에러 반환 (exit code 1)
- [ ] LCM > 1000ms 시 경고
- [ ] WCET > Period 시 에러
- [ ] 출력 포맷
  ```
  ✅ Schedule is VALID
     CPU Utilization: 65%
  ```

**완료 기준**:
- CPU 70% 초과 설정 → exit 1
- 정상 설정 → exit 0
- 결과 출력 명확

---

### TASK-019: CMake 빌드 통합
**예상 시간**: 4시간
**우선순위**: P2
**의존성**: TASK-017, TASK-018

**설명**:
빌드 시스템에 자동 생성을 통합합니다.

**체크리스트**:
- [ ] CMakeLists.txt 수정
  ```cmake
  # 도구 빌드
  add_executable(schedule_generator tools/schedule_generator.cpp)
  target_link_libraries(schedule_generator PRIVATE nlohmann_json::nlohmann_json)

  add_executable(validate_schedule tools/validate_schedule.cpp)

  # 스케줄 생성
  add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR}/generated/RTSchedule.h
      COMMAND validate_schedule ${CMAKE_SOURCE_DIR}/config/rt_schedule.json
      COMMAND schedule_generator
              ${CMAKE_SOURCE_DIR}/config/rt_schedule.json
              ${CMAKE_BINARY_DIR}/generated/RTSchedule.h
      DEPENDS config/rt_schedule.json
      COMMENT "Generating RT schedule..."
  )

  add_custom_target(generate_schedule
      DEPENDS ${CMAKE_BINARY_DIR}/generated/RTSchedule.h)
  add_dependencies(mxrc generate_schedule)
  ```
- [ ] generated/ 디렉토리 .gitignore 추가
- [ ] 빌드 순서 확인

**완료 기준**:
- `cmake .. && make` 시 RTSchedule.h 자동 생성
- config 수정 시 재빌드 트리거
- 검증 실패 시 빌드 중단

---

### TASK-020: 런타임 동적 로드 옵션
**예상 시간**: 5시간
**우선순위**: P3
**의존성**: TASK-017

**설명**:
선택적으로 런타임에 JSON을 로드할 수 있도록 합니다.

**체크리스트**:
- [ ] src/core/rt/DynamicRTExecutive.{h,cpp} 생성
- [ ] loadScheduleFromConfig(const char* path) 메서드
  - [ ] JSON 파싱
  - [ ] GCD/LCM 계산
  - [ ] minor_cycle_ms_, major_cycle_ms_ 설정
  - [ ] schedule_table_.resize(num_slots_)
- [ ] registerAction() 동적 등록
  - [ ] 주기가 minor의 배수인지 체크
  - [ ] 슬롯 테이블에 추가
- [ ] 초기화 단계에서만 호출 (RT 시작 전)

**완료 기준**:
- 재빌드 없이 JSON 수정 후 실행
- 잘못된 주기 설정 시 에러 메시지

---

## Phase 5: 상태 머신 (5일)

### TASK-021: RTStateMachine 기본 구조
**예상 시간**: 4시간
**우선순위**: P1
**의존성**: 없음

**설명**:
6개 상태와 전환 로직을 구현합니다.

**체크리스트**:
- [ ] src/core/rt/RTStateMachine.h 생성
  - [ ] enum class State { IDLE, INITIALIZING, READY, RUNNING, ERROR, SAFE_MODE }
  - [ ] State current_state_ = State::IDLE
  - [ ] void transition(State new_state)
  - [ ] State getState() const
- [ ] src/core/rt/RTStateMachine.cpp 생성
  - [ ] transition() 구현
    ```cpp
    if (current_state_ == new_state) return;
    Logger::info("State transition: {} → {}", current_state_, new_state);
    callExitAction(current_state_);
    current_state_ = new_state;
    callEntryAction(new_state);
    ```
- [ ] 상태 전환 테이블 (허용/불허 체크)

**완료 기준**:
- 상태 전환 로깅 확인
- 불법 전환 차단 (테스트)

---

### TASK-022: Entry/Cyclic/Exit Action 등록
**예상 시간**: 5시간
**우선순위**: P1
**의존성**: TASK-021

**설명**:
각 상태의 진입/반복/탈출 콜백을 등록합니다.

**체크리스트**:
- [ ] using ActionCallback = std::function<void(RTContext&)>
- [ ] std::map<State, ActionCallback> entry_actions_
- [ ] std::map<State, ActionCallback> cyclic_actions_
- [ ] std::map<State, ActionCallback> exit_actions_
- [ ] registerEntryAction(State, callback)
- [ ] registerCyclicAction(State, callback)
- [ ] registerExitAction(State, callback)
- [ ] tick() 메서드 (매 사이클 호출)
  ```cpp
  void tick(RTContext& ctx) {
      if (cyclic_actions_.count(current_state_)) {
          cyclic_actions_[current_state_](ctx);
      }
  }
  ```

**완료 기준**:
- Entry action: 상태 진입 시 1회 실행
- Cyclic action: 해당 상태일 때 매 사이클 실행
- Exit action: 상태 벗어날 때 1회 실행

---

### TASK-023: Guard Condition 구현
**예상 시간**: 4시간
**우선순위**: P2
**의존성**: TASK-021

**설명**:
Action 실행 전 상태 기반 필터링을 구현합니다.

**체크리스트**:
- [ ] using GuardCondition = std::function<bool(const RTStateMachine&)>
- [ ] struct RTActionSlot에 GuardCondition guard_ 추가
- [ ] RTExecutive::executeSlot() 수정
  ```cpp
  for (auto& slot : schedule_[current_slot_]) {
      if (slot.guard_ && !slot.guard_(state_machine_)) {
          continue;  // Guard 실패, 실행 안 함
      }
      slot.callback(context_);
  }
  ```
- [ ] 예시: 모터 구동은 RUNNING 상태에서만
  ```cpp
  rtExec.registerAction("motor_control", 1, motorCallback,
      [](const RTStateMachine& sm) {
          return sm.getState() == State::RUNNING;
      });
  ```

**완료 기준**:
- INITIALIZING에서 모터 Action 차단 확인
- RUNNING에서 모터 Action 실행 확인

---

### TASK-024: Heartbeat 실패 시 SAFE_MODE 전환
**예상 시간**: 3시간
**우선순위**: P1
**의존성**: TASK-021, TASK-014

**설명**:
Non-RT heartbeat 끊김 시 자동으로 안전 모드로 전환합니다.

**체크리스트**:
- [ ] RTExecutive에 checkHeartbeat() 메서드 추가
  ```cpp
  void checkHeartbeat(SharedMemoryRegion* shm, RTStateMachine* sm) {
      uint64_t nonrt_hb = shm->nonrt_heartbeat_ns.load();
      uint64_t now = getMonotonicTimeNs();
      if (now - nonrt_hb > 500'000'000ULL) {  // 500ms
          Logger::warn("Non-RT heartbeat lost, entering SAFE_MODE");
          sm->transition(State::SAFE_MODE);
      }
  }
  ```
- [ ] 100ms 주기 Action으로 등록
- [ ] SAFE_MODE entry action: 모터 정지

**완료 기준**:
- Non-RT kill -9 후 500ms 이내 SAFE_MODE 진입
- 모터 정지 확인

---

### TASK-025: 상태 머신 단위 테스트
**예상 시간**: 4시간
**우선순위**: P1
**의존성**: TASK-021~024

**설명**:
모든 상태 전환과 Action을 검증합니다.

**체크리스트**:
- [ ] tests/unit/rt/RTStateMachine_test.cpp
- [ ] 전환 시나리오
  - [ ] IDLE → INITIALIZING → READY → RUNNING
  - [ ] RUNNING → ERROR
  - [ ] ERROR → IDLE (clear_error)
  - [ ] RUNNING → SAFE_MODE (heartbeat 실패)
- [ ] Entry/Exit action 호출 확인
- [ ] Cyclic action 반복 실행 확인
- [ ] Guard condition 차단 확인
- [ ] 불법 전환 차단

**완료 기준**:
- 모든 테스트 통과
- 상태 전환 시간 < 100ms

---

## Phase 6: Non-RT 프로세스 통합 (1주)

### TASK-026: NonRTExecutive 구현
**예상 시간**: 6시간
**우선순위**: P1
**의존성**: TASK-013

**설명**:
기존 TaskExecutor를 래핑하는 Non-RT 프로세스를 만듭니다.

**체크리스트**:
- [ ] src/nonrt_main.cpp 생성
- [ ] NonRTExecutive 클래스 (또는 직접 main)
  - [ ] TaskExecutor 인스턴스 생성
  - [ ] ActionExecutor, SequenceEngine 연동
  - [ ] EventBus 연동
- [ ] 공유 메모리 연결
  ```cpp
  auto* shm = SharedMemoryManager::attach("/mxrc_shm");
  ```
- [ ] 주기적 heartbeat 갱신 (100ms)
  ```cpp
  shm->nonrt_heartbeat_ns.store(getMonotonicTimeNs());
  ```
- [ ] RT 상태 읽기 및 DataStore 반영
  ```cpp
  DataSynchronizer::applyRTStatus(datastore, shm);
  ```

**완료 기준**:
- Non-RT 프로세스 단독 실행 가능
- 공유 메모리 연결 성공
- Heartbeat 주기적 갱신 확인

---

### TASK-027: Non-RT 데이터 동기화
**예상 시간**: 4시간
**우선순위**: P1
**의존성**: TASK-026

**설명**:
Non-RT 쪽에서 DataStore와 공유 메모리를 동기화합니다.

**체크리스트**:
- [ ] src/core/rt/ipc/DataSynchronizer.cpp에 Non-RT 함수 추가
- [ ] syncToSharedMemory(DataStore* ds, SharedMemoryRegion* shm)
  ```cpp
  // DataStore → Shared Memory (Non-RT → RT)
  shm->nonrt_to_rt.sequence++;
  shm->nonrt_to_rt.max_velocity = ds->get<float>("max_velocity");
  // ...
  shm->nonrt_to_rt.timestamp_ns = getMonotonicTimeNs();
  ```
- [ ] 주기적 실행 (100ms 타이머)
- [ ] RT 상태 읽기
  ```cpp
  position_x = shm->rt_to_nonrt.position_x;
  ds->set("rt_position_x", position_x);
  ```

**완료 기준**:
- Non-RT에서 설정한 파라미터가 RT에 반영
- RT 센서 데이터가 Non-RT DataStore에 나타남

---

### TASK-028: EventBus 통합
**예상 시간**: 4시간
**우선순위**: P2
**의존성**: TASK-026

**설명**:
RT 상태 변경을 EventBus로 발행하여 구독자에게 알립니다.

**체크리스트**:
- [ ] RT 상태 변경 시 이벤트 발행
  - [ ] "rt.state.changed" 이벤트
  - [ ] Payload: { state: "RUNNING", timestamp: ... }
- [ ] Non-RT에서 구독
  ```cpp
  eventBus->subscribe("rt.state.changed", [](const Event& e) {
      Logger::info("RT state: {}", e.payload["state"]);
  });
  ```
- [ ] 센서 데이터 변경 이벤트
  - [ ] "rt.sensor.updated"

**완료 기준**:
- RT 상태 전환 시 Non-RT에서 이벤트 수신
- 로깅에 상태 변경 기록

---

### TASK-029: 기존 테스트 검증
**예상 시간**: 6시간
**우선순위**: P1
**의존성**: TASK-026~028

**설명**:
195개 기존 단위 테스트가 여전히 통과하는지 확인합니다.

**체크리스트**:
- [ ] `./run_tests` 실행
- [ ] TaskExecutor 테스트 (Non-RT 영역)
- [ ] ActionExecutor 테스트
- [ ] SequenceEngine 테스트
- [ ] DataStore 테스트 (66개)
- [ ] 실패 시 디버깅 및 수정
  - [ ] Non-RT 분리로 인한 인터페이스 변경 확인
  - [ ] 타이밍 의존성 제거

**완료 기준**:
- 195/195 테스트 통과
- 새로운 경고/에러 없음

---

### TASK-030: systemd 서비스 설정
**예상 시간**: 4시간
**우선순위**: P2
**의존성**: TASK-026

**설명**:
두 프로세스를 systemd 서비스로 관리합니다.

**체크리스트**:
- [ ] systemd/rt-executive.service 생성
  ```ini
  [Unit]
  Description=MXRC RT Executive
  After=network.target

  [Service]
  Type=simple
  ExecStart=/usr/local/bin/rt-executive
  Restart=no  # RT는 재시작 안 함

  [Install]
  WantedBy=multi-user.target
  ```
- [ ] systemd/nonrt-executive.service 생성
  ```ini
  [Service]
  ExecStart=/usr/local/bin/nonrt-executive
  Restart=always  # Non-RT는 자동 재시작
  RestartSec=2s
  ```
- [ ] 설치 스크립트
  ```bash
  sudo cp systemd/*.service /etc/systemd/system/
  sudo systemctl daemon-reload
  sudo systemctl enable rt-executive nonrt-executive
  ```

**완료 기준**:
- `systemctl start rt-executive` 성공
- `systemctl start nonrt-executive` 성공
- Non-RT kill 시 자동 재시작

---

## Phase 7: 통합 테스트 및 최적화 (1주)

### TASK-031: 전체 시스템 통합 테스트
**예상 시간**: 8시간
**우선순위**: P1
**의존성**: 모든 이전 태스크

**설명**:
RT/Non-RT 프로세스를 동시 실행하여 모든 기능을 검증합니다.

**체크리스트**:
- [ ] tests/integration/rt/full_system_test.cpp
- [ ] 시나리오 1: 정상 동작
  - [ ] RT/Non-RT 동시 시작
  - [ ] 센서 데이터 RT → Non-RT 확인
  - [ ] 파라미터 Non-RT → RT 확인
  - [ ] 상태 전환 (IDLE → RUNNING)
  - [ ] 1000회 사이클 안정성
- [ ] 시나리오 2: Non-RT 크래시
  - [ ] kill -9 nonrt-executive
  - [ ] RT 지속 동작 확인
  - [ ] systemd 재시작 확인
  - [ ] 재연결 후 동기화 복구
- [ ] 시나리오 3: Heartbeat 실패
  - [ ] Non-RT 일시정지 (kill -STOP)
  - [ ] RT SAFE_MODE 진입 확인
  - [ ] 재개 후 복구 (kill -CONT)
- [ ] 시나리오 4: 주기 변경
  - [ ] JSON 수정 ([1, 5, 10] → [2, 4, 8])
  - [ ] 재빌드 및 실행
  - [ ] 새 주기 동작 확인

**완료 기준**:
- 모든 시나리오 통과
- 예상치 못한 크래시 없음

---

### TASK-032: 장기 안정성 테스트
**예상 시간**: 4시간 (+ 24시간 대기)
**우선순위**: P1
**의존성**: TASK-031

**설명**:
24시간 연속 실행으로 메모리 누수와 안정성을 검증합니다.

**체크리스트**:
- [ ] 테스트 스크립트 작성
  ```bash
  systemctl start rt-executive nonrt-executive
  sleep 86400  # 24시간
  systemctl status rt-executive  # 실행 중?
  ```
- [ ] 메모리 사용량 모니터링
  ```bash
  watch -n 60 'ps aux | grep executive'
  ```
- [ ] Jitter 로깅 (1시간마다 히스토그램)
- [ ] 에러 로그 확인
- [ ] Valgrind/AddressSanitizer 실행 (1시간)

**완료 기준**:
- 24시간 동안 크래시 0건
- 메모리 사용량 증가 < 1MB
- 99.9% jitter ±10μs 유지

---

### TASK-033: 성능 최적화
**예상 시간**: 6시간
**우선순위**: P2
**의존성**: TASK-031

**설명**:
Jitter와 CPU 사용률을 최적화합니다.

**체크리스트**:
- [ ] 커널 파라미터 튜닝
  - [ ] isolcpus=1 (RT 코어 격리)
  - [ ] nohz_full=1 (타이머 인터럽트 제거)
  - [ ] rcu_nocbs=1 (RCU 콜백 다른 코어)
- [ ] IRQ 친화성 설정
  ```bash
  echo 0 > /proc/irq/*/smp_affinity_list  # 모든 IRQ를 코어 0으로
  ```
- [ ] RT 우선순위 조정 (90 → 99)
- [ ] 캐시 워밍 (초기화 시 더미 사이클)
- [ ] 프로파일링 (perf record)
  - [ ] Hot path 식별
  - [ ] 불필요한 연산 제거

**완료 기준**:
- Jitter 99.9% ±10μs → ±5μs (목표)
- CPU 사용률 < 5% (RT), < 20% (Non-RT)

---

### TASK-034: 문서 작성
**예상 시간**: 8시간
**우선순위**: P2
**의존성**: TASK-031

**설명**:
사용자 가이드와 개발자 가이드를 작성합니다.

**체크리스트**:
- [ ] docs/user_guide_rt_nonrt.md
  - [ ] 시스템 요구사항 (커널, 라이브러리)
  - [ ] 설치 방법 (커널 패치, 빌드)
  - [ ] 설정 방법 (JSON, systemd)
  - [ ] 실행 및 모니터링
  - [ ] 트러블슈팅 (jitter 과도, heartbeat 실패)
- [ ] docs/developer_guide_rt_actions.md
  - [ ] RT Action 작성 규칙
    - [ ] No dynamic allocation
    - [ ] No exceptions
    - [ ] No blocking I/O
    - [ ] WCET 제약
  - [ ] 새 주기 추가 방법
  - [ ] 상태 머신 사용법
  - [ ] 디버깅 팁 (GDB, perf)
- [ ] API 문서 (Doxygen)

**완료 기준**:
- 사용자가 가이드만 보고 시스템 실행 가능
- 개발자가 새 Action 추가 가능
- 문서 완성도 90% 이상

---

### TASK-035: 최종 검토 및 릴리스
**예상 시간**: 4시간
**우선순위**: P1
**의존성**: 모든 태스크

**설명**:
모든 성공 기준을 확인하고 릴리스를 준비합니다.

**체크리스트**:
- [ ] 성공 기준 체크리스트
  - [ ] SC-001: ±10μs jitter ✅
  - [ ] SC-002: Non-RT 크래시 후 RT 지속 ✅
  - [ ] SC-003~010: 모두 확인 ✅
- [ ] 테스트 결과 요약
  - [ ] 단위 테스트: X/X 통과
  - [ ] 통합 테스트: X/X 통과
  - [ ] 24시간 안정성: 통과
- [ ] 릴리스 노트 작성
  - [ ] 새 기능
  - [ ] 변경 사항
  - [ ] 알려진 제약사항
- [ ] Git 태그 생성 (v1.0.0-rt)

**완료 기준**:
- 모든 SC-001~010 통과
- 문서 완성
- 릴리스 노트 승인

---

## 의존성 그래프

```
Phase 1: RT Executive
TASK-001 (뼈대) ──┬──> TASK-002 (SCHED_FIFO) ──> TASK-005 (Jitter 테스트)
                  └──> TASK-003 (clock_nanosleep) ─┘
TASK-004 (GCD/LCM) ──────────────────────────────────> Phase 4

Phase 2: RTDataStore
TASK-006 (구조) ──> TASK-007 (set/get) ──> TASK-008 (Atomic) ──> TASK-010 (테스트)
                                       └──> TASK-009 (is_fresh) ─┘

Phase 3: IPC
TASK-011 (SharedMemory 구조) ──> TASK-012 (shm_open) ──> TASK-013 (Synchronizer) ──> TASK-014 (통합) ──> TASK-015 (테스트)

Phase 4: 동적 주기
TASK-016 (JSON 스키마) ──┬──> TASK-017 (generator) ──┬──> TASK-019 (CMake)
TASK-004 (ScheduleCalc) ─┤                            │
                         └──> TASK-018 (validator) ───┘
TASK-020 (런타임 로드) (독립적, 선택)

Phase 5: 상태 머신
TASK-021 (구조) ──┬──> TASK-022 (Entry/Exit) ──> TASK-025 (테스트)
                  ├──> TASK-023 (Guard) ────────┘
                  └──> TASK-024 (Heartbeat) ───┘

Phase 6: Non-RT
TASK-013 (Synchronizer) ──> TASK-026 (NonRTExecutive) ──┬──> TASK-027 (동기화) ──> TASK-029 (테스트)
                                                         ├──> TASK-028 (EventBus) ─┘
                                                         └──> TASK-030 (systemd)

Phase 7: 통합
모든 태스크 ──> TASK-031 (통합 테스트) ──┬──> TASK-032 (24시간)
                                         ├──> TASK-033 (최적화)
                                         ├──> TASK-034 (문서)
                                         └──> TASK-035 (릴리스)
```

---

## 진행률 추적

| Phase | Tasks | 완료 | 진행률 |
|-------|-------|------|--------|
| Phase 1 | TASK-001~005 | 0/5 | 0% |
| Phase 2 | TASK-006~010 | 0/5 | 0% |
| Phase 3 | TASK-011~015 | 0/5 | 0% |
| Phase 4 | TASK-016~020 | 0/5 | 0% |
| Phase 5 | TASK-021~025 | 0/5 | 0% |
| Phase 6 | TASK-026~030 | 0/5 | 0% |
| Phase 7 | TASK-031~035 | 0/5 | 0% |
| **총계** | **35 tasks** | **0/35** | **0%** |

---

## 리스크 및 완화 전략

각 태스크별 리스크는 plan.md의 "위험 요소 및 완화 전략" 섹션 참조.
추가 리스크 발생 시 이 섹션에 기록.
