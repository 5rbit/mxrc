# Feature Specification: EtherCAT 센서/모터 데이터 수신 인프라

**Feature Branch**: `001-ethercat-integration`
**Created**: 2025-11-23
**Status**: Draft
**Progress**: 1/5 (Spec → Plan → Tasks → Implementation → Completed)
**Last Updated**: 2025-11-23
**Input**: User description: "센서 데이터와 모터 데이터를 EtherCAT으로 수신하려고 해 기반 기술을 마련해"

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Task, Action, Sequence, API, JSON 등)
- 일반 설명, 요구사항, 시나리오는 모두 한글로 작성합니다

**예시**:
- ✅ 좋은 예: "사용자는 Task를 생성할 수 있어야 합니다"
- ❌ 나쁜 예: "User can create a task"

---

## Clarifications

### Session 2025-11-23

- Q: MVP에서 우선적으로 지원할 센서 타입은 무엇인가요? → A: 모션 제어용은 위치 센서(엔코더), 속도 센서, 토크/힘 센서를 타입별로 지원. 일반 센서는 범용 DI(Digital Input), AI(Analog Input)로 지원
- Q: MVP에서 우선적으로 지원할 모터 타입은 무엇인가요? → A: 범용 모터 드라이버 인터페이스 지원. BLDC 모터와 서보 드라이브를 우선순위로 구현
- Q: EtherCAT slave 설정 파일의 선호 포맷은 무엇인가요? → A: YAML 포맷 사용

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 - RT 제어 루프에서 센서 데이터 읽기 (Priority: P1)

RT Executive가 minor cycle(10ms)마다 센서 데이터를 EtherCAT을 통해 읽어서 DataStore에 저장하고, 이를 기반으로 제어 알고리즘을 실행할 수 있어야 합니다.

**Why this priority**: RT 제어의 가장 기본이 되는 기능이며, 센서 데이터 없이는 어떤 제어도 불가능합니다. 이 기능만으로도 센서 값을 모니터링하는 MVP가 가능합니다.

**Independent Test**: EtherCAT 시뮬레이터 또는 실제 장비로부터 센서 데이터를 읽고 DataStore에 저장된 값을 확인할 수 있습니다. 제어 출력 없이 읽기만 테스트 가능합니다.

**Acceptance Scenarios**:

1. **Given** RT Executive가 실행 중이고 EtherCAT 장비가 연결됨, **When** minor cycle이 시작됨, **Then** 모든 센서 데이터(온도, 압력, 위치 등)가 10ms 이내에 DataStore에 업데이트됨
2. **Given** 센서 데이터가 DataStore에 저장됨, **When** Non-RT 모니터링 시스템이 데이터를 요청함, **Then** 최신 센서 값과 타임스탬프가 반환됨
3. **Given** EtherCAT 통신 중, **When** 센서에서 에러 발생(센서 고장, 범위 초과 등), **Then** 에러 이벤트가 발행되고 safe mode로 전환됨

---

### User Story 2 - RT 제어 루프에서 모터 명령 전송 (Priority: P1)

RT Executive가 제어 알고리즘 계산 결과를 EtherCAT을 통해 모터에 전송하여 실시간 제어를 수행할 수 있어야 합니다.

**Why this priority**: 센서 읽기와 함께 RT 제어의 핵심입니다. 센서 → 제어 알고리즘 → 모터 출력의 완전한 제어 루프를 구성합니다.

**Independent Test**: 제어 명령을 EtherCAT으로 전송하고, 모터 드라이버 응답 또는 시뮬레이터 피드백을 확인할 수 있습니다. 간단한 위치/속도 명령 전송으로 테스트 가능합니다.

**Acceptance Scenarios**:

1. **Given** RT Executive가 실행 중이고 제어 명령이 계산됨, **When** minor cycle에서 모터 명령이 전송됨, **Then** 모든 모터에 명령이 10ms 이내에 전달되고 응답이 확인됨
2. **Given** 모터 명령이 전송됨, **When** EtherCAT 통신이 정상임, **Then** 모터 드라이버에서 명령 수신 확인(ACK)이 수신됨
3. **Given** 모터 명령 전송 중, **When** EtherCAT 통신 장애 발생, **Then** 에러가 감지되고 모든 모터가 안전 상태로 전환됨

---

### User Story 3 - EtherCAT 마스터 초기화 및 상태 관리 (Priority: P2)

시스템 시작 시 EtherCAT 마스터를 초기화하고, 슬레이브 장비들의 상태를 관리할 수 있어야 합니다.

**Why this priority**: P1 기능들이 동작하기 위한 전제조건이지만, 초기화는 한 번만 수행되므로 P1 기능보다 우선순위가 낮습니다.

