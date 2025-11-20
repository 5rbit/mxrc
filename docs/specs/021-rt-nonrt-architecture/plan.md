# 구현 계획: RT/Non-RT 이중 프로세스 아키텍처

**기능**: 021-rt-nonrt-architecture
**생성일**: 2025-11-20
**상태**: 계획

## 개요

MXRC 로봇 컨트롤러를 실시간(RT) 제어와 비실시간(Non-RT) 관리 영역으로 분리하는 이중 프로세스 아키텍처를 구현합니다. Cyclic Executive 패턴, RTDataStore, 동적 주기 설정, 프로세스 간 공유 메모리 통신을 포함합니다.

## 아키텍처 결정

### AD-001: 프로세스 vs 스레드 분리

**결정**: RT와 Non-RT를 별도 프로세스로 분리

**근거**:
- 완전한 메모리 격리 (Non-RT 크래시가 RT에 영향 없음)
- 독립적인 스케줄링 정책 (SCHED_FIFO vs SCHED_OTHER)
- systemd를 통한 독립적 재시작 관리

**대안**:
- 스레드 분리: 구현 간단하나 segfault 시 전체 프로세스 다운
- 컨테이너 분리: 최고 격리 수준이나 복잡도 증가

### AD-002: Cyclic Executive vs Priority-Based Scheduling

**결정**: Cyclic Executive 패턴 사용

**근거**:
- WCET 분석 용이 (정적 스케줄)
- Jitter 최소화 (절대 시간 기준 대기)
- 단순성 (컨텍스트 스위칭 없음)

**대안**:
- Rate Monotonic: 동적 우선순위 관리 필요
- EDF: 복잡도 높고 WCET 분석 어려움

### AD-003: 고정 메모리 vs 동적 할당

**결정**: RT 영역은 완전 고정 메모리 (사전 할당)

**근거**:
- WCET 예측 가능성
- 페이지 폴트 제거 (mlockall)
- 메모리 단편화 방지

**대안**:
- 동적 할당: 유연성 높으나 비결정적

### AD-004: 공유 메모리 vs 소켓 통신

**결정**: POSIX 공유 메모리 (shm_open/mmap)

**근거**:
- 마이크로초 단위 지연 (소켓 대비 10배 빠름)
- 커널 버퍼 없음 (직접 메모리 접근)
- Sequence number로 torn read 방지

**대안**:
- UNIX 소켓: 지연 크고 시스템 콜 오버헤드
- Message Queue: 공유 메모리보다 느림

### AD-005: 빌드 타임 vs 런타임 스케줄 생성

**결정**: 빌드 타임 생성을 기본으로, 런타임 로드는 선택적

**근거**:
- 빌드 타임: 오버헤드 없음, 컴파일 타임 검증
- 런타임 로드: 유연성 필요 시 옵션 제공

## 구현 단계

### Phase 1: 기반 구조 (1주)

**목표**: RT Executive 기본 구조 및 타이밍 메커니즘

**작업**:
1. RTExecutive 클래스 구현
   - Cyclic loop (while running)
   - clock_nanosleep 절대 시간 대기
   - SCHED_FIFO 우선순위 설정
   - CPU 코어 고정 (pthread_setaffinity_np)

2. 스케줄 계산 유틸리티
   - ScheduleCalculator::calculate() (GCD/LCM)
   - Minor/Major Cycle 자동 계산

3. 기본 타이밍 테스트
   - 1ms 주기 jitter 측정 (±10μs 목표)
   - 10만 사이클 안정성 테스트

**산출물**:
- src/core/rt/RTExecutive.{h,cpp}
- src/core/rt/util/ScheduleCalculator.{h,cpp}
- tests/unit/rt/RTExecutive_jitter_test.cpp

**성공 기준**:
- 1ms 주기 99.9% 사이클이 ±10μs jitter
- CPU 사용률 < 5% (1ms 주기, 빈 Action)

### Phase 2: RTDataStore (1주)

