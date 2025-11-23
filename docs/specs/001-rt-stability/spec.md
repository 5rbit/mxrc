# 기능 사양서: RT 아키텍처 안정성 개선

**기능 브랜치**: `001-rt-stability`
**생성일**: 2025-11-20
**상태**: 초안
**입력**: 사용자 설명: "안정성을 위해 개선할점이 있다면 개선 하려고해. 분석하고 개선해줘"

## 사용자 시나리오 및 테스트 *(필수)*

### 사용자 스토리 1 - 메모리 안전성 보장 (우선순위: P1)

개발자가 RTExecutive를 생성하고 사용할 때, 메모리 누수나 댕글링 포인터 없이 안전하게 리소스가 관리되어야 합니다. createFromPeriods() 호출 후 명시적 delete를 잊어버리거나, 예외 발생 시 메모리가 누수되는 상황을 방지해야 합니다.

**이 우선순위인 이유**: 메모리 누수는 장시간 실행되는 실시간 시스템에서 치명적입니다. 수 시간~수 일 동안 실행되는 로봇 제어 시스템에서 메모리 누수는 시스템 크래시로 이어집니다.

**독립 테스트**: Valgrind와 AddressSanitizer로 메모리 누수 검증 가능. createFromPeriods()를 반복 호출하여 메모리가 증가하지 않는지 확인.

**인수 시나리오**:

1. **주어진 상황** RTExecutive를 createFromPeriods()로 생성, **언제** std::unique_ptr로 관리, **그러면** 자동으로 메모리 해제되고 Valgrind에서 누수 없음
2. **주어진 상황** RTExecutive 생성 중 예외 발생, **언제** 스마트 포인터로 관리, **그러면** 부분 생성된 객체도 안전하게 정리됨
3. **주어진 상황** shared memory 생성 실패, **언제** RAII 패턴 적용, **그러면** shm_unlink 자동 호출되어 리소스 누수 없음

---

### 사용자 스토리 2 - 동시성 안전성 보장 (우선순위: P1)

개발자가 RTExecutive를 별도 스레드에서 run() 호출하고, 메인 스레드에서 stop()을 호출할 때, 데이터 레이스나 inconsistent state 없이 안전하게 종료되어야 합니다. state machine과 running_ 플래그 간의 동기화 문제를 해결해야 합니다.

**이 우선순위인 이유**: 데이터 레이스는 정의되지 않은 동작(UB)을 유발하며, 실시간 시스템에서는 예측 불가능한 타이밍 이슈로 이어집니다. ThreadSanitizer 없이는 탐지하기 어렵습니다.

**독립 테스트**: ThreadSanitizer로 데이터 레이스 검증 가능. 멀티스레드 환경에서 run()/stop() 반복 호출하여 race condition 없음을 확인.

**인수 시나리오**:

1. **주어진 상황** RTExecutive가 RUNNING 상태, **언제** 다른 스레드에서 stop() 호출, **그러면** running_ 플래그와 state machine이 원자적으로 동기화되어 안전하게 종료
2. **주어진 상황** RTDataStore에 여러 스레드 접근, **언제** read/write 동시 발생, **그러면** 데이터 레이스 없이 sequence number로 일관성 보장
3. **주어진 상황** 상태 전환 중 콜백 실행, **언제** 콜백에서 다시 이벤트 발생, **그러면** 재진입 문제 없이 안전하게 처리

---

### 사용자 스토리 3 - 에러 복구 메커니즘 (우선순위: P2)

실시간 우선순위 설정이 실패하거나 shared memory 생성이 실패했을 때, 시스템이 명확한 에러 상태로 전환되고 복구 가능한 상태를 유지해야 합니다. 부분 초기화된 상태에서 실행되는 것을 방지해야 합니다.

**이 우선순위인 이유**: 프로덕션 환경에서는 권한 부족, 리소스 고갈 등 다양한 에러 상황이 발생합니다. 에러를 무시하고 계속 실행하면 예측 불가능한 동작을 유발합니다.

**독립 테스트**: 권한 없는 환경에서 실행하여 RT priority 설정 실패 시나리오 검증. shared memory 이름 충돌 시나리오 검증.

