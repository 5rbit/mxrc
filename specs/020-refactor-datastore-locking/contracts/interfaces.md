# API 계약: DataStore 인터페이스

**날짜**: 2025-11-16
**기능**: DataStore 동시성 리팩토링

## 개요

이 문서는 `DataStore` 모듈의 공개 인터페이스(API)를 정의합니다. `DataStore` 동시성 리팩토링 기능은 `DataStore`의 내부 구현을 개선하는 것을 목표로 하며, **기존의 공개 인터페이스는 변경되지 않습니다.** 따라서 이 계약은 현재 `DataStore`가 제공하는 기능을 명확히 하고, 리팩토링 후에도 동일하게 유지됨을 보장합니다.

## DataStore 클래스 인터페이스

`DataStore`는 싱글톤 패턴으로 구현되며, 다음의 주요 메서드를 통해 데이터에 접근합니다.

### 1. `static DataStore& instance()`

-   **설명**: `DataStore`의 싱글톤 인스턴스를 반환합니다. 모든 `DataStore` 작업은 이 인스턴스를 통해 수행됩니다.
-   **반환 타입**: `DataStore&`
-   **예외**: 없음

### 2. `template<typename T> bool set(const std::string& key, const T& value)`

-   **설명**: 지정된 `key`에 `value`를 저장합니다. `value`의 타입은 템플릿 인자 `T`로 추론됩니다. `DataStore`는 내부적으로 `std::any` 또는 유사한 메커니즘을 사용하여 다양한 타입의 값을 저장할 수 있습니다.
-   **매개변수**:
    -   `key`: `const std::string&` - 데이터를 식별하는 고유 키.
    -   `value`: `const T&` - 저장할 데이터.
-   **반환 타입**: `bool` - 성공적으로 저장되면 `true`, 그렇지 않으면 `false` (예: 키가 유효하지 않거나 저장 공간 부족 등).
-   **예외**: 없음

### 3. `template<typename T> bool get(const std::string& key, T& out_value) const`

-   **설명**: 지정된 `key`에 해당하는 값을 검색하여 `out_value`에 저장합니다. `out_value`의 타입 `T`는 저장된 값의 타입과 일치해야 합니다. 타입 불일치 시 `false`를 반환합니다.
-   **매개변수**:
    -   `key`: `const std::string&` - 검색할 데이터의 키.
    -   `out_value`: `T&` - 검색된 값을 저장할 참조 변수.
-   **반환 타입**: `bool` - 성공적으로 값을 검색하고 타입이 일치하면 `true`, 키가 없거나 타입이 일치하지 않으면 `false`.
-   **예외**: 없음

### 4. `bool remove(const std::string& key)`

-   **설명**: 지정된 `key`에 해당하는 데이터를 `DataStore`에서 제거합니다.
-   **매개변수**:
    -   `key`: `const std::string&` - 제거할 데이터의 키.
-   **반환 타입**: `bool` - 성공적으로 제거되면 `true`, 키가 없으면 `false`.
-   **예외**: 없음

### 5. `bool contains(const std::string& key) const`

-   **설명**: `DataStore`에 지정된 `key`가 존재하는지 확인합니다.
-   **매개변수**:
    -   `key`: `const std::string&` - 존재 여부를 확인할 키.
-   **반환 타입**: `bool` - 키가 존재하면 `true`, 그렇지 않으면 `false`.
-   **예외**: 없음

## 변경 없음 보장

`DataStore` 동시성 리팩토링은 이 문서에 명시된 공개 인터페이스의 서명, 동작 또는 의미를 변경하지 않습니다. 기존 `DataStore`를 사용하는 모든 클라이언트 코드는 리팩토링 후에도 수정 없이 정상적으로 작동해야 합니다.
