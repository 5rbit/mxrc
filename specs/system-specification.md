팔렛트 셔틀 로봇 제어 시스템 개발 사양서

I. 프로젝트 개요 및 핵심 목표 (Project Overview and Objectives)

프로젝트 목표: 고효율 물류 자동화를 위한 24시간 무정지 팔레트 셔틀 로봇 제어 시스템 개발 및 현장 적용

1.1. 핵심 성능 지표 (KPI)

주행 속도: 최대 $\pm$ 2.5 m/s (최대 $\pm$ 2.0 m/s가속도)

위치 정밀도: $\pm$ 5 mm 이하 (목표 위치 도달 시)

임무 처리율 (Throughput): 현장 레이아웃에 따라 상이

1.2. 적용 환경

환경 상세: 상온(15°C ~ 35°C), 저습도(30%~60% RH), 미세 분진 환경 (Class 8/100,000 수준)

IP 등급 요구사항: $\ge$ 54

비고: 환경 및 IP 등급 명시

1.3. OS/개발 환경

내용: Linux Ubuntu 24.04 LTS, PREEMPT_RT, C++ std20 (STL, RAII, OOP), GUN, CMake, Git

보완/강화 사항: 강화 - POSIX Thread, Real-Time FIFO Scheduling 활용 명시.

II. 기능 개발 항목 (Feature Development)

2.1. Motion & Positioning (주행 및 위치 제어)

S-Curve Motion Profile

보완 내용: Motor 및 Drive의 정합성 검증을 위한 Trace 기능 포함. 프로파일 파라미터(Jerk, Accel, Velocity)를 GUI를 통해 동적 설정 가능해야 함.

기술 요구사항: 최소 2차(Jerk) 제어 구현. 튜닝 용이성을 위해 Parameter, Config Data 동적로드 연계 필수.

위치 보정 로직 (Localization)

보완 내용: 최종 위치 결정 방식은 RFID Tags, QR 코드 등 다양한 기술 후보군에 대한 FOC(First Of Concept) 테스트 및 평가 기준 검토를 통해 최종 결정.

기술 요구사항:

센서 퓨전: 최소 2가지 이상의 센서 데이터 (예: Odometry + 절대 위치 센서) 융합을 통한 위치 신뢰성 확보 로직 구현.

오류 내성: 외부 태그/마커 인식 실패 시, 관성 항법(Dead Reckoning) 모드로 자동 전환되어야 하며, Dead Reckoning 오차가 $\pm 20$ mm 초과 시 Critical 알람 발생.

정밀 보정 루틴: 목표 위치로부터 100 m 이내에서 $\pm 5$ mm 정밀도를 확보하기 위한 정밀 보정(Fine Positioning) 루틴 자동 활성화.

실시간 업데이트: 위치 정보는 Main Control Loop 주기(10 ms)를 준수하여 DataStore에 실시간 업데이트되어야 함.

주행 제어

보완 내용: Task 실행 중 오류 발생 시 **Task 정의 구조체(Task)**에 명시된 Failure Strategy(ABORT_MISSION, RETRY_TRANSIENT, SKIP_TASK)에 따라 자동 처리되어야 함. 최대 재시도 횟수(maxRetries)를 초과하거나 ABORT 전략 시 Critical 알람 발생과 함께 임무가 중단됨.

기술 요구사항: MissionManager와 Task Handler 모듈 간의 표준화된 오류 처리 인터페이스를 구현하고, Task 실패 시 롤백 및 에러 보고 로직을 연계하여 미션 단위의 신뢰성 확보.

안전 주행 및 영역 관리 (Safe Driving & Zone Management)

보완 내용: Motion Controller 레벨에서 레이저 센서(LDS) 데이터를 활용하여 실시간으로 안전 영역을 설정하고, 영역 침범 시 모션 프로파일에 직접 개입하여 안전 감속/정지 명령을 수행해야 함.

기술 요구사항:

Speed-Dependent Safety Field: 로봇의 현재 속도에 비례하여 보호 구역(Protective Field) 및 경고 구역(Warning Field)의 크기를 동적으로 조정하는 로직 구현.

Motion Interruption (AVOIDANCE 로직 연계 강화): 안전 구역 침범 시 S-Curve Motion Profile 제어 지령을 오버라이드하여 최대 Jerk 및 가속도를 준수하는 안전 감속 경로를 생성하고 Area Stop 상태로 천이한다. 이 때, AvoidReq 메시지를 MessageBus를 통해 StateMachine으로 전송하여 상태를 AVOIDANCE_PAUSED로 안전하게 천이시키고 모션 명령을 정지한다.

Obstacle Data Filtering: LDS 데이터의 노이즈 필터링 및 오경보 방지 로직을 구현하고, 충돌 방지 로직의 유효성 보장.

위치 보정 기술 평가 기준 (Criteria for Localization)

정확도 (Accuracy): 목표 $\pm 5$ mm 달성 여부

환경 견고성 (Robustness): 주변 환경 변화(먼지, 조명)에 대한 내성

업데이트 속도 (Latency): 10 ms 내 위치 정보 획득 및 반영 여부

유지보수 용이성 (Maintainability): 태그/비콘 설치 및 교체 난이도

2.2. Safety & Collision Avoidance (안전 및 충돌 방지)

Safety 인증 대응

보완 내용: 이상 발생 시 안전 정지 로직 (STO, ESTOP, Area Stop). ISO 3691-4 준수 목표 명시.

기술 요구사항: Safety PLC 또는 Safety Controller 연동 필수. 표준 준수 설계를 목표로 함.

충돌 방지 로직 (Collision Avoidance) - 다단계 Safety Field 및 자동 복구 로직 강화

보완 내용:

