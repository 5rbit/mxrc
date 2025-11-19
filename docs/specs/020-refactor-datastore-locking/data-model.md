# 데이터 모델: DataStore

**날짜**: 2025-11-16
**기능**: DataStore 동시성 리팩토링

## DataStore 엔티티

`DataStore`는 시스템 전반의 데이터를 키-값(Key-Value) 형태로 저장하고 관리하는 중앙 저장소입니다. 이 리팩토링은 `DataStore`의 외부 데이터 모델을 변경하지 않고 내부 구현의 동시성 메커니즘만 개선하는 것을 목표로 합니다.

### 1. 키 (Key)

-   **이름**: `key`
-   **타입**: `std::string`
-   **설명**: `DataStore` 내에서 특정 데이터를 고유하게 식별하는 문자열 식별자입니다.

### 2. 값 (Value)

-   **이름**: `value`
-   **타입**: `std::any` (또는 `std::variant`와 같은 타입 안전한 variant 타입)
-   **설명**: `DataStore`에 저장되는 실제 데이터입니다. 다양한 C++ 타입(예: `int`, `double`, `bool`, `std::string`, 사용자 정의 구조체 등)을 저장할 수 있도록 타입 소거(Type Erasure) 또는 variant 타입을 사용합니다.

### 3. 내부 저장소 (Internal Storage)

-   **타입**: `Concurrent Hash Map` (예: `tbb::concurrent_hash_map` 또는 유사한 동시성 컨테이너)
-   **설명**: `key`와 `value` 쌍을 저장하는 데 사용되는 내부 자료구조입니다. 동시성 해시 맵은 여러 스레드에서 동시에 안전하고 효율적으로 접근할 수 있도록 설계되었습니다.

### 4. 관계

-   `DataStore`는 독립적인 엔티티이며, 다른 시스템 엔티티(예: `Action`, `Sequence`, `Task`)는 `DataStore`의 키-값 인터페이스를 통해 데이터를 읽고 씁니다.
-   이 리팩토링은 `DataStore`와 다른 엔티티 간의 관계에 영향을 미치지 않습니다.