**인수 시나리오**:

1. **주어진 상황** RT priority 설정 권한 없음, **언제** run() 호출, **그러면** ERROR 상태로 전환되고 실행 거부
2. **주어진 상황** shared memory 이름 충돌, **언제** createShared() 호출, **그러면** 명확한 에러 반환과 함께 기존 리소스 정리
3. **주어진 상황** ERROR 상태에서 복구 시도, **언제** RESET 이벤트 발생, **그러면** INIT 상태로 전환되고 재초기화 가능

---

### 사용자 스토리 4 - 타입 안전성 강화 (우선순위: P3)

RTDataStore에 잘못된 타입으로 데이터를 읽거나 쓸 때, 런타임 에러 대신 컴파일 타임에 검증되거나 명확한 타입 검증이 이루어져야 합니다. DataKey enum과 실제 데이터 타입 간의 불일치를 방지해야 합니다.

**이 우선순위인 이유**: 타입 안전성은 버그 조기 발견에 중요하지만, 기존 시스템이 동작하는 상태이므로 상대적으로 낮은 우선순위입니다.

**독립 테스트**: 잘못된 타입으로 접근 시 런타임 검증 동작 확인. 타입 불일치 시나리오 테스트.

**인수 시나리오**:

1. **주어진 상황** ROBOT_X는 int32_t 타입, **언제** getDouble() 호출, **그러면** 명확한 에러 반환
2. **주어진 상황** 타입 메타데이터 등록, **언제** 잘못된 타입 접근, **그러면** 사전에 검증되어 에러 발생
3. **주어진 상황** DataKey 범위 초과, **언제** setInt32() 호출, **그러면** 경계 검사로 에러 반환

---

### 엣지 케이스

- RTExecutive 생성 중 예외 발생 시 부분 생성된 리소스는 어떻게 정리됩니까?
- shared memory 생성 후 mmap 실패 시 shm_unlink는 호출됩니까?
- state machine SHUTDOWN 상태에서 재사용 시도 시 어떻게 됩니까?
- running_ = false 설정과 state machine 전환 사이에 타이밍 윈도우가 있습니까?
- RTDataStore sequence number가 uint64_t 최대값을 넘으면 어떻게 됩니까?
- 여러 스레드에서 동시에 state transition callback 호출 시 어떻게 됩니까?
- RT priority 설정 실패 후 복구 없이 run() 계속 실행 시 어떻게 됩니까?

## 요구사항 *(필수)*

### 기능적 요구사항

#### 메모리 관리

- **FR-001**: RTExecutive::createFromPeriods()는 std::unique_ptr<RTExecutive>를 반환해야 합니다.
- **FR-002**: RTDataStoreShared는 RAII 패턴을 적용하여 소멸자에서 shm_unlink를 보장해야 합니다.
- **FR-003**: RTExecutive 생성 실패 시 부분 생성된 리소스(schedule_, state_machine_ 등)가 자동으로 정리되어야 합니다.
- **FR-004**: 모든 동적 메모리 할당은 스마트 포인터(std::unique_ptr, std::shared_ptr)로 관리되어야 합니다.

#### 동시성 안전성

- **FR-005**: RTExecutive::running_ 플래그는 std::atomic<bool>을 사용해야 합니다.
- **FR-006**: RTExecutive::stop()에서 running_ = false 설정과 state machine 전환은 원자적으로 동기화되어야 합니다.
- **FR-007**: RTDataStore의 read/write 연산은 데이터 레이스 없이 동작해야 합니다.
- **FR-008**: state transition callback 실행 중 재진입 시도는 안전하게 처리되어야 합니다.

#### 에러 처리

- **FR-009**: RT priority 설정 실패 시 RTStateMachine은 ERROR 상태로 전환되어야 합니다.
- **FR-010**: ERROR 상태에서 run()은 실행을 거부하고 -1을 반환해야 합니다.
- **FR-011**: shared memory 생성 실패 시 명확한 에러 코드와 로그를 반환해야 합니다.
- **FR-012**: RTExecutive::run()은 에러 발생 시 ERROR 상태로 전환하고 실행을 중단해야 합니다.