Safety Field 다단계 체계 재정의: LDS 센서의 안전 구역을 3단계의 명확한 체계로 재정의하고, 각 단계별 대응 액션을 명확히 구분한다.

상위 시스템 보고 및 자동 복구: 제어 정지 구역(Controlled Stop Field) 침범 시, AvoidReq 메시지를 상위 시스템에 보고하여 로봇을 일시 정지시키고, 장애물 해제 시 자동으로 이전 임무를 재개(AUTO 상태로 복구)할 수 있는 로직을 구현한다.

Deadlock 감지: 회피 일시 정지 상태가 장시간 지속될 경우 DEADLOCK 상태로 자동 천이하는 로직을 추가하여 시스템 정체를 방지한다.

기술 요구사항:

다단계 Safety Field 정의 및 동적 반영: LDS 센서의 안전 구역을 다음과 같이 3단계로 나누어 설정해야 하며, 각 구역의 범위(거리)는 Config File 또는 Parameter Store를 통해 동적으로 로드 및 변경될 수 있어야 한다. 로봇의 현재 속도에 따라 각 단계의 구역 크기를 비례적으로 조정하는 로직(Speed-Dependent Fields)을 구현한다.

Tier 1: Warning Field (경고 구역)

액션: 알람 발생 (Minor/Info 등급) 및 속도 유지.

보고: AVOID_INFO (DataStore 업데이트)

Tier 2: Controlled Stop Field (제어 정지 구역)

액션: S-Curve 기반 안전 감속을 통해 완전 정지.

보고: AvoidReq 메시지를 MessageBus를 통해 StateMachine에 전송하여 AVOIDANCE_PAUSED 상태로 천이 요청.

Tier 3: Emergency Stop Field (비상 정지 구역)

액션: 즉각적인 최대 가속도 기반의 Area Stop 또는 Hard Stop (ISO 3691-4 표준 준수).

보고: AlarmData (Critical 등급)를 AlarmHandler에 보고하고 StateMachine을 FAULT 또는 ESTOP 상태로 즉시 천이.

상위 시스템 연동, 자동 복구 및 Deadlock 처리:

MotionController는 Tier 2 침범 시 AvoidReq 메시지 발행 후 모션을 정지하며, StateMachine은 이를 수신하여 AUTO 상태를 AVOIDANCE_PAUSED 상태로 안전하게 천이시켜 현재 임무의 중단점을 확보해야 한다.

Deadlock 감지 및 전이: AVOIDANCE_PAUSED 상태로 진입 후 지정된 시간(예: 60초) 동안 AVOID_CLEAR 메시지를 수신하지 못할 경우 (즉, 장애물이 장시간 해제되지 않을 경우), StateMachine은 DEADLOCK 상태로 안전하게 천이해야 한다. 이 때 AlarmHandler에 DEADLOCK_DETECTED Critical 알람을 보고해야 한다.

장애물 감지 상황이 해제되고 Safety Field가 일정 시간(예: 500 ms) 동안 Clear 될 경우, MotionController는 AVOID_CLEAR 메시지를 MessageBus를 통해 발행한다.

StateMachine은 AVOID_CLEAR 메시지 수신 시, AVOIDANCE_PAUSED 상태에서 이전의 AUTO 상태 (예: DRIVING 또는 LOADING)로 자동으로 복구하여 임무를 재개해야 한다.

협업 기반 안전 연동: UWB/BLE 통신으로 수신된 타 로봇의 위치/속도 기반 데이터를 활용하여 안전 이격 거리를 확보한다. 이 거리를 위반하거나 충돌 예측 시, Deadlock 방지 알고리즘을 적용하여 강제 감속/정지 명령(Area Stop)을 수행하는 협업 기반의 충돌 방지 로직을 제공한다.

센서 상태 진단

보완 내용: LDS(레이저 거리 센서), Encoders, Safety Sensor 오염, 오류, 통신 이상 등 자체 상태 이상 감지 및 알람 발생 로직.

기술 요구사항: Alarm Handler 및 Log Handler와 연동.

2.3. Task & Mission Management: Componentization and Workflow (Task 및 임무 관리: 구성 요소화 및 워크플로우)

MissionManager는 로봇의 모든 행동을 **Task(구분 동작 단위)**와 이들을 묶는 **Sequence/Mission (워크플로우)**로 정의하여 실행합니다. 이는 임무의 신뢰성, 재사용성, 유연한 확장을 극대화하기 위함입니다.

2.3.1. Task (구분 동작 기능 단위) 구현 및 정의 (7.6.1 반영)

시스템의 모든 주요 동작은 Task라는 독립적이고 명확한 기능 단위로 구현되어야 합니다.

독립성 및 캡슐화 (Isolation & Encapsulation):

보완 내용: 각 Task는 최소한의 자율성을 가지며, 하나의 명확한 작업(예: DriveToPosition, LiftPallet, ChargeBattery)만을 수행해야 한다.

기술 요구사항: Task는 MissionManager와 DataStore에 대한 직접적인 의존성을 최소화하고, 모든 실행 로직을 execute() 메서드 내부에 캡슐화해야 한다. 외부 상태 변경은 오직 Task 입력/출력 및 DataStore 업데이트를 통해서만 이루어져야 한다.

재사용성 및 표준 인터페이스 (Reusability & Interface):

보완 내용: 모든 Task는 AbstractTask 인터페이스를 상속하여 구현되어야 한다. 이 인터페이스는 execute(), onPause(), onResume(), onAbort() 등의 표준 생명 주기 메서드를 정의한다.

기술 요구사항: Task Factory 패턴을 도입하여 상위 시스템의 요청(Task ID)에 따라 적절한 Task 구현체를 동적으로 생성할 수 있는 구조를 갖추어야 한다.

