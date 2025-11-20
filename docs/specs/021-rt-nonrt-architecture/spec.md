# 기능 사양서: RT/Non-RT 이중 프로세스 아키텍처

**기능 브랜치**: `021-rt-nonrt-architecture`
**생성일**: 2025-11-20
**상태**: 초안
**입력**: docs/research/003-realtime-architecture-analysis.md를 기반으로 RT/Non-RT 분리 아키텍처 구현

## 사용자 시나리오 및 테스트 *(필수)*

### 사용자 스토리 1 - 실시간 제어 루프 안정성 보장 (우선순위: P1)

로봇 엔지니어가 MXRC 시스템으로 로봇을 제어할 때, 센서 데이터 읽기와 모터 제어 명령이 정확히 1ms 주기로 실행되어야 하며, 이 주기가 흔들리지 않아야 합니다. Non-RT 영역(로깅, 모니터링)에서 문제가 발생하더라도 RT 제어 루프는 계속 동작하여 로봇이 안전하게 작동해야 합니다.

**이 우선순위인 이유**: 실시간 제어 루프의 안정성은 로봇의 안전과 직결됩니다. 제어 주기가 불안정하면 로봇이 떨리거나 위험한 동작을 할 수 있습니다. 이는 시스템의 핵심 가치이며, 다른 모든 기능의 전제 조건입니다.

**독립 테스트**: RT Executive만 단독으로 실행하여 1ms 주기의 안정성을 측정할 수 있습니다. Non-RT 프로세스 없이도 센서 읽기 → 제어 계산 → 액추에이터 출력 사이클이 완전히 동작하며, jitter 측정으로 실시간성을 검증할 수 있습니다.

**인수 시나리오**:

1. **주어진 상황** RT Executive가 1ms 주기로 설정되고 센서 읽기 Action이 등록됨, **언제** 100,000번의 사이클을 실행, **그러면** 99.9% 이상의 사이클이 ±10μs 이내의 jitter를 보임
2. **주어진 상황** RT와 Non-RT 프로세스가 모두 실행 중, **언제** Non-RT 프로세스가 크래시(segfault), **그러면** RT 프로세스는 중단 없이 계속 동작하며 마지막 파라미터 값으로 안전 모드 진입
3. **주어진 상황** RT Executive가 센서/제어 Action을 실행 중, **언제** Non-RT에서 로깅 시스템이 100% CPU 사용, **그러면** RT 사이클 시간에 영향 없음 (격리된 CPU 코어 사용)

---

### 사용자 스토리 2 - 유연한 주기 설정 (우선순위: P2)

로봇 타입에 따라 다른 제어 주기가 필요합니다. 정밀 매니퓰레이터는 1ms, 5ms, 10ms 주기가 필요하고, 모바일 로봇은 10ms, 20ms, 50ms 주기가 필요합니다. JSON 설정 파일을 수정하고 재빌드하면 원하는 주기 조합으로 시스템이 자동 설정되어야 합니다.

**이 우선순위인 이유**: P1(기본 RT 안정성)이 확보된 후, 다양한 로봇 타입을 지원하는 것이 시스템의 범용성을 높입니다. 하지만 단일 주기(1ms)만으로도 기본 동작은 가능하므로 P2입니다.

**독립 테스트**: `config/rt_schedule.json`에 `[1, 5, 10]` 주기를 정의하고 빌드하면, 생성된 `RTSchedule.h`가 Minor Cycle=1ms, Major Cycle=10ms로 설정됩니다. 각 Action이 지정된 주기(1ms, 5ms, 10ms)로 정확히 실행되는지 타임스탬프로 검증 가능합니다.

**인수 시나리오**:

1. **주어진 상황** `config/rt_schedule.json`에 `periods_ms: [1, 5, 10, 20]` 설정, **언제** CMake 빌드 실행, **그러면** `generated/RTSchedule.h`에 MINOR_CYCLE_MS=1, MAJOR_CYCLE_MS=20 생성
2. **주어진 상황** 1ms Action과 5ms Action이 등록됨, **언제** 100ms 동안 실행, **그러면** 1ms Action은 100회, 5ms Action은 20회 정확히 실행
3. **주어진 상황** 잘못된 주기 조합([3, 7, 11]) 설정, **언제** 빌드 시 검증 도구 실행, **그러면** CPU 사용률 초과 경고 또는 LCM 크기 경고 출력

---

### 사용자 스토리 3 - RT/Non-RT 데이터 동기화 (우선순위: P1)

RT 프로세스의 센서 데이터와 제어 상태를 Non-RT 프로세스에서 로깅하고 모니터링해야 합니다. 반대로 Non-RT에서 설정한 제어 파라미터(최대 속도, PID 게인)가 RT에 반영되어야 합니다. 동기화는 자동으로 주기적으로 이루어지며, 데이터 일관성이 보장되어야 합니다.

