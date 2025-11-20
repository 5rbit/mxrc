# MXRC 코드베이스 구조

## 최상위 디렉토리
```
mxrc/
├── src/                    # 소스 코드
├── tests/                  # 테스트 코드
├── docs/                   # 문서
├── examples/               # 예제 코드
├── build/                  # 빌드 결과물 (생성됨)
├── logs/                   # 로그 파일
├── dev/                    # 개발 도구 및 문서
├── CMakeLists.txt          # CMake 빌드 설정
└── README.md               # 프로젝트 개요
```

## src/core 구조

### Action Layer
```
src/core/action/
├── interfaces/
│   ├── IAction.h                   # Action 인터페이스
│   └── IActionFactory.h            # Factory 인터페이스
├── core/
│   ├── ActionExecutor.{h,cpp}      # Action 실행기
│   ├── ActionFactory.{h,cpp}       # Action 생성 팩토리
│   └── ActionRegistry.{h,cpp}      # Action 타입 등록
├── dto/
│   ├── ActionStatus.h              # Action 상태
│   └── ActionDefinition.h          # Action 정의
├── impl/
│   ├── DelayAction.{h,cpp}         # 지연 Action
│   └── MoveAction.{h,cpp}          # 이동 Action
└── util/
    ├── ExecutionContext.h          # 실행 컨텍스트
    └── Logger.h                    # 로거
```

### Sequence Layer
```
src/core/sequence/
├── interfaces/
│   ├── ISequenceEngine.h           # 시퀀스 엔진 인터페이스
│   └── IConditionProvider.h        # 조건 제공자 인터페이스
├── core/
│   ├── SequenceEngine.{h,cpp}      # 시퀀스 실행 엔진
│   ├── SequenceRegistry.{h,cpp}    # 시퀀스 등록
│   ├── ConditionEvaluator.{h,cpp}  # 조건 평가
│   └── RetryHandler.{h,cpp}        # 재시도 처리
└── dto/
    ├── SequenceDefinition.h        # 시퀀스 정의
    ├── SequenceStatus.h            # 시퀀스 상태
    ├── ConditionalBranch.h         # 조건부 분기
    └── RetryPolicy.h               # 재시도 정책
```

### Task Layer
```
src/core/task/
├── interfaces/
│   ├── ITask.h                     # Task 인터페이스
│   ├── ITaskExecutor.h             # Executor 인터페이스
│   └── ITriggerProvider.h          # 트리거 제공자
├── core/
│   ├── TaskExecutor.{h,cpp}        # Task 실행기
│   ├── TaskRegistry.{h,cpp}        # Task 등록
│   ├── PeriodicScheduler.{h,cpp}   # 주기 스케줄러
│   ├── TriggerManager.{h,cpp}      # 트리거 관리자
│   └── TaskMonitor.{h,cpp}         # Task 모니터
└── dto/
    ├── TaskDefinition.h            # Task 정의
    ├── TaskExecution.h             # Task 실행 정보
    ├── TaskStatus.h                # Task 상태
    └── TaskExecutionMode.h         # 실행 모드
```

### Data Management Layer
```
src/core/datastore/
├── managers/
│   ├── ExpirationManager.{h,cpp}   # TTL/LRU 정책
│   ├── AccessControlManager.{h,cpp}# 접근 제어
│   ├── MetricsCollector.{h,cpp}    # 성능 메트릭
│   └── LogManager.{h,cpp}          # 로그 관리
├── DataStore.{h,cpp}               # Facade 인터페이스
└── MapNotifier.h                   # Map 변경 알림
```

### Event Layer
```
src/core/event/
├── interfaces/
│   ├── IEvent.h                    # 이벤트 인터페이스
│   └── IEventBus.h                 # 이벤트 버스 인터페이스
├── core/
│   ├── EventBus.{h,cpp}            # 이벤트 버스
│   └── SubscriptionManager.h       # 구독 관리
├── dto/
│   ├── EventType.h                 # 이벤트 타입
│   ├── EventBase.h                 # 이벤트 기본
│   ├── ActionEvents.h              # Action 이벤트
│   ├── SequenceEvents.h            # Sequence 이벤트
│   ├── TaskEvents.h                # Task 이벤트
│   └── DataStoreEvents.h           # DataStore 이벤트
├── util/
│   ├── LockFreeQueue.h             # SPSC 큐
│   └── MPSCLockFreeQueue.h         # MPSC 큐
└── adapters/
    └── DataStoreEventAdapter.{h,cpp} # DataStore 어댑터
```

### Logging Layer
```
src/core/logging/
├── interfaces/
│   └── IBagWriter.h                # Bag 라이터 인터페이스
├── core/
│   ├── AsyncWriter.{h,cpp}         # 비동기 쓰기
│   ├── SimpleBagWriter.{h,cpp}     # 단순 Bag 라이터
│   ├── DataStoreBagLogger.{h,cpp}  # DataStore 로거
│   ├── BagReader.{h,cpp}           # Bag 리더
│   └── BagReplayer.{h,cpp}         # Bag 재생
├── dto/
│   ├── BagMessage.h                # Bag 메시지
│   ├── BagFooter.h                 # Bag 푸터
│   ├── IndexEntry.h                # 인덱스 엔트리
│   └── RetentionPolicy.h           # 보관 정책
├── util/
│   ├── Serializer.{h,cpp}          # 직렬화
│   ├── FileUtils.{h,cpp}           # 파일 유틸
│   ├── Indexer.{h,cpp}             # 인덱서
│   └── RetentionManager.{h,cpp}    # 보관 관리
├── Log.h                           # 로그 초기화
└── SignalHandler.h                 # 시그널 핸들러
```

## tests 구조
```
tests/
├── unit/                   # 단위 테스트
│   ├── action/            # 26 tests
│   ├── sequence/          # 33 tests
│   ├── task/              # 53 tests
│   ├── datastore/         # 66 tests
│   ├── event/             # 42+ tests
│   └── logging/           # 17 tests
├── integration/           # 통합 테스트
└── benchmark/             # 벤치마크 테스트
```

## docs 구조
```
docs/
├── architecture/          # 아키텍처 문서
├── issue/                # 이슈 문서
├── research/             # 연구 문서
├── specs/                # 사양 문서
│   ├── 001-project-overview/
│   ├── 017-action-sequence-orchestration/
│   ├── 018-async-logging/
│   ├── 020-refactor-datastore-locking/
│   └── 021-rt-nonrt-architecture/
└── templete/             # 템플릿
```