명확한 입/출력 정의 (Clear I/O):

보완 내용: Task 실행에 필요한 입력 매개변수(예: Target Position, Speed)와 Task 실행 결과(Success/Failure/Output Data)는 TaskContext 구조체를 통해 명확하게 정의되어 전달되어야 한다.

기술 요구사항: Task 실행 시점의 TaskContext는 MissionManager에 의해 관리되며, Task 완료 시 결과 데이터와 **실패 전략(FailureStrategy)**을 반환해야 한다.

2.3.2. Sequence 및 Mission 구성 구조 (워크플로우 오케스트레이션) (7.6.2 반영)

Task들을 논리적으로 연결하여 복잡한 임무(Mission)를 수행하는 Sequence 구조를 정의합니다.

조합 메커니즘 (Sequence Composition):

보완 내용: Task 간의 조합은 단순 순차 실행(Sequential) 외에도 조건부 분기(Conditional Branching - IF/ELSE), 반복(Looping - FOR/WHILE), 병렬 실행(Parallel) 등 복잡한 제어 흐름을 지원하는 워크플로우 오케스트레이션 구조를 갖추어야 한다. Behavior Tree 또는 계층적 State Machine 패턴을 활용하여 임무 구조 정의를 검토한다.

기술 요구사항: MissionManager는 Sequence의 실행을 전담하는 오케스트레이터(Orchestrator) 역할을 하며, Task의 실행 상태와 반환 값에 따라 다음 Task로의 천이를 결정해야 한다.

유연한 워크플로우 정의 및 관리:

보완 내용: Mission 정의는 JSON 또는 YAML 형태의 외부 설정 파일을 통해 로드 및 관리되어야 한다. 이를 통해 운영자는 로봇 코드를 재컴파일하지 않고도 새로운 임무를 신속하게 정의하고 적용할 수 있도록 유연성을 확보해야 한다.

기술 요구사항: Mission Configuration 파일은 **스키마 검증(Schema Validation)**을 필수적으로 수행해야 하며, 유효성이 확보된 Mission만 MissionManager의 큐에 등록할 수 있다.

데이터 및 상태 전달 (TaskContext Sharing):

보완 내용: Sequence 내에서 실행되는 Task 간에는 이전 Task의 출력 데이터 또는 상태 정보를 다음 Task의 입력으로 효율적으로 전달하고 공유할 수 있어야 한다.

기술 요구사항: MissionManager는 현재 Sequence에 대한 **공유 메모리 영역(SharedContext)**을 관리해야 하며, Task는 이 SharedContext를 통해 데이터를 읽고 쓸 수 있어야 한다. 모든 공유 데이터 접근은 스레드 안전성을 보장해야 한다.

2.3.3. Mission/Task 실행 관리 (Execution Management) - (기존 내용 보완)

임무 상태 및 시간 추적 (Task Audit Trail):

기술 요구사항: MissionState 및 TaskStatus 구조체를 DataStore에 저장하여 Task의 진행률, 순수 실행 시간(accumulatedExecutionTime), 및 **PAUSE 누적 시간(totalPauseDuration)**을 실시간으로 추적해야 함. Task 완료 시, 실행 결과와 시간을 포함하는 TaskHistory 구조를 LogModule에 기록하여 감사 이력을 확보해야 한다.

동적 임무 제어:

기술 요구사항: injectUrgentTask(BEFORE/AFTER) 및 reorderQueue 기능을 통해 상위 시스템/운영자가 임무 실행 중에도 Task 큐를 안전하게 조작할 수 있어야 함. (Critical Section 중 Task 삽입 거부 로직 필수)

오류 처리 및 복구:

기술 요구사항: Task 실행 중 오류 발생 시 **Task 정의 구조체(Task)**에 명시된 Failure Strategy(ABORT_MISSION, RETRY_TRANSIENT, SKIP_TASK)에 따라 자동 처리되어야 한다. 최대 재시도 횟수(maxRetries)를 초과하거나 ABORT 전략 시 Critical 알람 발생과 함께 임무가 중단됨.

화물/이송 로직:

기술 요구사항: 화물 확인 센서(Photo Sensor)의 데이터 신뢰성 확보 로직 및 재확인 절차를 Loading/Unloading Task 내부에 캡슐화해야 한다. 상위 제어기 Task 명령의 유효성 검증 및 실행 단계 추적 관리를 제공해야 한다.

2.4. Interface & Peripherals (인터페이스 및 주변 장치)

인터페이스 모듈 핵심 기능

통신 프로토콜 추상화 및 통합 처리: TCP/IP, EtherCAT, CANOpen, RFID 등 다양한 통신 프로토콜을 Interface Module을 통해 하나의 일관된 방식으로 처리하고 관리합니다.

실시간 데이터 연동 및 DataStore 반영: 외부 통신 채널에서 수집된 실시간 데이터를 시스템의 단일 진실 공급원(SSOT)인 DataStore에 스레드 안전하게 주기적으로 저장하여 모든 모듈이 최신 정보를 공유하도록 합니다.

주변 장치 인터락 및 순서 제어: 리프트, 충전기, 컨베이어(C/V) 등 주변 설비와의 안전한 상호 연동(Interlock) 순서를 관리하고, 통신 지연이나 응답 없음 상황에 대비하여 타임아웃 처리를 명확하게 수행합니다.

통신 오류 감지 및 자동 복구: 통신 채널의 상태 이상을 실시간으로 진단하고, 오류 발생 시 즉시 AlarmData를 통해 보고하며, 안정적인 서비스 유지를 위해 재연결(Reconnection) 전략을 통해 자동 복구를 시도합니다.