**이 우선순위인 이유**: RT 제어만 되고 데이터를 볼 수 없거나 파라미터를 조정할 수 없다면 실용성이 없습니다. 이는 P1 RT 안정성과 함께 시스템의 필수 기능입니다.

**독립 테스트**: RT 프로세스에서 RTDataStore에 센서 값을 쓰고, 20ms 후 Non-RT 프로세스의 DataStore에서 동일한 값을 읽을 수 있는지 확인합니다. 역방향도 동일하게 테스트합니다.

**인수 시나리오**:

1. **주어진 상황** RT에서 position_x=10.5를 RTDataStore에 기록, **언제** 10ms 동기화 주기 경과, **그러면** Non-RT DataStore에서 position_x=10.5 읽기 성공
2. **주어진 상황** Non-RT에서 max_velocity=5.0 설정, **언제** 100ms 동기화 주기 경과, **그러면** RT RTDataStore에서 max_velocity=5.0 적용되어 제어 로직에 반영
3. **주어진 상황** RT와 Non-RT가 동시에 같은 데이터를 읽기/쓰기, **언제** 동기화 실행, **그러면** Sequence number로 torn read 방지, 데이터 일관성 유지

---

### 사용자 스토리 4 - 상태 기반 안전 제어 (우선순위: P2)

로봇은 IDLE, INITIALIZING, READY, RUNNING, ERROR, SAFE_MODE 상태를 가집니다. 각 상태에서 허용되는 Action만 실행되어야 합니다. 예를 들어, INITIALIZING 상태에서는 모터 구동 Action이 차단되고, ERROR 상태에서는 비상 정지만 가능해야 합니다.

**이 우선순위인 이유**: 기본 RT 제어(P1)와 데이터 동기화(P1)가 있으면 로봇은 동작하지만, 상태 머신이 없으면 안전 관리가 어렵습니다. 이는 안전성 향상 기능이므로 P2입니다.

**독립 테스트**: RTStateMachine을 INITIALIZING 상태로 설정하고 모터 구동 Action 실행을 시도하면 guard condition에 의해 차단되는지 확인합니다. RUNNING 상태로 전환 후 동일 Action이 실행되는지 확인합니다.

**인수 시나리오**:

1. **주어진 상황** 상태가 INITIALIZING, **언제** 모터 구동 Action 실행 요청, **그러면** Guard condition 실패로 Action 실행되지 않음
2. **주어진 상황** 상태가 RUNNING, **언제** Non-RT heartbeat 500ms 이상 끊김 감지, **그러면** 자동으로 SAFE_MODE 전환 및 모터 정지
3. **주어진 상황** ERROR 상태, **언제** 사용자가 clear_error 명령 전송, **그러면** IDLE 상태로 전환 및 재초기화 가능

---

### 엣지 케이스

- RT 프로세스 시작 시 Non-RT 프로세스가 아직 준비되지 않았을 때: RT는 기본 파라미터로 SAFE_MODE에서 대기하며, Non-RT 연결 시 READY 전환
- 공유 메모리 접근 경합: Sequence number로 torn read 감지 및 재시도 메커니즘
- Major Cycle이 너무 큼([7, 11, 13] → LCM=1001ms): 빌드 시 검증 도구가 경고하고 빌드 실패
- RT 프로세스 자체 크래시: 하드웨어 Watchdog이 감지하여 시스템 리셋 (복구 불가 상황)
- 동적 주기 변경 중 RT 루프 실행: Major Cycle 경계에서만 변경 적용하여 실행 중 간섭 방지
- RTDataStore 키 범위 초과(513번 키 접근): 컴파일 타임 enum 제한으로 원천 차단

## 요구사항 *(필수)*

### 기능적 요구사항

#### RT Executive (Real-Time)

- **FR-001**: RT Executive는 JSON 설정 파일에서 정의된 주기(periods_ms)의 GCD를 Minor Cycle로, LCM을 Major Cycle로 자동 계산해야 합니다.
- **FR-002**: RT Executive는 SCHED_FIFO 우선순위 90으로 실행되어야 하며, 지정된 CPU 코어에 고정(isolcpus)되어야 합니다.
- **FR-003**: RT Executive는 clock_nanosleep(TIMER_ABSTIME)을 사용하여 절대 시간 기준 주기를 유지하여 drift를 방지해야 합니다.
- **FR-004**: RT Executive는 동적 메모리 할당, 예외 처리, 블로킹 I/O를 사용해서는 안 됩니다(컴파일 타임 체크).
- **FR-005**: RT Executive는 각 Action의 실행 시간을 측정하여 WCET 초과 시 경고를 기록해야 합니다.

