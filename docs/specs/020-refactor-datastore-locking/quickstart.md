# 빠른 시작 가이드: DataStore 사용법

**날짜**: 2025-11-16
**기능**: DataStore 동시성 리팩토링

## 개요

이 가이드는 `DataStore`의 기본적인 사용법을 설명합니다. `DataStore`는 시스템 전반의 데이터를 키-값 형태로 저장하고 검색하는 데 사용됩니다. `DataStore`는 싱글톤 패턴으로 구현되어 있으며, 모든 모듈에서 동일한 인스턴스에 접근할 수 있습니다.

## 1. DataStore 인스턴스 얻기

`DataStore`는 싱글톤이므로, `instance()` 메서드를 통해 접근합니다.

```cpp
#include "src/core/datastore/DataStore.h" // DataStore 헤더 파일 포함

// DataStore 인스턴스 얻기
mxrc::core::datastore::DataStore& dataStore = mxrc::core::datastore::DataStore::instance();
```

## 2. 데이터 저장 (set)

`set` 메서드를 사용하여 키-값 쌍을 저장합니다. `DataStore`는 다양한 타입의 데이터를 저장할 수 있습니다.

```cpp
#include <string>
#include <vector>

// 정수 값 저장
dataStore.set("robot_id", 123);

// 문자열 값 저장
dataStore.set("robot_status", std::string("RUNNING"));

// 부동 소수점 값 저장
dataStore.set("battery_level", 98.5f);

// 사용자 정의 구조체 또는 복합 타입 저장 (예시)
struct RobotPosition {
    double x, y, z;
};
RobotPosition pos = {10.0, 20.0, 5.0};
dataStore.set("current_position", pos);

// 벡터 저장
std::vector<int> sensor_data = {1, 2, 3, 4, 5};
dataStore.set("sensor_readings", sensor_data);
```

## 3. 데이터 검색 (get)

`get` 메서드를 사용하여 키에 해당하는 값을 검색합니다. 검색 시에는 저장된 값의 타입과 일치하는 타입으로 받아야 합니다.

```cpp
// 정수 값 검색
int robot_id;
if (dataStore.get("robot_id", robot_id)) {
    // 성공적으로 검색됨
    // spdlog::info("Robot ID: {}", robot_id);
} else {
    // 키가 없거나 타입이 일치하지 않음
    // spdlog::warn("Failed to get Robot ID");
}

// 문자열 값 검색
std::string robot_status;
if (dataStore.get("robot_status", robot_status)) {
    // spdlog::info("Robot Status: {}", robot_status);
}

// 부동 소수점 값 검색
float battery_level;
if (dataStore.get("battery_level", battery_level)) {
    // spdlog::info("Battery Level: {}", battery_level);
}

// 사용자 정의 구조체 검색
RobotPosition retrieved_pos;
if (dataStore.get("current_position", retrieved_pos)) {
    // spdlog::info("Retrieved Position: x={}, y={}, z={}", retrieved_pos.x, retrieved_pos.y, retrieved_pos.z);
}

// 잘못된 타입으로 검색 시도 (실패)
double wrong_type_id;
if (!dataStore.get("robot_id", wrong_type_id)) {
    // spdlog::warn("Attempted to get robot_id with wrong type (double), as expected.");
}
```

## 4. 데이터 제거 (remove)

`remove` 메서드를 사용하여 키에 해당하는 데이터를 `DataStore`에서 제거합니다.

```cpp
if (dataStore.remove("robot_status")) {
    // 성공적으로 제거됨
    // spdlog::info("Robot Status removed.");
} else {
    // 키가 없었음
    // spdlog::warn("Robot Status key not found for removal.");
}
```

## 5. 키 존재 여부 확인 (contains)

`contains` 메서드를 사용하여 특정 키가 `DataStore`에 존재하는지 확인합니다.

```cpp
if (dataStore.contains("robot_id")) {
    // spdlog::info("robot_id exists in DataStore.");
} else {
    // spdlog::info("robot_id does not exist in DataStore.");
}
```

## 동시성 고려사항 (리팩토링 후)

리팩토링 후 `DataStore`는 내부적으로 동시성 해시 맵을 사용하여 여러 스레드에서 동시에 안전하게 접근할 수 있습니다. 따라서 위 예시 코드들은 동시성 환경에서도 안전하게 동작할 것입니다.
