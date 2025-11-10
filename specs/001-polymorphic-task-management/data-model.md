# 데이터 모델 (Data Model): 다형적 Task 관리 모듈

## 1. Task 엔티티

Task는 시스템에서 관리되고 실행되는 개별 작업을 나타냅니다. 다형적 Task 관리 모듈의 핵심 엔티티로서, 종류에 상관없이 일관된 방식으로 처리될 수 있도록 추상화된 개념입니다.

### 속성:

-   **id (UUID/String)**: Task의 고유 식별자. 시스템 내에서 Task를 유일하게 구분합니다.
-   **name (String)**: Task의 식별 가능한 이름. 사용자 친화적인 이름으로, 중복될 수 없습니다.
-   **type (String)**: Task의 종류를 나타내는 문자열 (예: "DriveToPosition", "LiftPallet"). 이를 통해 Task Factory가 적절한 Task 구현체를 생성할 수 있습니다.
-   **parameters (JSON/Map<String, String>)**: Task 실행에 필요한 동적 파라미터 집합. Task 종류에 따라 달라집니다.
-   **status (Enum)**: Task의 현재 상태 (예: "PENDING", "RUNNING", "COMPLETED", "FAILED", "CANCELLED").
-   **progress (Integer)**: Task의 진행률 (0-100%).
-   **created_at (Timestamp)**: Task가 시스템에 등록된 시간.
-   **updated_at (Timestamp)**: Task의 상태 또는 파라미터가 마지막으로 업데이트된 시간.

### 관계:

-   Task는 TaskManager에 의해 관리됩니다.
-   Task는 특정 `TaskType`에 의해 정의된 로직을 따릅니다.

## 2. TaskManager 엔티티

TaskManager는 Task의 생명주기(정의, 등록, 생성, 실행 요청, 상태 모니터링)를 관리하는 핵심 모듈입니다. 다형적 Task 처리를 위해 `TaskType`에 기반한 Task 구현체 생성을 담당합니다.

### 속성:

-   **registered_task_types (Map<String, TaskTypeDefinition>)**: 시스템에 등록된 Task 유형 정의들의 컬렉션. 각 Task 유형은 고유한 이름과 해당 유형의 Task 인스턴스를 생성할 수 있는 팩토리 함수 또는 클래스 정보를 포함합니다.
-   **active_tasks (Map<UUID, Task>)**: 현재 시스템에 등록되어 관리되는 Task 객체들의 컬렉션.

### 관계:

-   TaskManager는 여러 Task 엔티티를 관리합니다.
-   TaskManager는 `TaskType` 정의를 사용하여 Task 인스턴스를 생성합니다.
-   TaskManager는 외부 저장소 모듈 및 외부 Task 실행기와 전용 어댑터/플러그인 계층을 통해 통합됩니다.

## 3. TaskType 엔티티 (개념적)

TaskType은 Task의 종류를 식별하는 개념적인 엔티티입니다. 실제 구현에서는 Task 유형 이름(String)과 해당 유형의 Task 인스턴스를 생성하는 팩토리 메커니즘으로 표현될 것입니다.

### 속성:

-   **name (String)**: Task 유형의 고유한 이름 (예: "DriveToPosition", "LiftPallet").
-   **description (String)**: Task 유형에 대한 설명.
-   **required_parameters (JSON Schema/Map<String, Type>)**: 해당 Task 유형에 필요한 매개변수 및 그 유형에 대한 정의.
-   **factory_info (String/Reference)**: 해당 Task 유형의 실제 Task 인스턴스를 생성하는 데 필요한 정보 (예: 클래스 이름, 팩토리 함수 포인터).

### 관계:

-   TaskType은 TaskManager에 의해 관리됩니다.
-   각 Task 인스턴스는 하나의 TaskType에 속합니다.