#### RTDataStore

- **FR-006**: RTDataStore는 고정 크기 배열(512개 키)로 구현되어야 하며, enum 기반 키 접근으로 타입 안전성을 보장해야 합니다.
- **FR-007**: RTDataStore는 int32, float, double, uint64, char[32] 타입을 지원해야 하며, 각 데이터는 타임스탬프와 sequence number를 포함해야 합니다.
- **FR-008**: RTDataStore의 모든 연산은 예외를 던지지 않아야 하며, 에러 코드(0=성공, -1=실패)를 반환해야 합니다.
- **FR-009**: RTDataStore는 atomic 연산으로 데이터 일관성을 보장해야 하며, lock-free여야 합니다.

#### 프로세스 간 통신

- **FR-010**: RT와 Non-RT 프로세스는 POSIX 공유 메모리(shm_open/mmap)로 통신해야 합니다.
- **FR-011**: RT → Non-RT 동기화는 10ms 주기로 상태 데이터(센서, 제어값)를 전송해야 합니다.
- **FR-012**: Non-RT → RT 동기화는 100ms 주기로 파라미터 데이터를 전송해야 합니다.
- **FR-013**: 각 프로세스는 heartbeat(atomic uint64 타임스탬프)를 공유 메모리에 기록해야 하며, 상대 프로세스는 이를 모니터링해야 합니다.
- **FR-014**: Non-RT heartbeat가 500ms 이상 끊기면 RT는 SAFE_MODE로 전환해야 합니다.

#### 상태 머신

- **FR-015**: RTStateMachine은 IDLE, INITIALIZING, READY, RUNNING, ERROR, SAFE_MODE 상태를 지원해야 합니다.
- **FR-016**: 각 상태는 entry, cyclic, exit action을 가질 수 있으며, 상태 전환 시 자동 호출되어야 합니다.
- **FR-017**: 각 Action은 guard condition을 가질 수 있으며, 현재 상태에서 허용되지 않으면 실행이 차단되어야 합니다.
- **FR-018**: 상태 전환은 Non-RT 명령 또는 RT 내부 조건(heartbeat 실패, 에러 감지)으로 트리거되어야 합니다.

#### 동적 주기 설정

- **FR-019**: 빌드 타임 스케줄 생성 도구는 JSON 설정에서 C++ 헤더(RTSchedule.h)를 자동 생성해야 합니다.
- **FR-020**: 스케줄 검증 도구는 CPU 사용률(Σ(WCET/Period))이 70%를 초과하면 빌드를 실패시켜야 합니다.
- **FR-021**: Major Cycle이 1000ms를 초과하면 경고를 출력해야 합니다.
- **FR-022**: 런타임 동적 로드 옵션은 초기화 단계(Non-RT)에서만 설정 파일을 읽어야 하며, RT 시작 후에는 변경 불가해야 합니다.

#### 장애 격리

- **FR-023**: RT 프로세스 크래시 시 하드웨어 Watchdog(10ms 주기)이 감지하여 시스템을 리셋해야 합니다.
- **FR-024**: Non-RT 프로세스 크래시 시 systemd가 자동으로 재시작해야 하며, RT는 마지막 파라미터로 계속 동작해야 합니다.
- **FR-025**: RT 프로세스는 mlockall(MCL_CURRENT|MCL_FUTURE)로 메모리를 잠가 페이지 폴트를 방지해야 합니다.

### 주요 엔티티

- **RTExecutive**: RT 프로세스의 Cyclic Executive 구현체. Minor/Major Cycle 관리, Action 스케줄링, 타이밍 제어 담당.
- **RTDataStore**: RT 전용 고정 크기 데이터 저장소. 512개 키, enum 기반 접근, lock-free, no exceptions.
- **RTStateMachine**: RT 프로세스의 상태 관리. 6개 상태, guard condition, entry/cyclic/exit action.
- **RTActionSlot**: 스케줄 테이블의 단위. action_id, 실행 함수 포인터, period_slots, next_slot 포함.
- **SharedMemoryRegion**: RT/Non-RT 간 공유 메모리 구조. rt_to_nonrt, nonrt_to_rt, heartbeat 영역 포함.
- **DataSynchronizer**: 공유 메모리를 통한 양방향 데이터 동기화 담당. RTDataStore ↔ DataStore 변환.
- **ScheduleCalculator**: 주기 배열에서 GCD/LCM 계산, 스케줄 파라미터 생성.
- **ScheduleGenerator**: 빌드 도구. JSON → C++ 헤더 변환.
- **ScheduleValidator**: 검증 도구. CPU 사용률, WCET, LCM 크기 체크.

## 성공 기준 *(필수)*

### 측정 가능한 결과