**Independent Test**: 시스템 시작 시 EtherCAT 마스터 초기화 로그와 슬레이브 장비 발견 로그를 확인할 수 있습니다. 통신 없이 초기화 과정만 테스트 가능합니다.

**Acceptance Scenarios**:

1. **Given** 시스템이 부팅됨, **When** EtherCAT 마스터 초기화가 시작됨, **Then** 5초 이내에 모든 슬레이브 장비가 발견되고 OP 상태로 전환됨
2. **Given** 슬레이브 장비가 OP 상태임, **When** RT Executive가 시작 요청을 받음, **Then** EtherCAT 통신이 활성화되고 cyclic 데이터 교환이 시작됨
3. **Given** EtherCAT 통신 중, **When** 슬레이브 장비가 오프라인됨, **Then** 에러가 감지되고 해당 장비 상태가 OFFLINE으로 업데이트됨

---

### User Story 4 - 다중 슬레이브 동기화 (Priority: P3)

여러 개의 EtherCAT 슬레이브 장비(센서, 모터)를 distributed clock(DC)으로 동기화하여 정확한 타이밍을 보장할 수 있어야 합니다.

**Why this priority**: 기본적인 통신은 DC 없이도 가능하지만, 정밀 제어를 위해서는 필요합니다. P1/P2 기능이 동작한 후 성능 개선으로 추가 가능합니다.

**Independent Test**: DC 동기화 상태를 모니터링하고, 각 슬레이브의 clock offset을 측정할 수 있습니다.

**Acceptance Scenarios**:

1. **Given** 모든 슬레이브가 OP 상태임, **When** DC 동기화가 활성화됨, **Then** 모든 슬레이브의 clock offset이 ±1μs 이내로 수렴됨
2. **Given** DC 동기화가 활성화됨, **When** cyclic 데이터 교환이 진행됨, **Then** 모든 장비의 타임스탬프가 동기화되어 jitter가 ±10μs 이내로 유지됨

---

### Edge Cases

- EtherCAT 네트워크에 슬레이브 장비가 없을 때 어떻게 처리하나요?
  - 초기화 시 경고 로그를 출력하고, RT Executive는 시작하지 않음
- 센서 데이터 범위를 벗어나는 값(out-of-range)이 수신되면 어떻게 하나요?
  - 에러 이벤트 발행, safe mode 전환, 해당 센서 값을 invalid로 표시
- EtherCAT 통신이 중단되었다가 복구되면 어떻게 하나요?
  - 자동 재연결 시도 (최대 3회), 재연결 성공 시 슬레이브 재초기화 및 통신 재개
- 모터 명령이 전송 중 lost되면 어떻게 하나요?
  - Watchdog timeout 감지, 모터를 안전 상태로 전환, 에러 이벤트 발행
- RT cycle deadline을 놓치면(EtherCAT 통신 지연으로) 어떻게 하나요?
  - Deadline miss 카운터 증가, 연속 3회 이상 시 safe mode 전환 및 에러 로그

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: 시스템은 부팅 시 EtherCAT 마스터를 초기화하고 네트워크 상의 모든 슬레이브 장비를 자동으로 발견해야 함
- **FR-002**: 시스템은 발견된 슬레이브 장비를 INIT → PREOP → SAFEOP → OP 상태로 순차 전환해야 함
- **FR-003**: RT Executive는 minor cycle(10ms)마다 모든 센서 데이터를 EtherCAT으로 읽어 DataStore에 저장해야 함
- **FR-004**: RT Executive는 제어 알고리즘 계산 후 모터 명령을 EtherCAT으로 전송해야 함
- **FR-005**: 시스템은 EtherCAT PDO(Process Data Object) 매핑을 통해 센서/모터 데이터를 교환해야 함
- **FR-006**: 시스템은 EtherCAT 통신 에러(lost frames, timeout, invalid data)를 감지하고 에러 이벤트를 발행해야 함
- **FR-007**: 시스템은 EtherCAT 통신 장애 시 safe mode로 전환하고 모든 모터를 안전 상태로 만들어야 함
- **FR-008**: 시스템은 distributed clock(DC)를 사용하여 모든 슬레이브 장비를 동기화해야 함 (P3)
- **FR-009**: 시스템은 EtherCAT 네트워크 토폴로지(ring, line)를 자동으로 감지하고 적응해야 함
- **FR-010**: 시스템은 센서 데이터와 모터 상태를 Non-RT 시스템이 접근할 수 있도록 DataStore를 통해 공유해야 함
- **FR-011**: 시스템은 EtherCAT 통신 통계(frame count, error rate, latency)를 수집하고 모니터링할 수 있어야 함
- **FR-012**: 시스템은 다음 센서 타입을 지원해야 함:
  - 모션 제어용 센서: 위치 센서(엔코더), 속도 센서, 토크/힘 센서 (타입별 전용 인터페이스)
  - 일반 센서: 범용 Digital Input(DI), Analog Input(AI) 인터페이스
