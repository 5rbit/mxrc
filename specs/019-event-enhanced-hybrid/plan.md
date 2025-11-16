# 구현 계획: Event-Enhanced Hybrid Architecture

**브랜치**: `019-event-enhanced-hybrid` | **날짜**: 2025-11-16 | **사양서**: [spec.md](./spec.md)
**입력**: `/specs/019-event-enhanced-hybrid/spec.md` 의 기능 사양서

## 요약

MXRC의 Action, Sequence, Task 계층에 이벤트 기반 아키텍처를 도입하여 관찰 가능성(Observability)과 확장성을 향상시킵니다. 핵심 설계 원칙은 크리티컬 패스를 보호하면서 논블로킹 이벤트 시스템을 통해 모니터링 및 로깅 기능을 추가하는 것입니다.

**주요 요구사항**:

- 모든 주요 상태 전환(Action/Sequence/Task)에서 이벤트 발행
- Lock-Free Queue 기반 비동기 이벤트 처리
- DataStore와 EventBus의 양방향 통합
- 이벤트 발행 오버헤드 <5%, 처리 지연 <10ms
- 크리티컬 패스에 영향 없는 논블로킹 발행

**기술적 접근**:

1. **EventBus 중심 아키텍처**: 중앙 이벤트 허브로 Lock-Free Queue 사용
2. **하이브리드 패턴**: 제어 흐름은 직접 호출, 관찰은 이벤트로 분리
3. **Optional 통합**: 기존 코드는 EventBus 없이도 동작 (하위 호환성)
4. **DataStore 어댑터**: 기존 Notifier와 EventBus를 연결하는 어댑터 패턴

## 기술 컨텍스트

**언어/버전**: C++20, GCC 13.2, CMake 3.16+
**주요 의존성**:

- spdlog (로깅, 기존 사용 중)
- Google Test (단위/통합 테스트)
- Lock-Free Queue 라이브러리 (NEEDS CLARIFICATION: 자체 구현 vs Boost.Lockfree)
- std::thread, std::atomic, std::chrono (C++20 표준)

**저장소**: 메모리 기반 (EventQueue), DataStore (기존 구현)
**테스트**: Google Test (단위 45개, 통합 15개, 성능 벤치마크 3개 예상)
**대상 플랫폼**: Linux Ubuntu 24.04 LTS with PREEMPT_RT
**프로젝트 유형**: 실시간 로봇 컨트롤러 (단일 프로젝트)
**성능 목표**:

- 이벤트 발행 오버헤드: 기존 실행 시간 대비 <5%
- 이벤트 처리 지연: 99%ile <10ms
- Lock-Free Queue 처리량: >10,000 ops/sec
- 크리티컬 패스 실행 시간: 변화 없음 (논블로킹 보장)

**제약 조건**:

- 크리티컬 패스는 절대 블로킹되지 않아야 함 (실시간 제약)
- 이벤트 시스템 장애가 핵심 제어 로직에 영향을 주지 않아야 함
- 기존 ActionExecutor, SequenceEngine, TaskExecutor는 EventBus 없이도 동작해야 함 (하위 호환성)
- PREEMPT_RT 환경에서 이벤트 처리 스레드는 낮은 우선순위로 동작

**규모/범위**:

- 새로운 모듈: src/core/event/ (약 2000 라인 예상)
- 수정되는 기존 모듈: ActionExecutor, SequenceEngine, TaskExecutor, DataStore (각 100-200 라인 추가)
- 이벤트 타입: 약 20개 (Action 5개, Sequence 9개, Task 5개, DataStore 1개)
- 예상 테스트 케이스: 60개 (단위 45개 + 통합 15개)

## Constitution 준수 확인

*GATE: 0단계 연구 시작 전에 반드시 통과해야 합니다. 1단계 설계 후 다시 확인합니다.*

### I. 실시간성 보장

**상태**: ✅ **통과**