모듈 간 낮은 결합도 유지: 다른 모듈(예: StateMachine, MissionManager)과는 DataStore를 통해서만 간접적으로 데이터를 교환하도록 설계하여 모듈 간 직접적인 의존성을 최소화하고 유연성을 확보합니다.

상위 제어기(WMS/WCS) 연동 로직 (Upper Controller Interfacing)

보완 내용: WMS/WCS와의 통신 안정성을 극대화하기 위한 연결 이중화(Redundancy) 및 실패 감지/복구 전략을 포함해야 한다.

기술 요구사항:

Connection Health Check: 주기적인 Heartbeat 또는 Keep-Alive 메시지 교환을 통해 상위 제어기의 실시간 연결 상태를 감지하고, 지정된 응답 지연 시간(Latency) 초과 시 통신 실패로 간주한다.

Failover & Reconnection Policy: Primary 통신 채널 실패 시, **지정된 횟수와 간격(예: 3회 재시도, 5초 간격)**으로 자동 재연결(Reconnection)을 시도하는 정책을 구현해야 한다. 이 재연결 시도가 모두 실패하고 통신이 불가하다고 최종 판단되면, 명령 릴레이 기능 활성화를 위한 준비 상태로 천이해야 한다.

Command Relay Activation: 자동 재연결 정책이 모두 실패하여 통신이 불가할 경우, 인접한 로봇을 임시 통신 라우터로 활용하는 명령 릴레이(Command Relay) 기능을 활성화한다. 이를 통해 해당 로봇의 임무 명령을 P2P 통신을 통해 수신하고, 임무 수행 완료 후 릴레이 경로를 통해 상위 제어기 복구 시까지 임시 결과를 보고한다.

RFID, QR Read 로직 (위치 보정 및 화물 식별)

보완 내용: 멀티-리더/멀티-태그 환경에서의 데이터 경합 및 중복 처리 로직을 개선하고, 로봇의 동적 상태와 연계하여 식별 신뢰도를 극대화한다.

기술 요구사항:

데이터 융합 및 신뢰도 평가: 복수의 RFID/QR 리더기에서 수집된 데이터를 비교 분석하여, 가장 높은 신뢰도를 가진 데이터(예: RSSI 값, 인식 횟수)를 최종 식별 결과로 채택하는 로직을 구현한다.

동적 데이터 필터링 (Motion Filtering): 로봇의 **현재 속도(currentSpeed)**가 지정된 임계값(예: 0.1 m/s)을 초과할 경우, 해당 시점에 수집된 위치 보정 태그 데이터의 **가중치(Weight)**를 낮추거나 무시하여 위치 보정의 정확도를 유지한다.

중복 및 에러 관리: 중복 식별된 태그(예: 동일 QR 코드 5회 연속 인식)는 단일 이벤트로 처리하며, 인식 오류율(Error Rate)이 지정 임계값(예: 3%)을 초과하는 리더기에 대해서는 Alarm Handler를 통해 센서 오염 경고 알람을 발생시킨다.

비동기 처리 구조 설계: Interface Module 내부에 태그/코드 리더기로부터의 데이터를 전담하여 수신 및 처리하는 비동기 스레드 풀(ThreadPool)을 구성하고, 식별 결과는 DataStore에 비동기적으로 저장하여 Main Control Loop의 실시간성을 보장한다.

재확인 로직 (Verification): Task 실행 중 특정 지점에서 RFID/QR 식별이 임무 수행의 필수 조건일 경우, 초기 실패 시 소프트웨어 적으로 재인식 루틴 (예: 후진 100mm 후 재시도)을 자동 활성화하여 임무 연속성을 확보한다.

Peripheral Interface

보완 내용: 주변 설비 (Lift, Charger, C/V)와의 상태 기반 안전 인터락을 구현하고, Fail-Safe 원칙에 따라 통신 오류 및 타임아웃 상황을 처리하는 고신뢰성 모듈을 구현한다.

기술 요구사항:

인터페이스 표준화: 모든 주변 설비는 PeripheralInterface를 상속하며, 표준화된 Command (Start/Stop/Reset) 및 Status (Ready/Busy/Fault) 구조체를 사용하여 상호작용한다.

State-Based Interlock Management: 인터락 순서 및 상태 천이는 단순히 시간 기반이 아닌, 로봇의 StateMachine과 연동된 전용 Interlock State (예: ChargingState, LiftingState) 내에서 관리되어야 한다.

동적 타임아웃 및 오류 복구: 타임아웃 값은 주변 장치의 유형 및 현재 명령(Task)의 성격에 따라 동적으로 설정되어야 하며, 일시적 오류 발생 시 Task::FailureStrategy에 따라 자동 재시도 로직을 활성화한다.

오류 전파 및 진단: 주변 설비 통신 실패 또는 Interlock 오류 발생 시 즉시 AlarmData를 생성하여 Alarm Handler에 보고하며, 해당 알람은 StateMachine을 FAULT 상태로 천이시키는 주요 요인으로 작용한다.

다형성 극대화: Interlock Module은 각 주변 장치의 특수 로직을 캡슐화한 개별 클래스(예: LiftInterlock, ChargerInterlock)를 다형성 패턴을 통해 관리하고 실행한다.

2.5. 디지털/아날로그 I/O 관리 (Digital/Analog I/O Management)

보완 내용: 로봇에 사용되는 모든 디지털(DI/DO) 및 아날로그(AI/AO) I/O 신호를 하드웨어로부터 추상화하고, 운영 로직과 I/O 정의를 완벽하게 분리하여 유지보수성과 유연성을 극대화한다.

기술 요구사항:

