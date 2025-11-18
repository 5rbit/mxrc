# 구현 계획: DataStore 동시성 리팩토링

**브랜치**: `020-refactor-datastore-locking` | **날짜**: 2025-11-16 | **사양서**: [./spec.md](./spec.md)
**입력**: `/specs/020-refactor-datastore-locking/spec.md` 의 기능 사양서

**참고**: 이 템플릿은 `/speckit.plan` 명령어로 채워집니다. 실행 워크플로우는 `.specify/templates/commands/plan.md` 를 참조하세요.

## 요약

이 기능은 `DataStore` 모듈의 심각한 성능 병목 현상을 해결하기 위한 것입니다. 현재 `DataStore`는 모든 쓰기 작업에 단일 전역 락(Global Lock)을 사용하여 동시성 환경에서 시스템 안정성과 처리량을 저해합니다. 이 계획은 `DataStore`의 내부 락 메커니즘을 동시성 해시 맵(Concurrent Hash Map) 기반으로 리팩토링하여, 여러 스레드가 서로 다른 키에 동시에 접근할 때 블로킹 없이 작업을 처리하고, 기존 인터페이스를 유지하면서 데이터 무결성을 보장하는 것을 목표로 합니다.

## 기술 컨텍스트

**언어/버전**: C++20, GCC 13.2, CMAKE
**주요 의존성**: RT-Linux, Eigen, spdlog
**저장소**: 내부 메모리 및 저장소, PostgreSQL, SQLite
**테스트**: Google Test
**대상 플랫폼**: Linux Ubuntu 24.04 LTS, PREEMPT_RT
**프로젝트 유형**: 메인 컨트롤러 로직 + WebServer + Client GUI
**성능 목표**: 제어 루프 <1ms 및 시스템 다운율 0%에 수렴
**제약 조건**: <200ms p95 응답 시간, <100MB 메모리 사용, 오프라인 작동 지원
**규모/범위**: 10k 동시 사용자 지원, 1M 라인 코드, 50개 사용자 인터페이스 화면

**기능별 컨텍스트**:
- **현재 문제**: `DataStore`는 모든 `set` 연산에 단일 전역 `static` 뮤텍스를 사용하여 동시 접근 시 직렬화 및 성능 병목 현상을 유발합니다. 이로 인해 `DataStoreEventAdapter`의 동시성 테스트에서 타임아웃이 발생합니다.
- **영향받는 컴포넌트**: `DataStore` (`src/core/datastore/DataStore.h`, `src/core/datastore/DataStore.cpp`), `EventBus` (`src/core/event/core/EventBus.h`), `DataStoreEventAdapter` (`src/core/event/adapters/DataStoreEventAdapter.cpp`), 그리고 `DataStore`와 상호작용하는 모든 태스크 모듈.
- **목표**: `DataStore`의 내부 락 메커니즘을 리팩토링하여, 여러 스레드가 서로 다른 키에 동시에 쓸 때 블로킹 없이 작업을 처리하고, 동일한 키에 대한 쓰기 시 데이터 무결성을 보장합니다. 기존 `DataStore` 인터페이스는 변경 없이 유지되어야 합니다.
- **핵심 결정 지점**: `DataStore`의 내부 저장소를 위한 동시성 데이터 구조/잠금 메커니즘 선택.

## Constitution 준수 확인

*GATE: 0단계 연구 시작 전에 반드시 통과해야 합니다. 1단계 설계 후 다시 확인합니다.*

- **실시간성 보장**: 전역 락 제거 및 동시성 데이터 구조 도입은 예측 불가능한 블로킹 시간을 줄여 실시간 제약 조건 준수에 긍정적인 영향을 미칠 것입니다.
- **신뢰성 및 안전성**: 검증된 동시성 데이터 구조를 사용함으로써 스레드 안전성을 강화하고, 기존의 전역 락으로 인한 잠재적 데드락 및 데이터 손상 위험을 완화합니다. C++20 표준 및 MISRA C++ 코딩 표준을 준수하여 구현될 것입니다.
- **테스트 주도 개발**: 기능 사양서에 명시된 `DataStoreEventAdapter` 동시성 테스트를 포함하여 포괄적인 단위 및 통합 테스트를 통해 변경 사항을 검증할 것입니다.
- **모듈식 설계**: `DataStore`의 내부 구현을 리팩토링하되, 외부 인터페이스(`get`, `set`, `delete`)는 변경하지 않으므로 기존 시스템의 모듈성을 유지하고 복잡성을 불필요하게 증가시키지 않습니다.
- **한글 문서화**: 모든 설계 결정, 구현 세부 사항 및 API 변경(없음)은 한글로 명확하게 문서화될 것입니다.
- **버전 관리**: 이 기능은 Semantic Versioning을 준수하는 브랜치(`020-refactor-datastore-locking`)에서 개발되며, 변경 사항은 명확하게 관리될 것입니다.

## 프로젝트 구조

### 문서 (이 기능)

```text
specs/020-refactor-datastore-locking/
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

**구조 결정**: 기존 프로젝트 구조를 유지하며, `src/core/datastore/` 내부 구현만 변경됩니다.

## 복잡성 추적

> **Constitution 준수 확인에 정당화가 필요한 위반 사항이 있는 경우에만 작성하십시오**

| 위반 사항 | 필요한 이유 | 더 간단한 대안이 거부된 이유 |
|-----------|------------|-------------------------------------|
|           |            |                                     |
