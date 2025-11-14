# 구현 계획: 동작 시퀀스 관리 시스템 (Action Sequence Orchestration)

**브랜치**: `017-action-sequence-orchestration` | **날짜**: 2025-11-14 | **사양서**: specs/017-action-sequence-orchestration/spec.md
**입력**: `/specs/017-action-sequence-orchestration/spec.md` 의 기능 사양서

**참고**: 이 템플릿은 `/plan` 명령어로 채워집니다. 실행 워크플로우는 `.specify/templates/commands/plan.md` 를 참조하세요.

## 요약

로봇 제어를 위한 3계층 시스템(Action, Sequence, Task)을 단계적으로 구현합니다.

**구현 순서**: Action → Sequence → Task
- **Phase 1 - Action Layer**: 로봇 동작의 기본 정의 함수 구현
- **Phase 2 - Sequence Layer**: 여러 Action의 순차/조건부/병렬 실행 조율
- **Phase 3 - Task Layer**: Action/Sequence를 포함하며 주기적/트리거 방식 실행 지원

각 계층은 독립적으로 테스트 가능하며, 다음 계층은 이전 계층을 기반으로 구축됩니다.

**주요 기능**:
- **Action**: 타임아웃, 취소, 에러 처리
- **Sequence**: 순차/병렬/조건부 실행, 재시도, 모니터링
- **Task**: 단일 실행/주기적 실행/트리거 실행, TaskManager 통합

**Mission**: Task 완료 후 별도 구현 예정 (이 사양에 포함되지 않음)

## 기술 컨텍스트

**언어/버전**: C++20, GCC 13.2, CMAKE 3.16+
**주요 의존성**: RT-Linux, spdlog (로깅), Google Test (테스트)
**저장소**: 메모리 기반 (상태 저장), 향후 DB 통합 가능
**테스트**: Google Test Framework (GTest)
**대상 플랫폼**: Linux Ubuntu 24.04 LTS PREEMPT_RT
**프로젝트 유형**: 메인 컨트롤러 로직 (실시간 성능 필수)
**성능 목표**: 동작 실행 오버헤드 < 1ms, 1000개 동작 시퀀스 < 100MB 메모리
**제약 조건**:
  - 제어 루프 실시간성 위반 금지
  - 메모리 누수 없음 (RAII 원칙 준수)
  - 스레드 안전성 보장 (다중 시퀀스 동시 실행)
  - 최대 시퀀스 깊이: 10단계, 병렬 그룹 최대 32개 동작
**규모/범위**:
  - 1000개 이상 동작을 가진 시퀀스 처리
  - 6개 시나리오 (순차, 조건, 병렬, 재시도, 템플릿, 모니터링)
  - 10개 기능 카테고리 (41개 세부 요구사항)

## Constitution 준수 확인

*GATE: 0단계 연구 시작 전에 반드시 통과해야 합니다. 1단계 설계 후 다시 확인합니다.*

- **단계적 구현**: ✅ Action → Sequence → Task 순서로 구현. 각 단계 완료 후 다음 단계 시작. 각 계층은 독립적으로 테스트 가능.
- **실시간성 보장**: ✅ Action 실행은 1ms 이내 오버헤드. Task 주기적 실행은 별도 스레드에서 관리. 타임아웃 체크는 경량 타이머 기반.
- **신뢰성 및 안전성**: ✅ RAII 원칙 적용 (shared_ptr, unique_ptr 사용), std::any를 통한 타입 안전성 보장, 스레드 안전성을 위해 뮤텍스 기반 동기화.
- **테스트 주도 개발**: ✅
  - Phase 1: Action 컴포넌트 단위 테스트 + 통합 테스트
  - Phase 2: Sequence 컴포넌트 단위 테스트 + 통합 테스트 (Action 기반)
  - Phase 3: Task 컴포넌트 단위 테스트 + 통합 테스트 (Sequence 기반)
- **모듈식 설계**: ✅
  - Phase 1: IAction, IActionFactory 인터페이스
  - Phase 2: ISequenceEngine, IConditionProvider 인터페이스
  - Phase 3: ITask, ITaskExecutor, ITriggerProvider 인터페이스
  - 각 계층은 느슨한 결합으로 독립적 개발 가능
- **한글 문서화**: ✅ 모든 클래스, 메서드, 상수에 한글 주석 포함. 설계 문서는 한글로 작성.
- **버전 관리**: ✅ 현재 spec 버전 1.0.0. 구현 후 1.0.0 태그로 관리. API 변경 시 시맨틱 버전 준수.