**목표**: 고정 메모리 기반 데이터 저장소

**작업**:
1. RTDataStore 구현
   - 고정 배열 (512 키)
   - Enum 기반 키 (타입 안전)
   - Union 기반 값 (int32, float, double, uint64, char[32])
   - Atomic 연산 (std::atomic)
   - Sequence number (torn read 방지)

2. 에러 코드 반환 (no exceptions)
   - set_i32(), set_f32(), set_f64() 등
   - get_i32(), get_f32(), get_f64() 등
   - is_fresh() (타임스탬프 기반)

3. 단위 테스트
   - 키별 읽기/쓰기
   - 타입 안전성
   - 동시 접근 (멀티스레드)

**산출물**:
- src/core/rt/RTDataStore.{h,cpp}
- tests/unit/rt/RTDataStore_test.cpp

**성공 기준**:
- 읽기/쓰기 연산 < 1μs (WCET)
- 메모리 사용량 = 32KB (고정)
- 동시 접근 시 데이터 일관성 100%

### Phase 3: 공유 메모리 통신 (5일)

**목표**: RT/Non-RT 프로세스 간 데이터 동기화

**작업**:
1. SharedMemoryRegion 구조 정의
   - rt_to_nonrt 영역 (센서/상태)
   - nonrt_to_rt 영역 (파라미터)
   - Heartbeat (양방향)

2. DataSynchronizer 구현
   - syncFromRTDataStore() (RT → Shared Memory)
   - syncToRTDataStore() (Shared Memory → RT)
   - applyNonRTParams() (Shared Memory → RTDataStore)
   - applyRTStatus() (Shared Memory → DataStore)

3. 프로세스 간 통신 테스트
   - 2개 프로세스 동시 실행
   - 10ms/100ms 주기 동기화
   - 지연 시간 측정

**산출물**:
- src/core/rt/ipc/SharedMemoryRegion.h
- src/core/rt/ipc/DataSynchronizer.{h,cpp}
- tests/integration/rt/ipc_sync_test.cpp

**성공 기준**:
- RT → Non-RT 지연 < 15ms
- Non-RT → RT 지연 < 120ms
- Heartbeat 실패 감지 < 500ms

### Phase 4: 동적 주기 설정 (1주)

**목표**: JSON 기반 스케줄 자동 생성

**작업**:
1. 빌드 도구 (schedule_generator)
   - JSON 파싱 (nlohmann_json)
   - GCD/LCM 계산
   - C++ 헤더 생성 (RTSchedule.h)

2. 검증 도구 (validate_schedule)
   - CPU 사용률 계산 (Σ(WCET/Period))
   - WCET vs Period 검증
   - LCM 크기 체크

3. CMakeLists.txt 통합
   - add_custom_command (헤더 생성)
   - 빌드 전 자동 실행
   - 설정 변경 시 재생성

**산출물**:
- tools/schedule_generator.cpp
- tools/validate_schedule.cpp
- config/rt_schedule.json (템플릿)
- CMakeLists.txt 수정

**성공 기준**:
- [1, 5, 10] → Minor=1ms, Major=10ms 생성
- CPU 70% 초과 시 빌드 실패
- LCM > 1000ms 시 경고

### Phase 5: 상태 머신 (5일)

**목표**: 상태 기반 안전 제어

**작업**:
1. RTStateMachine 구현
   - 6개 상태 정의 (IDLE, INITIALIZING, READY, RUNNING, ERROR, SAFE_MODE)
   - Entry/Cyclic/Exit action
   - Guard condition
   - 상태 전환 함수

2. Action 필터링
   - registerCyclicAction (상태별)
   - shouldExecute() (guard 체크)

3. 상태 전환 트리거
   - Non-RT 명령 (공유 메모리 큐)
   - RT 내부 조건 (heartbeat, 에러)

**산출물**:
- src/core/rt/RTStateMachine.{h,cpp}
- tests/unit/rt/RTStateMachine_test.cpp