#### 타입 안전성

- **FR-013**: RTDataStore는 DataKey와 실제 저장된 타입 간 불일치를 검증해야 합니다.
- **FR-014**: DataKey 범위 초과 접근 시 에러를 반환해야 합니다.
- **FR-015**: 타입 메타데이터(DataKey -> 타입 매핑)를 등록하고 검증하는 메커니즘을 제공해야 합니다.

#### 테스트 및 검증

- **FR-016**: AddressSanitizer로 메모리 안전성을 검증해야 합니다.
- **FR-017**: ThreadSanitizer로 데이터 레이스를 검증해야 합니다.
- **FR-018**: Valgrind로 메모리 누수가 없음을 검증해야 합니다.

### 주요 엔티티

- **RTExecutive**: 실시간 주기 실행기. 스마트 포인터로 관리되며 RAII 패턴 적용. running_ 플래그는 atomic으로 관리.
- **RTStateMachine**: RT 상태 머신. ERROR 상태 전환 시 시스템 실행 차단. SHUTDOWN 상태에서 재사용 불가.
- **RTDataStore**: 실시간 데이터 저장소. 타입 메타데이터 관리 및 검증. sequence number로 일관성 보장.
- **RTDataStoreShared**: shared memory 래퍼. RAII로 shm_unlink 보장. 예외 발생 시 리소스 자동 정리.

## 성공 기준 *(필수)*

### 측정 가능한 결과

- **SC-001**: AddressSanitizer 빌드로 전체 테스트 스위트(47 tests) 실행 시 메모리 에러 0건
- **SC-002**: ThreadSanitizer 빌드로 멀티스레드 테스트 실행 시 데이터 레이스 0건
- **SC-003**: Valgrind로 RTExecutive 생성/소멸 1000회 반복 시 메모리 누수 0바이트
- **SC-004**: RT priority 설정 실패 시 ERROR 상태로 전환되고 run() 실행 거부됨
- **SC-005**: shared memory 생성 실패 시 shm_unlink 호출 확인 (strace로 검증)
- **SC-006**: 잘못된 타입으로 RTDataStore 접근 시 에러 반환율 100%
- **SC-007**: 멀티스레드 환경에서 run()/stop() 1000회 반복 시 크래시 0건

## 구현 접근 방식

### Phase 1: 메모리 안전성 (P1)
1. RTExecutive::createFromPeriods() 반환 타입 변경: `RTExecutive*` → `std::unique_ptr<RTExecutive>`
2. RTDataStoreShared에 RAII 적용: 소멸자에서 shm_unlink 보장
3. 모든 테스트 코드 스마트 포인터로 업데이트
4. AddressSanitizer 검증

### Phase 2: 동시성 안전성 (P1)
1. running_ 플래그를 std::atomic<bool>로 변경
2. stop() 함수에 synchronization 추가 (mutex or atomic ordering)
3. state transition callback 재진입 방지 로직 추가
4. ThreadSanitizer 검증

### Phase 3: 에러 처리 (P2)
1. RT priority 설정 실패 시 ERROR 상태 전환 로직 추가
2. ERROR 상태에서 run() 실행 거부 로직 추가
3. shared memory 에러 처리 강화
4. 에러 복구 테스트 추가

### Phase 4: 타입 안전성 (P3)
1. DataKey 타입 메타데이터 시스템 설계
2. RTDataStore에 타입 검증 로직 추가
3. 타입 불일치 테스트 추가

## 제외 사항

- 성능 최적화는 이 기능에 포함되지 않습니다 (안정성에만 집중)
- API 변경은 최소화합니다 (기존 코드 호환성 유지)
- Non-RT 프로세스는 이 기능 범위에 포함되지 않습니다

## 가정

- AddressSanitizer, ThreadSanitizer는 이미 CMake에 설정되어 있습니다
- 테스트 환경에서 RT 권한(CAP_SYS_NICE, CAP_IPC_LOCK) 없이도 테스트 가능합니다
- C++17 이상 사용 가능합니다 (std::unique_ptr, std::atomic)