- **FR-013**: 시스템은 범용 모터 드라이버 인터페이스를 통해 다양한 모터를 지원해야 함:
  - 우선순위 1: BLDC(Brushless DC) 모터 드라이버
  - 우선순위 2: 서보 드라이버 (위치/속도/토크 제어 모드)
  - 확장 가능: 추가 모터 타입 플러그인 구조
- **FR-014**: 시스템은 EtherCAT slave configuration을 YAML 설정 파일로 관리해야 함

### Key Entities

- **EtherCAT Master**: EtherCAT 네트워크를 제어하는 마스터 장치, 슬레이브 장비와 통신하고 PDO 데이터를 교환함
- **EtherCAT Slave**: 네트워크에 연결된 센서, 모터, I/O 모듈 등의 장비, 각자 고유한 slave address와 PDO 매핑을 가짐
- **Sensor Data**: 센서에서 읽은 물리량 값, 타임스탬프, 유효성 플래그를 포함
  - Motion 센서: 위치(엔코더), 속도, 토크/힘 (전용 데이터 구조)
  - 범용 센서: Digital Input(boolean), Analog Input(실수값)
- **Motor Command**: 모터에 전송할 제어 명령, 명령 타입, 타임스탬프를 포함
  - BLDC 모터: 속도, 토크 명령
  - 서보 드라이버: 위치, 속도, 토크 제어 모드 및 명령값
  - 공통 인터페이스: 활성화/비활성화, 에러 리셋
- **PDO Mapping**: 센서/모터 데이터를 EtherCAT PDO 프레임에 매핑하는 구성, slave별로 input/output PDO 정의
- **DC Configuration**: Distributed clock 동기화 설정, 기준 clock, 동기화 주기, offset 등

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: RT Executive가 10ms minor cycle 내에 모든 센서 데이터를 읽고 모터 명령을 전송할 수 있어야 함 (latency < 8ms)
- **SC-002**: EtherCAT 통신 에러율이 0.1% 미만이어야 함 (10,000 cycle당 10회 이하)
- **SC-003**: 시스템 시작부터 EtherCAT 통신 활성화까지 5초 이내에 완료되어야 함
- **SC-004**: 연속 24시간 동작 중 EtherCAT 통신 단절 없이 안정적으로 유지되어야 함
- **SC-005**: Distributed clock 동기화 시 모든 슬레이브의 clock offset이 ±1μs 이내여야 함
- **SC-006**: RT cycle deadline miss 비율이 0.01% 미만이어야 함 (10,000 cycle당 1회 이하)
- **SC-007**: EtherCAT 통신 장애 발생 시 100ms 이내에 safe mode로 전환되어야 함
- **SC-008**: 센서 데이터 읽기 성공률이 99.9% 이상이어야 함
- **SC-009**: 모터 명령 전송 성공률이 99.9% 이상이어야 함

## Assumptions

- EtherCAT 네트워크 인터페이스는 전용 이더넷 포트를 사용합니다 (일반 LAN과 분리)
- EtherCAT 슬레이브 장비는 CoE(CANopen over EtherCAT) 프로토콜을 지원합니다
- RT Executive는 Linux PREEMPT_RT 커널에서 실행됩니다
- EtherCAT cycle time은 RT minor cycle과 동일한 10ms입니다
- 센서 데이터는 DataStore에 versioned data로 저장됩니다
- 모터 명령은 제어 알고리즘에서 계산된 값을 사용합니다
- EtherCAT 통신 에러 시 safe mode 전환은 RTStateMachine을 통해 처리됩니다
- DC 동기화는 P3 priority로, 기본 통신 후 추가 구현 가능합니다
- EtherCAT slave configuration은 YAML 파일로 작성되며, 시스템 시작 시 로드됩니다
- BLDC 모터와 서보 드라이버가 가장 일반적인 사용 사례이며, 우선 구현됩니다
- 범용 DI/AI 인터페이스는 표준 EtherCAT I/O 모듈을 통해 구현됩니다

## Out of Scope

- EtherCAT 슬레이브 펌웨어 개발 (타사 장비 사용 가정)
- EtherCAT 네트워크 진단 GUI 도구 (CLI 도구만 제공)
- Hot-swap 지원 (시스템 재시작 필요)
- 다중 EtherCAT 마스터 지원 (단일 마스터만 지원)
- EtherCAT over WiFi/Wireless (유선 연결만 지원)
- CoE 외 다른 프로토콜 지원 (EoE, FoE, SoE 등은 향후 확장)