## 프로젝트 구조

### 문서 (이 기능)

```text
specs/[###-feature]/
├── plan.md              # 이 파일 (`/speckit.plan` 명령어 출력)
├── research.md          # 0단계 출력 (`/speckit.plan` 명령어)
├── data-model.md        # 1단계 출력 (`/speckit.plan` 명령어)
├── quickstart.md        # 1단계 출력 (`/speckit.plan` 명령어)
├── contracts/           # 1단계 출력 (`/speckit.plan` 명령어)
└── tasks.md             # 2단계 출력 (`/speckit.tasks` 명령어 - `/speckit.plan`으로 생성되지 않음)
```

### 소스 코드 (리포지토리 루트)

```text
# 단일 프로젝트 (기본값)
├── src/                      # 💡 핵심 소스 코드 (Core Source Code)
│   ├── core/                 # 로봇의 상태 기계, 태스크 관리 등 상위 제어 로직
│   ├── controllers/          # PID, 역동역학 등 제어 알고리즘 구현
│   ├── hardware/             # 모터 드라이버, 센서 등 하드웨어 통신/인터페이스 (저수준)
│   ├── models/               # 로봇의 동역학 모델, Kinematics/Dynamics 정의 파일 (e.g., URDF, custom model)
│   ├── utils/                # 공통 유틸리티 함수, 수학 라이브러리 등
│   └── services/             # 통신, 로깅, 데이터베이스 연동 등 백그라운드 서비스
|
├── tests/                    # 🧪 테스트 코드
│   ├── unit/                 # 단위 테스트 (각 모듈/함수별)
│   └── integration/          # 통합 테스트 (Controller <-> Hardware Interface 등)
|
├── config/                   # ⚙️ 설정 파일 (Configurations)
│   ├── robot/                # 로봇 매개변수 (파라미터), 제어 이득(Gain) 등
│   └── system/               # 시스템 설정 (로그 레벨, 통신 포트 등)
|
├── tools/                    # 🛠 빌드, 배포, 펌웨어 업데이트 등 보조 스크립트
|
├── docs/                     # 📚 프로젝트 문서
│   ├── api/                  # API 문서 (Doxygen 등)
│   └── spec/                 # 설계 명세, 제어 알고리즘 설명 등
|
├── examples/                 # 🖼 예제 및 데모 코드
|
├── simulations/              # 💻 시뮬레이션 관련 파일 (선택 사항)
│   ├── assets/               # 시뮬레이션 환경 모델 (meshes 등)
│   └── launch/               # 시뮬레이션 실행 스크립트 (e.g., Gazebo launch files)
|
├── .gitignore
├── README.md
├── requirements.txt (또는 package.json, CMakeLists.txt 등)
└── LICENSE

**구조 결정**: [선택한 구조를 문서화하고 위에 캡처된 실제 디렉토리를 참조하십시오]

## 0단계: 연구 (Research)

### 핵심 기술 선택 사항 분석

**결정**: 스레드 풀 기반 병렬 실행 vs 비동기 이벤트 루프
- **선택**: 스레드 풀 (TaskExecutor 패턴)
- **이유**: TaskManager 통합을 위해 기존 스레드 풀 인프라 재사용
- **대안 검토**: 이벤트 루프는 더 가볍지만 기존 시스템과 통합 어려움

**결정**: 상태 저장 메커니즘
- **선택**: 메모리 기반 + 선택적 영속성 계층
- **이유**: 실시간 성능 보장, 로그는 메모리 기반으로 운영
- **대안 검토**: DB 직접 저장은 I/O 대기로 실시간성 위배

**결정**: 조건 평가 엔진
- **선택**: 간단한 식(Expression) 파서 (==, !=, <, >, AND, OR, NOT)
- **이유**: 복잡도 제한, 새로운 의존성 최소화
- **대안 검토**: Lua/Python은 의존성 증가, 더 복잡한 설계 필요

**결정**: 동작 결과 저장 방식
- **선택**: std::any를 사용한 타입 유연성
- **이유**: 다양한 동작 타입 지원, C++ 표준 기능만 사용
- **대안 검토**: void* 포인터는 타입 안전성 부족, Variant는 사전 타입 선언 필요

---

## 1단계: 설계 및 계약 (Phase 1: Design & Contracts)

### 데이터 모델 (Data Model)

#### 핵심 엔티티

**SequenceDefinition** (시퀀스 정의)
```cpp
struct SequenceDefinition {
    std::string id;                                    // 고유 식별자
    std::string name;                                  // 시퀀스 이름
    std::string version;                               // 버전 (e.g., "1.0.0")
    std::string description;                           // 설명
    std::vector<std::string> actionIds;               // 실행할 동작 ID 목록
    std::map<std::string, std::string> metadata;      // 메타데이터
};
```

**ActionExecutionResult** (동작 실행 결과)
```cpp
struct ActionExecutionResult {
    std::string actionId;                             // 동작 ID
    ActionStatus status;                              // 상태 (PENDING, RUNNING, COMPLETED, FAILED 등)
    float progress;                                    // 진행률 (0.0 ~ 1.0)
    std::string errorMessage;                         // 에러 메시지 (실패 시)
    long long executionTimeMs;                        // 실행 시간 (밀리초)
    int retryCount;                                    // 재시도 횟수
};
```

**SequenceExecutionResult** (시퀀스 실행 결과)
```cpp
struct SequenceExecutionResult {
    std::string executionId;                          // 실행 고유 ID
    std::string sequenceId;                           // 참조 시퀀스 ID
    SequenceStatus status;                            // 상태
    float progress;                                    // 전체 진행률
    std::vector<ActionExecutionResult> actionResults; // 각 동작의 실행 결과
    long long totalExecutionTimeMs;                   // 전체 실행 시간
};
```

#### 상태 머신

**ActionStatus** (동작 상태)
```
PENDING → RUNNING → PAUSED → COMPLETED
              ↓              ↑
           FAILED ←────────────
              ↓
         CANCELLED/TIMEOUT