하드웨어 추상화 계층(HAL): I/O 모듈의 물리적 접근 로직(예: EtherCAT 통신)은 I/O 관리 모듈의 핵심 동작 로직과 분리되어야 한다. 이를 통해 하드웨어가 변경되어도 제어기 로직의 수정 없이 HAL만 교체 가능해야 한다.

동적 설정 로드 (Dynamic Configuration): 모든 I/O의 정의 및 속성(Type, Delay, Filter)은 외부 Config File (JSON/YAML)을 통해 시스템 초기화 시 동적으로 로드되어야 한다.

디지털 I/O 속성 정의:

접점 정의: 센서 타입 (A접점/B접점) 정의 및 실시간 변경이 가능해야 하며, 입력 상태의 논리적 반전 처리를 지원해야 한다.

지연 기능 (Delay): OnDelay 및 OffDelay 기능을 지원하여, I/O 신호의 순간적인 노이즈를 필터링하고 안정적인 상태 인식을 보장해야 한다. (Delay 값은 Config 파일에서 ms 단위로 설정 가능해야 함.)

아날로그 I/O 필터링:

필터 설정: 아날로그 입력(AI) 센서 데이터의 노이즈를 제거하기 위해 이동 평균 필터(Moving Average Filter) 또는 저역 통과 필터(Low Pass Filter) 등의 필터 설정 파라미터(필터 계수, 윈도우 크기)를 Config 파일로 동적 설정 가능해야 한다.

Scale/Offset: 물리량 변환을 위한 스케일(Scale) 및 오프셋(Offset) 값 설정 기능 포함.

2.6. 이벤트/메시지 버스 (Event/Message Bus)

목적 및 역할: 시스템 전반의 모듈 간 비동기적 통신 및 메시지 전달을 위한 중앙 집중식 통로를 제공한다. 이는 모듈 간 직접적인 의존성을 제거하고 시스템의 유연성과 확장성을 확보하는 데 필수적이다.

기술 요구사항:

Publisher-Subscriber 패턴: 모든 모듈은 Message Bus를 통해 특정 이벤트에 **구독(Subscribe)**하거나, 이벤트를 **발행(Publish)**해야 한다. 직접적인 함수 호출을 지양하여 낮은 결합도를 유지해야 한다.

스레드 안전성 및 실시간성: 메시지 큐의 처리는 Lock-Free 또는 최소화된 동기화 기법을 사용하여 스레드 안전하게 구현되어야 하며, Main Control Loop의 실시간성 요구사항을 침해하지 않도록 처리 지연 시간(Latency)을 최소화해야 한다.

메시지 유형 정의: 알람, 상태 변경, 임무 진행 등 다양한 유형의 메시지 구조를 정의하고, 각 유형에 대한 우선순위 및 유효 기간(TTL, Time-To-Live)을 설정할 수 있어야 한다. (특히, AvoidReq 및 AVOID_CLEAR 메시지 유형이 정의되어야 함)

2.7. 알람 핸들러 (Alarm Handler)

목적 및 역할: 로봇 시스템에서 발생하는 모든 종류의 오류 및 경고 이벤트를 실시간으로 통합 관리하고, 알람의 발생/Clear 처리 및 등급별(Critical, Major, Minor, Info) 관리를 수행한다.

기술 요구사항:

알람 등급 및 심각도: 알람 코드는 심각도(Critical, Major, Minor, Info)와 발생 모듈(Module) 정보를 포함해야 하며, Critical 알람 발생 시 StateMachine이 즉시 FAULT 또는 ESTOP 상태로 천이하도록 연동되어야 한다.

알람 데이터 구조화: 모든 알람은 코드, 메시지, 발생 시간, 발생 모듈, 원인 및 대응 방안을 포함하는 구조화된 형식(AlarmData Structure)으로 DataStore에 저장되어야 한다.

Config 기반 관리: 알람 코드, 메시지, 원인 및 대응 방안은 외부 Config File (JSON/YAML)을 통해 동적으로 로드 및 관리되어야 한다. 이를 통해 현장 엔지니어가 알람 정의를 용이하게 수정할 수 있도록 한다.

이력 기록 및 감사: 발생한 모든 알람 및 Clear된 이력은 Log Handler와 연동되어 데이터베이스에 영구적으로 기록되어야 하며, 이는 시스템 진단 및 감사 이력에 활용된다.

2.8. 운영 환경 및 설정 데이터 관리 (Operational Configuration Management)

보완 내용: 로봇의 운영에 필수적인 레이아웃 정보 (적재/하역 위치 좌표, 충전소 위치, 경로 구역 정의 등) 및 **시스템 설정 값 (모션 프로파일, 센서 오프셋)**을 안전하게 관리하고 배포하는 메커니즘을 정의합니다.

기술 요구사항:

중앙 설정 서버 연동: HTTP/MQTT 등 표준 프로토콜을 사용하여 중앙 설정 서버(Configuration Server)로부터 JSON 또는 YAML 형태의 구조화된 데이터를 주기적으로 또는 필요 시점에 가져올 수 있어야 합니다.

파일 기반 로드 및 유효성 검증: 서버 연동 실패 상황에 대비하여, USB/로컬 파일 시스템에 저장된 설정 파일을 로드하는 기능을 제공해야 합니다. 로드된 데이터는 스키마 검증 및 운영 데이터 유효 범위 검사를 필수적으로 수행해야 합니다.

동적 적용 및 롤백: 로봇이 AUTOREADY 또는 PAUSE 상태일 때만 설정 변경을 허용해야 하며, 동적 적용 실패 시 자동으로 이전의 유효한 설정으로 롤백하는 안전 장치를 구현해야 합니다.

변경 이력 및 감사: 모든 설정 변경(서버 업데이트, 파일 로드, 수동 변경)은 변경 주체, 시간, 이전 값/새 값을 포함하여 LogModule에 기록되어야 하며, 이는 문제 발생 시 감사 이력으로 활용됩니다.

