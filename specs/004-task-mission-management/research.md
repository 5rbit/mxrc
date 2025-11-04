# 리서치: MissionManager와 영속성 계층 분리

**브랜치**: `004-task-mission-management` | **날짜**: 2025-11-04 | **사양서**: [spec.md](./spec.md)

## 문제 정의

`MissionManager`를 포함한 `Task & Mission Management` 모듈은 현재 데이터 저장 및 복구 로직과 강하게 결합될 가능성이 있습니다. SOLID 원칙, 특히 단일 책임 원칙(SRP)과 의존성 역전 원칙(DIP)을 준수하기 위해, 비즈니스 로직을 영속성 메커니즘의 구체적인 구현으로부터 분리해야 합니다. 이를 통해 시스템의 모듈성, 테스트 용이성, 유연성을 향상시킬 수 있습니다.

## 리서치 목표

- `MissionManager`와 영속성 계층을 효과적으로 분리하기 위한 C++ 디자인 패턴을 식별합니다.
- 분리를 위한 추상 인터페이스를 정의합니다.
- 향후 다양한 데이터 저장소(인메모리, SQLite, PostgreSQL 등)를 쉽게 교체하거나 추가할 수 있는 구조를 제안합니다.

## 리서치 결과 및 결정

웹 리서치 결과, **리포지토리 패턴(Repository Pattern)**과 **의존성 주입(Dependency Injection)**을 함께 사용하는 것이 가장 효과적인 해결책으로 결정되었습니다.

### 1. 결정: 리포지토리 패턴 및 의존성 주입 채택

- **리포지토리 패턴**: 도메인 계층(`MissionManager`)과 데이터 매핑 계층(`DataStore` 구현체) 사이의 중개자 역할을 하는 리포지토리 인터페이스(`IDataStore`)를 도입합니다. `MissionManager`는 이 인터페이스에만 의존하게 되므로, 실제 데이터가 어떻게 저장되고 조회되는지에 대한 세부 사항을 알 필요가 없습니다.

- **의존성 주입**: `MissionManager`가 직접 `DataStore`의 구체적인 인스턴스를 생성하는 대신, 외부(예: `main.cpp`의 Composition Root)에서 생성된 `IDataStore` 구현체의 인스턴스를 생성자를 통해 주입받습니다. 이를 통해 런타임에 사용할 영속성 메커니즘을 유연하게 변경할 수 있으며, 단위 테스트 시 모의(Mock) 객체를 쉽게 주입할 수 있습니다.

### 2. 제안된 인터페이스: `IDataStore.h`

`Task & Mission Management` 모듈의 일부로 순수 가상 클래스인 `IDataStore` 인터페이스를 정의합니다. 이 인터페이스는 영속성에 필요한 핵심 기능들을 추상 메서드로 선언합니다.

```cpp
// specs/004-task-mission-management/contracts/IDataStore.h (예시)

#pragma once

#include <memory>
#include <vector>
#include "MissionStateDto.h" // DTO (Data Transfer Object)
#include "TaskStateDto.h"    // DTO

class IDataStore {
public:
    virtual ~IDataStore() = default;

    // Mission의 현재 상태를 저장하거나 업데이트합니다.
    virtual bool saveMissionState(const MissionStateDto& missionState) = 0;

    // 특정 ID의 Mission 상태를 불러옵니다.
    virtual std::optional<MissionStateDto> loadMissionState(const std::string& missionId) = 0;

    // 특정 Mission의 모든 Task 상태 이력을 저장합니다.
    virtual bool saveTaskHistory(const std::string& missionId, const std::vector<TaskStateDto>& taskHistory) = 0;

    // 특정 Mission의 Task 상태 이력을 불러옵니다.
    virtual std::vector<TaskStateDto> loadTaskHistory(const std::string& missionId) = 0;

    // 시스템 재시작 시 복구해야 할 진행 중이던 Mission 목록을 가져옵니다.
    virtual std::vector<std::string> getPendingMissionIds() = 0;
};
```

### 3. 데이터 전송 객체 (DTOs)

도메인 객체(`Mission`, `Task`)와 영속성 계층 간의 데이터 전송을 위해 DTO를 사용합니다. DTO는 비즈니스 로직이 없는 순수 데이터 구조체입니다.

```cpp
// specs/004-task-mission-management/contracts/MissionStateDto.h (예시)

#pragma once

#include <string>
#include <chrono>

struct MissionStateDto {
    std::string missionId;
    std::string missionStatus; // e.g., "RUNNING", "PAUSED"
    std::chrono::system_clock::time_point lastUpdated;
    // ... 기타 필요한 상태 정보
};
```

## 대안

- **DAO(Data Access Object) 패턴**: 리포지토리 패턴과 유사하지만, 보통 더 저수준의 데이터 접근에 초점을 맞춥니다. 리포지토리 패턴이 도메인 객체의 컬렉션처럼 동작하여 더 도메인 중심적인 접근을 제공하므로, `MissionManager`의 요구사항에 더 적합하다고 판단했습니다.
- **Active Record 패턴**: 도메인 객체 자체가 저장/조회 로직을 갖는 패턴입니다. 이는 도메인 모델과 영속성 로직을 강하게 결합시켜 이번 리팩터링의 목표에 부합하지 않습니다.

## 결론

리포지토리 패턴과 의존성 주입을 통해 `MissionManager`와 `DataStore`를 성공적으로 분리할 수 있습니다. `IDataStore` 인터페이스를 정의하고, `MissionManager`가 이 인터페이스에만 의존하도록 리팩터링을 진행합니다. 실제 `DataStore` 구현체(예: `InMemoryDataStore` for testing, `SQLiteDataStore` for production)는 별도의 모듈에서 개발되며, 런타임에 주입됩니다. 이 접근 방식은 시스템의 유연성과 테스트 용이성을 극대화합니다.