- 이벤트 발행은 논블로킹 방식 (tryPush, 실패 시 즉시 반환)
- 이벤트 큐 오버플로우 시에도 크리티컬 패스에 영향 없음
- 이벤트 처리는 별도 저우선순위 스레드에서 수행
- 성능 기준: 이벤트 발행 오버헤드 <5%, 크리티컬 패스 실행 시간 불변

**증거**: spec.md의 FR-1.3 (논블로킹 발행), NFR-1 (성능 요구사항), 위험 완화 전략

### II. 신뢰성 및 안전성

**상태**: ✅ **통과**

- RAII 원칙: std::shared_ptr로 이벤트 및 구독자 관리
- 예외 격리: 구독자 콜백 예외가 다른 구독자나 발행자에게 전파되지 않음
- 메모리 안전성: 스마트 포인터 사용, 수동 메모리 관리 없음
- 스레드 안전성: Lock-Free Queue, std::atomic, std::mutex 적절히 사용
- 잠재적 위험: 5가지 위험 요소 식별 및 완화 전략 문서화

**증거**: spec.md의 FR-3.2 (예외 격리), 위험 요소 섹션, RAII 설계 패턴

### III. 엄격한 테스트 주도 개발

**상태**: ✅ **통과**

- 단위 테스트: 45개 (EventBus 12, LockFreeQueue 8, SubscriptionManager 6, DataStoreAdapter 8, 계층별 11)
- 통합 테스트: 15개 (종단 간 플로우 8, 성능/안정성 7)
- 성능 벤치마크: 3개 (오버헤드, 지연, 처리량)
- 테스트 우선 접근: 각 컴포넌트별 테스트 전략 명시
- 성능 검증: 벤치마크를 통한 성능 기준 달성 확인

**증거**: spec.md의 테스트 전략 섹션, 성공 기준 (SC-P-001~003, SC-F-001~003, SC-R-001~002)

### IV. 모듈식 설계

**상태**: ✅ **통과**

- 새로운 event 계층 추가: src/core/event/ (기존 계층과 독립)
- Optional 통합: EventBus는 nullptr 기본값, 기존 코드는 EventBus 없이도 동작
- 명확한 API: IEvent, IEventBus 인터페이스 정의
- 느슨한 결합: 이벤트 시스템 장애가 크리티컬 패스에 영향 없음
- 확장 가능: 새로운 구독자 추가 시 기존 코드 수정 불필요

**증거**: spec.md의 구현 아키텍처 섹션, 디렉토리 구조, NFR-3 (확장성)

### V. 명확하고 일관된 한글 문서화

**상태**: ✅ **통과**

- spec.md: 완전한 한글 사양서 (요구사항, 아키텍처, 구현 가이드)
- plan.md: 한글 구현 계획 (이 문서)
- research.md, data-model.md, quickstart.md 생성 예정
- 용어 정리: 부록에 핵심 용어 한글 정의
- 코드 주석: 한글 주석 작성 예정

**증거**: spec.md 전체 (한글로 작성), 부록 A (용어 정리)

### VI. 소스코드 형상 관리 및 버전 관리

**상태**: ✅ **통과**

- Git 브랜치: 019-event-enhanced-hybrid
- Semantic Versioning: Phase 구분 (Phase 1A-1D)
- 변경 사항 추적: 기존 컴포넌트 수정 내역 명시
- 재현 가능성: 명확한 의존성 및 빌드 절차

**증거**: spec.md의 구현 단계 섹션, 의존성 섹션

### 종합 평가

**결과**: ✅ **모든 Constitution 원칙 준수**

이벤트 시스템은 크리티컬 패스를 보호하는 논블로킹 설계로 실시간성을 보장하고, RAII 및 예외 격리로 신뢰성을 확보하며, 포괄적인 테스트 전략으로 품질을 보증합니다. 모듈식 설계로 기존 시스템과의 느슨한 결합을 유지하며, 한글 문서화 및 버전 관리를 철저히 수행합니다.

## 프로젝트 구조

### 문서 (이 기능)