- **SC-001**: RT Executive의 1ms 주기 실행 시 99.9% 이상의 사이클이 ±10μs jitter 이내로 실행됨
- **SC-002**: Non-RT 프로세스 크래시 후 RT 프로세스는 중단 없이 최소 10초 이상 계속 동작함
- **SC-003**: RT → Non-RT 데이터 동기화 지연이 15ms 이하임 (10ms 주기 + 5ms 여유)
- **SC-004**: Non-RT → RT 파라미터 동기화 지연이 120ms 이하임 (100ms 주기 + 20ms 여유)
- **SC-005**: 임의의 주기 조합([1, 5, 10, 20, 50, 100])으로 설정 시 빌드가 성공하고 각 주기가 정확히 실행됨
- **SC-006**: CPU 사용률 70% 초과 설정 시 빌드 검증 도구가 실패 반환함
- **SC-007**: RT 프로세스 메모리 사용량이 2.5MB 이하로 고정됨 (동적 할당 없음)
- **SC-008**: RTDataStore 읽기/쓰기 연산이 1μs 이내 완료됨 (WCET)
- **SC-009**: 상태 전환(IDLE → INITIALIZING → READY → RUNNING)이 100ms 이내 완료됨
- **SC-010**: 하드웨어 Watchdog이 RT 크래시를 20ms 이내 감지하고 리셋 신호 송출함

### 비기능적 요구사항

- **NFR-001**: 실시간성 - RT 코드는 WCET 분석 가능해야 하며, 모든 경로가 결정적이어야 함
- **NFR-002**: 안전성 - RT 크래시 시 하드웨어 Watchdog, Non-RT 크래시 시 RT 독립 동작으로 이중 안전망 확보
- **NFR-003**: 유지보수성 - 기존 Action 인터페이스 유지로 기존 코드(195개 테스트) 재사용
- **NFR-004**: 확장성 - 새로운 주기 추가 시 JSON 수정 및 재빌드만으로 적용 가능
- **NFR-005**: 이식성 - PREEMPT_RT Linux + POSIX 표준만 사용하여 하드웨어 독립성 확보

## 가정 사항

1. **하드웨어**: 최소 2코어 CPU, 하드웨어 Watchdog 지원
2. **OS**: Ubuntu 24.04 + PREEMPT_RT 패치 커널
3. **권한**: RT 프로세스 실행 시 root 권한 또는 CAP_SYS_NICE 권한 보유
4. **네트워크**: RT 프로세스는 네트워크 I/O를 수행하지 않음 (Non-RT가 담당)
5. **로깅**: RT 프로세스의 로그는 lock-free queue로 Non-RT에 전달하여 비동기 기록
6. **기존 코드**: ActionExecutor, SequenceEngine은 Non-RT 영역에서 계속 사용됨
7. **테스트**: 195개 기존 단위 테스트는 Non-RT 영역 테스트로 유지됨
8. **Action 개수**: 최대 100개 Action을 사전 할당된 풀에서 관리
9. **데이터 키**: RTDataStore는 512개 키로 충분하며, 초과 시 컴파일 에러 발생

## 범위 밖

다음 항목은 이번 구현에 포함되지 않습니다:

- **멀티 코어 RT 스케줄링**: 현재는 단일 RT 코어만 사용, Partitioned Cyclic Executive는 향후 확장
- **네트워크 통신**: RT 프로세스는 로컬 제어만 담당, 네트워크는 Non-RT가 처리
- **GUI/시각화**: 모니터링 UI는 별도 프로젝트 (Non-RT 프로세스의 데이터를 구독)
- **동적 Task 추가**: 런타임 중 새 Task 추가는 불가, 재시작 필요
- **분산 시스템**: 단일 머신에서만 동작, 다중 로봇 협업은 범위 밖
- **실시간 네트워킹**: EtherCAT, TSN 등은 향후 확장
- **RTOS 포팅**: 현재는 Linux + PREEMPT_RT만 지원

## 의존성

- **외부 라이브러리**: spdlog (로깅), nlohmann_json (설정 파싱), TBB (Non-RT DataStore)
- **빌드 도구**: CMake 3.16+, GCC 10+ (C++20)
- **선행 작업**: DataStore 리팩토링 완료 (66개 테스트 통과)
- **후행 작업**: 실제 로봇 하드웨어 통합, 센서/액추에이터 드라이버 연동

## 문서 참조

- **연구 문서**: [docs/research/003-realtime-architecture-analysis.md](../../research/003-realtime-architecture-analysis.md)
- **현재 코드**: src/core/task/core/PeriodicScheduler.{h,cpp}, src/core/task/core/TaskExecutor.{h,cpp}
- **DataStore**: src/core/datastore/DataStore.{h,cpp}
- **테스트**: tests/unit/task/\*.cpp (195개)