```

**SequenceStatus** (시퀀스 상태)
```
PENDING → RUNNING → PAUSED → COMPLETED
              ↓              ↑
           FAILED ←────────────
              ↓
         CANCELLED
```

### API 계약 (API Contracts)

#### SequenceRegistry Interface
```cpp
class SequenceRegistry {
    void registerSequence(const SequenceDefinition& def);
    std::shared_ptr<const SequenceDefinition> getSequence(const std::string& id);
    bool hasSequence(const std::string& id);
    std::vector<std::string> getAllSequenceIds();
};
```

#### SequenceEngine Interface (구현 예정)
```cpp
class SequenceEngine {
    std::string execute(const std::string& sequenceId, const std::map<std::string, std::any>& params);
    void pause(const std::string& executionId);
    void resume(const std::string& executionId);
    void cancel(const std::string& executionId);
    SequenceExecutionResult getStatus(const std::string& executionId);
};
```

#### IAction Interface (동작 인터페이스)
```cpp
class IAction {
    virtual void execute(ExecutionContext& context) = 0;
    virtual void cancel() = 0;
    virtual ActionStatus getStatus() const = 0;
    virtual float getProgress() const = 0;
};
```

### 빠른 시작 (Quickstart)

#### 기본 시퀀스 정의 및 실행 예제 (향후 구현)
```cpp
// 1. 시퀀스 정의 생성
SequenceDefinition pickAndPlace;
pickAndPlace.id = "pick_and_place_v1";
pickAndPlace.name = "Pick and Place";
pickAndPlace.actionIds = {"grip_obj", "move_to_target", "release_obj"};

// 2. 레지스트리에 등록
SequenceRegistry registry;
registry.registerSequence(pickAndPlace);

// 3. 시퀀스 실행
SequenceEngine engine(registry, actionFactory);
std::string executionId = engine.execute("pick_and_place_v1", {});

// 4. 진행 상황 모니터링
while (true) {
    auto result = engine.getStatus(executionId);
    std::cout << "Progress: " << result.progress << "%" << std::endl;
    if (result.status == SequenceStatus::COMPLETED) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
```

---

## 2단계: 작업 생성 (Phase 2: Task Generation)

*주의: 작업 생성은 `/tasks` 명령어로 수행됩니다. 이 섹션은 최종 설계 후 자동 생성됩니다.*

---

## 복잡성 추적 (위반 사항 없음)

| 항목 | 상태 | 근거 |
|------|------|------|
| 새로운 모듈 (sequence) | ✅ 승인 | 기존 taskmanager와 독립적, 명확한 API 경계 |
| std::any 사용 | ✅ 승인 | C++ 표준, 타입 안전성 보장 (any_cast 사용) |
| 멀티스레딩 | ✅ 승인 | TaskExecutor 기반 스레드 풀 재사용, 뮤텍스 동기화 |
| 메모리 기반 저장소 | ✅ 승인 | 실시간성 보장, 영속성은 선택적으로 계층화 |