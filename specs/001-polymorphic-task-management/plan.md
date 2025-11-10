# 구현 계획: 다형적 Task 관리 모듈

**브랜치**: `001-polymorphic-task-management` | **날짜**: 2025-11-05 | **사양서**: /specs/001-polymorphic-task-management/spec.md
**입력**: `/specs/001-polymorphic-task-management/spec.md` 의 기능 사양서

**참고**: 이 템플릿은 `/speckit.plan` 명령어로 채워집니다. 실행 워크플로우는 `.specify/templates/commands/plan.md` 를 참조하세요.

## 요약

다형적 Task 관리 모듈은 Task의 종류에 상관 없이 Task를 할당 받고 실행할 수 있도록 변경됩니다. 언제든 새로운 유형의 Task를 정의하고 쉽게 추가할 수 있으며 (다형성), 이는 시스템의 확장성과 유연성을 크게 향상시킵니다. 외부 시스템과의 통합은 전용 어댑터/플러그인 계층을 통해 이루어지며, Task 생명주기 이벤트 로깅 및 상태/진행률 메트릭 제공을 통해 관측성을 확보합니다.

## 기술 컨텍스트

**언어/버전**: C++20, GCC 13.2, CMAKE
**주요 의존성**: RT-Linux, Eigen, spdlog
**저장소**: 내부 메모리 및 저장소, PostgreSQL, SQLite
**테스트**: Google Test
**대상 플랫폼**: Linux Ubuntu 24.04 LTS, PREEMPT_RT
**프로젝트 유형**: 메인 컨트롤러 로직 + WebServer + Client GUI
**성능 목표**: 제어 루프 < 1ms 및 시스템 다운율 0%에 수렴
**제약 조건**: Task 관리 모듈은 다른 모듈과의 낮은 결합도를 유지해야 하며, SOLID 원칙을 준수하여 독립적으로 실행 가능해야 합니다.
**규모/범위**: Task의 종류에 상관 없이 일관된 내용으로 다양하게 처리 가능하도록 지원합니다.

## Constitution 준수 확인

*GATE: 0단계 연구 시작 전에 반드시 통과해야 합니다. 1단계 설계 후 다시 확인합니다.*

- **실시간성 보장**: Pass (이 기능은 Task 실행을 비실시간 요구사항으로 가정하며, 직접적인 실시간 제약 조건을 도입하지 않습니다.)
- **신뢰성 및 안전성**: Partial (FR-009에 따라 현재 단계에서는 접근 제어를 구현하지 않으므로, 이 부분은 잠재적 위험으로 남아있습니다.)
- **테스트 주도 개발**: Pass (새로운 Task 유형의 정의 및 등록, 실행, 다형적 동작 확인에 대한 사용자 스토리가 명확하며, 각 스토리에 대한 독립 테스트 기준이 제시되어 있습니다. 이는 TDD 원칙에 부합합니다.)
- **모듈식 설계**: Pass (`ITask` 인터페이스와 `registerTaskType` 메커니즘은 개방-폐쇄 원칙을 강력하게 지원하여 새로운 Task 유형 추가 시 기존 코드 수정 없이 확장을 가능하게 합니다. 어댑터/플러그인 계층을 통한 외부 시스템 통합도 모듈성을 강화합니다.)
- **한글 문서화**: Pass (`research.md`, `data-model.md`, `quickstart.md`, `contracts/` 파일들이 한글로 작성되었으며, 설계 결정이 명확하게 문서화되었습니다.)
- **버전 관리**: Pass (이 기능 자체는 버전 관리 방식에 직접적인 영향을 주지 않지만, 프로젝트 전체적으로 Semantic Versioning을 준수할 것으로 가정합니다.)

## 프로젝트 구조

### 문서 (이 기능)

```text
specs/001-polymorphic-task-management/
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
│   │   └── task/             # Task 관리 모듈 관련 코드 (기존 Task, TaskManager, OperatorInterface)
│   │   └── task/polymorphic/ # 다형적 Task 관리 모듈 관련 코드 (ITask, ITaskManager, IOperatorInterface 구현체)
│   ├── controllers/          # PID, 역동역학 등 제어 알고리즘 구현
│   ├── hardware/             # 모터 드라이버, 센서 등 하드웨어 통신/인터페이스 (저수준)
│   ├── models/               # 로봇의 동역학 모델, Kinematics/Dynamics 정의 파일 (e.g., URDF, custom model)
│   ├── utils/                # 공통 유틸리티 함수, 수학 라이브러리 등
│   └── services/             # 통신, 로깅, 데이터베이스 연동 등 백그라운드 서비스
|
├── tests/                    # 🧪 테스트 코드
│   ├── unit/                 # 단위 테스트 (각 모듈/함수별)
│   │   └── task/             # Task 관리 모듈 단위 테스트
│   │   └── task/polymorphic/ # 다형적 Task 관리 모듈 단위 테스트
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

**구조 결정**: 다형적 Task 관리 모듈 관련 코드는 `src/core/task/polymorphic/` 디렉토리 내에 위치하며, 관련 단위 테스트는 `tests/unit/task/polymorphic/`에 위치한다. 이는 기존 프로젝트의 모듈화된 구조를 따르며, Task 관리 모듈의 독립성을 보장한다.

## 복잡성 추적

> **Constitution 준수 확인에 정당화가 필요한 위반 사항이 있는 경우에만 작성하십시오**

| 위반 사항 | 필요한 이유 | 더 간단한 대안이 거부된 이유 |
|-----------|------------|-------------------------------------|
| II. 최고 수준의 신뢰성 및 안전성 (FR-009) | 현재 개발 단계의 범위 및 우선순위를 고려하여 Task 정의/실행에 대한 접근 제어(Authorization)를 구현하지 않습니다. | 초기 개발 단계에서 기능 구현에 집중하고, 추후 보안 요구사항이 명확해지면 추가 구현 예정입니다. |
