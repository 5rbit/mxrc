# 빠른 시작 가이드: Task & Mission Management 리팩터링

**브랜치**: `004-task-mission-management` | **날짜**: 2025-11-04 | **사양서**: [spec.md](./spec.md)

## 개요

이 문서는 `DataStore` 의존성이 제거된 `Task & Mission Management` 모듈을 빌드하고 테스트하는 방법을 안내합니다. 리팩터링의 핵심은 `MissionManager`가 `IDataStore` 인터페이스에만 의존하도록 변경된 것입니다.

## 전제 조건

- C++20을 지원하는 컴파일러 (GCC 13.2 이상 권장)
- CMake 3.16 이상
- Google Test 라이브러리 (프로젝트에 내장되어 있거나 시스템에 설치되어 있어야 함)

## 빌드 방법

1.  **빌드 디렉터리 생성**:
    프로젝트 루트에서 빌드 디렉터리를 생성하고 이동합니다.

    ```bash
    mkdir -p build
    cd build
    ```

2.  **CMake 실행**:
    CMake를 실행하여 빌드 파일을 생성합니다. 테스트 빌드를 활성화하는 옵션을 포함해야 할 수 있습니다.

    ```bash
    cmake .. -DBUILD_TESTS=ON
    ```

3.  **컴파일**:
    `make` 명령어를 사용하여 전체 프로젝트를 컴파일합니다.

    ```bash
    make -j$(nproc)
    ```

## 테스트 방법

리팩터링의 성공 여부를 검증하기 위해 단위 테스트와 통합 테스트를 실행하는 것이 중요합니다.

### 1. 단위 테스트 실행

`MissionManager`가 `IDataStore`의 구체적인 구현 없이도 독립적으로 동작하는지 확인하는 테스트를 실행합니다. 테스트에서는 `MissionManager::getInstance()` 호출 시 `IDataStore`의 모의(Mock) 객체를 주입하여 사용합니다.

빌드 디렉터리에서 `ctest`를 실행하거나, 특정 테스트 실행 파일을 직접 실행할 수 있습니다.

```bash
# 빌드 디렉터리에서 실행

# 모든 테스트 실행
ctest

# 또는 MissionManager 관련 테스트만 실행 (실행 파일 이름은 실제와 다를 수 있음)
./tests/unit/task/MissionManager_test
```

### 2. 주요 테스트 시나리오

- **`MissionManager` 초기화 테스트**: `IDataStore`의 모의 객체를 주입하여 `MissionManager`가 정상적으로 생성되는지 확인합니다.
- **Mission 실행 테스트 (In-Memory)**: `DataStore` 없이도 Mission이 시작, 실행, 완료되는지 확인합니다. 상태 변경 이력은 메모리 내에서만 기록됩니다.
- **상태 복구 테스트 (실패 시나리오)**: `getPendingMissionIds`가 빈 벡터를 반환하도록 모의 객체를 설정했을 때, `MissionManager`가 복구 로직을 수행하지 않는지 확인합니다.
- **의존성 확인**: `MissionManager`의 컴파일 의존성을 확인하여 `DataStore`의 구체적인 구현 클래스에 대한 `include`가 없는지 검증합니다.

## 예상 결과

- 모든 빌드 과정이 성공적으로 완료되어야 합니다.
- `MissionManager` 및 관련 컴포넌트의 모든 단위 테스트가 통과해야 합니다.
- 테스트 결과, `MissionManager`가 `IDataStore` 인터페이스 외에 다른 영속성 관련 구체 클래스에 의존하지 않음이 증명되어야 합니다.