**성공 기준**:
- 상태 전환 < 100ms
- Guard condition 동작 100%
- SAFE_MODE 자동 진입 (heartbeat 실패)

### Phase 6: Non-RT 프로세스 (1주)

**목표**: 기존 코드 통합 및 Non-RT 프로세스 구현

**작업**:
1. NonRTExecutive 구현
   - TaskExecutor 통합
   - ActionExecutor 사용
   - EventBus 연동

2. 공유 메모리 클라이언트
   - DataStore ↔ Shared Memory 동기화
   - 주기적 heartbeat 송신

3. systemd 서비스 설정
   - rt-executive.service
   - nonrt-executive.service
   - 재시작 정책 (Non-RT만)

**산출물**:
- src/nonrt_main.cpp
- systemd/rt-executive.service
- systemd/nonrt-executive.service

**성공 기준**:
- Non-RT 크래시 시 RT 지속 동작
- systemd 자동 재시작 < 5초
- 195개 기존 테스트 통과

### Phase 7: 통합 테스트 및 최적화 (1주)

**목표**: 전체 시스템 통합 및 성능 검증

**작업**:
1. 통합 테스트 시나리오
   - RT/Non-RT 동시 실행
   - 장애 주입 (Non-RT kill -9)
   - 장기 안정성 (24시간)

2. 성능 최적화
   - Jitter 분석 및 감소
   - CPU 사용률 최적화
   - 메모리 사용량 측정

3. 문서 작성
   - 사용자 가이드 (설정, 실행)
   - 개발자 가이드 (Action 작성)
   - 트러블슈팅

**산출물**:
- tests/integration/rt/full_system_test.cpp
- docs/user_guide_rt_nonrt.md
- docs/developer_guide_rt_actions.md

**성공 기준**:
- 모든 SC-001~SC-010 기준 통과
- 24시간 연속 실행 안정성
- 문서 완성도 90% 이상

## 파일 구조

```
src/core/rt/
├── RTExecutive.{h,cpp}              # Cyclic Executive 구현
├── RTDataStore.{h,cpp}              # RT 데이터 저장소
├── RTStateMachine.{h,cpp}           # 상태 머신
├── RTContext.h                      # RT 실행 컨텍스트
├── util/
│   ├── ScheduleCalculator.{h,cpp}   # GCD/LCM 계산
│   └── TimeUtils.{h,cpp}            # 시간 유틸리티
└── ipc/
    ├── SharedMemoryRegion.h         # 공유 메모리 구조
    └── DataSynchronizer.{h,cpp}     # 동기화 로직

tools/
├── schedule_generator.cpp           # 빌드 도구: JSON → C++
└── validate_schedule.cpp            # 검증 도구

config/
└── rt_schedule.json                 # 주기 설정 템플릿

generated/ (빌드 시 생성)
└── RTSchedule.h                     # 자동 생성 헤더

tests/unit/rt/
├── RTExecutive_test.cpp
├── RTDataStore_test.cpp
├── RTStateMachine_test.cpp
└── ScheduleCalculator_test.cpp

tests/integration/rt/
├── ipc_sync_test.cpp
├── full_system_test.cpp
└── fault_injection_test.cpp

systemd/
├── rt-executive.service
└── nonrt-executive.service
```

## 테스트 전략

### 단위 테스트
- RTExecutive: 타이밍 정확도, jitter 측정
- RTDataStore: CRUD 연산, 동시 접근, 타입 안전성
- RTStateMachine: 상태 전환, guard condition, entry/exit action
- ScheduleCalculator: GCD/LCM 계산 정확성

### 통합 테스트
- IPC: 양방향 데이터 동기화, 지연 시간
- Full System: RT + Non-RT 동시 실행, 24시간 안정성
- Fault Injection: Non-RT 크래시, heartbeat 실패

### 성능 테스트
- Jitter: 10만 사이클, 히스토그램 분석
- CPU: RT 코어 사용률, Non-RT 영향도
- 메모리: RT 고정 메모리, 누수 검사