GUI 연동: GUI를 통해 운영자가 설정 데이터 파일을 직접 업로드하거나 서버로부터 최신 데이터를 가져오도록 명령할 수 있어야 합니다.

2.9. 데이터 영속성 및 무결성 (Data Persistence & Integrity)

보완 내용: 로봇의 현재 상태, 임무 진행 상황, 위치 정보 등 DataStore에 존재하는 동적 상태 데이터가 통신 단절, 비정상적인 전원 차단, 하드웨어 오류와 같은 예기치 않은 이벤트 발생 시에도 유실되지 않고 복구 가능하도록 보장하는 메커니즘을 정의합니다.

기술 요구사항:

비휘발성 데이터 저장소: NVRAM (Non-Volatile RAM), 임베디드 데이터베이스 (SQLite 등), 또는 고속 SSD를 활용하여 데이터 영속성을 확보해야 합니다.

주기적 체크포인트 (Periodic Checkpoint): 로봇의 핵심 상태 데이터 (예: 최종 위치, 현재 MissionID, TaskQueue 상태)를 정해진 시간 간격(예: 500 ms)마다 비휘발성 저장소에 동기적으로 기록해야 합니다.

임무 완료 시점 기록: Task의 SUCCESS 또는 FAIL과 같은 임무 생명 주기 전환 시점에는 즉시 해당 상태를 비휘발성 저장소에 기록하여 임무 진행 상태의 유실을 최소화합니다.

재부팅 시 상태 복구: 시스템 재부팅 후, 영속성 저장소에 기록된 최신 체크포인트 데이터를 DataStore에 로드하여 이전 상태로 복구해야 합니다. 복구된 상태는 State::FAULT 또는 State::MANUAL과 같이 안전한 초기 상태로 천이하기 위한 기반 데이터로 활용되어야 한다.

저장 무결성 검증: 저장된 데이터에 대해 CRC 또는 해시 검증을 수행하여 데이터 손상(Corruption) 여부를 확인하고, 손상된 경우 복구 실패 알람을 발생시키고 기본값으로 초기화해야 한다.

2.10. 예측 유지보수 및 오류 처리 (Predictive Maintenance & Fault Tolerance)

예측 유지보수 로직 (Predictive Maintenance)

보완 내용: Drive (전류, 토크 변동), Motor (온도, 진동), BMS (셀 불균형), 그리고 IMU 센서의 장기적인 드리프트/바이어스 변화 데이터를 포함한 다양한 센서 데이터를 분석하여, 시스템 고장 발생 전 **예측 진단 알람 (Predictive Diagnostic Alarm)**을 발생시키는 로직을 구현한다. 또한, 모든 모듈의 상태를 종합하여 **통합 건강 점수(Integrated Health Score)**를 산출하여 GUI에 제공한다.

기술 요구사항:

상태 진단 모델: 데이터 아카이브(DB) 기반의 상태 진단 모델 (Simple Heuristics 또는 Machine Learning)을 적용하여 고장 임박 여부를 판단한다.

IMU 드리프트 분석: IMU 자이로 및 가속도 데이터의 장기간 바이어스(Bias) 변화와 드리프트(Drift) 누적 속도를 정기적으로 분석하여, 위치 보정 시스템의 성능 저하를 예측하고 경고한다.

기계적 이상 진동 분석: Motor Drive 또는 전용 센서(IMU)의 고주파 진동(Vibration) 데이터를 실시간으로 모니터링하여 베어링 마모, 비정상적인 구동 소음 등 기계적 결함을 조기에 감지한다.

Health Score API: 모든 모듈 (Motion, Power, Localization, Peripheral)의 건전성 지표를 종합하여 0~100점 사이의 Health Score를 산출하고, 이를 Diagnostics Module의 표준 API를 통해 노출한다.

2.11. 상태 관리

아키텍처 개요: State 디자인 패턴을 적용하여 각 운영 상태별 로직을 독립적으로 캡슐화하고, 상태 천이(Transition)의 명확성과 안정성을 확보한다.

핵심 운영 상태: 시스템 초기화(BOOT), 수동 조작(MANUAL), 임무 대기(AUTOREADY), 자율 임무 실행(AUTO), 일시 정지(PAUSE), 회피 일시 정지(AVOIDANCE_PAUSED), 교착 상태(DEADLOCK), 오류(FAULT), 비상 정지(ESTOP) 등의 최상위 상태를 정의한다. (DEADLOCK 상태 추가 반영)

상태 천이 제어: 상태 천이 조건은 중앙 DataStore의 실시간 데이터(예: Safety, Alarm, Battery Level, AvoidReq 상태)를 기반으로 결정되며, **Change Notifier (Observer 패턴)**를 통해 데이터 변경에 즉각적으로 반응한다. 특히, AVOIDANCE_PAUSED 상태에서 지정된 Deadlock 타이머 만료 시 DEADLOCK 상태로 자동 천이하는 로직을 포함해야 한다.

계층적 상태 관리 (HSM): 복잡한 AUTO 상태 내부에는 DRIVING, LOADING 등 세부 동작을 관리하는 계층적 상태 머신(Sub-States) 구조를 도입하여 코드 복잡도를 관리한다.

리소스 관리: 모든 상태는 진입(enterState) 및 이탈(exitState) 시 해당 상태에서 사용한 리소스를 안전하게 활성화 및 정지/정리하는 의무 사항을 가진다.

2.12. Over-The-Air (OTA) 업데이트 및 펌웨어 관리

OTA 대응 로직