```text
specs/019-event-enhanced-hybrid/
├── spec.md              # 기능 사양서 (이미 작성됨)
├── plan.md              # 이 파일 (구현 계획)
├── research.md          # Phase 0 출력 (Lock-Free Queue 선택, 설계 패턴 연구)
├── data-model.md        # Phase 1 출력 (이벤트 타입, 구독 모델)
├── quickstart.md        # Phase 1 출력 (EventBus 사용 가이드)
├── contracts/           # Phase 1 출력 (이벤트 인터페이스 정의)
│   ├── IEvent.h         # 이벤트 기본 인터페이스
│   ├── IEventBus.h      # EventBus 인터페이스
│   └── events.yaml      # 모든 이벤트 타입 정의
└── tasks.md             # Phase 2 출력 (`/tasks` 명령어로 생성)
```

### 소스 코드 (리포지토리 루트)

```text
src/core/
├── event/                           # 🆕 새로운 이벤트 시스템 계층
│   ├── interfaces/
│   │   ├── IEvent.h                 # 이벤트 기본 인터페이스
│   │   └── IEventBus.h              # EventBus 인터페이스
│   ├── core/
│   │   ├── EventBus.{h,cpp}         # 중앙 이벤트 버스
│   │   ├── EventDispatcher.{h,cpp}  # 이벤트 처리 스레드
│   │   └── SubscriptionManager.{h,cpp} # 구독 관리
│   ├── dto/
│   │   ├── EventType.h              # 이벤트 타입 열거형
│   │   ├── EventBase.h              # 기본 이벤트 구조체
│   │   ├── ActionEvents.h           # Action 이벤트들
│   │   ├── SequenceEvents.h         # Sequence 이벤트들
│   │   ├── TaskEvents.h             # Task 이벤트들
│   │   └── DataStoreEvents.h        # DataStore 이벤트
│   ├── util/
│   │   ├── LockFreeQueue.{h,cpp}    # Lock-Free 큐
│   │   ├── EventFilter.h            # 이벤트 필터
│   │   └── EventStats.h             # 이벤트 통계
│   └── adapters/
│       └── DataStoreEventAdapter.{h,cpp} # DataStore ↔ EventBus 어댑터
│
├── action/                          # 기존 Action 계층 (수정)
│   └── core/
│       └── ActionExecutor.{h,cpp}   # EventBus 통합 추가
│
├── sequence/                        # 기존 Sequence 계층 (수정)
│   └── core/
│       └── SequenceEngine.{h,cpp}   # EventBus 통합 추가
│
└── task/                            # 기존 Task 계층 (수정)
    └── core/
        └── TaskExecutor.{h,cpp}     # EventBus 통합 추가

tests/
├── unit/
│   └── event/                       # 🆕 이벤트 시스템 단위 테스트
│       ├── EventBus_test.cpp        # 12 tests
│       ├── EventDispatcher_test.cpp
│       ├── SubscriptionManager_test.cpp # 6 tests
│       ├── LockFreeQueue_test.cpp   # 8 tests
│       └── DataStoreEventAdapter_test.cpp # 8 tests
│
└── integration/
    └── event/                       # 🆕 이벤트 시스템 통합 테스트
        ├── event_flow_test.cpp      # 종단 간 이벤트 플로우 (8 tests)
        └── event_performance_test.cpp # 성능 벤치마크 (7 tests)
```

**구조 결정**:

- 새로운 `src/core/event/` 모듈을 추가하여 이벤트 시스템을 캡슐화
- 기존 action, sequence, task 계층은 최소한의 수정 (EventBus 통합)
- 계층적 구조 유지: interfaces → core → dto → util → adapters
- 테스트도 동일한 구조로 구성 (unit/event, integration/event)

## 복잡성 추적

> **Constitution 준수 확인에 정당화가 필요한 위반 사항이 있는 경우에만 작성하십시오**

**해당 사항 없음** - 모든 Constitution 원칙을 준수하며 복잡성 위반 사항 없음.