### 안전 테스트
- Watchdog: RT 크래시 감지 시간
- Safe Mode: 자동 전환 시나리오
- Graceful Degradation: Non-RT 없이 RT만 동작

## 위험 요소 및 완화 전략

### R-001: Jitter 목표 미달 (±10μs)

**위험**: Linux는 RTOS가 아니므로 커널 인터럽트로 인한 jitter 발생 가능

**완화**:
- CPU 격리 (isolcpus 커널 파라미터)
- IRQ 친화성 설정 (다른 코어로 이동)
- NO_HZ_FULL 커널 옵션 (타이머 인터럽트 제거)
- 우선순위 최대 (SCHED_FIFO 99)

**측정**: 히스토그램 분석으로 99.9 퍼센타일 확인

### R-002: 공유 메모리 동기화 복잡도

**위험**: Sequence number 로직 오류 시 torn read 발생

**완화**:
- 단순화된 프로토콜 (single writer 보장)
- 광범위한 단위 테스트
- 동시 접근 시나리오 검증 (스레드 새니타이저)

**측정**: 10만 회 동기화 시 torn read 0건

### R-003: 기존 코드 호환성

**위험**: Non-RT 영역에서 기존 ActionExecutor 사용 시 예상치 못한 문제

**완화**:
- 195개 기존 테스트 전부 통과 필수
- 인터페이스 변경 없이 내부 구현만 분리
- Phase 6에서 충분한 통합 테스트

**측정**: 기존 테스트 100% 통과

### R-004: 하드웨어 Watchdog 부재

**위험**: 일부 시스템에 하드웨어 Watchdog 없을 수 있음

**완화**:
- 소프트웨어 Watchdog 대체 구현
- systemd의 WatchdogSec 활용
- 문서에 하드웨어 Watchdog 권장 명시

**측정**: SW Watchdog 30ms 이내 감지

### R-005: 과도한 스케줄 복잡도

**위험**: 너무 많은 주기 조합 시 LCM 폭발

**완화**:
- 검증 도구로 빌드 전 체크
- LCM 1000ms 제한 (경고)
- 주기는 2의 거듭제곱 권장 (문서화)

**측정**: 검증 도구가 부적절한 설정 차단

## 의존성 관리

### 외부 라이브러리
- **spdlog**: Non-RT 로깅 (RT는 lock-free queue로 전달)
- **nlohmann_json**: 설정 파싱 (빌드 도구만 사용)
- **TBB**: Non-RT DataStore (기존 유지)

### 커널 설정
- **PREEMPT_RT 패치**: 필수
- **isolcpus**: 권장 (RT 코어 격리)
- **NO_HZ_FULL**: 선택적 (jitter 최소화)

### 권한
- **CAP_SYS_NICE**: RT 우선순위 설정
- **CAP_IPC_LOCK**: mlockall 허용

## 마일스톤

| Phase | 목표 | 기간 | 완료 기준 |
|-------|------|------|-----------|
| Phase 1 | RT Executive 기본 | 1주 | ±10μs jitter |
| Phase 2 | RTDataStore | 1주 | <1μs WCET |
| Phase 3 | 공유 메모리 | 5일 | <15ms 지연 |
| Phase 4 | 동적 주기 설정 | 1주 | 빌드 통합 |
| Phase 5 | 상태 머신 | 5일 | 상태 전환 동작 |
| Phase 6 | Non-RT 통합 | 1주 | 기존 테스트 통과 |
| Phase 7 | 통합 테스트 | 1주 | 모든 SC 통과 |

**총 예상 기간**: 6.5주

## 검토 및 승인

- [ ] 아키텍처 리뷰 (Phase 1 완료 후)
- [ ] 보안 리뷰 (Phase 3 완료 후, 공유 메모리 권한)
- [ ] 성능 리뷰 (Phase 7 완료 후)
- [ ] 최종 승인 (모든 테스트 통과 후)