보완 내용: A/B Dual Partition 기반의 안전한 업데이트 환경 구현. HTTP API를 통한 패키지 다운로드 및 디지털 서명(Digital Signature) 검증 전략을 포함하여 보안 및 무결성을 극대화한다. 업데이트 중 로봇은 안전 영역으로 이동하여 임무를 정지(PAUSE 또는 AUTOREADY 상태)한 후 진행되어야 하며, MissionManager와 StateMachine은 OTA 상태를 실시간으로 인지해야 한다.

기술 요구사항:

A/B Partitioning & Atomic Switch: 현재 활성화된 파티션(A) 외의 비활성화된 파티션(B)에 패키지를 다운로드 및 설치한다. 설치 완료 후 아토믹(Atomic)한 방식으로 부트 파티션을 전환하여 업데이트의 안정성을 확보한다.

보안 무결성 검증 (Integrity & Authentication): 다운로드된 펌웨어 패키지에 대해 SHA-256 해시 검증과 더불어, ECDSA 또는 RSA 기반의 디지털 서명 검증을 필수적으로 수행하여 패키지의 변조 및 위변조 여부를 확인한다.

자동 롤백 (Automatic Rollback): 업데이트된 펌웨어로 부팅(BOOT 상태)한 후, 시스템 핵심 기능(Main Control Loop, 통신, 센서 초기화)에 대한 **부팅 후 건전성 검사(Post-Boot Health Check)**를 수행해야 한다. 검사 실패 시, Boot Loader는 자동으로 이전의 유효한 파티션으로 롤백해야 하며, 롤백 상태를 LogModule에 기록한다.

GUI 기반 펌웨어 업로드: 운영자가 GUI의 전용 OTA View를 통해 펌웨어 파일을 직접 업로드(HTTP POST 또는 Web Socket 기반 파일 전송)할 수 있어야 한다. 업로드된 파일은 서버에서 보안 검증 후 로봇으로 전송된다.

OTA 상태 보고: OTA 진행률, 현재 버전, 대상 버전, 설치/검증 성공 여부, 롤백 상태 등 모든 단계는 DataStore에 저장되고 상위 시스템 및 GUI에 실시간으로 보고되어야 한다.

권한 및 배포 관리: OTA 실행은 RBAC(Role-Based Access Control)에 의해 보호되어야 하며, 지정된 관리자만 실행할 수 있도록 통제한다. 또한, 단계별 배포(Staged Rollout) 전략을 지원할 수 있도록 설계하여, 일부 로봇에 먼저 업데이트를 적용하고 안정성을 검증하는 기능을 제공한다.

2.13. 추가 필수 핵심 모듈 (Crucial Additional Modules)

Diagnostics Module

목적 및 역할: 시스템 부하, CPU 사용량, 메모리 누수, 스레드 상태 등 RTOS 레벨의 성능 진단 및 모니터링.

패턴 적용: Health Check 및 Performance Monitoring API 제공.

Database Connector

목적 및 역할: PostgreSql 연동을 위한 독립 모듈. 커넥션 풀링(Connection Pooling) 및 쿼리 비동기 처리.

패턴 적용: ORM 라이브러리 (e.g. pqxx, Soci) 사용 검토.

경로 계획 및 최적화 엔진 (Path Planning & Optimization Engine)

목적 및 역할: 지도 데이터(Map Data)와 실시간 센서 데이터를 기반으로 최적화된 경로 및 속도 프로파일을 생성.

패턴 적용: Strategy Pattern (다양한 경로 탐색 알고리즘(A*, Dijkstra) 선택 적용을 위함).

보안 키 관리 모듈 (Secure Key Management)

목적 및 역할: 펌웨어 서명 검증 키, DB 접속 인증 정보 등 중요 보안 요소들을 안전하게 저장하고 관리.

패턴 적용: 하드웨어 기반 보안 모듈 (TPM 또는 Secure Element) 연동을 통한 키 보호.

III. Robot Controller 개발 기본 구현 사양 (Enhanced Architecture)

3.1. 아키텍처 및 구현 표준

모듈

내용

보완/강화 사항

데이터 관리

DataStore Module (중앙 집중식, 싱글톤)

강화 - 모든 동적 상태는 DataStore를 통해 접근하며, Lock-Free 또는 최소화된 동기화 기법 (e.g., Atomic, Mutex) 적용.

상태 관리

StateMachine (State 패턴 적용)

강화 - 상태 천이 (Transition) 시 DataStore의 특정 데이터에 대한 Change Notifier 패턴 (Observer) 연계. (특히 AvoidReq, AVOID_CLEAR, Deadlock 타이머 만료 상태 구독/처리 로직 추가)

하드웨어 추상화

IO, Drive, BMS Module

강화 - 각 모듈은 인터페이스(Interface)와 구현체(Implementation)를 분리하여 시뮬레이션 환경으로의 전환 용이성 확보 (Mocking).

Interface Module

Polymorphism (TCP/IP, Modbus TCP, EtherCAT, RFID, CANOpen) / 패턴: Factory Method

강화 - 인터페이스 초기화 및 통신 오류 시 재연결(Reconnection) 전략 명시.

IV. Robot GUI 개발 기본 구현 사양 (GUI Development)

4.1. 기술 및 아키텍처

영역

내용

비고

스택

Web기반 적응형, 반응형 GUI (Web Server/ SSR/ React framework)

Tailwind CSS를 활용한 반응형 UI 프레임워크 활용.

통신

Web Socket 기반의 실시간 양방향 데이터 통신 필수.

데이터 전송 부하 최소화를 위한 데이터 압축 및 최적화.

OTA 관리 모듈

WebBluetoothAPI 기반 OTA 관리.

주의 - WebBluetoothAPI 미지원 환경 대응 (대체: USB/Ethernet 직접 연결 통한 펌웨어 업데이트).

