# MXRC 코드 스타일 및 규약

## 설계 원칙

### 1. RAII (Resource Acquisition Is Initialization)
- 모든 리소스는 생성자에서 할당, 소멸자에서 해제
- 스마트 포인터 사용 (shared_ptr, unique_ptr)
- 수동 메모리 관리 금지

### 2. 인터페이스 기반 설계
- 모든 확장 지점에 인터페이스 제공 (I-prefix: IAction, ITask, etc.)
- 의존성 주입 (Dependency Injection)
- 느슨한 결합 (Loose Coupling)

### 3. OOP 원칙
- 모듈 간 상호 의존성(Chaining) 최소화
- 고성능 아키텍처에 초점
- 명확한 상태 머신 구현

## 디렉토리 구조 규약

### 계층별 디렉토리
```
src/core/<layer>/
├── interfaces/     # I-prefix 인터페이스 (순수 가상 클래스)
├── core/          # 핵심 구현체
├── dto/           # Data Transfer Objects
├── impl/          # 구체적 구현체
└── util/          # 유틸리티 클래스/함수
```

### 네이밍 규약
- **클래스**: PascalCase (ActionExecutor, TaskRegistry)
- **인터페이스**: I-prefix (IAction, ITaskExecutor)
- **파일**: 클래스 이름과 동일 (ActionExecutor.h, ActionExecutor.cpp)
- **네임스페이스**: 소문자 (mxrc::core::action)

## 테스트 규약

### 테스트 구조
```
tests/
├── unit/           # 단위 테스트 (각 클래스별)
│   ├── action/
│   ├── sequence/
│   ├── task/
│   └── datastore/
└── integration/    # 통합 테스트 (계층 간 상호작용)
```

### 테스트 네이밍
- **파일**: `<ClassName>_test.cpp`
- **테스트 스위트**: `TEST(<ClassName>, <TestCase>)`
- **예시**: `TEST(ActionExecutor, Execute_ValidAction_ReturnsSuccess)`
- **테스트 기능 명시**: `Execute_ValidAction_ReturnsSuccess : <간략설명>`
- **테스트 주석**: 한글로 주요한 내용만 주석 (기술적 용어는 영어)

## 로깅 규약

### 로그 레벨
- **trace**: 상세한 실행 흐름
- **debug**: 디버깅 정보
- **info**: 일반 정보
- **warn**: 경고
- **error**: 오류
- **critical**: 치명적 오류 (즉시 플러시됨)

### 로그 사용
```cpp
spdlog::info("Task {} completed successfully", taskId);
spdlog::error("Action {} failed: {}", actionId, error);
```

## 메모리 안전

### AddressSanitizer
- 모든 빌드에서 항상 활성화
- 메모리 누수, 버퍼 오버플로우, use-after-free 자동 감지
- 테스트 실행 시 메모리 안전성 검증 필수