데이터 시각화

Real-Time Trace Plot 및 Log/Alarm 이력 조회 결과에 대한 시각화 (Charts).

recharts 등 React 기반 차트 라이브러리 사용.

4.2. 필수 View 및 상세 기능 보완

View 이름

상세 기능

DriveView (Commission)

Drive Setup, 실시간 전류/속도/위치 Trace Plot, 파라미터 백업/복원 기능 및 Drive 펌웨어 버전 관리 기능.

AlarmView / LogView

필터 및 조회 기능 강화 (시간, 등급, 모듈별 필터링). Log 상세 내용 (스택 트레이스) 확인 기능.

TaskView

현재 Task 진행률, Task Queue 조회, Task 강제 취소/우선순위 변경 (Supervisor/Admin 권한 명시).

StatusView

Drive, Sensor, IO 등 모든 모듈의 상태를 직관적인 색상/아이콘으로 표현하는 대시보드. (특히 AVOIDANCE_PAUSED 및 DEADLOCK 상태에 대한 명확한 UI 인디케이터 제공)

InterfaceView

주변 설비와의 인터페이스 프로토콜 통신 상태, RTT(왕복 시간) 모니터링, Interface Health Check 테스트 기능.

Manual (Jog) 기능

Safety Interlock 필수 적용. 속도/거리 단위 설정 기능 및 위치 제어/속도 제어 선택 조그 기능.

Audit/Archive

Log, Alarm, Task, 파라미터 변경 이력에 대한 DB Archive 및 변경 주체 (사용자 ID/시간) 기록.

예측 진단 View (P-Diag View)

예측 유지보수 모듈에서 수집된 Drive, Battery, Motor의 건강 점수(Health Score) 및 잔여 수명(RUL) 시각화.

OTA 관리 View (OTA Management View)

펌웨어 파일 업로드 (File Upload) 기능 (GUI를 통해 펌웨어 패키지를 서버에 직접 전송), 현재 로봇의 펌웨어 버전, 사용 가능(배포 가능) 펌웨어 목록 조회, 업데이트 진행 상태(다운로드, 검증, 설치, 재부팅, 롤백 상태) 실시간 모니터링, 그리고 업데이트 시작/취소 명령 기능(RBAC 권한 필요).

설정 관리 View (Configuration Management View)

레이아웃 설정 데이터 (예: 적재 위치, 충전소 좌표)의 서버 연동 및 로컬 파일 업데이트 기능. 현재 활성화된 설정 데이터 조회 및 시각화. 새로운 설정 적용 시 유효성 검증 결과 표시 및 롤백 이력 조회 기능. 중요 설정에 대한 변경 승인 요청 및 Audit Log 확인 기능.

V. 품질 및 테스트 전략 (Quality and Testing Strategy)

5.1. 테스트 계획

테스트 유형

내용

목표

단위 테스트 (Unit Test)

모든 C++ 모듈에 대한 Unit Test 코드 작성.

코드 커버리지 95% 이상 (C++ Google Test Framework 활용).

통합 테스트 (Integration Test)

모듈 간 상호 작용 및 하드웨어 추상화 계층의 유효성 검증. (특히, AvoidReq 메시지 전파, AVOIDANCE_PAUSED 상태 천이 및 자동 복구, 그리고 Deadlock 타이머 만료 시 DEADLOCK 상태로의 전이 로직 검증 포함)

HIL(Hardware-in-the-Loop) 테스트 환경 구축 검토.

실시간 성능 테스트

RT Preemption Latency, Main Control Loop 주기 (Cycle Time) 측정 및 모니터링.

제어 주기 jitter 10 $\mu s$ 이하 유지.

GUI 테스트

반응형/적응형 UI 및 실시간 데이터 업데이트 (Web Socket) 부하 테스트.

Cypress 기반 E2E(End-to-End) 테스트 자동화.

자율 주행 시뮬레이션 테스트

경로 계획 및 교통 관제 로직의 동작 검증을 위한 가상 환경 시뮬레이터 연동 테스트.



5.2. 개발 프로세스

영역

내용

도구

버전 관리

Git Flow 전략 기반 브랜치 관리 (Feature, Develop, Release, Hotfix).

Git

CI/CD

빌드, 정적 분석, Unit Test, 펌웨어 패키징 자동화.

Jenkins

코드 리뷰

모든 병합(Merge) 전 동료 검토 (Peer Review) 필수.

Gerrit 또는 GitHub/GitLab Pull Request 기능 활용.

문서화

Doxygen을 활용한 코드 레벨 API 문서 자동 생성.

Doxygen

VI. 보안 및 환경 설정 (Security and Configuration)

영역

내용

비고

펌웨어 무결성

OTA 패키지의 디지털 서명 검증 로직 구현.

서명 실패 시 업데이트 거부 및 롤백.

접근 제어 (Access Control)

GUI 및 수동 조작 (Jog) 기능에 대한 사용자 등급별 권한 관리 (Role-Based Access Control, RBAC).

Supervisor, Technician, Operator 등 최소 3등급 권한 분리.

네트워크 보안

관제 시스템 및 P2P 통신에 대한 데이터 암호화 (e.g., TLS/DTLS 검토).

포트 제한 및 방화벽 설정.

파라미터 관리

Parameter Handler를 통해 관리되는 모든 파라미터의 변경 이력 추적 및 승인 프로세스 검토.

중요 파라미터는 2인 승인 또는 Audit Log 기록 후 적용.

보안 부팅 (Secure Boot)

부팅 시 펌웨어의 무결성 및 출처를 검증하고, 서명되지 않은 코드는 실행을 거부하는 로직.

Trusted Platform Module (TPM) 또는 Secure Element 연동 검토.